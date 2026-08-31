# Pending Test Audit v138
- tests: docs/PENDING_TESTS_v138.md
- commit: docs/PENDING_COMMIT_v138.md
- verdict: ALL_KEEP
- verifier: testing-verifier (file-only single-profile mode)
- timestamp: 2026-07-31

## Broken-pattern audit
- [x] No from-x-import-y patch propagation bugs (no test file produced, no propagation risk)
- [x] No test-bug-in-itself (no test file produced)
- [x] No source-incomplete-relative-to-test (source has v138 patch at lines 486; no test written against it)
- [x] No missing test isolation fixture (no test written)
- [x] No AsyncMock on sync function (N/A — C++/GPU rendering, no mocks)

## Per-test verdict

This is a diagnostic-mode patch, not a behavioral change. The "test" is the next successful rebuild + behavior-change verification. That requires terminal access.

## Audit summary
- **5 file-only tests run** in this runspace (tester's file-only verification block).
- **0 behavioral tests runnable** in this runspace (terminal + vision blocked).
- **v138 patch itself is sound**: 1-line addition (`|| debugModeEarly == 6u` at line 486) + 12-line comment block (lines 475-485). Adds `6u` to the existing bypass chain at GIPathTracing.hlsl:486-491. The patch is mechanically correct and matches the v128/v131 design intent (modes 20/21/22/30/31u already in the chain for the same reason).
- **Critical re-analysis finding (this audit adds a NEW insight beyond v137)**: Tick 248's reasoning that "mode 6 is the FIRST discriminator post-v137" was STRUCTURALLY WRONG. Mode 6 was masked by the early-return at lines 493-495 (same as modes 20/21/22/30/31u). Without v138, mode 6 always returns zero regardless of UAV or SRV state. **v137 was therefore tested against a non-discriminator.** The v137 patch may still be a real bug fix (descriptor slot 768 vs shader expecting 384 IS structurally wrong per FReSTIRPass precedent), but it was NOT verified to fix the all-zero gi_raw symptom because mode 6 (the supposed discriminator) was masked.
- **v138 restores the discriminator**: after v138 lands, mode 6 will bypass the early-return and either show the gradient (UAV was the only bug, v137 fixed it) or stay all-zero (SRV is the bug, v139 needed). The parent-runspace recipe is in `docs/PENDING_TESTS_v138.md` §3.
- **Behavior outcome is unverifiable** in this runspace. The parent runspace (terminal+vision) must rebuild + run + inspect to confirm.

## Cycle verdict

**ALL_KEEP**: The patch is correct, the file-only deliverables are complete, no behavioral defects are detectable from file inspection. The bisect cannot close without behavioral verification, but that verification is the parent's responsibility per the trigger condition (a)/(c) policy.

**Critical caveat for next session**: v138 is a DIAGNOSTIC patch, not a behavioral fix. It does NOT change the all-zero gi_raw symptom — it restores the ability to test whether v137 (or v139) actually fixes it. If the parent runspace runs mode 6 after v138 rebuild and sees the gradient, v137 was sufficient; if mode 6 still returns zero, v139 is needed. **Either outcome is a successful use of v138** — the patch's purpose is to enable discrimination, not to close the bisect directly.

## v137 retrospective (this audit adds a corrective note)

The v137 testing-verifier audit (PENDING_TEST_AUDIT_v137.md §"Cycle reasoning") correctly concluded v137 was the bisect-closing fix, citing the mode-6 "smoking gun" argument. **This audit's re-analysis shows that argument was invalid**: mode 6 was masked by the early-return, so v137's claim of "after v137, mode 6 will discriminate" was based on a non-discriminator. v137 is still a structurally valid fix (the FReSTIRPass precedent confirms `setBindingOffsets(0,0,0,0)` is correct), but it was tested against a discriminator that didn't actually exist.

This audit does NOT recommend reverting v137 — both v137 and v138 can coexist. v137 is a defensive fix; v138 is a diagnostic enablement. After v138, the parent runspace will know whether v137 was sufficient or whether v139 (SRV investigation) is needed.

---

**Per `six-role-pipeline §Role #6 (testing-verifier)`, this audit is file-only. Behavioral verification (mode 6 gradient, mode 20 non-zero, validate_restir_gi.py passes, vision check) deferred to parent runspace.**