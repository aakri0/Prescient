#!/usr/bin/env python3
"""evaluate.py — comprehensive evaluation suite (issue #24).

Produces every number that goes into EVALUATION.md.

Mode 1 — Prediction accuracy: for each function in the held-out
testcases/evaluation/ corpus, compare the model's predicted compile time
against the time actually measured by running opt -O2 with the timing
plugin (R2, MAE, RMSE, MAPE, and per-pass MAPE).

Mode 2 — Adaptive pipeline effectiveness: compare baseline full-O2
compilation against the adaptive pipeline, both in compile-time savings
and in the execution speed (code quality) of the resulting binaries.

Outputs:
  docs/evaluation_results.json
  docs/evaluation_plots/prediction_scatter.png
  docs/evaluation_plots/per_pass_accuracy.png
  docs/evaluation_plots/savings_vs_quality.png

Usage:
    python3 scripts/evaluate.py \\
        --eval-dir testcases/evaluation \\
        --models-dir models \\
        --plugin build/IRComplexityEstimator.so \\
        --docs-dir docs
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import tempfile
import time
from pathlib import Path

import joblib
import matplotlib
import numpy as np
import pandas as pd

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

# Per-pass targets the model was trained on (issue #18). LICM is a loop
# pass and is intentionally not timed, so it has no model.
PER_PASS_TARGETS = ["time_GVNPass", "time_LoopVectorizePass",
                    "time_SLPVectorizerPass"]
LOG_PRED_CLIP = 20.0  # matches train_model.py / predict.py
EXEC_REPS = 5         # binary timing repetitions; the minimum is reported


def log(msg: str) -> None:
    print(f"[evaluate] {msg}", flush=True)


def find_tool(candidates: list[str]) -> str | None:
    from shutil import which
    for name in candidates:
        if which(name):
            return name
    return None


def run(cmd: list[str], **kw) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


# --------------------------------------------------------------------------
# model loading / prediction
# --------------------------------------------------------------------------
def load_models(models_dir: Path):
    meta = json.loads((models_dir / "training_metadata.json").read_text())
    mf = meta["model_files"]
    scaler = joblib.load(models_dir / mf["feature_scaler"])
    total = joblib.load(models_dir / mf["total_time_linear"])
    per_pass = {}
    for target, fname in mf.get("per_pass", {}).items():
        path = models_dir / fname
        if path.is_file():
            per_pass[target] = joblib.load(path)
    return meta["features"], scaler, total, per_pass


def feature_matrix(funcs: list[dict], features: list[str]) -> np.ndarray:
    rows = [[float(f.get(name, 0) or 0) for name in features] for f in funcs]
    return np.asarray(rows, dtype=float)


def predict_us(model, scaled: np.ndarray) -> np.ndarray:
    pred_log = np.clip(model.predict(scaled), 0.0, LOG_PRED_CLIP)
    return np.clip(np.expm1(pred_log), 0.0, None)


# --------------------------------------------------------------------------
# metrics
# --------------------------------------------------------------------------
def regression_metrics(actual: np.ndarray, predicted: np.ndarray) -> dict:
    actual = np.asarray(actual, dtype=float)
    predicted = np.asarray(predicted, dtype=float)
    err = predicted - actual
    ss_res = float(np.sum(err ** 2))
    ss_tot = float(np.sum((actual - actual.mean()) ** 2))
    r2 = 1.0 - ss_res / ss_tot if ss_tot > 0 else float("nan")
    mae = float(np.mean(np.abs(err)))
    rmse = float(np.sqrt(np.mean(err ** 2)))
    mask = actual > 0
    mape = (float(np.mean(np.abs(err[mask] / actual[mask])) * 100)
            if mask.any() else float("nan"))
    return {"r2": r2, "mae": mae, "rmse": rmse, "mape": mape}


# --------------------------------------------------------------------------
# per-file pipeline
# --------------------------------------------------------------------------
def per_function_time(csv_path: Path) -> pd.DataFrame:
    if not csv_path.is_file():
        return pd.DataFrame(columns=["function_name", "pass_name", "time_us"])
    return pd.read_csv(csv_path)


def time_binary(binary: Path, reps: int) -> tuple[float, str]:
    """Run a binary `reps` times; return (min wall seconds, stdout)."""
    best = float("inf")
    output = ""
    for _ in range(reps):
        start = time.perf_counter()
        proc = run([str(binary)])
        elapsed = time.perf_counter() - start
        best = min(best, elapsed)
        output = proc.stdout
    return best, output


def process_file(c_file: Path, clang: str, opt: str, plugin: Path,
                 features: list[str], scaler, total_model, per_pass_models,
                 work: Path) -> dict | None:
    """Run the whole baseline/adaptive evaluation for one C file."""
    name = c_file.stem
    ll = work / f"{name}.ll"
    feat_json = work / f"{name}.features.json"
    base_csv = work / f"{name}.baseline.csv"
    adap_csv = work / f"{name}.adaptive.csv"
    pred_json = work / f"{name}.predictions.json"
    base_bc = work / f"{name}.baseline.bc"
    adap_bc = work / f"{name}.adaptive.bc"
    base_bin = work / f"{name}.baseline.bin"
    adap_bin = work / f"{name}.adaptive.bin"

    # 1. compile to IR (optnone disabled so the O2 pipeline actually runs)
    r = run([clang, "-O0", "-Xclang", "-disable-O0-optnone",
             "-emit-llvm", "-S", str(c_file), "-o", str(ll)])
    if r.returncode != 0:
        log(f"ERROR: clang failed for {c_file.name}: {r.stderr.strip()}")
        return None

    # 2. extract features
    r = run([opt, "-load-pass-plugin", str(plugin), "-passes=ir-complexity",
             f"-complexity-output={feat_json}", "-disable-output", str(ll)])
    if r.returncode != 0 or not feat_json.is_file():
        log(f"ERROR: feature extraction failed for {c_file.name}")
        return None
    funcs = json.loads(feat_json.read_text())
    if not funcs:
        log(f"WARNING: {c_file.name} has no functions — skipping")
        return None

    # 3. baseline O2 run (timed)
    t0 = time.perf_counter()
    r = run([opt, "-load-pass-plugin", str(plugin), "-passes=default<O2>",
             f"-timing-output={base_csv}", str(ll), "-o", str(base_bc)])
    baseline_compile_s = time.perf_counter() - t0
    if r.returncode != 0:
        log(f"ERROR: baseline O2 failed for {c_file.name}")
        return None

    # 4. predictions
    scaled = scaler.transform(feature_matrix(funcs, features))
    total_pred = predict_us(total_model, scaled)
    predictions = []
    for i, fn in enumerate(funcs):
        predictions.append({"function_name": fn["function_name"],
                            "complexity_tier": "low" if total_pred[i] < 500
                            else "medium" if total_pred[i] < 5000 else "high"})
    pred_json.write_text(json.dumps(predictions))

    # 5. adaptive run (timed)
    env = {**os.environ, "COMPLEXITY_PREDICTIONS": str(pred_json)}
    t0 = time.perf_counter()
    r = run([opt, "-load-pass-plugin", str(plugin), "-passes=adaptive-pipeline",
             f"-timing-output={adap_csv}", str(ll), "-o", str(adap_bc)],
            env=env)
    adaptive_compile_s = time.perf_counter() - t0
    if r.returncode != 0:
        log(f"ERROR: adaptive run failed for {c_file.name}")
        return None

    # 6. build binaries
    ok = True
    for bc, binary in ((base_bc, base_bin), (adap_bc, adap_bin)):
        rr = run([clang, str(bc), "-o", str(binary)])
        ok = ok and rr.returncode == 0
    if not ok:
        log(f"WARNING: {c_file.name} did not link to a binary — "
            f"skipping its quality measurement")
        base_ms = adap_ms = None
        correct = None
    else:
        # 7. measure execution time + correctness
        base_s, base_out = time_binary(base_bin, EXEC_REPS)
        adap_s, adap_out = time_binary(adap_bin, EXEC_REPS)
        base_ms = base_s * 1000.0
        adap_ms = adap_s * 1000.0
        correct = base_out == adap_out

    # actual per-function / per-pass times from the baseline timing CSV
    base_timing = per_function_time(base_csv)
    actual_total = base_timing.groupby("function_name")["time_us"].sum() \
        if not base_timing.empty else pd.Series(dtype=float)

    func_records = []
    for i, fn in enumerate(funcs):
        fname = fn["function_name"]
        func_records.append({
            "function_name": fname,
            "source_file": c_file.name,
            "predicted_us": float(total_pred[i]),
            "actual_us": float(actual_total.get(fname, 0.0)),
        })

    return {
        "file": c_file.name,
        "functions": func_records,
        "baseline_timing": base_timing,
        "scaled": scaled,
        "func_names": [f["function_name"] for f in funcs],
        "baseline_compile_ms": baseline_compile_s * 1000.0,
        "adaptive_compile_ms": adaptive_compile_s * 1000.0,
        "baseline_exec_ms": base_ms,
        "adaptive_exec_ms": adap_ms,
        "correct": correct,
    }


# --------------------------------------------------------------------------
# plotting
# --------------------------------------------------------------------------
def plot_scatter(records: list[dict], r2: float, out: Path) -> None:
    fig, ax = plt.subplots(figsize=(8, 7))
    normal_x, normal_y, fail_x, fail_y, fail_names = [], [], [], [], []
    for rec in records:
        x = max(rec["actual_us"], 1.0)
        y = max(rec["predicted_us"], 1.0)
        if "test07" in rec["source_file"]:
            fail_x.append(x); fail_y.append(y)
            fail_names.append(rec["function_name"])
        else:
            normal_x.append(x); normal_y.append(y)

    lo = 1.0
    hi = max([1.0] + normal_x + normal_y + fail_x + fail_y) * 1.5
    ax.plot([lo, hi], [lo, hi], "k--", linewidth=1,
            label="perfect prediction")
    ax.scatter(normal_x, normal_y, c="#1f77b4", s=55, alpha=0.8,
               label="evaluation functions")
    if fail_x:
        ax.scatter(fail_x, fail_y, c="#d62728", s=110, marker="X",
                   label="failure case (test07)", zorder=5)
        for fx, fy, fn in zip(fail_x, fail_y, fail_names):
            ax.annotate(f"  {fn} (failure case)", (fx, fy),
                        fontsize=9, color="#d62728", va="center")
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlim(lo, hi)
    ax.set_ylim(lo, hi)
    ax.set_xlabel("Actual total compile time (µs)")
    ax.set_ylabel("Predicted total compile time (µs)")
    ax.set_title(f"Predicted vs actual compile time  (R² = {r2:.3f})")
    ax.legend(loc="lower right")
    ax.grid(True, which="both", linestyle=":", alpha=0.4)
    fig.tight_layout()
    fig.savefig(out, dpi=150, bbox_inches="tight")
    plt.close(fig)


def plot_per_pass(per_pass_mape: dict, out: Path) -> None:
    fig, ax = plt.subplots(figsize=(8, 5))
    names = list(per_pass_mape.keys())
    values = [per_pass_mape[n] for n in names]
    ax.bar(names, values, color="#ff7f0e")
    ax.set_ylabel("MAPE (%)")
    ax.set_title("Per-pass prediction accuracy (lower is better)")
    for i, v in enumerate(values):
        ax.text(i, v, f"{v:.0f}%", ha="center", va="bottom", fontsize=9)
    fig.tight_layout()
    fig.savefig(out, dpi=150, bbox_inches="tight")
    plt.close(fig)


def plot_savings_vs_quality(per_file: list[dict], out: Path) -> None:
    fig, ax = plt.subplots(figsize=(8, 6))
    xs = [r["compile_savings_pct"] for r in per_file]
    ys = [r["quality_regression_pct"] for r in per_file]
    ax.scatter(xs, ys, c="#2ca02c", s=70)
    for r in per_file:
        ax.annotate(f"  {r['file']}",
                    (r["compile_savings_pct"], r["quality_regression_pct"]),
                    fontsize=8, va="center")
    ax.axhline(0.0, color="black", linewidth=0.8)
    ax.axvline(0.0, color="black", linewidth=0.8)
    ax.set_xlabel("Compile-time savings (%)")
    ax.set_ylabel("Code quality regression (%)")
    ax.set_title("Adaptive pipeline: compile savings vs quality regression")
    ax.grid(True, linestyle=":", alpha=0.4)
    fig.tight_layout()
    fig.savefig(out, dpi=150, bbox_inches="tight")
    plt.close(fig)


# --------------------------------------------------------------------------
# main
# --------------------------------------------------------------------------
def main() -> int:
    parser = argparse.ArgumentParser(description="Run the evaluation suite.")
    parser.add_argument("--eval-dir", default="testcases/evaluation")
    parser.add_argument("--models-dir", default="models")
    parser.add_argument("--plugin", default="build/IRComplexityEstimator.so")
    parser.add_argument("--docs-dir", default="docs")
    args = parser.parse_args()

    eval_dir = Path(args.eval_dir)
    models_dir = Path(args.models_dir)
    plugin = Path(args.plugin).resolve()
    docs_dir = Path(args.docs_dir)
    plots_dir = docs_dir / "evaluation_plots"

    if not (models_dir / "training_metadata.json").is_file():
        log(f"ERROR: no trained model in {models_dir} — run training first")
        return 1
    if not plugin.is_file():
        log(f"ERROR: plugin not found: {plugin} — run ./build.sh first")
        return 1

    clang = find_tool(["clang-17", "clang"])
    opt = find_tool(["opt-17", "opt"])
    if not clang or not opt:
        log("ERROR: clang-17/opt-17 not found — run scripts/setup_env.sh")
        return 1

    c_files = sorted(eval_dir.glob("*.c"))
    if not c_files:
        log(f"ERROR: no .c files in {eval_dir}")
        return 1

    features, scaler, total_model, per_pass_models = load_models(models_dir)

    results = []
    with tempfile.TemporaryDirectory(prefix="evaluate_") as tmp:
        work = Path(tmp)
        for c_file in c_files:
            log(f"evaluating {c_file.name}")
            res = process_file(c_file, clang, opt, plugin, features, scaler,
                               total_model, per_pass_models, work)
            if res is not None:
                results.append(res)

    if not results:
        log("ERROR: no evaluation files processed successfully")
        return 1

    # --- Mode 1: prediction accuracy -------------------------------------
    func_records = [fr for res in results for fr in res["functions"]]
    actual = np.array([fr["actual_us"] for fr in func_records])
    predicted = np.array([fr["predicted_us"] for fr in func_records])
    overall = regression_metrics(actual, predicted)

    # per-pass MAPE: predicted (per-pass model) vs actual (timing CSV)
    per_pass_mape = {}
    for target, model in per_pass_models.items():
        pass_name = target[len("time_"):]
        pred_list, act_list = [], []
        for res in results:
            timing = res["baseline_timing"]
            by_pass = (timing[timing["pass_name"] == pass_name]
                       .groupby("function_name")["time_us"].sum()
                       if not timing.empty else pd.Series(dtype=float))
            pp_pred = predict_us(model, res["scaled"])
            for i, fname in enumerate(res["func_names"]):
                pred_list.append(pp_pred[i])
                act_list.append(float(by_pass.get(fname, 0.0)))
        per_pass_mape[pass_name] = regression_metrics(
            np.array(act_list), np.array(pred_list))["mape"]

    # --- Mode 2: adaptive effectiveness ----------------------------------
    per_file = []
    for res in results:
        bc = res["baseline_compile_ms"]
        ac = res["adaptive_compile_ms"]
        savings = (bc - ac) / bc * 100.0 if bc > 0 else 0.0
        be, ae = res["baseline_exec_ms"], res["adaptive_exec_ms"]
        if be and be > 0 and ae is not None:
            regression = (ae - be) / be * 100.0
        else:
            regression = 0.0
        per_file.append({
            "file": res["file"],
            "baseline_compile_ms": bc,
            "adaptive_compile_ms": ac,
            "compile_savings_pct": savings,
            "baseline_exec_ms": be,
            "adaptive_exec_ms": ae,
            "quality_regression_pct": regression,
            "output_matches": res["correct"],
        })

    total_base_compile = sum(r["baseline_compile_ms"] for r in per_file)
    total_adap_compile = sum(r["adaptive_compile_ms"] for r in per_file)
    compile_savings_pct = ((total_base_compile - total_adap_compile)
                           / total_base_compile * 100.0
                           if total_base_compile > 0 else 0.0)
    avg_quality_regression = float(np.mean(
        [r["quality_regression_pct"] for r in per_file]))
    over_5pct = sum(1 for r in per_file if r["quality_regression_pct"] > 5.0)

    # --- write outputs ---------------------------------------------------
    plots_dir.mkdir(parents=True, exist_ok=True)
    plot_scatter(func_records, overall["r2"],
                 plots_dir / "prediction_scatter.png")
    plot_per_pass(per_pass_mape, plots_dir / "per_pass_accuracy.png")
    plot_savings_vs_quality(per_file, plots_dir / "savings_vs_quality.png")

    results_json = {
        "prediction_accuracy": {
            "model": "LinearRegression",
            "n_functions": len(func_records),
            "r2": overall["r2"],
            "mae_us": overall["mae"],
            "rmse_us": overall["rmse"],
            "mape_pct": overall["mape"],
            "per_pass_mape_pct": per_pass_mape,
            "functions": func_records,
        },
        "adaptive_effectiveness": {
            "compile_time_savings_pct": compile_savings_pct,
            "avg_quality_regression_pct": avg_quality_regression,
            "files_over_5pct_regression": over_5pct,
            "n_files": len(per_file),
            "per_file": per_file,
        },
    }
    (docs_dir / "evaluation_results.json").write_text(
        json.dumps(results_json, indent=2))

    log(f"wrote {docs_dir}/evaluation_results.json and 3 plots in {plots_dir}/")

    # --- required stdout block (copy-paste into EVALUATION.md) -----------
    print()
    print("=== PREDICTION ACCURACY ===")
    print("Model: LinearRegression")
    print(f"R²: {overall['r2']:.2f}   MAE: {overall['mae']:.0f}µs   "
          f"RMSE: {overall['rmse']:.0f}µs   MAPE: {overall['mape']:.1f}%")
    print("=== ADAPTIVE PIPELINE ===")
    print(f"Compile-time savings: {compile_savings_pct:.1f}%")
    print(f"Code quality regression (avg): {avg_quality_regression:.1f}%")
    print(f"Functions with >5% quality regression: "
          f"{over_5pct} / {len(per_file)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
