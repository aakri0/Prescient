# Prescient

A pre-pipeline compile-time predictor for LLVM 17 that picks an optimisation
pipeline per function, before any pass runs.

## Problem

Building a large C/C++ codebase at `-O2` is dominated by a small number of
hot functions: a long-running loop optimisation pass on a single 4000-line
function can take longer than the rest of the file combined. The compiler
has no way to know in advance which functions justify that cost — every
function pays the full price of the pipeline whether it benefits from it or
not. The result is wasted CI minutes, slower edit-build-test loops and
real money paid to optimise functions that the optimiser barely changes.
Prescient predicts the per-function compile cost from a cheap static
analysis of the unoptimised IR and uses the prediction to skip the
expensive passes on functions that do not need them.

## Solution

Prescient ships an out-of-tree LLVM plugin and a small scikit-learn model.
Four components feed each other:

```
   .c source                                       optimised IR / binary
       │                                                     ▲
       ▼                                                     │
 ┌──────────────┐    features.json    ┌──────────────┐  predictions.json   ┌──────────────────┐
 │  Feature     │ ──────────────────▶ │  Compile-    │ ──────────────────▶ │  Adaptive        │
 │  extractor   │                     │  time model  │                     │  pipeline pass   │
 │  (LLVM pass) │                     │  (Python)    │                     │  (LLVM pass)     │
 └──────────────┘                     └──────────────┘                     └──────────────────┘
       ▲                                     ▲
       │                                     │ timings.csv
       │ training C files            ┌──────────────────┐
       └────────────────────────────▶│  Timing wrapper  │
                                     │  (LLVM PIC)      │
                                     └──────────────────┘
```

1. **Feature extractor** (`src/passes/IRComplexityPass.cpp`) — an LLVM
   module pass that scans every defined function and writes a JSON record
   of twelve cheap structural features (instruction count, loop depth,
   cyclomatic complexity, PHI density, type complexity, alias-proxy
   density, …).
2. **Timing wrapper** (`src/timing/PassTimingWrapper.cpp`) — a
   `PassInstrumentationCallbacks` hook that records the wall time of every
   function-level pass into a CSV. Used to label training data.
3. **Compile-time model** (`src/model/`) — a `LinearRegression` on
   `log1p(total_compile_time_us)`, with `Ridge` and `RandomForest` as
   reference baselines, plus one `LinearRegression` per timed pass.
4. **Adaptive pipeline pass** (`src/passes/AdaptivePipeline.cpp`) — reads
   the predictions and runs the function-simplification pipeline at
   `O1` (with vectorisers skipped) for `low`-tier functions and at full
   `O2` for everything else.

## Documentation

The full project documentation lives in the repository root alongside this
README:

| Document | What it covers |
|---|---|
| [DESIGN.md](DESIGN.md) | Why the feature set and model were chosen — design rationale and trade-offs |
| [IMPLEMENTATION.md](IMPLEMENTATION.md) | How it is wired into LLVM — APIs used, pass internals, known gotchas |
| [EVALUATION.md](EVALUATION.md) | Measured prediction accuracy and adaptive-pipeline savings |
| [CONTRIBUTING.md](CONTRIBUTING.md) | How to add features, passes, models or training data |

## Prerequisites

- Ubuntu 22.04
- LLVM 17 (`llvm-17`, `clang-17`, `llvm-17-dev` — installed by
  [scripts/setup_env.sh](scripts/setup_env.sh) from `apt.llvm.org`)
- CMake ≥ 3.20
- Python ≥ 3.10 with the packages in [requirements.txt](requirements.txt)

`scripts/setup_env.sh` is the single source of truth for the toolchain
versions; the CI workflow uses the same script.

## Quick Start

Five commands on a fresh Ubuntu 22.04 clone:

```bash
git clone https://github.com/aakri0/Prescient.git && cd Prescient
./scripts/setup_env.sh
./build.sh
./scripts/demo.sh
python3 scripts/evaluate.py
```

The first three commands provision the toolchain and build
`build/IRComplexityEstimator.so`. The fourth runs the narrated five-act
demo. The fifth runs the full evaluation suite and refreshes
`docs/evaluation_results.json`.

## Usage

Extract features from a C file:

```bash
$ ./run.sh extract testcases/training/t02_nested_loops.c
[IRComplexity] Analyzing function: matmul_3x3
extract: features written to output/features.json
```

Excerpt of `output/features.json`:

```json
[
  {
    "function_name": "matmul_3x3",
    "instruction_count": 92,
    "basic_block_count": 11,
    "cyclomatic_complexity": 6,
    "max_loop_depth": 3,
    "loop_instruction_ratio": 0.8478,
    "phi_density": 0.0978,
    "type_complexity_normalized": 1.4239,
    "alias_proxy_density": 0.3261
  }
]
```

Predict its compile cost (after `train` has populated `models/`):

```bash
$ python3 src/model/predict.py --features output/features.json \
        --models-dir models/ --output output/predictions.json
[predict] wrote 1 prediction(s) to output/predictions.json (low=0, medium=1, high=0)
```

Run the full adaptive workflow on one file, with a savings report:

```bash
$ ./scripts/run_adaptive.sh testcases/training/t02_nested_loops.c
==> 1/8 compiling testcases/training/t02_nested_loops.c to IR
...
============ Adaptive Pipeline Summary ============
  input              : testcases/training/t02_nested_loops.c
  baseline wall time : 248 ms
  adaptive wall time : 196 ms
  wall time saved    : 52 ms
  savings report     : output/savings_report.md
  correctness        : PASS (no test harness — stub main)
===================================================
```

## Repository Layout

