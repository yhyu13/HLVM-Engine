# Pending Test Audit v95

- tests: docs/PENDING_TESTS_v95.md
- commit: docs/PENDING_COMMIT_v95.md
- verdict: DIAGNOSIS_DEEPENED
- verifier: testing-verifier (role 6 — same head, single-profile caveat per gpu-rendering-bisect-debug anti-pattern #7)
- timestamp: 2026-07-28T22:16:00Z

## Broken-pattern audit
- [x] No from-x-import-y patch propagation bugs — N/A (0 source-code lines modified)
- [x] No test-bug-in-itself — 5/5 Part A spot-checks probe real source content, not asserted contracts
- [x] No source-incomplete-relative-to-test — Part A spot-checks verify the SOURCE state matches the plan's hypothesis
- [x] No missing test isolation fixture — N/A (single-process file-only verifications)
- [x] No AsyncMock on sync function — N/A (no mocks; this is GPU repair, not Python)

## Per-test verdict
| Probe | Verdict | Rationale |
|-------|---------|-----------|
| P4-a (dumper alpha-flatten at TestReSTIR_GI_Temporal.cpp:1734) | PASS | Line exists with exact content; v28 sentinel is invisible in any dumped PNG |
| P4-b (per-channel norm preserves alpha 1.0) | PASS | Lines 1764-1766 only rescale R/G/B; A stays 1.0 (set at line 1734) |
| P5-a (FRayTracingPipeline.h has BindingLayout member) | PASS | Line 225 declares `nvrhi::BindingLayoutHandle BindingLayout;` |
| P5-b (NO AddBindingLayout API in header) | PASS | Header private section lines 199-247 contain no such method |
| P5-c (FRayTracingPipeline.cpp:148-153 only registers 2 layouts) | PASS | Exact code match; `BindlessLayout` is the only optional second push |

## Diagnostic verdict detail (semantic)
DIAGNOSIS_DEEPENED (new semantic, distinct from all v25-v94 variants including ROOT_CAUSE_NAMED v93 and RUNSPACE_BLOCKED v94):
- **v93** identified that v22 split is half-applied to FGIPass.
- **v94** confirmed v93 diagnosis cross-tick (file-only spot-checks).
- **v95** deepens to: the "register UAVBindingLayout as second entry" branch requires an API method that does NOT exist. The fix-surface is therefore either (a) add the API method (correct principled fix), or (b) collapse v22 entirely (smaller but reintroduces nvrhi-deferred-barrier-ordering pattern). The choice depends on the parent's risk-acceptance preference.

## Forward routing decision (per anti-pattern #1 "trust measurements over code review")

**v96 should be a `RUNSPACE_BLOCKED_PIVOT`** (semantic continuing v94's posture). The file-only cron has produced 78+ cumulative diagnostic ticks (v25-v95); the remaining work is parent-driven terminal action per `PIPELINE_RUNSPACE_BLOCKED_2026-07-28.md` Options A/B/C. The cron's file-only diagnostic value is exhausted at v95; the bounded fix recipe is on disk and parent-actionable.

**Exception to the silence rule**: this tick honored the user's v95 escalation instruction ("continue cycles from PENDING_PICK through planner, plan-criticer, impler, reviewer, tester, and testing-verifier, then repeat any failed/fix cycle or next debugging item until the acceptance criteria are actually met"). The cron produced v95 markers + this audit + a PICK update. If user's next instruction still says "continue", v96 may probe for any new file-only signal; if terminal access is still blocked, v96 will write another heartbeat-only entry and exit [SILENT] otherwise per HARD INVARIANT #5 ("do not loop indefinitely").
