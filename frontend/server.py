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

from flask import Flask, jsonify, request, send_from_directory

REPO_ROOT = Path(__file__).resolve().parent.parent
FRONTEND  = Path(__file__).resolve().parent
PLUGIN    = REPO_ROOT / "build" / "IRComplexityEstimator.so"
MODELS    = REPO_ROOT / "models"
PREDICT   = REPO_ROOT / "src" / "model" / "predict.py"

CLANG = which("clang-17") or which("clang")
OPT   = which("opt-17")   or which("opt")

app = Flask(__name__, static_folder=str(FRONTEND), static_url_path="")


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

        # 3) predict — only if a trained model is available
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

        elapsed_ms = int((time.perf_counter() - started) * 1000)
        return jsonify({
            "features": features,
            "predictions": predictions,
            "models_available": predictions is not None,
            "predict_log": predict_log,
            "elapsed_ms": elapsed_ms,
            "language": lang,
        })


def main() -> int:
    host = os.environ.get("HOST", "0.0.0.0")
    port = int(os.environ.get("PORT", 8080))
    debug = os.environ.get("FLASK_DEBUG", "").lower() in ("1", "true", "yes")
    print(f"[prescient-web] serving on http://{host}:{port}  "
          f"(plugin={'OK' if PLUGIN.is_file() else 'MISSING'}, "
          f"models={'OK' if (MODELS/'training_metadata.json').is_file() else 'MISSING'})",
          flush=True)
    app.run(host=host, port=port, debug=debug)
    return 0


if __name__ == "__main__":
    sys.exit(main())