```
.
├── build.sh                       # configure + build the LLVM plugin
├── run.sh                         # extract / train / predict / evaluate / adaptive / demo entry point
├── CMakeLists.txt                 # LLVM-17 plugin build configuration
├── Dockerfile, docker-compose.yml # reproducible build environment
├── requirements.txt               # Python dependencies (scikit-learn, pandas, …)
├── README.md                      # overview + quick start
├── DESIGN.md                      # why this design — feature & model rationale
├── IMPLEMENTATION.md              # how it's wired — LLVM APIs, gotchas
├── EVALUATION.md                  # measured prediction & savings results
├── CONTRIBUTING.md                # how to add features / passes / data
├── src/
│   ├── passes/
│   │   ├── IRComplexityPass.cpp   # feature extractor (LLVM module pass)
│   │   └── AdaptivePipeline.cpp   # tier-driven per-function pipeline
│   ├── timing/
│   │   └── PassTimingWrapper.cpp  # PassInstrumentationCallbacks → CSV
│   └── model/
│       ├── train_model.py         # 5-fold CV + final fit + per-pass models
│       ├── predict.py             # CLI: features.json → predictions.json
│       └── feature_importance.py  # docs/feature_importance_report.md + plots
├── scripts/
│   ├── setup_env.sh               # provisions LLVM 17 + Python deps
│   ├── generate_corpus.py         # build the joined training CSV
│   ├── time_o2_pipeline.sh        # run O2 under the timing plugin
│   ├── run_adaptive.sh            # end-to-end adaptive workflow for one .c
│   ├── generate_savings_report.py # markdown savings report
│   ├── evaluate.py                # full evaluation suite (issue #24)
│   └── demo.sh                    # narrated five-act demo
├── testcases/
│   ├── training/                  # ten C files used for training (t01…t10)
│   └── evaluation/                # eight held-out C files (test01…test08)
├── models/                        # populated by train_model.py
└── docs/                          # generated reports & plots (regenerated)
```

## Components

**Feature extractor.** The pass walks every defined function once, summing
per-instruction contributions for size, CFG edges, loop body coverage, PHI
density, type complexity and memory-op density. It writes one JSON record
per function, with a fixed schema that the Python model relies on.
[IMPLEMENTATION.md](IMPLEMENTATION.md#feature-implementation-details)
documents the exact LLVM API used for each feature.

**Timing wrapper.** A single `PassInstrumentationCallbacks` instance hooks
`registerBeforeNonSkippedPassCallback` and `registerAfterPassCallback`,
stamps a `steady_clock` time-point on every function-level pass entry, and
emits one CSV row per (function, pass) on exit. Module- and loop-level
passes are filtered out by an `any_cast<const Function *>` check. The CSV
is flushed after every row so a crash mid-pipeline never loses earlier
timings.

**Compile-time model.** Training is in `log1p` space because compile
times are heavy-tailed: a 1 ms pass and a 1 s pass differ by a factor of a
thousand and ordinary least squares on the raw scale would let the long
tail dominate the fit. The final artefact is one `LinearRegression` plus a
`StandardScaler`; `Ridge` and `RandomForest` are trained alongside for
honest cross-validation comparison. Predictions clip log-space output to
`expm1(20.0)` so an extrapolating linear model cannot emit an unbounded
microsecond value.

**Adaptive pipeline pass.** Reads `predictions.json` (from the
`COMPLEXITY_PREDICTIONS` environment variable) and dispatches per-function:
the `low` tier runs `buildFunctionSimplificationPipeline(O1)` with
`LoopVectorizePass` and `SLPVectorizerPass` filtered out via
`registerShouldRunOptionalPassCallback`; the `medium` and `high` tiers run
the full O2 simplification pipeline. Any missing or malformed prediction
falls back to medium — never to `low` — so an empty predictions file
degrades to a normal O2 compile rather than silently skipping work.

## Evaluation

Summary numbers from `scripts/evaluate.py` over the eight held-out files in
`testcases/evaluation/` — see [EVALUATION.md](EVALUATION.md) for
the full breakdown and the per-pass, per-test-case tables.

| Metric | Value |
|---|---|
| Functions evaluated | 17 |
| Total-time R² (log scale) | 0.78 |
| Total-time MAE | 412 µs |
| Total-time MAPE | 31 % |
| Adaptive compile-time savings | 22 % |
| Average code-quality regression | 0.4 % |
| Files exceeding 5 % quality regression | 0 / 8 |

## Limitations

- **Alias-proxy density is a proxy metric, not a direct measurement.** It
  counts loads, stores, GEPs and memory intrinsics per instruction; it does
  not run alias analysis. Two functions with identical alias-proxy density
  can differ wildly in real aliasing complexity.
- **The model predicts post-optimisation cost from pre-optimisation IR.**
  Any pass with a data-dependent early exit (constant folding, dead-code
  elimination, profile-guided pruning) can collapse work that the static
  features still see — `testcases/evaluation/test07_failure_case.c` is the
  worked example, analysed in [EVALUATION.md](EVALUATION.md#failure-case-analysis).
- **Training-set size is small (ten C files, ~30 functions).** Cross-validation
  R² is reported but a wider corpus is needed before the absolute numbers
  generalise.
- **Linux / LLVM 17 only.** The plugin loads `IRComplexityEstimator.so`
  via `opt -load-pass-plugin`; the macOS / Windows toolchains are not in
  scope. CI runs on `ubuntu-22.04` exclusively.
- **The adaptive pass only acts on the `low` tier.** Medium and high tiers
  get the same O2 pipeline, so the upper end of the savings curve is bounded
  by how often we correctly identify low-tier functions.

## License

MIT — see [LICENSE](LICENSE) for the full text.
