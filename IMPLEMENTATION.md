# IMPLEMENTATION.md — How Prescient is wired into LLVM

This document is for the LLVM-familiar reader who needs to read or
extend the plugin. It does not motivate design choices (see
[DESIGN.md](DESIGN.md)) and does not report results (see
[EVALUATION.md](EVALUATION.md)). It documents *which LLVM APIs are
called from which file* and *which subtle LLVM 17 behaviours we depend
on*.

## 1. LLVM pass architecture

The whole project ships as a single loadable plugin,
`build/IRComplexityEstimator.so`, that registers three things on the
host process's `PassBuilder`:

- the `ir-complexity` module pass (feature extractor)
- the `adaptive-pipeline` module pass (tier-driven per-function
  pipeline)
- the per-pass timing instrumentation (no pass, just a
  `PassInstrumentationCallbacks` hook)

**New PM only.** The new pass manager (NPM) is LLVM 17's default and the
legacy PM is on its way out — every new feature lands on the NPM and the
legacy versions of `LoopVectorizePass` and `SLPVectorizerPass` we depend
on for the skip mechanism (§3) only exist on the NPM. We therefore made
no attempt to support the legacy PM.

**Plugin loading chain.** The exact chain from `dlopen` to a registered
pass is:

1. `opt -load-pass-plugin ./build/IRComplexityEstimator.so` calls
   `dlopen(3)` on the `.so`.
2. `opt` looks up the `llvmGetPassPluginInfo` symbol — declared in
   [IRComplexityPass.cpp:321](src/passes/IRComplexityPass.cpp) — and
   calls it.
3. The function returns a `llvm::PassPluginLibraryInfo` struct
   containing a `RegisterPassBuilderCallbacks` lambda.
4. `opt` invokes that lambda with its own `PassBuilder`. The lambda
   calls `PB.registerPipelineParsingCallback` (so `ir-complexity` and
   `adaptive-pipeline` can be named on the `-passes=` command line) and
   forwards to `registerPassTiming(PB)` and
   `registerAdaptivePipeline(PB)` to wire the timing callbacks and the
   adaptive-pipeline registration into the same `PassBuilder`.

We deliberately do *not* use `registerVectorizerStartEPCallback` or any
other extension-point hook. Those EPs run a pass at a fixed point in the
standard pipeline; we want our pass to be addressable by name on the
command line for offline use (`opt -passes="ir-complexity"`), and that is
exactly what `registerPipelineParsingCallback` gives us.

**Adding a new pass.** The mechanical steps to add another module pass
that ships in the same plugin:

1. Add a new `.cpp` to `src/passes/` defining a `struct YourPass :
   PassInfoMixin<YourPass>` with a `PreservedAnalyses run(Module &,
   ModuleAnalysisManager &)` method and `static bool isRequired()
   { return true; }`.
2. Add a free function `void registerYourPass(PassBuilder &PB)` at the
   bottom of the file that calls
   `PB.registerPipelineParsingCallback(...)` with the pass's command-line
   name.
3. Forward-declare `registerYourPass` in
   [IRComplexityPass.cpp](src/passes/IRComplexityPass.cpp) and call it
   from the plugin's `RegisterPassBuilderCallbacks` lambda — exactly the
   way `registerAdaptivePipeline` is wired today.
4. Append the new `.cpp` to the `add_library(IRComplexityEstimator
   MODULE …)` call in [CMakeLists.txt](CMakeLists.txt) and rebuild.

There is no `.h` header for the plugin: the only cross-file linkage is
the three free functions named above, declared inline in
`IRComplexityPass.cpp`. This is intentional — the plugin has exactly one
public symbol (`llvmGetPassPluginInfo`) and adding headers would only
encourage out-of-plugin coupling.

## 2. Feature implementation details

All extraction lives in [IRComplexityPass.cpp:109
`analyzeFunction`](src/passes/IRComplexityPass.cpp). One scan of each
basic block computes every feature; the only nested traversal is the
recursive `scoreType` for type complexity. Per-feature APIs:

**Loop depth.** Header: `llvm/Analysis/LoopInfo.h`. The pass gets the
function's `LoopInfo` via
`FAM.getResult<LoopAnalysis>(F)`, where `FAM` is the
`FunctionAnalysisManager` retrieved with
`MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager()`.
`LoopInfo::getLoopsInPreorder()` yields every loop including nested
ones; per-loop depth is `Loop::getLoopDepth()`. Loop body coverage is
computed from `LoopInfo`'s **top-level loops only** (the iterator
`begin()/end()`, not `getLoopsInPreorder`), because each top-level
loop's `getBlocks()` already covers its nested loops, so iterating both
would double-count.

**CFG edges and cyclomatic complexity.** Header: `llvm/IR/CFG.h`. For
each `BasicBlock &BB` the pass uses
`std::distance(succ_begin(&BB), succ_end(&BB))` to count outgoing edges.
Cyclomatic complexity is `M = E - N + 2`, guarded by an explicit
underflow check (`if (E + 2 > N)`) so a single-block function with no
successors returns `M = 1` rather than wrapping around.

