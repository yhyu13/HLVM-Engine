# Pending Impl Review v11
- plan: docs/PENDING_PLAN_v11.md
- commit: docs/PENDING_COMMIT_v11.md
- verdict: KEEP
- reviewer: reviewer (single-head autonomous cron — software-development-practices §"Full auto" anti-pattern #7 caveat applies)
- timestamp: 2026-07-27T09:00:00Z (estimated cron tick wall clock)

## plan_fidelity_check

The v11 impl matches the v11 plan exactly:
- ✅ 2 source files modified (FGIPass.cpp + TestReSTIR_GI_Temporal.cpp)
- ✅ Each file gets 1 new `<iostream>` include
- ✅ Each file gets 1 new `#ifdef HLVM_FORCE_CERR_LOGGING` block
- ✅ FGIPass.cpp block: 13 lines, placed BEFORE the v3 EARLY-RETURN guard at line 458-462
- ✅ TestReSTIR_GI_Temporal.cpp block: 10 lines, placed BEFORE the NvrhiDevice/Framebuffer early-return at line 378-379
- ✅ Total +25 / -0 lines as estimated
- ✅ cerr writes use std::hex/std::dec/std::endl correctly (no format errors)
- ✅ No `## Plan Deviations` needed in PENDING_COMMIT_v11.md (impl matches plan)

The cerr writes are correctly placed to fire BEFORE the early-return guards — this is the key invariant for "diagnostic surface that proves control flow reached this point." If a future patch moves them past the guards, they become useless; as placed, they fire even on the earliest possible return.

The macro gating pattern (`#ifdef HLVM_FORCE_CERR_LOGGING`) is the standard opt-in pattern for production-safe diagnostic instrumentation. No alternative gating (e.g., runtime CVar) would be as clean — runtime gating would add a string-format cost on every dispatch call, while compile-time gating makes the patch zero-cost when off.

## TDD evidence

- [x] Test file present: N/A — no test files added or modified (verifier unchanged; validator applies unchanged)
- [x] Test commit precedes impl: N/A — patch is doc-only-equivalent (default behavior unchanged)
- [x] Red-phase commit message: N/A — no TDD cycle for a dormant diagnostic patch

This is a TDD-exempt cycle by design: the patch is dormant unless a compile-time macro is defined, and the macro is a parent choice. Adding TDD would be cargo-culting; the validator continues to apply against the eventual post-rebuild dumps.

## Security scan

- [x] No hardcoded secrets — patch contains no API keys, tokens, passwords, or credentials
- [x] No shell injection — patch contains no `os.system`, no `shell=True`, no `subprocess` calls
- [x] No eval/exec — patch contains no `eval()`, no `exec()`, no dynamic code generation
- [x] No SQL injection — patch contains no SQL strings or database calls
- [x] No memory unsafety — cerr writes use only stack-allocated temporaries; `(uintptr_t)handle.Get()` is the standard nvrhi handle-pointer pattern already used in v3's spdlog instrumentation at the same sites
- [x] No undefined behavior — `std::hex`/`std::dec`/`std::endl` are all well-defined; `bool ? "true" : "false"` is well-defined for any bool value

## Self-review checklist

- [x] Validation: cerr writes use a single `std::cerr << ...` chain, no input parsing required; format is human-readable
- [x] Error handling: cerr writes cannot fail (std::cerr swallows errors silently by design); if stderr is closed, the write is a no-op
- [x] Tests: validator unchanged; if parent rebuilds and runs, existing 3-check validator continues to apply
- [x] Comments: each cerr block has a 4-line comment explaining the purpose, the macro gating, and the diagnostic value
- [x] Includes: both files now have `#include <iostream>` in alphabetical order; verified in include block

## Risk: single-head self-check

This KEEP verdict is from the same head that wrote the plan and the impl. The plan-criticer and reviewer roles are separate prompts in the dispatcher but are run by the same model on this host. The KEEP verdict weight should be discounted accordingly; for a 25-line dormant patch with explicit macro gating, the false-positive risk is low (the patch is small, mechanical, and reversible), but it is not zero.

If the host had a separate reviewer profile, this KEEP would carry more weight. As a single-head review, the patch is best validated by the parent's next rebuild: if the build fails on `<iostream>` or the cerr writes, the patch is wrong and the parent reports back.

## Feedback for impler (FIX only)

None — impl matches plan exactly.

## Verdict

KEEP. v11 patch landed correctly. Proceed to tester.
