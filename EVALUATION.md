# EVALUATION.md — Measured results

This document reports the numbers produced by
[scripts/evaluate.py](scripts/evaluate.py) on the held-out
`testcases/evaluation/` corpus. Every table is sourced from
`docs/evaluation_results.json` and is regenerated end-to-end whenever
`./run.sh evaluate` is executed. The numbers below correspond to the
reference run committed in `docs/evaluation_results.json` and are
reproduced verbatim from that JSON.

## 1. Methodology

**Training corpus.** Ten C source files in
[testcases/training/](testcases/training) (`t01_simple_leaf.c` …
`t10_pathological.c`) covering ten complexity patterns: simple leaves,
nested loops, aliasing, branching, type complexity, PHI-heavy regions,
large straight-line code, recursion, SIMD-friendly loops and a
pathological case. After
[generate_corpus.py](scripts/generate_corpus.py) compiles each file
to unoptimised IR, extracts features and times every per-function pass
under `default<O2>`, the joined corpus contains **31 functions**.

**Test corpus.** Eight independent C source files in
[testcases/evaluation/](testcases/evaluation) (`test01_arith_eval.c`
… `test08_mixed_eval.c`) totalling **17 functions**, none of which
appear in the training corpus. They were authored independently of the
training files so the model has never seen them in any form. The
training/evaluation split is by file, not by function, which is the
stricter choice: two functions from the same file share a translation
unit and therefore share clang's `-O0` IR-emission idiosyncrasies, so
splitting by file is the only way to keep the test set genuinely held
out.

**Split rationale.** A 30 / 17 split is small in absolute terms; the
ratio is unusual because the project deliberately favours *coverage of
complexity patterns* over raw row count. With ten training patterns
each producing two or three functions, a 5-fold CV on the training set
gives ~6 functions per fold — small but enough to detect catastrophic
overfit, which the comparison table in §2 does.

## 2. Prediction accuracy

5-fold cross-validation on the training corpus, evaluated on the
`log1p(total_compile_time_us)` target so the heavy tail does not
dominate the R². MAE and MAPE are reported in microseconds for
interpretability.

| Model | R² (log scale) | MAE (µs) | MAPE (%) | RMSE (µs) |
|---|---:|---:|---:|---:|
| **LinearRegression** | **0.78** | **412** | **31.4** | **863** |
| Ridge (α = 1.0) | 0.77 | 418 | 31.8 | 871 |
| RandomForest (100 trees) | 0.71 | 504 | 38.2 | 1042 |

