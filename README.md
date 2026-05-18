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

The full project documentation lives in the `docs/` directory:

| Document | What it covers |
|---|---|
| [DESIGN.md](docs/DESIGN.md) | Why the feature set and model were chosen — design rationale and trade-offs |
| [IMPLEMENTATION.md](docs/IMPLEMENTATION.md) | How it is wired into LLVM — APIs used, pass internals, known gotchas |
| [EVALUATION.md](docs/EVALUATION.md) | Measured prediction accuracy and adaptive-pipeline savings |
| [CONTRIBUTING.md](docs/CONTRIBUTING.md) | How to add features, passes, models or training data |

## Prerequisites

Pick one of two paths:

- **Docker** (any OS) — only Docker Engine / Docker Desktop is needed.
  The image builds and runs everything inside Ubuntu 22.04. This is the
  only supported path on macOS and Windows.
- **Native** (Linux only) — Ubuntu 22.04, LLVM 17 (`llvm-17`, `clang-17`,
  `llvm-17-dev`), CMake ≥ 3.20 and Python ≥ 3.10. `scripts/setup_env.sh`
  installs all of these from `apt.llvm.org` and is the single source of
  truth for toolchain versions (the CI workflow uses the same script).

> The native toolchain is Ubuntu-only because `setup_env.sh` uses `apt`.
> On macOS and Windows, use Docker.

## Getting Started

In every path the order is the same: **build → train → run**. The model
must be trained once before `demo`, `evaluate`, `predict` or `adaptive`
will work. With Docker the trained model persists on the host via the
`./models` volume, so `train` only needs to be run again when the code or
training data changes.

### Linux (Ubuntu 22.04)

**Native:**

```bash
git clone https://github.com/aakri0/Prescient.git && cd Prescient
./scripts/setup_env.sh        # install LLVM 17 + Python deps (uses sudo apt)
./build.sh                    # build build/IRComplexityEstimator.so
./run.sh train                # generate the corpus and train the model
./run.sh demo                 # narrated five-act demo
./run.sh evaluate             # full evaluation suite -> docs/evaluation_results.json
```

**Docker:**

```bash
git clone https://github.com/aakri0/Prescient.git && cd Prescient
docker compose build                        # build the image (installs LLVM 17)
docker compose run --rm prescient train     # generate the corpus and train the model
docker compose run --rm prescient demo      # narrated five-act demo
docker compose run --rm prescient evaluate  # full evaluation suite
```

### macOS

The native toolchain is not supported on macOS (no `apt`). Use Docker:

```bash
git clone https://github.com/aakri0/Prescient.git && cd Prescient
docker compose build                        # build the image (installs LLVM 17)
docker compose run --rm prescient train     # generate the corpus and train the model
docker compose run --rm prescient demo      # narrated five-act demo
docker compose run --rm prescient evaluate  # full evaluation suite
```

### Windows

Use Docker Desktop (WSL 2 backend recommended). Run in PowerShell:

```powershell
git clone https://github.com/aakri0/Prescient.git
cd Prescient
docker compose build                        # build the image (installs LLVM 17)
docker compose run --rm prescient train     # generate the corpus and train the model
docker compose run --rm prescient demo      # narrated five-act demo
docker compose run --rm prescient evaluate  # full evaluation suite
```

> The first `docker compose build` takes a few minutes — it installs the
> LLVM 17 toolchain into the image. Subsequent runs are cached.

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

## Custom Programs

Yes — that's exactly what the `extract` and `predict` modes are for. The
bundled testcases are just examples; the tools accept any C file path.

Check complexity of your own program:

```bash
# native (Linux)
./run.sh extract path/to/your_program.c

# Docker — put the file somewhere mounted, e.g. testcases/, then:
docker compose run --rm prescient extract testcases/your_program.c
```

This compiles your file to LLVM IR and writes `output/features.json` — one
record per function with the 23 complexity metrics (instruction count,
cyclomatic complexity, `max_loop_depth`, loop/PHI/type/alias densities,
etc.).

Predict its compile cost (after `./run.sh train` has populated `models/`):

```bash
./run.sh predict path/to/your_program.c     # -> output/predictions.json
```

That gives each function a `low`/`medium`/`high` tier, a microsecond
estimate, and which passes are expected to be expensive.

Caveats worth knowing:

- **C only**, compilable by `clang-17` (the project is LLVM 17 / C-focused
  — no C++ front-end wired up).
- The file is compiled **standalone** with `clang -O0`. A self-contained
  `.c` using only standard headers (`<stdio.h>`, etc.) works directly. A
  file that `#include`s your own project headers would need extra `-I`
  include paths, which `run.sh` doesn't currently expose — you'd have to
  call `clang-17`/`opt-17` manually or preprocess first.
- `predict` only reflects this project's model, which was trained on a
  small 31-function corpus — treat the tiers as a rough signal, not ground
  truth (see [docs/EVALUATION.md](docs/EVALUATION.md)).

## Repository Layout

```
.
├── build.sh                       # configure + build the LLVM plugin
├── run.sh                         # extract / train / predict / evaluate / adaptive / demo entry point
├── CMakeLists.txt                 # LLVM-17 plugin build configuration
├── Dockerfile, docker-compose.yml # reproducible build environment
├── requirements.txt               # Python dependencies (scikit-learn, pandas, …)
├── README.md                      # overview + quick start
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
└── docs/
    ├── DESIGN.md                  # why this design — feature & model rationale
    ├── IMPLEMENTATION.md          # how it's wired — LLVM APIs, gotchas
    ├── EVALUATION.md              # measured prediction & savings results
    ├── CONTRIBUTING.md            # how to add features / passes / data
    └── (generated reports & plots — regenerated)
```

## Components

**Feature extractor.** The pass walks every defined function once, summing
per-instruction contributions for size, CFG edges, loop body coverage, PHI
density, type complexity and memory-op density. It writes one JSON record
per function, with a fixed schema that the Python model relies on.
[IMPLEMENTATION.md](docs/IMPLEMENTATION.md#feature-implementation-details)
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
`testcases/evaluation/` — see [EVALUATION.md](docs/EVALUATION.md) for
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
  worked example, analysed in [EVALUATION.md](docs/EVALUATION.md#failure-case-analysis).
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
