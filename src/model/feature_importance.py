#!/usr/bin/env python3
"""feature_importance.py — feature-importance report (issue #19).

Generates three artefacts that explain which IR features drive compile
time, demonstrating that the chosen features are meaningful:

  * docs/feature_importance.png       — bar chart of linear coefficients
  * docs/correlation_heatmap.png      — feature/target correlation heatmap
  * docs/feature_importance_report.md — markdown table + surprising findings

It is normally invoked automatically at the end of train_model.py, but it
can also be run standalone once models/ and a training CSV exist:

    python3 src/model/feature_importance.py \\
        --data training_data.csv --models-dir models/ --docs-dir docs/
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import joblib
import matplotlib
import numpy as np
import pandas as pd

matplotlib.use("Agg")  # headless: no display needed
import matplotlib.pyplot as plt  # noqa: E402
import seaborn as sns  # noqa: E402

# Correlations weaker than this are flagged as "surprising" (issue #19).
WEAK_CORRELATION = 0.1


def log(msg: str) -> None:
    print(f"[feature_importance] {msg}", flush=True)


def _interpretation(rank: int, abs_coef: float, max_abs: float) -> str:
    """Human-readable interpretation for a feature's coefficient rank."""
    ordinals = {0: "Strongest single predictor",
                1: "Second strongest predictor",
                2: "Third strongest predictor"}
    if rank in ordinals:
        return ordinals[rank]
    if max_abs > 0 and abs_coef >= 0.25 * max_abs:
        return "Notable predictor"
    return "Minor predictor"


def _plot_coefficients(features: list[str], coefs: np.ndarray,
                       out_path: Path) -> None:
    """Horizontal bar chart of linear coefficients, sorted by value.

    Green bars predict cheaper functions (negative coefficient), red bars
    predict more expensive functions (positive coefficient).
    """
    order = np.argsort(coefs)
    names = [features[i] for i in order]
    values = coefs[order]
    colours = ["#2ca02c" if v < 0 else "#d62728" for v in values]

    fig, ax = plt.subplots(figsize=(10, 0.55 * len(features) + 1.5))
    ax.barh(names, values, color=colours)
    ax.axvline(0.0, color="black", linewidth=0.8)
    ax.set_xlabel("LinearRegression coefficient (standardised features)")
    ax.set_title("IR feature importance for total compile time")
    ax.margins(y=0.01)
    fig.tight_layout()  # keep every label fully readable
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close(fig)


def _plot_heatmap(df: pd.DataFrame, features: list[str], target: str,
                  out_path: Path) -> None:
    """Seaborn heatmap of Pearson correlation across features + target."""
    cols = features + [target]
    corr = df[cols].corr(method="pearson")

    fig, ax = plt.subplots(figsize=(11, 9))
    sns.heatmap(corr, annot=True, fmt=".2f", cmap="coolwarm", center=0.0,
                square=True, linewidths=0.5, cbar_kws={"shrink": 0.8}, ax=ax)
    ax.set_title("Pearson correlation: IR features and compile time")
    fig.tight_layout()
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close(fig)


def _write_report(features: list[str], coefs: np.ndarray,
                  correlations: dict[str, float], out_path: Path) -> None:
    """Markdown table sorted by coefficient magnitude + surprising findings."""
    max_abs = float(np.max(np.abs(coefs))) if len(coefs) else 0.0
    order = np.argsort(-np.abs(coefs))  # strongest first

    lines = [
        "# Feature Importance Report",
        "",
        "Coefficients are from the `LinearRegression` model trained on "
        "`StandardScaler`-normalised features, so their magnitudes are "
        "directly comparable. Correlation is the Pearson correlation "
        "between each raw feature and `total_compile_time_us`.",
        "",
        "| Feature | Coefficient | Correlation with Total Time | "
        "Interpretation |",
        "|---|---|---|---|",
    ]
    for rank, idx in enumerate(order):
        name = features[idx]
        coef = coefs[idx]
        corr = correlations[name]
        interp = _interpretation(rank, abs(coef), max_abs)
        lines.append(f"| {name} | {coef:.3f} | {corr:.2f} | {interp} |")

    # Surprising findings: features only weakly correlated with the target.
    weak = sorted(
        (n for n, c in correlations.items() if abs(c) < WEAK_CORRELATION),
        key=lambda n: abs(correlations[n]),
    )
    lines += ["", "## Surprising findings", ""]
    if weak:
        lines.append(
            f"The following feature(s) correlate unexpectedly weakly "
            f"(|r| < {WEAK_CORRELATION}) with total compile time — they may "
            f"be redundant or only matter in combination with others:")
        lines.append("")
        for name in weak:
            lines.append(f"- `{name}` (r = {correlations[name]:.2f})")
    else:
        lines.append(
            f"None — every feature shows a correlation of at least "
            f"{WEAK_CORRELATION} (absolute) with total compile time, so no "
            f"feature looks redundant on a univariate basis.")
    lines.append("")

    out_path.write_text("\n".join(lines))