**PHI nodes.** PHIs are detected with `isa<PHINode>(&I)` inside the main
instruction-iteration loop. Per-PHI incoming-edge count comes from
`PN->getNumIncomingValues()`. The `max_phi_in_single_bb` field tracks
the maximum across blocks, not the total — see
[DESIGN.md §2 "PHI density"](DESIGN.md#2-feature-selection-rationale) for
why.

**Type complexity.** The recursive `scoreType` in
[IRComplexityPass.cpp:79](src/passes/IRComplexityPass.cpp) walks
LLVM types via `Type::getTypeID()` plus `dyn_cast<StructType>` /
`dyn_cast<ArrayType>` / `dyn_cast<VectorType>`. `StructType::elements()`
gives the element-type range for the recursion. Pointer depth is
tracked with a separate counter so we can report
`max_pointer_depth` independently of the structural score. The
recursion is depth-capped at 10 — without that, a self-referential
struct (a linked list whose payload contains another such list) would
recurse forever.

**Alias proxy.** Three `isa` checks in the main loop (`LoadInst`,
`StoreInst`, `GetElementPtrInst`) plus a fourth for memory intrinsics
(`isa<IntrinsicInst>` followed by an ID switch over `Intrinsic::memcpy`,
`Intrinsic::memmove`, `Intrinsic::memset`). The density is
`total_memory_ops / instruction_count`.

**Function size.** Three numbers from the `Function` API: `F.size()`
(basic-block count), `F.arg_size()` (argument count) and a manual sum
of `BB.size()` across all blocks (instruction count). Call sites are
counted with `isa<CallInst>(I) || isa<InvokeInst>(I)` inside the same
loop. No callee analysis is performed; the count is local.

## 3. Timing instrumentation

[PassTimingWrapper.cpp](src/timing/PassTimingWrapper.cpp) defines a
single `PassTimingInstrumentation` class with a process-lived instance
(returned by a function-local `static`, so it never relies on global
constructor order). Hook registration:

```cpp
PIC.registerBeforeNonSkippedPassCallback([this](StringRef PassID, Any IR) {
    onBeforePass(PassID, IR);
});
PIC.registerAfterPassCallback([this](StringRef PassID, Any IR,
                                     const PreservedAnalyses &) {
    onAfterPass(PassID, IR);
});
```

`registerBeforeNonSkippedPassCallback` (not `registerBeforePass`) is the
correct hook — it fires only when the pass will actually run, which
means our `before` events match the `after` events even when other
plugins filter passes out via `ShouldRun` callbacks. Pairing the wrong
hooks here is a classic source of off-by-one stale entries in the
`StartTimes` map.

**Why the `any_cast<const Function *>` filter is necessary.** The new
PM's instrumentation passes IR units wrapped in `llvm::Any`, and the
underlying pointer can be `const Function *`, `const Module *`, `const
Loop *` or `const LazyCallGraph::SCC *` depending on whether the pass
runs at function, module, loop or CGSCC scope. Recording a time for a
module or loop pass under a single "function name" is meaningless — we
would over-count module passes against the first function alphabetically.
The filter:

```cpp
StringRef functionName(const Any &IR) {
    if (const Function *const *FP = any_cast<const Function *>(&IR))
        return (*FP)->getName();
    return StringRef();
}
```

returns an empty `StringRef` when the IR unit is *not* a function, and
both `onBeforePass` and `onAfterPass` early-return on that. (Note: we
use the pointer-returning overload `any_cast<T>(&v)` rather than
`any_isa<T>` — `any_isa` is deprecated in LLVM 17 and removed in 18.)

**CSV layout and flushing.** One row per `(function, pass)` pair, four
columns: `function_name,pass_name,time_us,timestamp`. Both
`function_name` and `pass_name` are emitted as quoted CSV fields with
double-quote doubling per RFC 4180, because LLVM pass names such as
`RequireAnalysisPass<A, B, C>` contain commas. The CSV is flushed after
every row (`OS->flush()`), so a later opt crash never loses the
preceding rows.

`registerTimingCallbacks(PIC)` is reused from
[AdaptivePipeline.cpp:113](src/passes/AdaptivePipeline.cpp) so the
*nested* pipelines that the adaptive pass runs go through the same
instrumentation and land in the same CSV.

## 4. Opaque pointer handling

LLVM 17 makes opaque pointers the default: an `i32*` and a `%struct.S*`
have the same `Type` and there is no way to recover the pointee type
from the pointer type alone. Our type-complexity recursion handles this
explicitly in
[IRComplexityPass.cpp:84-92](src/passes/IRComplexityPass.cpp):

```cpp
if (T->isPointerTy()) {
    unsigned d = ptrDepth + 1;
    if (d > maxPtrDepth) maxPtrDepth = d;
    if (T->isOpaquePointerTy())
        return 1;  // pointee type is not recoverable
    return 1 + scoreType(T->getNonOpaquePointerElementType(), ...);
}
```

The `isOpaquePointerTy()` check is mandatory: calling
`getNonOpaquePointerElementType()` on an opaque pointer triggers an
LLVM assertion. The contribution of an opaque pointer to the
type-complexity score is therefore 1 rather than `1 + score(pointee)`,
which is honest — we cannot see the pointee — but it does reduce the
discriminative power of the feature on modern IR. The reduction is
acknowledged in [DESIGN.md §2 "Type complexity"](DESIGN.md#2-feature-selection-rationale).

Pointer *depth* (the maximum `ptrDepth` argument seen during the
recursion) is unaffected — the recursion still increments it once per
pointer level, and even with opaque pointers the surface form of a type
preserves nesting.

## 5. JSON output

We write JSON by hand with `llvm::raw_fd_ostream` and `llvm::format`,
not through a JSON library. The reasons are simple:

- The plugin must be self-contained. Adding a dependency (nlohmann,
  RapidJSON, …) widens the link surface and the build matrix.
- The schema is fixed and shallow — a flat array of flat objects.
  Manual emission is ~30 lines of code in `writeJSON`
  ([IRComplexityPass.cpp:206](src/passes/IRComplexityPass.cpp)) and
  has no failure modes a library would catch.
- Floats are emitted with `format("%.4f", v)` for stable diffability of
  the JSON across runs.

The `jstr` lambda escapes `"` and `\` for safety against function names
containing those characters (rare but legal in mangled C++ symbols).

**Output path is a `cl::opt`.** `ComplexityOutput` is registered as
`-complexity-output=<path>` via `llvm::cl::opt`. The same flag pattern
is used for `-timing-output=<path>` in `PassTimingWrapper.cpp`. Both
default to a file in the current working directory so the plugin can be
exercised with one-line `opt` invocations.

## 6. Python ↔ C++ integration

The contract between the C++ extractor and the Python model is two
files plus one promise:

1. **`features.json`** — flat array, schema fixed by
   `FunctionFeatures` ([IRComplexityPass.cpp:33](src/passes/IRComplexityPass.cpp)).
2. **`models/feature_scaler.joblib`** — the `StandardScaler` saved by
   `train_model.py` after fitting on the training corpus. Predictions
   must apply *this* scaler — refitting at predict time would scale a
   single function against itself and destroy the distance-from-training
   meaning of every feature.
3. **`models/training_metadata.json`** — the *promise* part. Its
   `features` array lists the feature column names in the exact order
   the scaler and the model expect. `predict.py` builds the input
   vector by iterating that list, not by sorting JSON keys, so adding a
   feature in C++ and forgetting to update `FEATURE_COLUMNS` produces a
   clear missing-feature warning rather than a silent column shift.

`predict.py` enforces the contract: missing features are filled with 0
*and* logged, never silently ignored.

## 7. Known LLVM gotchas

We hit each of these during implementation and the code carries an
explicit guard for it. Worth documenting because anyone extending the
plugin will hit them again.

**Declaration check before iteration.** `Function::isDeclaration()`
returns `true` for externals — functions with no body. Iterating their
basic blocks gives garbage (`F.size() == 0`, no `BasicBlock`s to scan)
and any feature read on them is uninformative noise. Both
[IRComplexityPass.cpp:279](src/passes/IRComplexityPass.cpp) and
[AdaptivePipeline.cpp:135](src/passes/AdaptivePipeline.cpp) skip
declarations before doing anything.

**Loop-simplify form dependency.** `LoopAnalysis` does not itself put
loops into LCSSA or simplified form. We only consume `getLoopDepth()`
and `getBlocks()`, both of which are well-defined on un-simplified
loops, so we do not need to add a `LoopSimplify` requirement. If you add
a feature that consumes preheaders or single-exit form, you must request
`LoopSimplify` before reading.

**`-O0` adds `optnone`.** clang stamps every `-O0` function with the
`optnone` attribute, which makes the subsequent O2 pipeline skip every
real pass — producing an empty timing CSV and a useless training row.
Both `generate_corpus.py` and `evaluate.py` pass `-Xclang
-disable-O0-optnone` for exactly this reason. Symptom of forgetting: a
`timings.csv` with two or three module-level rows and no function-level
rows.

**`any_isa` is deprecated.** As of LLVM 17 the API expects
`any_cast<T>(&v) != nullptr`. The deprecation is silent at compile
time; the only sign is that examples on the LLVM wiki using `any_isa`
fail to build. We use the pointer-returning overload throughout (see
[PassTimingWrapper.cpp:44-47](src/timing/PassTimingWrapper.cpp)).

**Linking the plugin against LLVM component libraries.** Do not. The
plugin is `dlopen`'d into `opt`, which already provides every LLVM
symbol; linking the component libraries into the plugin registers the
LLVM command-line options a second time and aborts `opt` with `Option
registered more than once`. The comment block above
`llvm_map_components_to_libnames` in
[CMakeLists.txt:42-50](CMakeLists.txt) documents this — the call is
present only so a future contributor reading the CMake file does not
add `target_link_libraries(IRComplexityEstimator ${LLVM_COMPONENT_LIBS})`
thinking it was forgotten.
