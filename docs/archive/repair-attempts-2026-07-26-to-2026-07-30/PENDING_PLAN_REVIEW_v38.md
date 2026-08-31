# Pending Plan Review v38 — default-ON cerr log of the actual DebugMode value reaching the cbuffer write

## Verdict
- **KEEP** — v38 plan correctly identifies a real diagnostic-surface gap (the `HLVM_PT_DEBUG_MODE` env-var / `r_GI_DebugMode` CVar flow has no stderr-bypass surface) and proposes a precisely-scoped one-line fix that aligns with the v12 default-ON cerr pattern.

## Design soundness
- The plan correctly diagnoses that line 470-475 in `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp` has **no log of the actual value** that lands in `Data.Params5[0]`. This is a genuine gap.
- The patch is purely additive (one cerr statement + comment). It does not change any code path or behavior.
- The 5-row decision matrix (effective=6, cvar=0, env_var=6 etc.) covers all the diagnostic shapes the cerr line can produce, each with a clear downstream action.
- Backward compatibility: existing pre-v38 builds will not produce the cerr line, but they will still work as before. The new line is strictly informational.

## Plan completeness
- 1 file modified, +11 / -0 lines — minimal blast radius.
- No new `#include` needed (existing `<iostream>` and `<cstdlib>` covers it; `<cstdlib>` is needed for `std::getenv` which is already used at line 471).
- HARD INVARIANT #2 does NOT fire (not a test file).
- skip_impl_review: yes (correctly justified per the "<50 line non-test diff" rule).
- 4-branch decision matrix covers all post-rebuild evidence shapes.
- Goal gate correctly notes that criterion (b/c/d) remain unverifiable without parent rebuild.

## Feedback for planner (FIX only)
- None — plan is well-scoped, minimal, and addresses a real diagnostic-evidence gap.
- Optional minor: the comment block could mention that the cerr line surfaces the cvar-vs-env-var override ordering (CVar read first, env var overrides) explicitly. But this is a nice-to-have, not a requirement.

## Self-review checklist
- [x] Validation: 4-branch decision matrix correctly disambiguates the cbuffer-update path.
- [x] Error handling: `std::getenv` is called twice (once for the override check, once for the log); the second call is safe because env vars are inherited at process start and don't change mid-test. `DebugModeEnvForLog` is checked for nullptr before deref.
- [x] Tests: 1 static test (mechanical patch correctness) + 3 parent-driven runtime tests (cerr line shape, case-6u behavior, validator).

## Single-head caveat
- Same model writes planner + plan-criticer. The KEEP is a self-check. The patch is mechanically simple (one cerr statement using already-included types) so the verdict is reproducible.

## Recommendation
- KEEP. Proceed to impler (apply the cerr write patch).
