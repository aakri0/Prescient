#!/usr/bin/env python3
"""Prescient web frontend — backend.

A small Flask app that drives the existing analysis pipeline from
submitted source code. POST /api/analyze compiles the code to IR with
clang-17, runs the IRComplexityPass plugin, and (when a trained model is
present) runs predict.py — returning everything as JSON to the SPA.

Designed for local use:
    python3 frontend/server.py            # http://localhost:8080
    docker compose up frontend            # same, in the reproducible image
"""
from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from shutil import which

import re

from flask import Flask, jsonify, request, send_from_directory

REPO_ROOT = Path(__file__).resolve().parent.parent
FRONTEND  = Path(__file__).resolve().parent
PLUGIN    = REPO_ROOT / "build" / "IRComplexityEstimator.so"
MODELS    = REPO_ROOT / "models"
PREDICT   = REPO_ROOT / "src" / "model" / "predict.py"
CXXFILT   = which("c++filt") or which("llvm-cxxfilt-17") or which("llvm-cxxfilt")

CLANG = which("clang-17") or which("clang")
OPT   = which("opt-17")   or which("opt")

app = Flask(__name__, static_folder=str(FRONTEND), static_url_path="")


def demangle_names(names: list[str]) -> dict[str, str]:
    """Batch-demangle C++ names via c++filt (identity for C names)."""
    if not CXXFILT or not names:
        return {n: n for n in names}
    try:
        r = subprocess.run(
            [CXXFILT], input="\n".join(names),
            capture_output=True, text=True, timeout=5)
        demangled = r.stdout.strip().split("\n")
        if len(demangled) == len(names):
            return dict(zip(names, demangled))
    except Exception:
        pass
    return {n: n for n in names}


def source_function_names(code: str) -> set[str]:
    """Extract function names that appear to be *defined* in the source.

    Simple regex match — catches ``type name(...)  {`` patterns. Used to
    filter out compiler-generated or library-inlined functions.
    """
    # Match: optional return type, function name, paren-list, opening brace
    pattern = re.compile(
        r'(?:^|[\n;{}])\s*'                     # start of line / after statement
        r'(?:[\w* \t]+?)'                        # return type (greedy-reluctant)
        r'\b(\w+)\s*\([^)]*\)\s*(?:const\s*)?'  # func_name(args)
        r'\{',                                    # opening brace
        re.MULTILINE)
    return {m.group(1) for m in pattern.finditer(code)}


def load_model_metrics() -> dict | None:
    """Return model accuracy metrics from training_metadata.json."""
    meta_path = MODELS / "training_metadata.json"
    if not meta_path.is_file():
        return None
    try:
        meta = json.loads(meta_path.read_text())
        return {
            "cv_scores": meta.get("cv_scores", {}),
            "n_samples": meta.get("n_samples"),
            "trained_at": meta.get("trained_at"),
            "primary_target": meta.get("primary_target"),
            "target_transform": meta.get("target_transform"),
            "per_pass_targets": meta.get("per_pass_targets", []),
        }
    except Exception:
        return None


@app.get("/")
def index():
    return send_from_directory(FRONTEND, "index.html")


@app.get("/api/health")
def health():
    return jsonify({
        "plugin_present": PLUGIN.is_file(),
        "models_present": (MODELS / "training_metadata.json").is_file(),
        "clang": CLANG or "MISSING",
        "opt":   OPT   or "MISSING",
        "model_metrics": load_model_metrics(),
    })


