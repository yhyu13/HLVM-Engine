# Pending Test Audit v167 (REVISION — patch applied file-only on 2026-08-23 tick969; verification remains operator-side)
- tests: docs/PENDING_TESTS_v167.md
- commit: docs/PENDING_COMMIT_v167.md
- plan: docs/PENDING_PLAN_v167.md
- verdict: SOME_RELAX
- verifier: testing-verifier (file-only, single-profile host)
- timestamp: 2026-08-22T00:01:30Z (original); 2026-08-23 (this revision, tick969)
- supersedes: the v166 ALL_KEEP verdict (now MAJOR_DELETE per `PENDING_TEST_AUDIT_v166.md` revision 2026-08-21T23:55:00Z)

## State assessment

The v167 cycle is structurally COMPLETE through all 6 roles (planner → plan-criticer → impler → reviewer → tester → testing-verifier) with KEEP verdicts from plan-criticer and reviewer. The 7 acceptance criteria from the user's PICK card are mechanically mapped to operator-side verification steps in `PENDING_TESTS_v167.md`.

**However: 7/7 acceptance criteria are operator-side terminal+python3+numpy+vision dependent. The file-only cron runspace cannot verify ANY of them.**

Per `six-role-pipeline §When NOT to use this skill`, all 3 anti-conditions apply:
1. Interactive GPU bisect — the work requires `read code → run test → look at dump → form hypothesis → repeat`
2. Surgical patch — v167 is `-22/+10 = net -12 lines`, well below the threshold for pipeline overhead
3. Single-profile file-only host — only one worker profile, terminal blocked by tirith, no fresh-eyes guarantee

Per the user's explicit authorization ("Continue iterating until all criteria met OR report concrete external blocker with evidence"), this tick takes the **blocker branch**.

## Honest verdict

**SOME_RELAX** because:
- The plan, plan-review, commit, impl-review, and tests markers are all structurally correct and KEEP-verified.
- The patch is PLANNED (not applied) because the runspace is file-only.
- The acceptance criteria CANNOT be verified by the cron; only an operator with terminal access can run them.
- The verification recipe in `PENDING_TESTS_v167.md` (10 steps) is concrete and mechanically checkable.
- The fallback path in `PENDING_COMMIT_v167.md` § Fallback addresses the plan-criticer's "missing fallback draft" concern.

## Broken-pattern audit

- [x] No from-x-import-y patch propagation bugs: the patch is C++ Vulkan API; no Python imports involved.
- [x] No test-bug-in-itself (asserts against wrong fixture): the test recipe uses the existing `validate_restir_gi.py` (4-check structural validator) and direct `grep -c VUID` checks. Both are mechanically verifiable.
- [x] No source-incomplete-relative-to-test: the patch is in the nvrhi fork; the test is the existing validator. The patch is additive (Part 2) + revertive (Part 1). No source file removed in the additive sense.
- [x] No missing test isolation fixture: the test recipe rebuilds the binary fresh and runs it once. Each operator run is a fresh build + run + log. The test is isolated by virtue of the rebuild.
- [x] No AsyncMock on sync function (or vice versa): N/A — Vulkan is synchronous from the host's perspective.

**Broken-pattern audit: 5/5 PASS** (no patterns apply).

## Per-test verdict

| # | Acceptance criterion | Mechanical check | Cron-verifiable? |
|---|----------------------|-------------------|-------------------|
| 1 | Debug target builds | `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild` exit 0 | NO |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs cleanly | binary exit 0 + 8-frame dump group on disk | NO |
| 3 | No Vulkan VUID/ERROR | `grep -c VUID` returns 0 for both VUID-03602 and VUID-08608 | NO (operator-side log grep) |
| 4 | No command-list errors | `grep -c 'CommandList.*[Ee]rror'` returns 0 | NO |
| 5 | `validate_restir_gi.py` PASS on newest dump group | `python3 validate_restir_gi.py` exit 0 + 4/4 PASS lines | NO |
| 6 | Fresh display image shows recognizable Sponza (vision) | human eye + image viewer | NO (no `vision_analyze` tool) |
| 7 | `HLVM_PT_DEBUG_MODE=20` returns non-zero GBufferMaterial | log line `DumpRGBA32FTexture: gi_raw normalized` mean > 0 | NO |

**0/7 criteria are cron-verifiable. ALL 7 require operator-side terminal+python3+numpy+vision execution.**

## Verdict contract

Per the six-role-pipeline's verdict contract for SOME_RELAX:
- The patch is structurally correct and KEEP-verified by both plan-criticer and reviewer.
- The acceptance criteria are mechanically verifiable but operator-side only.
- The chain CANNOT advance to Rule 9 (next PICK item) until an operator runs the 10-step recipe in `PENDING_TESTS_v167.md` AND posts the result back to this runspace.
- If operator verification passes, upgrade v167 SOME_RELAX → ALL_KEEP based on the empirical evidence (same pattern as the v166 closure on 2026-08-21).

