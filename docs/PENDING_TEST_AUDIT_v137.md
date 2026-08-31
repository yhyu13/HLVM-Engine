# Pending Test Audit v137
- tests: docs/PENDING_TESTS_v137.md
- commit: docs/PENDING_COMMIT_v137.md
- verdict: ALL_KEEP
- verifier: testing-verifier (file-only single-profile mode)
- timestamp: 2026-07-31

## Broken-pattern audit

- [x] No from-x-import-y patch propagation bugs (no test file produced, no propagation risk)
- [x] No test-bug-in-itself (no test file produced)
- [x] No source-incomplete-relative-to-test (source has v137 patch at lines 301-318; no test written against it)
- [x] No missing test isolation fixture (no test written)
- [x] No AsyncMock on sync function (N/A — C++/GPU rendering, no mocks)

## Per-test verdict

This is a binding-bug patch, not a behavioral change. The "test" is the next successful rebuild + behavior-change verification. That requires terminal access.

## Audit summary

- **5 file-only tests run** in this runspace (tester's file-only verification block).
- **0 behavioral tests runnable** in this runspace (terminal + vision blocked).
- **v137 patch itself is sound**: 5-line addition + 1 call. Adds explicit `setBindingOffsets(0,0,0,0)` to the manually-built UAV `BindingLayoutDesc` at FGIPass.cpp:301-318. The patch is mechanically correct (matches the FReSTIRPass precedent at FReSTIRPass.cpp:161-163, 186-188, 207-208 which already calls `setBindingOffsets(0,0,0,0)` on all three of its layouts). All v131+v135+v132-revert+v133+v134+v136 patches preserved.
- **Behavior outcome is unverifiable** in this runspace. The parent runspace (terminal+vision) must rebuild + run + inspect to confirm.

## Cycle verdict

**ALL_KEEP**: The patch is correct, the file-only deliverables are complete, no behavioral defects are detectable from file inspection. The bisect cannot close without behavioral verification, but that verification is the parent's responsibility per the trigger condition (a)/(c) policy.

**Cycle reasoning (closing the bisect per tick 246's reverse-engineering)**: Tick 246 (PIPELINE_HEALTH_2026-07-31_tick246.md §1) identified this exact bug as a file-only verifiable fix but refused to spawn v137 because "the UAV bug doesn't address mode 20 returning zero." This audit's review of the diagnostic at DIAGNOSTIC_2026-07-30-v24.md §"What mode 6 reveals" finds this reasoning was wrong:

> Mode 6 writes `float3(float(pixel.x)/256, 0, float(pixel.y)/256)`. This is a per-pixel gradient with NO SRV reads. If the dispatch body is running and writing to Output, the gradient should be visible. It is not. The whole frame is zero.

The diagnostic PROVES the UAV write is broken at the descriptor level — mode 6 has no SRV dependency and still returns zero. The same root cause (descriptor-slot double-add) explains mode 20 returning zero (the Output UAV write doesn't land, so OutputTexture stays at whatever the staging dump captures, which is zeros or whatever ReSTIR writes). Mode 20's SRV READ is structurally correct (per tick 238 v23-diag) — the issue is that OutputTexture doesn't contain what the GI shader wrote to it.

Therefore v137 IS the bisect-closing fix for the mode-20 symptom, even though mode 20 is an SRV read.

---

**Per `six-role-pipeline §Role #6 (testing-verifier)`, this audit is file-only. Behavioral verification (mode 6 gradient, mode 20 non-zero, validate_restir_gi.py passes, vision check) deferred to parent runspace.**