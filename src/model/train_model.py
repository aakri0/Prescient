#!/usr/bin/env python3
"""train_model.py — train compile-time prediction models (issue #18).

Learns to predict per-function compile time from IR-complexity features.
The models are intentionally simple: the academic value of the project is
the feature engineering and data-collection infrastructure, not the model.

Pipeline:
  1. Load the joined training CSV produced by generate_corpus.py
  2. Drop rows with NaN feature/target values
  3. 5-fold cross-validate LinearRegression, Ridge and RandomForest on the
     primary target (log1p-transformed), printing a comparison table
  4. Fit the final models on all data and save them to models/
  5. Fit a per-pass LinearRegression for each tracked pass
  6. Save a StandardScaler and training_metadata.json
  7. Hand off to feature_importance.py for the importance report

Usage:
    python3 src/model/train_model.py \\
        --data training_data.csv --models-dir models/ --docs-dir docs/
"""
from __future__ import annotations

import argparse
import json
import sys
from datetime import datetime, timezone
from pathlib import Path

import joblib
import numpy as np
import pandas as pd
from sklearn.ensemble import RandomForestRegressor
from sklearn.linear_model import LinearRegression, Ridge
from sklearn.metrics import mean_absolute_error, r2_score
from sklearn.model_selection import KFold
from sklearn.preprocessing import StandardScaler

import _render

# Feature columns (X) — the exact set and order is recorded in
# training_metadata.json and must be reused verbatim by predict.py.
FEATURE_COLUMNS = [
    "instruction_count",
    "basic_block_count",
    "argument_count",
    "cyclomatic_complexity",
    "max_loop_depth",
    "loop_count",
    "loop_instruction_ratio",
    "phi_density",
    "max_phi_in_single_bb",
    "type_complexity_normalized",
    "alias_proxy_density",
    "max_pointer_depth",
]

PRIMARY_TARGET = "total_compile_time_us"

# Per-pass targets — one interpretable LinearRegression is trained per pass.
PER_PASS_TARGETS = [
    "time_GVNPass",
    "time_LICMPass",
    "time_LoopVectorizePass",
    "time_SLPVectorizerPass",
]

RANDOM_STATE = 42

# Models are fit in log1p space. A linear model fit on few samples can
# extrapolate wildly; clipping the log-space prediction before expm1 keeps
# microsecond-scale error metrics finite. expm1(20) is roughly 485 seconds.
LOG_PRED_CLIP = 20.0


def log(msg: str) -> None:
    print(f"[train_model] {msg}", flush=True)


def mape(y_true: np.ndarray, y_pred: np.ndarray) -> float:
    """Mean absolute percentage error (%), ignoring zero-valued targets."""
    y_true = np.asarray(y_true, dtype=float)
    y_pred = np.asarray(y_pred, dtype=float)
    mask = y_true > 0
    if not mask.any():
        return float("nan")
    return float(np.mean(np.abs((y_true[mask] - y_pred[mask]) / y_true[mask])) * 100)


def make_models() -> dict:
    """Return a fresh, unfitted instance of each model under comparison."""
    return {
        "LinearRegression": LinearRegression(),
        "Ridge": Ridge(alpha=1.0),
        "RandomForest": RandomForestRegressor(
            n_estimators=100, random_state=RANDOM_STATE
        ),
    }


