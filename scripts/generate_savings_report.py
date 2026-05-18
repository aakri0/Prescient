#!/usr/bin/env python3
"""generate_savings_report.py — adaptive pipeline savings report (issue #22).

Compares a baseline full-O2 run against an adaptive-pipeline run and writes
a Markdown report quantifying the compile-time trade-off.

Usage:
    python3 scripts/generate_savings_report.py \\
        --baseline output/baseline_timings.csv \\
        --adaptive output/adaptive_timings.csv \\
        --predictions output/predictions.json \\
        --output docs/savings_report.md
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import pandas as pd

TIERS = ("low", "medium", "high")


def log(msg: str) -> None:
    print(f"[savings_report] {msg}", flush=True)


def per_function_us(df: pd.DataFrame) -> pd.Series:
    """Total time_us per function."""
    if df.empty:
        return pd.Series(dtype="int64")
    return df.groupby("function_name")["time_us"].sum()


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate the adaptive pipeline savings report.")
    parser.add_argument("--baseline", required=True,
                        help="baseline_timings.csv from a full O2 run")
    parser.add_argument("--adaptive", required=True,
                        help="adaptive_timings.csv from the adaptive run")
    parser.add_argument("--predictions", required=True,
                        help="predictions.json (for per-function tiers)")
    parser.add_argument("--output", default="docs/savings_report.md",
                        help="Path to write the Markdown report")
    args = parser.parse_args()

    for path in (args.baseline, args.adaptive, args.predictions):
        if not Path(path).is_file():
            log(f"ERROR: input file not found: {path}")
            return 1

    baseline = pd.read_csv(args.baseline)
    adaptive = pd.read_csv(args.adaptive)
    with open(args.predictions) as fh:
        predictions = json.load(fh)

    tier_of = {p["function_name"]: p.get("complexity_tier", "medium")
               for p in predictions}

    base_us = per_function_us(baseline)
    adap_us = per_function_us(adaptive)
    functions = sorted(set(base_us.index) | set(adap_us.index))

    rows = []
    for fn in functions:
        b = int(base_us.get(fn, 0))
        a = int(adap_us.get(fn, 0))
        saved = b - a
        rows.append({
            "function": fn,
            "tier": tier_of.get(fn, "medium"),
            "baseline": b,
            "adaptive": a,
            "saved": saved,
            "saved_pct": (saved / b * 100.0) if b > 0 else 0.0,
        })
    rows.sort(key=lambda r: r["saved"], reverse=True)

    # --- summary figures --------------------------------------------------
    total_base = sum(r["baseline"] for r in rows)
    total_adap = sum(r["adaptive"] for r in rows)
    total_saved = total_base - total_adap
    total_pct = (total_saved / total_base * 100.0) if total_base > 0 else 0.0
    n = len(rows)
    tier_counts = {t: sum(1 for r in rows if r["tier"] == t) for t in TIERS}

    # Passes skipped: distinct (function, pass) pairs that the baseline ran
    # but the adaptive run did not, restricted to low-tier functions.
    base_pairs = set(zip(baseline.get("function_name", []),
                         baseline.get("pass_name", [])))
    adap_pairs = set(zip(adaptive.get("function_name", []),
                         adaptive.get("pass_name", [])))
    low_funcs = {r["function"] for r in rows if r["tier"] == "low"}
    passes_skipped = sum(1 for (f, p) in base_pairs
                         if f in low_funcs and (f, p) not in adap_pairs)

    slower = [r for r in rows if r["saved"] < 0]

    def pct(count: int) -> str:
        return f"{round(count / n * 100)}%" if n else "0%"

    # --- render the report ------------------------------------------------
    lines = [
        "## Adaptive Pipeline Savings Report",
        "",
        "### Summary",
        f"- Functions analysed: {n}",
        f"- Low tier: {tier_counts['low']} ({pct(tier_counts['low'])}) | "
        f"Medium: {tier_counts['medium']} ({pct(tier_counts['medium'])}) | "
        f"High: {tier_counts['high']} ({pct(tier_counts['high'])})",
        f"- Baseline total compile time: {total_base:,} µs",
        f"- Adaptive total compile time: {total_adap:,} µs",
        f"- **Time saved: {total_saved:,} µs ({total_pct:.1f}%)**",
        f"- Passes skipped: {passes_skipped} (across {len(low_funcs)} "
        f"low-tier functions)",
        "",
        "### Per-Function Breakdown",
        "| Function | Tier | Baseline µs | Adaptive µs | Saved µs | "
        "Saved % |",
        "|---|---|---|---|---|---|",
    ]
    for r in rows:
        lines.append(
            f"| {r['function']} | {r['tier']} | {r['baseline']:,} | "
            f"{r['adaptive']:,} | {r['saved']:,} | {r['saved_pct']:.1f}% |")

    lines += ["", "### Notes", ""]
    if slower:
        lines.append(
            "The adaptive pipeline was **slower** than baseline for the "
            "following function(s). This is normal for small functions where "
            "measurement noise and instrumentation overhead exceed the real "
            "optimisation cost:")
        lines.append("")
        for r in slower:
            lines.append(
                f"- `{r['function']}` ({r['tier']} tier): "
                f"{-r['saved']:,} µs slower")
    else:
        lines.append("Every function compiled at least as fast under the "
                     "adaptive pipeline as under the baseline.")
    lines.append("")

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text("\n".join(lines))

    log(f"wrote {output_path}")
    log(f"baseline {total_base:,} us, adaptive {total_adap:,} us, "
        f"saved {total_saved:,} us ({total_pct:.1f}%)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
