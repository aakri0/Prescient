"""_render.py — tiny dependency-free helpers for tabular terminal output.

Shared by train_model.py and predict.py so their results are easy to read
at a glance instead of being buried in JSON. No external dependencies.
"""
from __future__ import annotations

BANNER_WIDTH = 78


def banner(title: str) -> str:
    """A section banner that stands out from a wall of log lines."""
    bar = "=" * BANNER_WIDTH
    return f"\n{bar}\n  {title}\n{bar}"


def render_table(headers: list, rows: list[list],
                 aligns: list[str] | None = None) -> str:
    """Render an aligned plain-text table.

    headers : column titles
    rows    : list of rows, each a list of cells (str() is applied to each)
    aligns  : optional per-column alignment — '<' (left) or '>' (right);
              defaults to left-aligned for every column
    """
    headers = [str(h) for h in headers]
    ncols = len(headers)
    aligns = aligns or ["<"] * ncols
    cells = [[str(c) for c in row] for row in rows]

    widths = [len(headers[i]) for i in range(ncols)]
    for row in cells:
        for i in range(ncols):
            widths[i] = max(widths[i], len(row[i]))

    def fmt(row: list[str]) -> str:
        return "  " + "   ".join(
            f"{row[i]:{aligns[i]}{widths[i]}}" for i in range(ncols))

    sep = "  " + "-" * (sum(widths) + 3 * (ncols - 1))
    return "\n".join([fmt(headers), sep] + [fmt(r) for r in cells])