def cross_validate(X: np.ndarray, y: np.ndarray, n_splits: int) -> dict:
    """5-fold CV for every model on the log1p-transformed primary target.

    The scaler is refitted inside each fold so the reported scores do not
    leak information from the held-out fold. R2 is reported on the log1p
    scale (the space the models are actually fit in, which avoids expm1
    blow-up dominating the score); MAE and MAPE are on the microsecond
    scale so they stay directly interpretable.
    """
    kf = KFold(n_splits=n_splits, shuffle=True, random_state=RANDOM_STATE)
    scores = {name: {"r2": [], "mae": [], "mape": []} for name in make_models()}

    for train_idx, test_idx in kf.split(X):
        scaler = StandardScaler().fit(X[train_idx])
        X_tr = scaler.transform(X[train_idx])
        X_te = scaler.transform(X[test_idx])
        y_tr_log = np.log1p(y[train_idx])
        y_te_log = np.log1p(y[test_idx])

        for name, model in make_models().items():
            model.fit(X_tr, y_tr_log)
            pred_log = np.clip(model.predict(X_te), 0.0, LOG_PRED_CLIP)
            pred_us = np.clip(np.expm1(pred_log), 0.0, None)
            scores[name]["r2"].append(r2_score(y_te_log, pred_log))
            scores[name]["mae"].append(mean_absolute_error(y[test_idx], pred_us))
            scores[name]["mape"].append(mape(y[test_idx], pred_us))

    return {
        name: {
            "r2": float(np.mean(s["r2"])),
            "mae": float(np.mean(s["mae"])),
            "mape": float(np.nanmean(s["mape"])),
        }
        for name, s in scores.items()
    }


def print_cv_table(cv: dict) -> None:
    """Print the cross-validation comparison as a table, marking the best."""
    print(_render.banner("MODEL COMPARISON  (5-fold cross-validation)"))
    print("  R2 is on the log1p scale; MAE and MAPE are on the microsecond")
    print("  scale. Higher R2 is better; lower MAE and MAPE are better.")
    print()
    best = max(cv, key=lambda m: cv[m]["r2"])
    headers = ["Model", "R2", "MAE (us)", "MAPE (%)", ""]
    aligns = ["<", ">", ">", ">", "<"]
    rows = [[name, f'{s["r2"]:.4f}', f'{s["mae"]:,.1f}', f'{s["mape"]:.1f}',
             "<- best R2" if name == best else ""]
            for name, s in cv.items()]
    print(_render.render_table(headers, rows, aligns))


