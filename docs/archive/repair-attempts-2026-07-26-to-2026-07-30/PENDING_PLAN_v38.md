# Pending Plan v38 — add default-ON cerr log of the actual DebugMode value reaching the cbuffer write

## State-machine routing decision
- Read `PENDING_PICK.md`, all v37 markers (PLAN/PLAN_REVIEW/COMMIT/IMPL_REVIEW/TESTS/TEST_AUDIT, all KEEP/ALL_KEEP), `PIPELINE_HEALTH_2026-07-27.md` tail.
- v37 cycle is complete at audit ALL_KEEP. Rule 9 fires → next item from PICK.
- Topmost unchecked items in `PENDING_PICK.md` are all parent-evidence-gated:
  - v33 (parent-evidence-gated)
  - v36 (parent-evidence-gated)
  - v32 (parent-driven, 12 branches keyed to evidence shape)
  - v15 (parent-driven)
  - v13a decision matrix (parent-driven)
  - v17 (parent-evidence-gated, mode-7 sentinel)
- Cron's prompt grants `enabled_toolsets: ["terminal", "file"]`, but every terminal probe in this tick (and the prior 12+ ticks) was blocked by tirith (`pending_approval: tirith:unknown`). Effective toolset remains file-only.
- The cron's prompt explicitly says: "If blocked by an external issue, record exact evidence in a marker and continue with the next mechanically actionable fix; do not silently stop."
- v38 is the next mechanically actionable file-only fix that closes a real diagnostic-surface gap and advances the pipeline without fabricating evidence or claiming tests passed.

## Why v38 — the gap being closed

The `HLVM_PT_DEBUG_MODE` env var / `r_GI_DebugMode` CVar flow has **no cerr-style diagnostic surface** for the actual value that lands in `Data.Params5[0]`. The code at `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:470-475`:

```cpp
int DebugMode = CVar_r_GI_DebugMode.GetValue();
if (const char* DebugModeEnv = std::getenv("HLVM_PT_DEBUG_MODE"))
{
    DebugMode = std::atoi(DebugModeEnv);
}
Data.Params5[0] = static_cast<float>(DebugMode);
```

If parent runs with `HLVM_PT_DEBUG_MODE=6` and:
- The CVar returns 0 (default) — env var overrides correctly → `Data.Params5[0] = 6.0f` → case 6u fires → per-pixel gradient appears
- The CVar returns non-zero and overrides the env var (no — the CVar is read first, then env var is checked, so env var should win) → `Data.Params5[0] = cvar_value` → case 6u does NOT fire
- `std::getenv` returns nullptr (env var silently dropped) → `Data.Params5[0] = cvar_value` → case 6u does NOT fire
- The env var contains a non-numeric value (e.g. "six") → `std::atoi("six") = 0` → `Data.Params5[0] = 0.0f` → case 0 (final) — no debug mode

All four of these would produce a silent failure where "case 6u didn't fire" looks identical in the dump. The only difference is the **value that landed in `Data.Params5[0]` before writeBuffer**. v38 emits that value to cerr in the same default-ON pattern as v12.

This is a one-line additive patch that bypasses spdlog entirely. The next parent rebuild will produce a cerr line `[RGI] FGIPass::WriteConstants: DebugMode effective=<N> (cvar=<M> env_var=<ptr>)` for every frame. The diagnostic value of this line is enormous:

| Effective value | CVar value | Env var | Diagnostic meaning |
|-----------------|-----------|---------|---------------------|
| 6 | 0 | "6" | env var override working correctly → case 6u should fire |
| 0 | 0 | "6" | env var being silently dropped → bug at `std::getenv` level |
| 0 | 0 | nullptr | env var not set (parent forgot to set HLVM_PT_DEBUG_MODE) |
| 0 | 5 | nullptr | CVar set to 5 → case 5u should fire |
| 6 | 6 | nullptr | CVar set to 6 → case 6u should fire |
| 0 | 0 | "garbage" | `std::atoi` returns 0 on garbage → env var set but unreadable |
| 6 | 6 | "garbage" | CVar read first, then env var read but `atoi` returns 0, then DebugMode=0… wait, no: in the current code, env var OVERRIDES. So if env var="garbage", DebugMode=0, even if CVar was 6. This would also surface in the cerr line as `effective=0 env_var=0x... (string=garbage)`. |

Note: cerr shows the pointer and the CVar value but to make the env-var string itself visible, the patch needs `std::getenv` to be called twice (once for the check, once for the log). That's a trivial 2-line change.

## Patch shape (1 file modified, +~12 / -0 lines)

Insert after line 475 in `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp`:

```cpp
        // v38 patch (six-role-pipeline): default-ON cerr log of the actual
        // DebugMode value reaching the cbuffer write. The env-var override
        // (HLVM_PT_DEBUG_MODE) and the CVar (r_GI_DebugMode) flow had no
        // spdlog-bypass surface; if parent runs with HLVM_PT_DEBUG_MODE=6 and
        // case 6u doesn't fire, this line disambiguates:
        //   - effective != env-var-value   → env var being silently dropped
        //   - effective != cvar-value      → cvar being overridden by env var
        //   - effective == cvar value, env_var=nullptr → parent forgot to set env var
        // Same default-ON pattern as v12 (bypasses spdlog level-filter).
        const char* DebugModeEnvForLog = std::getenv("HLVM_PT_DEBUG_MODE");
        std::cerr << "[RGI] FGIPass::WriteConstants: DebugMode effective=" << DebugMode
                  << " cvar=" << CVar_r_GI_DebugMode.GetValue()
                  << " env_var=" << (DebugModeEnvForLog ? DebugModeEnvForLog : "<null>")
                  << " Params5[0]=" << Data.Params5[0]
                  << std::endl;
```

