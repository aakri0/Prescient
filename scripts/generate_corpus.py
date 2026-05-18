#!/usr/bin/env python3
"""generate_corpus.py — build the joined training dataset (issue #16).

For every C source file in an input directory this script:

  1. Compiles it to unoptimised IR  (clang-17 -O0 -emit-llvm -S)
  2. Extracts IR-complexity features (opt -passes=ir-complexity)
  3. Collects per-pass O2 timings     (scripts/time_o2_pipeline.sh)
  4. Pivots the timings so each pass becomes its own ``time_<Pass>`` column
  5. Joins features + timings on ``function_name``
  6. Adds ``total_compile_time_us`` = sum of every ``time_*`` column

The per-function rows from all files are concatenated into one master CSV.

Usage:
    python3 scripts/generate_corpus.py \\
        --input-dir testcases/training/ \\
        --output training_data.csv \\
        --plugin ./build/IRComplexityEstimator.so
"""
from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

import pandas as pd

SCRIPT_DIR = Path(__file__).resolve().parent
TIME_SCRIPT = SCRIPT_DIR / "time_o2_pipeline.sh"


def find_tool(candidates: list[str]) -> str | None:
    """Return the first tool on PATH from a list of candidate names."""
    for name in candidates:
        if shutil.which(name):
            return name
    return None


def log(msg: str) -> None:
    print(f"[generate_corpus] {msg}", flush=True)


def process_file(
    c_file: Path,
    clang: str,
    opt: str,
    plugin: Path,
    workdir: Path,
) -> pd.DataFrame | None:
    """Run the full pipeline for one C file.

    Returns a per-function DataFrame, or None if the file could not be
    processed (the caller counts this as an error).
    """
    stem = c_file.stem
    ll_file = workdir / f"{stem}.ll"
    feat_file = workdir / f"{stem}.features.json"
    timing_file = workdir / f"{stem}.timings.csv"

    # 1. Compile to unoptimised IR.
    #    -Xclang -disable-O0-optnone omits the `optnone` attribute that
    #    clang otherwise stamps on every -O0 function. The IR stays fully
    #    unoptimised (clang runs no passes), but `optnone` would make the
    #    later O2 timing pipeline skip every real pass, so it must be off.
    res = subprocess.run(
        [clang, "-O0", "-Xclang", "-disable-O0-optnone",
         "-emit-llvm", "-S", str(c_file), "-o", str(ll_file)],
        capture_output=True, text=True,
    )
    if res.returncode != 0:
        log(f"ERROR: clang failed for {c_file.name}: "
            f"{res.stderr.strip() or 'unknown error'} — skipping")
        return None

    # 2. Extract IR-complexity features.
    res = subprocess.run(
        [opt, "-load-pass-plugin", str(plugin), "-passes=ir-complexity",
         f"-complexity-output={feat_file}", "-disable-output", str(ll_file)],
        capture_output=True, text=True,
    )
    if res.returncode != 0 or not feat_file.exists():
        log(f"ERROR: feature extraction failed for {c_file.name}: "
            f"{res.stderr.strip() or 'unknown error'} — skipping")
        return None

    with feat_file.open() as fh:
        features = json.load(fh)
    if not features:
        log(f"WARNING: {c_file.name} produced no functions — skipping")
        return None
    features_df = pd.DataFrame(features)

    # 3. Collect per-pass O2 timings.
    res = subprocess.run(
        [str(TIME_SCRIPT), str(ll_file), str(timing_file)],
        capture_output=True, text=True,
        env={**__import__("os").environ, "PLUGIN": str(plugin)},
    )
    if res.returncode != 0 or not timing_file.exists():
        log(f"ERROR: O2 timing failed for {c_file.name}: "
            f"{res.stderr.strip() or 'unknown error'} — skipping")
        return None

    timings_df = pd.read_csv(timing_file)

    # 4/5. Pivot timings (one column per pass) and join onto the features.
    #      A pass may run several times per function in O2, so sum the times.
    if timings_df.empty:
        pivot = pd.DataFrame(index=pd.Index([], name="function_name"))
    else:
        pivot = timings_df.pivot_table(
            index="function_name", columns="pass_name",
            values="time_us", aggfunc="sum", fill_value=0,
        )
        pivot.columns = [f"time_{c}" for c in pivot.columns]

    feat_funcs = set(features_df["function_name"])
    timed_funcs = set(pivot.index)

    # Edge case: functions timed but never seen by feature extraction.
    for orphan in sorted(timed_funcs - feat_funcs):
        log(f"WARNING: function '{orphan}' in timings but not features "
            f"({c_file.name}) — skipping it")

    merged = features_df.merge(
        pivot, how="left", left_on="function_name", right_index=True,
    )

    # Edge case: functions with features but no timings (trivially
    # optimised) — fill their pass-time columns with 0.
    time_cols = [c for c in merged.columns if c.startswith("time_")]
    if time_cols:
        merged[time_cols] = merged[time_cols].fillna(0)

    log(f"processed {c_file.name}: {len(merged)} function(s)")
    return merged


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Build the joined training dataset from C source files.")
    parser.add_argument("--input-dir", required=True,
                        help="Directory of .c source files")
    parser.add_argument("--output", required=True,
                        help="Path to the master training CSV to write")
    parser.add_argument("--plugin", required=True,
                        help="Path to IRComplexityEstimator.so")
    args = parser.parse_args()

    input_dir = Path(args.input_dir)
    output_path = Path(args.output)
    plugin = Path(args.plugin).resolve()

    if not input_dir.is_dir():
        log(f"ERROR: input directory not found: {input_dir}")
        return 1
    if not plugin.is_file():
        log(f"ERROR: plugin not found: {plugin} (run ./build.sh first)")
        return 1
    if not TIME_SCRIPT.is_file():
        log(f"ERROR: timing script not found: {TIME_SCRIPT}")
        return 1

    clang = find_tool(["clang-17", "clang"])
    opt = find_tool(["opt-17", "opt"])
    if not clang or not opt:
        log("ERROR: clang-17/opt-17 not found — run scripts/setup_env.sh")
        return 1

    c_files = sorted(input_dir.glob("*.c"))
    if not c_files:
        log(f"ERROR: no .c files found in {input_dir}")
        return 1

    frames: list[pd.DataFrame] = []
    errors = 0
    with tempfile.TemporaryDirectory(prefix="corpus_") as tmp:
        workdir = Path(tmp)
        for c_file in c_files:
            df = process_file(c_file, clang, opt, plugin, workdir)
            if df is None:
                errors += 1
            else:
                frames.append(df)

    if not frames:
        log(f"ERROR: no files processed successfully ({errors} error(s))")
        return 1

    # Concatenate every file's rows; align pass columns across files and
    # fill the gaps (a pass that ran for one file but not another) with 0.
    corpus = pd.concat(frames, ignore_index=True, sort=False)
    time_cols = [c for c in corpus.columns if c.startswith("time_")]
    if time_cols:
        corpus[time_cols] = corpus[time_cols].fillna(0).astype("int64")

    # total_compile_time_us = sum of all per-pass time columns.
    corpus["total_compile_time_us"] = (
        corpus[time_cols].sum(axis=1) if time_cols else 0
    )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    corpus.to_csv(output_path, index=False)

    log(f"Processed {len(c_files)} files, {len(corpus)} functions, "
        f"{errors} errors")
    log(f"wrote {output_path} "
        f"({len(corpus.columns)} columns, {len(time_cols)} pass-time columns)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