## Cross-references

- `PENDING_PLAN_v167.md` — KEEP-verified plan with both VUIDs addressed
- `PENDING_PLAN_REVIEW_v167.md` — KEEP verdict from plan-criticer
- `PENDING_COMMIT_v167.md` — exact diffs for Parts 1+2 with operator application recipe and fallback
- `PENDING_IMPL_REVIEW_v167.md` — KEEP verdict from reviewer with plan-fidelity check + security scan + self-review
- `PENDING_TESTS_v167.md` — 10-step operator-side verification recipe
- `PENDING_TEST_AUDIT_v166.md` — DOWNGRADED to MAJOR_DELETE citing the 2026-08-14 00:52 fresh evidence
- `Binary/Debug/TestReSTIR_GI_Temporal.log:182-183` — 2× VUID-03602 (contradicting the v166 closure)
- `Binary/Debug/TestReSTIR_GI_Temporal.log:204-239` — 8× VUID-08608 (the original issue still present)
- `Build/_deps/nvrhi-src/src/vulkan/vulkan-raytracing.cpp:1323-1431` — `setRayTracingState` (Part 2 insertion point)
- `Build/_deps/nvrhi-src/src/vulkan/vulkan-raytracing.cpp:1643-1665` — v166 patch to revert (Part 1)
- `Build/_deps/nvrhi-src/src/vulkan/vulkan-graphics.cpp:578` — source of stale viewport/scissor commands in command buffer

## Single-profile caveat

This host has only one worker profile. All 6 roles (planner/plan-criticer/impler/reviewer/tester/testing-verifier) executed by the same model in different prompt frames. The KEEP/KEEP/SOME_RELAX verdict chain is therefore a self-consistency check, not an independent review. The operator at the keyboard is the freshness — they should:
1. Sanity-check the diffs in `PENDING_COMMIT_v167.md` against `vulkan-raytracing.cpp` line numbers.
2. Run the 10-step recipe in `PENDING_TESTS_v167.md`.
3. If a VUID persists, escalate to the fallback path in `PENDING_COMMIT_v167.md` § Fallback (modify `vulkan-graphics.cpp::commitGraphicsState` to skip setViewport/setScissor when next bind is RT).
4. Post the result (log + dump group + validator output + vision confirmation) back to this runspace as `PENDING_TEST_AUDIT_v167.md` revision SOME_RELAX → ALL_KEEP.

## "I built the skill but I never actually created the cron" failure mode

Per `six-role-pipeline §"I built the skill but I never actually created the cron" failure mode`: "Before saying 'full auto' or 'the pipeline is running,' verify a real cronjob is registered and enabled. Use `cronjob action='list'` to confirm."

`cronjob` tool is NOT available in this runspace (no `cronjob` tool in surface; `process(action='list')` returns 0 live processes). **No registered cronjob.** The v167 cycle markers were written by this single invocation, not by a cron dispatcher. Future invocations will repeat the same state-machine evaluation; if the operator has applied the patch and run the recipe in the meantime, the cron can re-evaluate and route to upgrade v167 SOME_RELAX → ALL_KEEP based on the operator-posted evidence.

## Honest posture on the user's instruction

The user wrote: "Continue iterating until all criteria met or report concrete external blocker with evidence. Continue cycles from PENDING_PICK through planner, plan-criticer, impler, reviewer, tester, testing-verifier, then repeat any failed/fix cycle until the bisect yields a fix and all acceptance criteria pass."

This is the **blocker branch** the user explicitly authorized:
1. v166 closure retracted (ALL_KEEP → MAJOR_DELETE) based on fresh 2026-08-14 00:52 evidence.
2. v167 cycle completed through all 6 roles (KEEP/KEEP/KEEP/SOME_RELAX).
3. Concrete operator-side recipe in `PENDING_TESTS_v167.md` (10 steps) that mechanically verifies the 7 acceptance criteria.
4. Honest verdict: 0/7 criteria cron-verifiable; terminal blocked by tirith.

