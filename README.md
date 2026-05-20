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
3. **Compile-time model** (`src/model/`) — `LinearRegression`, `Ridge`
   and `RandomForest` regressors on `log1p(total_compile_time_us)`,
   trained on 272 functions from 40 diverse C programs, plus one
   `LinearRegression` per timed pass.
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

The order is always: **build → train → run**. The model must be trained
once before `demo`, `evaluate`, `predict` or `adaptive` will work.
The trained model persists on the host in `./models`, so `train` only
needs to be rerun when the code or training data changes.

### Docker (any OS — recommended)

Two convenience scripts wrap every `docker compose` command so you
never have to remember the raw Docker incantation:

```bash
git clone https://github.com/aakri0/Prescient.git && cd Prescient
./start.sh train              # first run auto-builds the image (~5 min)
./start.sh demo               # narrated five-act demo
./start.sh evaluate           # full evaluation suite
./start.sh                    # web UI at http://localhost:8080
./stop.sh                     # stop the web UI
```

Every `./start.sh <mode>` is a single command — it builds the image if
needed, then runs the requested pipeline step in a temporary container.
Run `./start.sh help` for the full list of modes.

### Native Linux (Ubuntu 22.04)

If you'd rather skip Docker entirely:

```bash
git clone https://github.com/aakri0/Prescient.git && cd Prescient
./scripts/setup_env.sh        # install LLVM 17 + Python deps (uses sudo apt)
./build.sh                    # build build/IRComplexityEstimator.so
./run.sh train                # generate the corpus and train the model
./run.sh demo                 # narrated five-act demo
./run.sh evaluate             # full evaluation suite
```

> `setup_env.sh` uses `apt`, so the native path is Ubuntu-only.
> macOS and Windows users should use the Docker path above.

## Web Frontend

A polished web UI is bundled — a Monaco-based code editor (the same
editor VS Code uses) on the left and a live analysis panel on the right,
with tabs for the IR feature table, compile-time predictions and
predicted per-pass cost. A draggable resizer between the two panels lets
you adjust widths to your preference.

```bash
./start.sh                            # open http://localhost:8080
./stop.sh                             # stop it
```

Native (Linux only):

```bash
pip install -r requirements.txt
python3 frontend/server.py            # open http://localhost:8080
```

Six built-in samples are available from the dropdown in both **C and
C++ variants** (simple add, branchy classify, triple-nested loops,
memory-heavy blur, nested structs, and the documented failure case).
Switching the language dropdown automatically loads the corresponding
sample set. The sample selector retains the current selection and shows
"Custom" when you edit the code manually. The Features tab shows the
IR complexity table alongside split **Before (O0)** and **After (O2)**
optimization impact tables with per-cell delta percentages.
`Ctrl + Enter` runs the analysis. See [frontend/](frontend/) for the
code.

## Usage

Extract features from a C file — `extract` prints a readable table and
writes the full detail to `output/features.json`:

```bash
$ ./run.sh extract testcases/sample_math.c
```

```
==============================================================================
  IR COMPLEXITY FEATURES  (14 function(s))
==============================================================================
  Function     Insts   BBs   Cyclo   Loops   Depth   PHIs   MemOps   AliasD   TypeCx
  ---------------------------------------------------------------------------------
  add              8     1       1       0       0      0        4     0.50     2.38
  power           25     5       2       1       1      0       12     0.48     2.44
  is_prime        33    10       4       1       1      0       14     0.42     2.55
  ...
extract: features written to output/features.json
```

`output/features.json` holds the complete 23-field record per function
(the table above shows a readable subset):

```json
[
  {
    "function_name": "is_prime",
    "instruction_count": 33,
    "basic_block_count": 10,
    "cyclomatic_complexity": 4,
    "max_loop_depth": 1,
    "loop_instruction_ratio": 0.6970,
    "phi_density": 0.0,
    "type_complexity_normalized": 2.5455,
    "alias_proxy_density": 0.4242
  }
]
```

Predict its compile cost (after `./run.sh train` has populated `models/`):

```bash
$ ./run.sh predict testcases/training/t02_nested_loops.c
```

`predict` prints readable tables — IR features, compile-time predictions
and predicted per-pass cost — and writes the full detail to
`output/predictions.json`:

