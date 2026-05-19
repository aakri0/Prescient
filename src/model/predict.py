#!/usr/bin/env python3
"""predict.py — compile-time prediction CLI (issue #20).

Takes the IR features of unseen functions and predicts their compile time
*before* the optimization pipeline runs. This is what `./run.sh predict`
calls internally.

    python3 src/model/predict.py \\
        --features features.json \\
        --models-dir models/ \\
        --output predictions.json \\
        --low-threshold 500 --high-threshold 5000
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import joblib
import numpy as np

import _render

# A pass is recommended for skipping when its predicted cost exceeds this
# many microseconds and the function is not already in the "high" tier.
SKIP_COST_THRESHOLD_US = 1000.0

# Columns shown in the IR-feature table — a readable subset covering all six
# feature dimensions. (key, header, kind) where kind is "i" int or "f" float.
FEATURE_VIEW = [
    ("instruction_count",          "Insts",  "i"),
    ("basic_block_count",          "BBs",    "i"),
    ("cyclomatic_complexity",      "Cyclo",  "i"),
    ("loop_count",                 "Loops",  "i"),
    ("max_loop_depth",             "Depth",  "i"),
    ("phi_node_count",             "PHIs",   "i"),
    ("total_memory_ops",           "MemOps", "i"),
    ("alias_proxy_density",        "AliasD", "f"),
    ("type_complexity_normalized", "TypeCx", "f"),
]

# Models predict in log1p space. Clip the log-space prediction before expm1
# so an extrapolating linear model cannot emit an absurd microsecond value.
LOG_PRED_CLIP = 20.0


def predict_us(model, scaled: np.ndarray) -> float:
    """Predict a microsecond value from a log1p-trained model."""
    pred_log = float(np.clip(model.predict(scaled)[0], 0.0, LOG_PRED_CLIP))
    return float(np.expm1(pred_log))


def log(msg: str) -> None:
    print(f"[predict] {msg}", flush=True)


def fmt_value(v: float) -> str:
    """Format a feature value compactly for the tier rationale."""
    if float(v).is_integer():
        return str(int(v))
    return f"{float(v):.2f}"


def classify_tier(total_us: float, low: float, high: float) -> str:
    if total_us < low:
        return "low"
    if total_us < high:
        return "medium"
    return "high"


def confidence_from_scaled(scaled: np.ndarray) -> str:
    """Confidence reflects how far the input sits from the training data.

    `scaled` is the feature vector after StandardScaler, i.e. in units of
    training-set standard deviations. Inputs close to the training
    distribution yield high confidence; far-out inputs (extrapolation)
    yield low confidence.
    """
    max_abs = float(np.max(np.abs(scaled))) if scaled.size else 0.0
    if max_abs <= 2.0:
        return "high"
    if max_abs <= 4.0:
        return "medium"
    return "low"


def predict_one(func: dict, features: list[str], scaler, total_model,
                per_pass_models: dict[str, object], low: float,
                high: float) -> dict:
    """Build the prediction record for a single function."""
    name = func.get("function_name", "<unknown>")

    # Assemble the feature vector in the exact trained order. Missing
    # fields are filled with 0 and reported, never silently ignored.
    raw = []
    for feat in features:
        if feat in func and func[feat] is not None:
            raw.append(float(func[feat]))
        else:
            log(f"WARNING: function '{name}' is missing feature '{feat}' "
                f"— filling with 0")
            raw.append(0.0)
    raw = np.asarray(raw, dtype=float).reshape(1, -1)
    scaled = scaler.transform(raw)

    # --- total compile time ----------------------------------------------
    total_us = predict_us(total_model, scaled)
    tier = classify_tier(total_us, low, high)

    # --- tier rationale: the two features that moved the prediction most --
    # Contribution of feature i to a linear model = coef_i * scaled_value_i.
    coefs = np.asarray(getattr(total_model, "coef_", np.zeros(len(features))),
                       dtype=float).ravel()
    contributions = np.abs(coefs * scaled.ravel())
    top2 = np.argsort(-contributions)[:2]
    rationale = ", ".join(f"{features[i]}={fmt_value(raw[0][i])}" for i in top2)

    # --- per-pass predictions --------------------------------------------
    expensive = []
    for target, model in per_pass_models.items():
        pass_name = target[len("time_"):] if target.startswith("time_") else target
        pass_us = predict_us(model, scaled)
        expensive.append({"pass_name": pass_name,
                          "predicted_us": int(round(pass_us))})
    expensive.sort(key=lambda p: p["predicted_us"], reverse=True)

    # --- skip recommendations --------------------------------------------
    skip = [p["pass_name"] for p in expensive
            if p["predicted_us"] > SKIP_COST_THRESHOLD_US
            and tier in ("low", "medium")]

    return {
        "function_name": name,
        "predicted_total_us": int(round(total_us)),
        "predicted_total_ms": round(total_us / 1000.0, 2),
        "complexity_tier": tier,
        "tier_rationale": rationale,
        "expensive_passes": expensive,
        "skip_recommendation": skip,
        "confidence": confidence_from_scaled(scaled),
    }


def print_feature_table(functions: list[dict]) -> None:
    """Show the extracted IR-complexity features as a table."""
    headers = ["Function"] + [h for _, h, _ in FEATURE_VIEW]
    aligns = ["<"] + [">"] * len(FEATURE_VIEW)
    rows = []
    for f in functions:
        row = [f.get("function_name", "?")]
        for key, _, kind in FEATURE_VIEW:
            v = f.get(key, 0) or 0
            row.append(f"{float(v):.2f}" if kind == "f" else f"{int(v):d}")
        rows.append(row)
    print(_render.banner(f"IR COMPLEXITY FEATURES  ({len(functions)} function(s))"))
    print(_render.render_table(headers, rows, aligns))


def print_prediction_tables(predictions: list[dict],
                            output_path: Path) -> None:
    """Show compile-time predictions and per-pass costs as tables."""
    # --- headline predictions --------------------------------------------
    headers = ["Function", "Tier", "Pred (us)", "Pred (ms)", "Confidence"]
    aligns = ["<", "<", ">", ">", "<"]
    rows = [[p["function_name"], p["complexity_tier"],
             f'{p["predicted_total_us"]:,}',
             f'{p["predicted_total_ms"]:.2f}',
             p["confidence"]] for p in predictions]
    print(_render.banner("COMPILE-TIME PREDICTIONS"))
    print(_render.render_table(headers, rows, aligns))

    # --- predicted per-pass cost -----------------------------------------
    pass_names = sorted({pp["pass_name"]
                         for p in predictions
                         for pp in p["expensive_passes"]})
    if pass_names:
        headers = ["Function"] + pass_names
        aligns = ["<"] + [">"] * len(pass_names)
        rows = []
        for p in predictions:
            by_pass = {pp["pass_name"]: pp["predicted_us"]
                       for pp in p["expensive_passes"]}
            rows.append([p["function_name"]]
                        + [f'{by_pass.get(n, 0):,}' for n in pass_names])
        print(_render.banner("PREDICTED PER-PASS COST  (microseconds)"))
        print(_render.render_table(headers, rows, aligns))

    # --- summary ---------------------------------------------------------
    tiers = {t: sum(1 for p in predictions if p["complexity_tier"] == t)
             for t in ("low", "medium", "high")}
    n_skip = sum(1 for p in predictions if p["skip_recommendation"])
    print()
    print(f"  Tier summary : low={tiers['low']}  medium={tiers['medium']}  "
          f"high={tiers['high']}   (of {len(predictions)} function(s))")
    print(f"  Skip advice  : {n_skip} function(s) have pass(es) worth skipping")
    print(f"  JSON output  : {output_path}  (full detail, machine-readable)")
    print()


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Predict per-function compile time from IR features.")
    parser.add_argument("--features", required=True,
                        help="features.json produced by IRComplexityPass")
    parser.add_argument("--models-dir", default="models",
                        help="Directory containing trained models")
    parser.add_argument("--output", default="predictions.json",
                        help="Path to write predictions.json")
    parser.add_argument("--low-threshold", type=float, default=500.0,
                        help="Upper bound (us) of the 'low' tier")
    parser.add_argument("--high-threshold", type=float, default=5000.0,
                        help="Lower bound (us) of the 'high' tier")
    args = parser.parse_args()

    features_path = Path(args.features)
    models_dir = Path(args.models_dir)
    if not features_path.is_file():
        log(f"ERROR: features file not found: {features_path}")
        return 1
    if args.low_threshold >= args.high_threshold:
        log("ERROR: --low-threshold must be below --high-threshold")
        return 1

    # --- load metadata and models ----------------------------------------
    meta_path = models_dir / "training_metadata.json"
    if not meta_path.is_file():
        log(f"ERROR: {meta_path} not found — run train_model.py first")
        return 1
    meta = json.loads(meta_path.read_text())
    features = meta["features"]
    model_files = meta.get("model_files", {})

    try:
        scaler = joblib.load(models_dir / model_files["feature_scaler"])
        total_model = joblib.load(models_dir / model_files["total_time_linear"])
    except (KeyError, FileNotFoundError) as exc:
        log(f"ERROR: could not load core models: {exc}")
        return 1

    per_pass_models = {}
    for target, fname in model_files.get("per_pass", {}).items():
        path = models_dir / fname
        if path.is_file():
            per_pass_models[target] = joblib.load(path)
        else:
            log(f"WARNING: per-pass model missing for {target} ({fname})")

    # --- load input features ---------------------------------------------
    with features_path.open() as fh:
        functions = json.load(fh)
    if not isinstance(functions, list):
        log("ERROR: features file must contain a JSON array")
        return 1

    predictions = [
        predict_one(func, features, scaler, total_model, per_pass_models,
                    args.low_threshold, args.high_threshold)
        for func in functions
    ]

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w") as fh:
        json.dump(predictions, fh, indent=2)

    # Human-readable tables (the JSON file above keeps the full detail).
    print_feature_table(functions)
    print_prediction_tables(predictions, output_path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
