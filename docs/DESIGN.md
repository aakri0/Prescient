# DESIGN.md — Why Prescient looks the way it does

This document is the intellectual record of the project: not *what* the
code does (that is [IMPLEMENTATION.md](IMPLEMENTATION.md)) and not *what
numbers it produces* (that is [EVALUATION.md](EVALUATION.md)), but *why
the design choices are what they are and what we considered and rejected*.

## 1. Problem decomposition

Predicting per-function compile time decomposes cleanly into four
sub-problems, each with its own technical content. Treating them as one
indivisible "ML problem" was the first thing we ruled out, because it
would have hidden which step was actually limiting accuracy.

**1a. Feature extraction from unoptimised IR.** The model has to predict
the cost of the *future* optimised pipeline using only what is visible in
the IR *before* the pipeline runs. The challenge is that unoptimised
clang IR is verbose: most of its instructions will be folded, hoisted or
deleted by the very passes whose cost we are trying to predict. A naive
"count the instructions" feature will systematically overestimate easy
functions and only weakly predict hard ones. The interesting design
question is which structural properties of unoptimised IR survive — or
strongly correlate with what survives — into the optimised form.

**1b. Data collection.** A regression model is only as good as its
labels, and labelling requires actually running the O2 pipeline through a
timing harness on enough functions to give a stable distribution. LLVM's
new pass manager exposes `PassInstrumentationCallbacks`, but its
instrumentation hooks fire for module, function, loop and SCC passes
indistinguishably; only function-level passes have a meaningful
per-function time. Building a corpus that joins per-function features to
per-function timings (and gracefully handles passes that fire several
times per function) is a real piece of infrastructure work, not a wrapper
around `-time-passes`.

**1c. Modelling.** Compile times are heavy-tailed: a small leaf function
takes microseconds, a pathological function takes tens of milliseconds, a
factor of ~10⁴ between the two. A linear model fit on raw microseconds
would let the tail dominate every loss; a model fit on log1p microseconds
is well-behaved but predictions have to be back-transformed safely. The
modelling question is not "which fancy model" but "what is the smallest
model that captures the heavy tail without overfitting and stays
interpretable enough to debug".

**1d. Pipeline integration.** A prediction is only useful if it changes
what the compiler does. The integration challenge is that LLVM's pass
managers are nested — module → CGSCC → function → loop — and an "adaptive
pipeline" must hook the function pass manager *while still using LLVM's
canonical `buildFunctionSimplificationPipeline`* so optimisation
guarantees stay intact. The clean answer is to build O1 and O2 pipelines
once at module-pass entry and dispatch per function, never to assemble
ad-hoc pipelines from raw pass names.

## 2. Feature selection rationale

The twelve features in `FEATURE_COLUMNS` (see
[train_model.py](../src/model/train_model.py)) cluster into six groups.
Each group earns its place by a hypothesised correlation with at least
one expensive O2 pass, and each group has a *known* failure mode.

**Function size — `instruction_count`, `basic_block_count`,
`argument_count`.** Every per-function pass scales at least linearly with
instruction count: InstCombine, GVN, EarlyCSE all walk every instruction.
Argument count is the cheapest possible cardinality proxy for caller
inlining bonus calculations. *Limitation:* size correlates with cost but
does not measure it — a function with a thousand `mov` instructions and
no control flow compiles in a fraction of the time of a thousand
instructions arranged into deep loops.

**Cyclomatic complexity — `cyclomatic_complexity`,
`max_bb_successors`.** Classic McCabe metric `E − N + 2`. Predicts cost
for any pass that enumerates control-flow paths: JumpThreading,
SimplifyCFG, CorrelatedValuePropagation. `max_bb_successors` catches the
specific case of giant `switch` statements that defeat path-based
heuristics. *Limitation:* unreachable or trivially-dead branches inflate
the metric, and they will be precisely the ones SimplifyCFG eats first.

**Loop metrics — `loop_count`, `max_loop_depth`,
`loop_instruction_ratio`.** Loop-cost is dominated by LICM, LoopVectorize
and LoopRotate, all of which run per-loop and at multiple analysis
depths. Depth and body coverage matter independently: a depth-3 nest
with a tiny body and a single deep loop with a huge body have very
different cost profiles. *Limitation:* the extractor cannot distinguish a
loop whose trip count is statically one (folded away) from a real loop.

