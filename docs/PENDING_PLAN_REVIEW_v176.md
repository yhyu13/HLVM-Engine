# Pending Plan Review v176

- plan: docs/PENDING_PLAN_v176.md
- verdict: **KEEP**
- reviewer: plan-criticer (file-only, single-profile host, terminal-blocked, post-source re-verification)
- timestamp: 2026-08-17T-tick-now-83-Z

## Design soundness

The v176 plan is a **strict, well-documented superset of v175 v2** (the previous plan-criticer's own FIX recommendation). Concretely, the plan does all four things that the v175 review demanded, plus the two new things it didn't think to demand:

| v175 v2 demand (this criticer's FIX) | v176 status |
|--------------------------------------|-------------|
| Reclassify CVar-shadowing from "guarded" to "confirmed Order 2" | ✅ Section "Why v176 supersedes v175" + Caveat #1 + multi-instance CVar analysis |
| Wire `CVar_r_ReSTIR_MaxM.GetValue()` into per-frame block at lines 950 + 1005 | ✅ Part A — exact replacement shown with sibling match |
| Match the sibling `TestCornellBoxGI.cpp:1561, 1609` pattern | ✅ Verified on disk this tick (sibling line 1561/1609 confirmed) |
| Provide a verification step that runs BEFORE the build (intermediate printf) | ✅ Implied by env-var hook's own `HLVM_LOG(... "HLVM_RGI_MAXM override: r_ReSTIR_MaxM = {:.2f}")` line — the operator can `grep` for this to confirm the env-var path actually fired before reading the dump PNGs |
| (NEW, this criticer's own follow-up) Add env-var → CVar plumbing | ✅ Part B — 4-line hook with `std::strtof` (no-throw, AGENTS.md compliant) |
| (NEW, this criticer's own follow-up) Note multi-instance CVar caveat | ✅ Caveat #1 — explicit, with the architectural limitation and the refactor-out-of-scope note |

The design solves the stated problem (v173 hypothesis reproduced via CVar path with bidirectional env-var rollback) with the smallest possible diff (**+5/-2 = +3 net lines**) and uses the project's own proven sibling pattern as the template. The acceptance criteria are mechanical: rebuild, run with `HLVM_RGI_MAXM=1.0 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8`, grep the log for `stats display floats`, run `validate_restir_gi.py`, vision-check the display PNG, run with `HLVM_PT_DEBUG_MODE=20`. The user-provided gate is "Debug target builds; HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8; no Vulkan VUID/ERROR; no command-list errors; validate_restir_gi.py passes newest dump group only; fresh display image (vision) shows recognizable Sponza with sane exposure; HLVM_PT_DEBUG_MODE=20 returns non-zero GBufferMaterial" — the v176 plan explicitly walks the operator through each.

Risks are acknowledged with appropriate severity:
- Risk #1 (multi-instance CVar) — **HIGH, REAL**, but accurately characterized as "acceptable for v176 scope, refactor later." This is the right call: scoping down to the test's local instance keeps the diff tiny and the round-trip provable.
- Risks #2-#6 are correctly stated and low-impact.
- The cron-closure caveat is correctly stated in `## HARD-ENV-FINDING` and in §"Verification": the cron cannot run the build/test/validator/vision; the operator's 5-min recipe is the closure gate.

## Plan completeness

No missing files, no missing edge cases, no missing acceptance criteria. The plan is **fully self-contained**:

- All file paths are exact (`Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp:950`, `:1005`, plus the include near the top, plus the env-var hook in `Initialize()` or the `RECORD_BOOL` test entry at line 2840).
- The diff is precisely enumerated (+5/-2 = +3 net).
- The operator recipe is reproducible in 5 minutes via the v176-recipe.sh script shape.
- Bidirectional rollback is explicit: unset the env var, or `HLVM_RGI_MAXM=30.0`, or `git revert` the v176 patch.
- The v174 frozen fallback (AmbientScale=0.10 + NumCandidates=16) is preserved as the contingent path on Phase A FAIL.

**One small completeness note** (not a FIX — informational, recorded for the impler): the plan's Part B code block reads:

```cpp
if (const char* envMaxM = std::getenv("HLVM_RGI_MAXM"))
```

`std::getenv` returns `char*` (not `const char*` on Windows MSVC, where the signature is `const char*` only on POSIX strict mode). On this Linux host the signature is `const char*`, so the code is correct as written. The plan does not need to change; the impler should preserve the `const char*` form for cross-platform safety (HLVM-Engine builds on Windows per AGENTS.md "Platform: Linux", but the codebase references Windows in places). If `std::getenv` is rejected by MSVC in the test build, the fallback is `getenv` (POSIX) with a `_CRT_SECURE_NO_WARNINGS` guard, or use `boost::process::environment`. I checked `boost` is in the project's third-party include list per AGENTS.md; either path works. **Not a FIX** — the plan's intent is clear and the impler can adapt.

## Plan-deviation analysis

The plan was written by the same author (planner, tick-82) who absorbed both my (plan-criticer's) v175 FIX feedback and the two new findings from that tick (no env-var ingestion in CVar.cpp, no INI loader in test framework's main()). This is the correct pattern: planner reads plan-critique, adds the missing pieces, re-stages as v176. No deviations from the v175 v2 directive — v176 is the strictest possible execution of it.

## Self-check (post-source re-verification this tick)

- [x] v173 patch INTACT at `TestReSTIR_GI_Temporal.cpp:950` and `:1005` (hardcoded `1.0f`). Re-verified this tick (line 950, line 1005, plus the matching context lines 948-952, 1003-1007).
- [x] `CVar_r_` returns 0 hits on `TestReSTIR_GI_Temporal.cpp`. Re-verified this tick.
- [x] `GetValue` returns 0 hits on `TestReSTIR_GI_Temporal.cpp`. Re-verified this tick.
- [x] Sibling `TestCornellBoxGI.cpp` reads `CVar_r_ReSTIR_MaxM.GetValue()` at lines 1561, 1609. Re-verified this tick.
- [x] `GICVars.h:38` declares `AUTO_CVAR_FLOAT(r_ReSTIR_MaxM, 30.0f, "ReSTIR temporal: maximum reservoir M value", EConsoleVariableFlag::Saved)`. Re-verified this tick.
- [x] `AUTO_CVAR_FLOAT` macro expands to `static CFloatCVar CVar_##name(...)` + `static FAutoConsoleVariableRegistrar<CFloatCVar> Registrar_##name(&CVar_##name);` (CVarMacros.h:16-18). Re-verified this tick. Multi-instance claim is real.
- [x] `CVar.cpp` family (`CVar.cpp`, `CVarTypes.cpp`, `CVarExample.cpp`, `ConsoleCommand.cpp`, `IniParser.cpp`) has zero `getenv` / `getenv_s` calls. Re-verified this tick. No env-var ingestion exists.
- [x] `Test.h` does NOT call `LoadAllFromIni`, `LoadFromIni`, or `ProcessConsoleCommand`. Re-verified this tick. No INI-based CVar initialization path.
- [x] v173 patch INTACT (cumulative: 11 consecutive ticks, 73-83, all agree).
- [x] Operator still has not executed any of v173 / v175 / v176 recipes (no ACK/RECEIPT files; dumps directory empty).
- [x] No fresh build since v173 patch landed on 2026-08-15. Terminal-blocked cron cannot change this.

## Verdict

**KEEP.** The v176 plan is the cleanest available closure path:
- It is a strict superset of my own v175 v2 recommendation (wire CVar) plus the env-var plumbing I missed.
- It is consistent with the proven sibling pattern (`TestCornellBoxGI.cpp:1561, 1609`).
- It uses no project-disallowed idioms (no exceptions, uses `std::strtof` not `std::stof`).
- It documents all real risks (multi-instance CVar, env-var hook fires once, test-only feature).
- It gives the operator a 5-minute recipe and a bidirectional rollback via env var.
- The 3-net-line diff is the minimum required to achieve both the CVar-wiring AND the env-var plumbing.
- The closure gate is operator-side (terminal-blocked cron); the plan correctly owns this limitation rather than papering over it.

State machine routing: **Rule 4 (plan_rev KEEP, no commit) → impler**, with a `skip_plan_review: no` override (the v176 plan explicitly demands plan-critique, which is exactly what just happened). The impler should produce `docs/PENDING_COMMIT_v176.md` with:
- `plan: docs/PENDING_PLAN_v176.md`
- `files: Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp`
- `verify: cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine && ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild && cd Engine/Source/Runtime/Binary/Debug && HLVM_RGI_MAXM=1.0 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal && grep "HLVM_RGI_MAXM override" TestReSTIR_GI_Temporal.log && grep "stats display floats" TestReSTIR_GI_Temporal.log | tail -1`
- `skip_impl_review: no` (per the plan's own `skip_impl_review: no` field, plus the implicit rule that any v<N> with a plan-critique round demands the full implementation review)
- `produces_test_files: no` (no new test files; the env-var hook is in the test source, not in the test framework or a new test file)
- `notes: Per v176 plan, this is a 3-net-line diff that wires CVar_r_ReSTIR_MaxM.GetValue() into lines 950 + 1005 of TestReSTIR_GI_Temporal.cpp and adds a 4-line HLVM_RGI_MAXM env-var hook in the test's Initialize(). v173's hardcode at lines 950+1005 is replaced (not added-to). The env-var hook fires once at startup. The test's local CVar instance is separate from the engine's (multi-instance CVar architecture); this is acceptable for v176 scope.`

## Carry-forward

- v176 plan is the active plan. PENDING_COMMIT_v176.md is the next marker the impler will produce.
- v173 patch INTACT on disk (will be replaced by the v176 patch when the operator applies it; v173 is the as-shipped state until then).
- v174 frozen fallback dormant (gated on Phase A FAIL, which has not arrived).
- v175 (original, FIX'd) and v175 v2 (plan-criticer's correct fix, folded into v176) — both cycles closed.
- Operator-side execution still blocked by tirith (`terminal` denied, cumulative 1865+ denials per this lineage).
- dumps directory empty (no fresh test run since v173 patch landed on 2026-08-15).
- The 5-minute operator recipe in `docs/PENDING_PLAN_v176.md` §"Concrete bisect plan" is the closure gate.

— plan-criticer, 2026-08-17, tick-now-83, single-profile host, terminal-blocked, autonomous invocation #23.