This is 11 new lines (1 code line + 1 comment block + the actual cerr statement with multiple fields). The existing cerr at line 487 (v12) already added the `<iostream>` include, so no new include needed.

## Files produced
- `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp` (modified; +11 / -0 lines)
- `docs/PENDING_PLAN_v38.md` (this file, new)
- `docs/PENDING_PLAN_REVIEW_v38.md` (new)
- `docs/PENDING_COMMIT_v38.md` (new)
- `docs/PENDING_IMPL_REVIEW_v38.md` (new)
- `docs/PENDING_TESTS_v38.md` (new)
- `docs/PENDING_TEST_AUDIT_v38.md` (new)
- `docs/PENDING_PICK.md` (modified — v38 marked [x], v39 staged)
- `docs/PIPELINE_HEALTH_2026-07-27.md` (modified — appended v38 tick section)

## skip_plan_review: no
- This is a C++ source-code change to a load-bearing file (FGIPass.cpp is included by every consumer of the GI pass). Even though the patch is one-line + comment, the per-role audit trail must be preserved for parent's review-on-demand.
- HARD INVARIANT #2 does NOT fire: this is not a test file.

## produces_test_files: no
- Source code only. No test files.
- skip_impl_review: yes (per the rule "yes only for <50 line non-test diffs" — this is 11 lines and not a test file).
- BUT: the per-role audit trail is still useful for future ticks, so the full chain will still be invoked even though skip_impl_review=yes is justified.

## Test strategy (mostly self-verifiable, parent for runtime)
1. **Static tests (this tick, file-only)**:
   - `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp` contains new cerr write
   - Patch is between line 475 (`Data.Params5[0] = ...`) and line 477 (`CmdList->writeBuffer`)
   - No new `#include` needed (`<iostream>` and `<cstdlib>` already present)
   - Patch is mechanically correct (one cerr statement, no logic change)
2. **Runtime tests (parent-driven, terminal blocked by tirith)**:
   - Parent rebuilds and runs with `HLVM_PT_DEBUG_MODE=6`
   - stderr should show `[RGI] FGIPass::WriteConstants: DebugMode effective=6 cvar=0 env_var=6 Params5[0]=6` for every frame
   - This confirms: (a) env var reaches `std::getenv`, (b) `std::atoi` parses it, (c) the value lands in `Data.Params5[0]`, (d) the cbuffer write happens before the dispatch
   - If the cerr line shows `effective=0 env_var=6` → bug is in `std::atoi` (env var not parsed correctly)
   - If the cerr line shows `effective=0 env_var=<null>` → bug is in env-var passing (parent shell or test harness dropped it)
   - If the cerr line shows `effective=0 cvar=0 env_var=<null>` → parent didn't set the env var (expected for default-mode runs)

## Risks
- **Single-head host caveat**: all 6 roles share the same model. Verdicts are self-checks. Patch is purely additive (no behavior change in the GPU path; just adds a stderr line) so verdicts are reproducible.
- **Tirith block persists**: effective toolset remains file-only. v38 is the only meaningful action this tick can take without parent terminal access.
- **Cerr output volume**: 8 frames × 1 line per frame = 8 lines per test run. Negligible.
- **Build risk**: minimal. Adding a single cerr statement that uses already-included headers and already-imported types. No new compile-time dependencies.

## Decision matrix (post-parent-rebuild, post-v38)
- `effective=6 cvar=0 env_var=6` + `gi_raw` shows per-pixel gradient (case 6u fires) → cbuffer update path is healthy; bug is downstream → v39 stages trace/payload investigation
- `effective=6 cvar=0 env_var=6` + `gi_raw` STILL 0 (case 6u doesn't fire) → bug is downstream of cbuffer write (binding layout, dispatch, slangc dead-strip) → v39 stages UAV-write sentinel in a separate debug mode
- `effective=0 cvar=0 env_var=6` → `std::atoi` is broken or env var passed wrong → fix the test harness or the env-var read
- `effective=0 cvar=0 env_var=<null>` → parent didn't set the env var; this is a documentation gap → v39 stages README update
- `effective=5 cvar=5 env_var=<null>` → CVar-only path is working → env var isn't reaching the test (likely a parent shell issue) → v39 stages `r_GI_DebugMode` documentation

## Goal gate (unchanged)
**FAILED/UNVERIFIED** — six-criterion gate from the prompt:
- (a) Debug target builds cleanly — UNVERIFIED (tirith blocks terminal)
- (b) Fresh `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM>=8` run — UNVERIFIED
- (c) No command-list-already-open errors — UNVERIFIED
- (d) No Vulkan ERROR/VUID in fresh log — UNVERIFIED
- (e) Validator passes newest dump group — UNVERIFIED
- (f) Display visibly contains recognizable non-uniform Sponza — UNVERIFIED

After v38 lands, the diagnostic surface gains a new cerr line that disambiguates the cbuffer-update path. No `PIPELINE_GOAL_DONE_<date>.md` written.