**PHI density — `phi_density`, `max_phi_in_single_bb`.** PHIs are a
direct cost signal for SROA, Mem2Reg and GVN. The maximum count in a
single block is a sharper signal than the average: a single 50-input PHI
costs more than fifty 1-input PHIs scattered across the function.
*Limitation:* clang's `-O0 -disable-O0-optnone` IR contains
mem-to-mem moves rather than PHIs, so the density reads low for
optnone-disabled IR even when the optimised form will be PHI-heavy.

**Type complexity — `type_complexity_normalized`,
`max_pointer_depth`.** Nested pointer-to-struct types blow up
InstCombine's GEP simplification, Mem2Reg's type splitting and SLP's
vectorisation heuristics. The recursive `scoreType` (in
[IRComplexityPass.cpp](../src/passes/IRComplexityPass.cpp)) is bounded
by depth ≤ 10 to handle self-referential types. *Limitation:* LLVM 17
defaults to opaque pointers, so the recursion bottoms out at depth 1 for
pointer types and the score loses much of its discriminative power on
modern IR.

**Alias proxy density — `alias_proxy_density`, plus the raw load /
store / GEP / memintrinsic counts.** Loads, stores, GEPs and memory
intrinsics drive every alias-analysis-consuming pass: GVN, LICM,
DSE, MemCpyOpt. Their *count* is what we can observe statically; their
real cost depends on how many of them alias, which requires running
alias analysis — exactly what we are trying to avoid. *Limitation:*
this is a proxy and we say so prominently in the README. Two functions
with identical load/store counts can differ tenfold in true aliasing
work.

## 3. Why static analysis over dynamic profiling

Dynamic compile-time profiling — instrument the optimiser, run it once
to learn each function's actual cost, then decide what to skip on later
builds — is the obvious alternative. It is rejected for one structural
reason and several practical ones.

**Cold-start.** A profile-guided approach has no signal on a function it
has never compiled. The first build of any new function pays full O2.
For interactive development, where the bulk of edited files are
fresh-touched, that is exactly the case we most want to optimise.
Static prediction is *zero-shot* by construction: a function with
features falling inside the trained distribution gets a useful
prediction the first time it is compiled.

**Trade-off.** Static prediction trades accuracy for availability. A
profile-guided number is correct (modulo measurement noise) for the
function it was measured on; a static prediction is an approximation
that holds across the whole distribution. The project explicitly accepts
that trade — the `low` tier is only ever opted *out* of work, and the
default for unknown functions is `medium` (full O2), so a wrong
prediction loses some compile-time savings but never correctness.

**Operational.** A profile-guided system has to manage a profile store,
keep it fresh across source edits and decide when a profile is stale.
Prescient's only persistent state is a ~5 KB scaler plus a ~10 KB
linear model in `models/`, both reproducible from the training set with
one command.

## 4. Model choice