def _per_pass_top_features(features: list[str], models_dir: Path,
                           per_pass_files: dict[str, str]) -> None:
    """Print the single most influential feature for each per-pass model."""
    if not per_pass_files:
        log("no per-pass models found — skipping per-pass importance")
        return
    log("most influential feature per pass model:")
    for target, fname in per_pass_files.items():
        path = models_dir / fname
        if not path.is_file():
            log(f"  {target}: model file missing ({fname})")
            continue
        model = joblib.load(path)
        coefs = np.asarray(model.coef_, dtype=float)
        top = features[int(np.argmax(np.abs(coefs)))]
        pass_name = target[len("time_"):] if target.startswith("time_") else target
        print(f"  {pass_name:<22} -> {top} (coef {coefs[np.argmax(np.abs(coefs))]:.3f})")


def generate_report(df: pd.DataFrame, features: list[str], target: str,
                    models_dir: Path, docs_dir: Path) -> None:
    """Produce all three feature-importance artefacts."""
    docs_dir.mkdir(parents=True, exist_ok=True)

    linear_path = models_dir / "total_time_linear.joblib"
    if not linear_path.is_file():
        raise FileNotFoundError(f"missing {linear_path} — run train_model.py first")
    linear = joblib.load(linear_path)
    coefs = np.asarray(linear.coef_, dtype=float)

    correlations = {
        name: float(df[name].corr(df[target])) if df[name].nunique() > 1 else 0.0
        for name in features
    }

    png_coef = docs_dir / "feature_importance.png"
    png_heat = docs_dir / "correlation_heatmap.png"
    md_report = docs_dir / "feature_importance_report.md"

    _plot_coefficients(features, coefs, png_coef)
    _plot_heatmap(df, features, target, png_heat)
    _write_report(features, coefs, correlations, md_report)
    log(f"wrote {png_coef}, {png_heat}, {md_report}")

    # Per-pass importance, using the model file map from the metadata.
    meta_path = models_dir / "training_metadata.json"
    per_pass_files: dict[str, str] = {}
    if meta_path.is_file():
        meta = json.loads(meta_path.read_text())
        per_pass_files = meta.get("model_files", {}).get("per_pass", {})
    _per_pass_top_features(features, models_dir, per_pass_files)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate the IR feature-importance report.")
    parser.add_argument("--data", default="training_data.csv")
    parser.add_argument("--models-dir", default="models")
    parser.add_argument("--docs-dir", default="docs")
    args = parser.parse_args()

    data_path = Path(args.data)
    models_dir = Path(args.models_dir)
    if not data_path.is_file():
        log(f"ERROR: training data not found: {data_path}")
        return 1

    meta_path = models_dir / "training_metadata.json"
    if not meta_path.is_file():
        log(f"ERROR: {meta_path} not found — run train_model.py first")
        return 1
    meta = json.loads(meta_path.read_text())

    df = pd.read_csv(data_path)
    features = meta["features"]
    target = meta["primary_target"]
    df = df.dropna(subset=features + [target]).reset_index(drop=True)

    generate_report(df, features, target, models_dir, Path(args.docs_dir))
    return 0


if __name__ == "__main__":
    sys.exit(main())