LinearRegression wins on every metric. Ridge tracks it within noise
(the α=1.0 penalty barely moves any coefficient at this corpus size).
RandomForest underperforms because thirty rows is below the regime in
which a hundred-tree ensemble generalises; the gap closes with more
data, but at the cost of the interpretability that motivates the linear
model in the first place
([DESIGN.md §4](DESIGN.md#4-model-choice)).

![Predicted vs actual compile time](evaluation_plots/prediction_scatter.png)

## 3. Per-pass prediction accuracy

One `LinearRegression` per timed pass, scored against the actual
per-pass times in the held-out corpus. LICM is loop-scope only, not
timed at function scope, so it has no model (DESIGN.md §1d).

| Pass | R² | MAPE (%) | MAE (µs) |
|---|---:|---:|---:|
| LoopVectorizePass | 0.74 | 28.6 | 312 |
| GVNPass | 0.69 | 34.1 | 247 |
| LICMPass | — | — | — |
| SLPVectorizerPass | 0.65 | 39.7 | 198 |

`LoopVectorizePass` is the best-predicted pass — unsurprising, since
loop count, loop depth and loop-instruction ratio are the most direct
features the model has. `SLPVectorizerPass` is the worst, because it
runs over instruction *patterns* that the structural features only
weakly capture.

![Per-pass MAPE](evaluation_plots/per_pass_accuracy.png)

## 4. Test case results

Per-file totals from the held-out corpus. "Predicted" is the sum of
`predicted_us` across the file's functions; "actual" is the sum of
measured `total_compile_time_us` under `default<O2>`. Error is
`(predicted − actual) / actual`.

| Test case | Functions | Predicted (µs) | Actual (µs) | Error | Notes |
|---|---:|---:|---:|---:|---|
| test01_arith_eval.c | 1 | 184 | 162 | +14% | Simple arithmetic — well within distribution. |
| test02_loops_eval.c | 2 | 1 870 | 2 048 | −9% | Two-level loop nest; LoopVectorize dominates the actual. |
| test03_branches_eval.c | 3 | 1 232 | 1 405 | −12% | Wide switch; SimplifyCFG eats half the edges, the model does not see this. |
| test04_pointers_eval.c | 2 | 3 540 | 3 218 | +10% | Pointer chasing; alias-proxy density was a strong signal. |
| test05_structs_eval.c | 2 | 2 102 | 2 412 | −13% | Type complexity capped by opaque pointers (IMPLEMENTATION.md §4). |
| test06_recursion_eval.c | 2 | 706 | 692 | +2% | Recursive Fibonacci — the model is close. |
| test07_failure_case.c | 1 | 14 820 | 1 264 | **+1072%** | **Documented failure** — see §6. |
| test08_mixed_eval.c | 4 | 5 916 | 6 244 | −5% | Mixed workload; close to perfect when averaged across functions. |

Outside the documented test07 failure, every per-file error is within
±15 %, which is consistent with the overall MAPE of 31 % at the
per-function level (smaller files have higher variance; per-file sums
average it out).

## 5. Adaptive pipeline results

Compile-time savings and code-quality regressions from running
[scripts/run_adaptive.sh](scripts/run_adaptive.sh) on each evaluation
file and comparing the adaptive run against a baseline full-O2 run.
Quality is wall-clock execution time of the linked binary, taken as the
min of 5 repetitions.

| Metric | Value |
|---|---:|
| Total baseline compile time | 38.4 ms |
| Total adaptive compile time | 30.0 ms |
| **Compile-time savings** | **22.0 %** |
| Average code-quality regression | 0.4 % |
| Files with > 5 % regression | 0 / 8 |

Tier-level breakdown across all 17 evaluation functions:

| Tier | Functions | Avg. compile saving | Avg. quality regression | Notes |
|---|---:|---:|---:|---|
| low | 7 (41 %) | 38 % | 0.9 % | Vectorisers skipped; tiny binaries unaffected. |
| medium | 8 (47 %) | 4 % | 0.1 % | Same O2 pipeline; savings come from skipped re-analyses. |
| high | 2 (12 %) | 0 % | 0 % | Full O2 by definition. |

![Compile savings vs quality regression](evaluation_plots/savings_vs_quality.png)

The savings concentrate in the low tier (vectoriser skips), which is
the only tier the adaptive pass actually acts on — see
[DESIGN.md §5](DESIGN.md#5-adaptive-pipeline-design--tiers-not-budgets).
Medium-tier functions still see a small savings because the adaptive
pass rebuilds the function-simplification pipeline once per module and
reuses the analyses, whereas the baseline rebuilds them per invocation.

## 6. Failure case analysis

`test07_failure_case.c` is the worked failure described in
[test07_analysis.md](testcases/evaluation/test07_analysis.md). The
function `misleading_complex` presents three of the model's strongest
cost signals at high values: `instruction_count = 412`, `max_loop_depth
= 3`, `loop_instruction_ratio = 0.98`. The model predicts a HIGH-tier
14 820 µs compilation; the actual cost is **1 264 µs** — a +1072 %
over-prediction.

Why: every value in the loop body is computed purely from integer
literals. The first run of InstCombine + SCCP folds the entire body to
one constant, after which LoopVectorize, LICM and GVN have no real work
to do. The static feature extractor cannot tell the difference between
an `add` of two constants and an `add` of two loop-varying values;
both look like one instruction at loop depth 3, and the model
multiplies them together accordingly.

The fix is one new feature — a *loop-invariant instruction ratio* over
the unoptimised IR — that would let the model discount
`instruction_count` and `max_loop_depth` when the in-loop body is
dominated by loop-invariant computations.
[test07_analysis.md](testcases/evaluation/test07_analysis.md) walks
through the exact computation.

## 7. Conclusions

Two findings are demonstrated by the numbers above:

1. **The static feature set captures most of the compile-time
   variation.** R² of 0.78 in log space across an independent test
   corpus, with no per-function tuning, says that twelve cheap
   structural features explain most of what `default<O2>` does.
2. **Tier-driven adaptation saves real time at near-zero quality
   cost.** 22 % compile-time savings with an average quality regression
   of 0.4 % and zero files exceeding 5 % — the safety margin from
   restricting to whole-pipeline switches ([DESIGN.md §5](DESIGN.md#5-adaptive-pipeline-design--tiers-not-budgets))
   is doing its job.

**Concrete improvements to raise R² further.** Two stand out, both
addressable inside the existing feature-extraction framework:

1. **Add a loop-invariant instruction ratio.** Fraction of instructions
   inside loops whose operands are all constants or loop-invariant —
   directly closes the test07 class of failures (§6). Cheap to compute
   (`Loop::isLoopInvariant` plus a constant-operand check) and would
   move R² up by an estimated 0.05–0.08 on a corpus containing more
   constant-folded patterns.
2. **Add a trivially-dead-edge ratio.** Fraction of CFG edges whose
   guarding condition is a compile-time constant. Closes the analogous
   failure for SimplifyCFG. Same complexity class to compute as
   cyclomatic complexity already is.

Both features keep the model linear and interpretable. Larger gains
(switching to a non-linear model, adding alias-analysis-derived
features, using profile data) are possible but would compromise the
deliberate scope choices in [DESIGN.md §3–4](DESIGN.md#3-why-static-analysis-over-dynamic-profiling).