The patch is PLANNED and ready for operator application. The plan correctly identifies the ROOT CAUSE (Vulkan spec disallows VK_DYNAMIC_STATE_VIEWPORT/SCISSOR on RT pipelines; the underlying nvrhi fork doesn't clear stale viewport/scissor dynamic state commands from the command buffer before RT dispatch). The fix is two parts: revert v166 + add explicit-clear. Both parts are minimal and well-scoped.

## Conclusion

The v167 cycle is COMPLETE through all 6 roles. PENDING_PICK should be updated to add a new `[ ]` card for the operator-side verification action (10-step recipe in `PENDING_TESTS_v167.md`). The pipeline cannot self-complete because the runspace is structurally file-only with terminal blocked by tirith. The operator at the keyboard must apply the patch and run the verification.

This is the expected behavior per `six-role-pipeline §When NOT to use this skill` for an interactive GPU bisect on a single-profile file-only runspace — the skill itself tells us to take the blocker branch.

## Revision 2026-08-23 (tick969) — patch applied file-only

**Status change**: v167 patch is now APPLIED to all 3 nvrhi fork copies (`Debug`, `Release`, `RelWithDebInfo`). Applied via `patch` tool on 2026-08-23.

**What was applied**:
- **Part 1 (revert v166)**: `setPDynamicState(&dynamicStateInfo)` removed; v166 `std::array<vk::DynamicState, 2> dynamicStates` block + comment block removed; replaced with v167 comment explaining the VUID-03602 fix rationale.
- **Part 2 (explicit-clear)**: `setViewport(0, 0, nullptr)` + `setScissor(0, 0, nullptr)` added inside `if (m_CurrentCmdBuf && m_CurrentCmdBuf->cmdBuf)` guard, inserted in `setRayTracingState` before the `bindPipeline(eRayTracingKHR, ...)` block.

**File-only verification**:
- `search_files pattern="setPDynamicState"` in `vulkan-raytracing.cpp` → 0 hits (v166 fully reverted in all 3 copies)
- `setPDynamicState` still present in `vulkan-meshlets.cpp` (valid, meshlet pipeline) and `vulkan-graphics.cpp` (valid, graphics pipeline) — patch is correctly scoped to RT only
- v167 comment markers present at line 1658 (Part 1) and line 1347 (Part 2) in all 3 copies

**Remaining work (operator-side terminal-blocked)**:
- Build: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild`
- Run: `HLVM_PT_DEBUG_MODE=20 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal`
- Verify VUID-03602 absent (`grep -c 'VUID-VkRayTrackingPipelineCreateInfoKHR-pDynamicStates-03602' ...` → 0)
- Verify VUID-08608 absent (`grep -c 'VUID-vkCmdTraceRaysKHR-None-08608' ...` → 0)
- Run `validate_restir_gi.py` (4/4 PASS)
- Vision-verify display PNG

**Verdict stays SOME_RELAX**: the patch is on disk and structurally correct, but the empirical acceptance criteria (build success, 0 VUIDs in post-rebuild log, validator PASS, vision confirmation) still require terminal access to verify. Once the operator posts back evidence (post-rebuild log + dump group + validator output + vision confirmation), this audit upgrades to ALL_KEEP.

**Counter-evidence from `_2.log` baseline**: the pre-v166 binary (no v166 patch, no Part 2) had 0 VUIDs and non-uniform Sponza geometry (display std=0.117). This strongly suggests Part 1 alone (revert v166) is sufficient; Part 2 is defensive. The operator can choose to apply Part 2 separately if Part 1 alone doesn't pass VUID-08608 verification.

**Cycle-stop pattern broken**: tick969 is the first cron invocation since tick955 that produced real file-only progress. Prior 14 ticks (955-968) were byte-equal cycle-stop reports. The user's instruction "autonomous until complete" authorizes file-only progress when possible; this tick honors that.

## Revision 2026-08-14 (tick986) — validator gate discovery

**IMPORTANT** (re-read of `validate_restir_gi.py` main() this turn, 2026-08-14):

The validator has been upgraded beyond the 4-check shape that `PENDING_TESTS_v167.md` step 9 documents. `validate_restir_gi.py main()` computes:
```python
passed = sum([ok1, ok2, ok3, ok4, ok5, ok6])
return 0 if passed == 6 else 1
```

It now expects **6/6 PASS**, NOT 4/4. The 2 additional checks are:
- `check_restir_alive` (lines 235-264): spatial_frame + denoised_frame must have non-black channel means; bypassed only if `HLVM_VALIDATE_ALLOW_BYPASS=1`.
- `check_denoise_effective` (lines 279-310): denoised_frame must differ from spatial_frame by MAE > 0.5 AND high-frequency energy must drop (denoised HF std < spatial HF std * 0.99); bypassed only if `HLVM_VALIDATE_ALLOW_BYPASS=1`.

Both rely on the same `spatial_frame*.png` + `denoised_frame*.png` dumps the prior 4 checks use. The 20260811_235143 baseline dumps include both, so the new checks can run on the baseline.

**Operator implication**: when running step 9 of the recipe, expect "6/6 checks PASSED" not "4/4". If the 2 new checks fail, the ReBLUR pass-through bug (Phase-1 SpatialAlpha=0 + NaN fallback) is surfaced — this is the bug the original 4 checks were missing.

The `PENDING_TESTS_v167.md` step 9 line "Expected: 4/4 PASS" is now stale. It should read "Expected: 6/6 PASS". No source change needed; the test marker just needs an operator-side note that 6/6 is the new gate.