def fit_and_save(model, X_scaled: np.ndarray, y: np.ndarray, path: Path) -> None:
    """Fit a model on the log1p target and persist it."""
    model.fit(X_scaled, np.log1p(y))
    joblib.dump(model, path)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Train compile-time prediction models from IR features.")
    parser.add_argument("--data", default="training_data.csv",
                        help="Joined training CSV from generate_corpus.py")
    parser.add_argument("--models-dir", default="models",
                        help="Directory to write model/scaler/metadata files")
    parser.add_argument("--docs-dir", default="docs",
                        help="Directory for the feature-importance report")
    parser.add_argument("--no-report", action="store_true",
                        help="Skip the feature-importance report step")
    args = parser.parse_args()

    data_path = Path(args.data)
    models_dir = Path(args.models_dir)
    models_dir.mkdir(parents=True, exist_ok=True)

    if not data_path.is_file():
        log(f"ERROR: training data not found: {data_path}")
        return 1

    df = pd.read_csv(data_path)

    missing = [c for c in FEATURE_COLUMNS + [PRIMARY_TARGET] if c not in df.columns]
    if missing:
        log(f"ERROR: training data is missing required columns: {missing}")
        return 1

    # Drop rows with NaN in any feature or in the primary target.
    before = len(df)
    df = df.dropna(subset=FEATURE_COLUMNS + [PRIMARY_TARGET]).reset_index(drop=True)
    dropped = before - len(df)

    print(_render.banner("TRAINING DATA"))
    print(_render.render_table(
        ["Property", "Value"],
        [["source CSV", str(data_path)],
         ["rows loaded", str(before)],
         ["rows dropped (NaN)", str(dropped)],
         ["usable rows", str(len(df))],
         ["feature columns", str(len(FEATURE_COLUMNS))],
         ["primary target", PRIMARY_TARGET]],
        ["<", "<"]))

    if len(df) < 10:
        log(f"WARNING: only {len(df)} usable rows — results will be unreliable "
            f"(issue #18 expects >= 10 functions)")
    if len(df) < 2:
        log("ERROR: not enough data to train")
        return 1

    X = df[FEATURE_COLUMNS].to_numpy(dtype=float)
    y = df[PRIMARY_TARGET].to_numpy(dtype=float)

    # --- cross-validation comparison --------------------------------------
    n_splits = min(5, len(df))
    cv = cross_validate(X, y, n_splits)
    print_cv_table(cv)

    if cv["LinearRegression"]["r2"] < 0.5:
        log(f"WARNING: LinearRegression R2 = {cv['LinearRegression']['r2']:.4f} "
            f"(< 0.5). The dataset is likely too small or too noisy — collect "
            f"more training samples before trusting predictions.")

    # --- fit and save the final models on all data ------------------------
    scaler = StandardScaler().fit(X)
    X_scaled = scaler.transform(X)
    joblib.dump(scaler, models_dir / "feature_scaler.joblib")

    primary_model_files = {
        "total_time_linear": ("total_time_linear.joblib", LinearRegression()),
        "total_time_ridge": ("total_time_ridge.joblib", Ridge(alpha=1.0)),
        "total_time_rf": ("total_time_rf.joblib",
                          RandomForestRegressor(n_estimators=100,
                                                random_state=RANDOM_STATE)),
    }
    for key, (fname, model) in primary_model_files.items():
        fit_and_save(model, X_scaled, y, models_dir / fname)

    # --- per-pass LinearRegression models ---------------------------------
    per_pass_files: dict[str, str] = {}
    for target in PER_PASS_TARGETS:
        if target not in df.columns:
            log(f"WARNING: per-pass target '{target}' not in training data "
                f"— skipping that model")
            continue
        y_pass = df[target].to_numpy(dtype=float)
        pass_name = target[len("time_"):]
        fname = f"per_pass_{pass_name}_linear.joblib"
        fit_and_save(LinearRegression(), X_scaled, y_pass, models_dir / fname)
        per_pass_files[target] = fname

    # --- training metadata ------------------------------------------------
    metadata = {
        "features": FEATURE_COLUMNS,
        "primary_target": PRIMARY_TARGET,
        "per_pass_targets": list(per_pass_files.keys()),
        "target_transform": "log1p",
        "trained_at": datetime.now(timezone.utc).isoformat(),
        "n_samples": len(df),
        "n_rows_dropped_nan": dropped,
        "model_files": {
            "feature_scaler": "feature_scaler.joblib",
            "total_time_linear": "total_time_linear.joblib",
            "total_time_ridge": "total_time_ridge.joblib",
            "total_time_rf": "total_time_rf.joblib",
            "per_pass": per_pass_files,
        },
        "cv_scores": cv,
    }
    with (models_dir / "training_metadata.json").open("w") as fh:
        json.dump(metadata, fh, indent=2)

    # --- saved-artifacts summary -----------------------------------------
    artifact_rows = [
        ["Feature scaler", f"{models_dir}/feature_scaler.joblib"],
        ["Total-time model (LinearRegression)",
         f"{models_dir}/total_time_linear.joblib"],
        ["Total-time model (Ridge)", f"{models_dir}/total_time_ridge.joblib"],
        ["Total-time model (RandomForest)",
         f"{models_dir}/total_time_rf.joblib"],
    ]
    for target, fname in per_pass_files.items():
        artifact_rows.append([f"Per-pass model ({target[len('time_'):]})",
                              f"{models_dir}/{fname}"])
    artifact_rows.append(["Training metadata",
                          f"{models_dir}/training_metadata.json"])
    print(_render.banner("SAVED ARTIFACTS"))
    print(_render.render_table(["Artifact", "File"], artifact_rows, ["<", "<"]))

    # --- feature-importance report (issue #19) ----------------------------
    if not args.no_report:
        try:
            sys.path.insert(0, str(Path(__file__).resolve().parent))
            import feature_importance

            feature_importance.generate_report(
                df, FEATURE_COLUMNS, PRIMARY_TARGET, models_dir,
                Path(args.docs_dir))
        except Exception as exc:  # noqa: BLE001 - report is non-critical
            log(f"WARNING: feature-importance report failed: {exc}")

    log("training complete")
    return 0


if __name__ == "__main__":
    sys.exit(main())