Three models are trained on `log1p(total_compile_time_us)` with
`StandardScaler`-normalised features: `LinearRegression`,
`Ridge(alpha=1.0)` and `RandomForestRegressor(n_estimators=100)`. The
cross-validation comparison is in
[EVALUATION.md §2](EVALUATION.md#2-prediction-accuracy).

**RandomForest is the most accurate model.** With 272 training functions
from 40 diverse C programs, RandomForest achieves R² = 0.62, MAE = 467 µs
and MAPE = 49 % — substantially better than LinearRegression (R² = 0.53,
MAE = 1,154 µs) and Ridge (R² = 0.54, MAE = 1,040 µs). The corpus is
now large enough for the ensemble to generalise without severe overfitting.

**Why LinearRegression is still the default for tier assignment.**

1. *Interpretability.* The trained model has twelve coefficients on
   standardised inputs, so their magnitudes are directly comparable and
   the feature-importance report can be generated mechanically from
   `coef_`. The `predict.py` "tier rationale" line ("phi_density=0.18,
   instruction_count=412") falls out of the same coefficients —
   impossible with a random forest without SHAP or similar.
2. *Microsecond inference.* The whole pipeline must not itself become a
   compile-time cost. A linear predict over twelve features is a
   handful of multiply-adds; loading and running the RandomForest is two
   orders of magnitude slower and dominates the time the prediction is
   trying to save on tiny functions.

**Why we keep all three.** Ridge serves as a regularised comparison,
RandomForest gives the best point estimates for the web frontend's
predictions, and LinearRegression provides the interpretable tier
assignments and feature-importance coefficients. All three are shipped
so users can compare.

## 5. Adaptive pipeline design — tiers, not budgets

The natural alternative to discrete tiers is a continuous compile-time
budget: predict each function's cost, sum across the module, allocate a
budget proportional to predicted savings, run as many passes as the
budget allows. We rejected this for one reason.

**Correctness guarantees.** Every shipped LLVM pipeline (`O0`, `O1`,
`O2`, `O3`) is a fixed sequence whose interactions have been audited:
SROA before Mem2Reg, EarlyCSE before GVN, LICM before LoopRotate, and so
on. Running an arbitrary subset of these passes risks ordering bugs
(SROA without an earlier mem2reg may leave allocas unpromoted; LICM
without LoopSimplify may miss invariants). The tiered design only
ever switches between *whole* shipped pipelines (O1 vs O2), with one
narrow exception: the `LoopVectorizePass` / `SLPVectorizerPass` skip,
which is implemented as a `registerShouldRunOptionalPassCallback`
filter — the same mechanism LLVM uses internally to skip optional passes
under `-O0`, so it is already audited to be a safe drop.

**Tradeoff acknowledgement.** A budget approach could in principle save
more by stopping mid-pipeline on a per-function basis. Tiers leave that
on the table. The simplification is a deliberate trade: a smaller savings
ceiling in exchange for a much smaller correctness-risk surface that an
external compiler engineer can review and reason about.

## 6. Alternatives considered

**Profile-guided compilation (PGO).** Rejected for the cold-start reason
in §3. PGO requires a representative runtime profile, which is precisely
what is unavailable in CI on a freshly edited branch — the very case
this project is optimising for.

**ML-based pass ordering (MLGO / Inliner-RL).** Google's MLGO trains
reinforcement-learning agents to pick pass orders or inlining decisions.
We rejected it because (a) the training infrastructure (RL on TFLite
models inside LLVM) is large compared to a single-file scikit-learn
pipeline, (b) it solves a different problem — optimising the *quality*
of the output, not the *cost* of producing it — and (c) interpretability
is even worse than RandomForest.

**LLVM's existing `-time-passes`.** The `-time-passes` flag is what we
use to collect labels, but it is fundamentally *retrospective*: it tells
you what the pipeline cost *after* running it. Useful as a measurement
tool, useless as a decision input. The whole project exists because
nothing in shipped LLVM gives you the cost *before* the pipeline runs.

## 7. Known failure modes

See [test07_analysis.md](../testcases/evaluation/test07_analysis.md) for
the worked example: `misleading_complex` in
[test07_failure_case.c](../testcases/evaluation/test07_failure_case.c)
presents a depth-3 loop nest full of instructions whose operands are all
literal constants. The model sees a large `instruction_count` and a
maximum `max_loop_depth` and predicts HIGH-tier; InstCombine + SCCP fold
the entire body to a single constant on the first pass and the function
compiles in a fraction of the predicted time. This is the canonical
class of failure: **any pass with a data-dependent early-exit can defeat
any purely static feature set**.

Three function classes that share this property and would benefit from
new features:

1. **Constant-folding-heavy functions** (test07): defeated by SCCP /
   InstCombine. Fix: add a *loop-invariant instruction ratio* feature —
   the fraction of in-loop instructions whose operands are all constants
   or loop-invariant. Cheap to compute from the existing extractor.
2. **Dead-branch-heavy functions** (e.g. C code generated from a macro
   expansion that statically picks one branch). Defeated by SimplifyCFG.
   Fix: a *trivially-dead-edge ratio* — successors whose condition is a
   compile-time constant.
3. **Pure-template instantiations** in C++ where all the type-level work
   is folded by the front-end. Out of scope: we only test on C.

The honest scope statement is in the README's Limitations section: static
features can be made robust to common constant-folding illusions, but
they cannot be made exact.
