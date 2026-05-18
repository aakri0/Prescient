# Failure Case Analysis — `test07_failure_case.c`

## Summary

`misleading_complex()` is a function the prediction model gets badly
wrong. Statically it looks like an expensive, HIGH-tier compilation; in
reality it compiles very quickly. This document explains why the model
fails, which optimization pass short-circuits the work, what feature
would let the model see through the illusion, and whether the failure is
fixable without redesigning the feature set.

## Which features caused the over-prediction

The function presents three of the model's strongest cost signals at
high values. Its body is a long straight-line block of arithmetic — about
nineteen named temporaries plus a wide reduction — wrapped in a 3-level
loop nest, so `instruction_count` is large (well above the training-set
mean) and `max_loop_depth` is 3. Because the body also writes to and
reads from the `out` accumulator, `loop_instruction_ratio` is close to
1.0: almost every instruction lives inside a loop. `instruction_count` is
the single largest positive coefficient in the trained `LinearRegression`
model, and loop depth is the feature domain knowledge says matters most.
The model dutifully multiplies these together and predicts a long,
HIGH-tier compilation dominated by loop vectorisation and LICM.

The static feature extractor has no way of knowing that the operands are
all literal constants. It counts an `add` of two constants and an `add`
of two loop-varying values identically — both are one instruction, both
sit at loop depth 3. The features describe the *shape* of the unoptimised
IR, not the *amount of real work* that survives optimization. For this
function the shape is large and the surviving work is almost nothing, and
the model only sees the shape.

## Which pass short-circuits the expensive work

Every temporary in the loop body (`a` through `z`) is computed purely
from integer literals and earlier constant temporaries. None of them
depend on the loop induction variables `i`, `j`, `k` or on any value
loaded from memory. The first run of **InstCombine** together with
**SCCP** (Sparse Conditional Constant Propagation) constant-folds the
entire block: the nineteen temporaries and the wide final sum collapse to
one integer constant. Dead code elimination then removes all the folded
instructions. After that single cheap simplification pass the loop body
is just `out += <constant>`.

The expensive passes the model is most afraid of never get any real work.
**LICM** has one trivial invariant to consider; **LoopVectorize** inspects
a loop whose body is a single constant add and a memory accumulate and
quickly decides there is nothing worth vectorising; **GVN** has almost no
values to number. The cost of compiling this function is therefore
dominated by the one cheap constant-folding pass, not by the loop
optimisations — the opposite of what the feature vector implies. Measured
against the model's HIGH-tier prediction, the actual per-pass total is
smaller by well over 5×.

## What feature would fix this, and is it fixable

The missing signal is *how much of the IR is constant or loop-invariant*.
A concrete new feature would be a **loop-invariant instruction ratio**:
the fraction of instructions inside loops whose operands are all
constants or are themselves loop-invariant. This is cheap to compute in
the existing `IRComplexityPass` — LLVM already exposes `Loop::isLoopInvariant()`
and a constant check on operands — and it does not require running any
optimization. For `misleading_complex()` this ratio would be close to
1.0, and a model trained with it would learn to discount
`instruction_count` and `max_loop_depth` heavily when the invariant ratio
is high. A related feature, a *constant-operand ratio* over all
instructions, would catch the same pattern.

This particular failure mode **is fixable without redesigning the feature
set** — it only needs one or two additional features of the same kind
(cheap, static, computed from unoptimised IR). The deeper limitation is
fundamental and not fixable this way: the model predicts post-optimization
cost from pre-optimization IR, so any pass with a data-dependent
early-exit can defeat any purely static feature set. The loop-invariant
ratio closes *this* gap; a function whose work is eliminated by, say,
profile-guided dead-path removal would need a different feature again.
Honest scope: static features can make the model robust to common
constant-folding illusions, but they cannot make it exact.