```
==============================================================================
  COMPILE-TIME PREDICTIONS
==============================================================================
  Function          Tier     Pred (us)   Pred (ms)   Confidence
  -------------------------------------------------------------
  matrix_multiply   medium       4,180        4.18   high
  matrix_scale      medium       2,344        2.34   high
  matrix_trace      medium       1,514        1.51   high

  Tier summary : low=0  medium=3  high=0   (of 3 function(s))
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

The `extract` and `predict` modes accept any C file path — the bundled
testcases are only examples. A ready-made sample, `testcases/sample_math.c`
(a small program of arithmetic, loop, recursion and branch functions), is
included to try the modes on:

```bash
./start.sh extract testcases/sample_math.c
./start.sh predict testcases/sample_math.c
```

Check complexity of your own program:

```bash
./start.sh extract path/to/your_program.c
./start.sh predict path/to/your_program.c
```

`start.sh` automatically bind-mounts the file's directory into the
container, so any path on your machine works — the file doesn't have to
live under `testcases/`.

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

- **C and C++** are both supported in the web frontend. The CLI pipeline
  (`extract`, `predict`) works on C files compilable by `clang-17`.
- The file is compiled **standalone** with `clang -O0`. A self-contained
  `.c` using only standard headers (`<stdio.h>`, etc.) works directly. A
  file that `#include`s your own project headers would need extra `-I`
  include paths, which `run.sh` doesn't currently expose — you'd have to
  call `clang-17`/`opt-17` manually or preprocess first.
- The model was trained on a 272-function corpus from 40 diverse C
  programs. RandomForest achieves R² = 0.62 and MAE = 467 µs — usable
  for tier ranking but not precise microsecond estimates (see
  [docs/EVALUATION.md](docs/EVALUATION.md)).

## Repository Layout

```
.
├── start.sh                       # single entry point (Docker) — ./start.sh help
├── stop.sh                        # stop the web UI
├── build.sh                       # configure + build the LLVM plugin (native Linux)
├── run.sh                         # extract / train / predict / evaluate / adaptive / demo (native Linux)
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
│       ├── feature_importance.py  # docs/feature_importance_report.md + plots
│       └── _render.py             # shared terminal-table helper
├── frontend/                      # web UI (Monaco editor + Flask backend)
│   ├── server.py                  # /api/analyze drives the existing pipeline
│   ├── index.html
│   ├── style.css                  # professional dark theme
│   └── app.js                     # editor + render logic
├── scripts/
│   ├── setup_env.sh               # provisions LLVM 17 + Python deps
│   ├── generate_corpus.py         # build the joined training CSV
│   ├── time_o2_pipeline.sh        # run O2 under the timing plugin
│   ├── run_adaptive.sh            # end-to-end adaptive workflow for one .c
│   ├── generate_savings_report.py # markdown savings report
│   ├── evaluate.py                # full evaluation suite (issue #24)
│   └── demo.sh                    # narrated five-act demo
├── testcases/
│   ├── training/                  # 40 C files used for training (t01…t40)
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
tail dominate the fit. Three models are trained: `LinearRegression`,
`Ridge` and `RandomForest` (which achieves the best R² = 0.62 on the
272-function corpus). All share a `StandardScaler`; predictions clip
log-space output to `expm1(20.0)` so an extrapolating model cannot emit
an unbounded microsecond value.

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
| Training functions | 272 (from 40 C programs) |
| Best model R² (5-fold CV) | 0.62 (RandomForest) |
| Best model MAE | 467 µs |
| Best model MAPE | 49 % |
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
- **Training-set size is moderate (40 C files, 272 functions).** Cross-validated
  R² is 0.53–0.62 depending on the model. Adding more real-world programs
  (especially larger ones) would improve generalization further.
- **Linux / LLVM 17 only.** The plugin loads `IRComplexityEstimator.so`
  via `opt -load-pass-plugin`; the macOS / Windows toolchains are not in
  scope. CI runs on `ubuntu-22.04` exclusively.
- **The adaptive pass only acts on the `low` tier.** Medium and high tiers
  get the same O2 pipeline, so the upper end of the savings curve is bounded
  by how often we correctly identify low-tier functions.

## License

MIT — see [LICENSE](LICENSE) for the full text.