@app.post("/api/analyze")
def analyze():
    data = request.get_json(silent=True) or {}
    code = (data.get("code") or "").strip()
    lang = data.get("language", "c")
    if not code:
        return jsonify({"error": "no source code provided"}), 400
    if not CLANG or not OPT:
        return jsonify({"error": "clang-17 / opt-17 not found on PATH"}), 503
    if not PLUGIN.is_file():
        return jsonify({"error": f"plugin not built at {PLUGIN} — run ./build.sh"}), 503

    suffix = ".cpp" if lang == "cpp" else ".c"
    started = time.perf_counter()

    with tempfile.TemporaryDirectory(prefix="prescient_web_") as tmp:
        tmp = Path(tmp)
        src  = tmp / f"input{suffix}"
        ll   = tmp / "input.ll"
        feat = tmp / "features.json"
        pred = tmp / "predictions.json"
        src.write_text(code)

        # 1) compile to IR — disable-O0-optnone so the IR is usable downstream
        r = subprocess.run(
            [CLANG, "-O0", "-Xclang", "-disable-O0-optnone",
             "-emit-llvm", "-S", str(src), "-o", str(ll)],
            capture_output=True, text=True)
        if r.returncode != 0:
            return jsonify({
                "stage": "compile",
                "error": (r.stderr or "clang failed").strip(),
            })

        # 2) run the IR-complexity pass
        r = subprocess.run(
            [OPT, "-load-pass-plugin", str(PLUGIN),
             "-passes=ir-complexity",
             f"-complexity-output={feat}",
             "-disable-output", str(ll)],
            capture_output=True, text=True)
        if r.returncode != 0 or not feat.is_file():
            return jsonify({
                "stage": "extract",
                "error": (r.stderr or "feature extraction failed").strip(),
            })

        features = json.loads(feat.read_text())

        # 3) optimized-IR comparison: run -O2 then re-extract features
        features_o2 = None
        opt_ll = tmp / "optimized.ll"
        opt_feat = tmp / "features_o2.json"
        r2 = subprocess.run(
            [OPT, "-O2", "-S", str(ll), "-o", str(opt_ll)],
            capture_output=True, text=True)
        if r2.returncode == 0 and opt_ll.is_file():
            r2 = subprocess.run(
                [OPT, "-load-pass-plugin", str(PLUGIN),
                 "-passes=ir-complexity",
                 f"-complexity-output={opt_feat}",
                 "-disable-output", str(opt_ll)],
                capture_output=True, text=True)
            if r2.returncode == 0 and opt_feat.is_file():
                features_o2 = json.loads(opt_feat.read_text())

        # 4) predict — only if a trained model is available
        predictions = None
        predict_log = None
        if (MODELS / "training_metadata.json").is_file():
            r = subprocess.run(
                [sys.executable, str(PREDICT),
                 "--features", str(feat),
                 "--models-dir", str(MODELS),
                 "--output", str(pred)],
                capture_output=True, text=True, cwd=str(REPO_ROOT))
            if r.returncode == 0 and pred.is_file():
                predictions = json.loads(pred.read_text())
            else:
                predict_log = (r.stderr or r.stdout or "predict.py failed").strip()

        # 5) Demangle function names (C++ mangling → source names)
        all_names = [f["function_name"] for f in features]
        name_map = demangle_names(all_names)
        for f in features:
            f["display_name"] = name_map.get(f["function_name"],
                                              f["function_name"])
        if features_o2:
            o2_names = [f["function_name"] for f in features_o2]
            o2_name_map = demangle_names(o2_names)
            for f in features_o2:
                f["display_name"] = o2_name_map.get(f["function_name"],
                                                     f["function_name"])
        if predictions:
            for p in predictions:
                p["display_name"] = name_map.get(p["function_name"],
                                                  p["function_name"])

        # 6) Filter to functions defined in the user's source code,
        #    so #include'd library internals don't clutter the output.
        user_funcs = source_function_names(code)
        if user_funcs:
            def is_user(name: str) -> bool:
                return name in user_funcs

            features = [f for f in features if is_user(f["function_name"])]
            if features_o2:
                features_o2 = [f for f in features_o2
                               if is_user(f["function_name"])]
            if predictions:
                predictions = [p for p in predictions
                               if is_user(p["function_name"])]

        elapsed_ms = int((time.perf_counter() - started) * 1000)
        return jsonify({
            "features": features,
            "features_o2": features_o2,
            "predictions": predictions,
            "models_available": predictions is not None,
            "predict_log": predict_log,
            "elapsed_ms": elapsed_ms,
            "language": lang,
            "model_metrics": load_model_metrics(),
        })


def main() -> int:
    host = os.environ.get("HOST", "0.0.0.0")
    port = int(os.environ.get("PORT", 8080))
    debug = os.environ.get("FLASK_DEBUG", "").lower() in ("1", "true", "yes")

    plugin_ok = PLUGIN.is_file()
    models_ok = (MODELS / "training_metadata.json").is_file()
    print(f"\n  Prescient web UI running at: http://localhost:{port}\n"
          f"  plugin={'OK' if plugin_ok else 'MISSING'}  "
          f"models={'OK' if models_ok else 'MISSING'}\n"
          f"  Press Ctrl+C to stop.\n",
          flush=True)

    # Suppress Werkzeug's multi-URL listing ("Running on 0.0.0.0 / 127.0.0.1
    # / 172.x.y.z") — that confused users into thinking three sites were running.
    # Our custom message above is the single clean output.
    import logging
    logging.getLogger("werkzeug").setLevel(logging.ERROR)

    app.run(host=host, port=port, debug=debug, use_reloader=False)
    return 0


if __name__ == "__main__":
    sys.exit(main())
