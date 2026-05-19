#!/usr/bin/env python3
"""show_features.py — print an IR-complexity features.json as a table.

Used by `run.sh extract` (and importable by predict.py) so the extracted
feature records are shown in a readable table instead of being left buried
in features.json.

    python3 src/model/show_features.py output/features.json
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

import _render

# Columns shown in the IR-feature table — a readable subset covering all
# six feature dimensions. (json key, column header, "i" int / "f" float).
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


def print_feature_table(functions: list) -> None:
    """Render the extracted IR-complexity features as a table."""
    headers = ["Function"] + [h for _, h, _ in FEATURE_VIEW]
    aligns = ["<"] + [">"] * len(FEATURE_VIEW)
    rows = []
    for f in functions:
        row = [f.get("function_name", "?")]
        for key, _, kind in FEATURE_VIEW:
            v = f.get(key, 0) or 0
            row.append(f"{float(v):.2f}" if kind == "f" else f"{int(v):d}")
        rows.append(row)
    print(_render.banner(
        f"IR COMPLEXITY FEATURES  ({len(functions)} function(s))"))
    print(_render.render_table(headers, rows, aligns))
    print()
    print("  Full 23-field detail per function is in the features JSON.")


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: show_features.py <features.json>", file=sys.stderr)
        return 2
    path = Path(sys.argv[1])
    if not path.is_file():
        print(f"show_features: file not found: {path}", file=sys.stderr)
        return 1
    try:
        functions = json.loads(path.read_text())
    except json.JSONDecodeError as exc:
        print(f"show_features: invalid JSON in {path}: {exc}", file=sys.stderr)
        return 1
    if not isinstance(functions, list) or not functions:
        print(f"show_features: {path} has no function records", file=sys.stderr)
        return 1
    print_feature_table(functions)
    return 0


if __name__ == "__main__":
    sys.exit(main())
