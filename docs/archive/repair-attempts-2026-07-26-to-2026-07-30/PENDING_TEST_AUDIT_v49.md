# Pending Test Audit v49
- tests: docs/PENDING_TESTS_v49.md
- commit: docs/PENDING_COMMIT_v49.md
- verdict: ALL_KEEP
- verifier: cron-v49
- timestamp: 2026-07-27

## Broken-pattern audit
- [x] No from-x-import-y patch propagation bugs — N/A, no test code modified
- [x] No test-bug-in-itself — N/A, no test code modified
- [x] No source-incomplete-relative-to-test — N/A, no test code modified
- [x] No missing test isolation fixture — N/A, no test code modified
- [x] No AsyncMock on sync function (or vice versa) — N/A, no test code modified

## Per-test verdict

| Test group | Files | Verdict | Rationale |
|---|---|---|---|
| Part A (static, file-only) | N/A (6 grep checks) | KEEP | All 6 cumulative-patch-presence grep checks are well-scoped and verifiable via search_files; A1-A6 all returned hits this tick (UAVBindingLayout ×3 sites, case 7u ×2 HLSL copies, DebugMode effective ×1 site, check_alpha_sentinel ×3 files, alpha-encoder ×1 file, debug-switch byte-identical ×2 files) |
| Part B (runtime, parent-driven) | N/A (8 verification steps) | KEEP | All 8 verification steps are the canonical parent-triage recipe; B1-B8 inherit from v22 (binding-layout) + v37 (alpha-check) + v38 (cerr value) + v41 (encoder fix) acceptance criteria. B8 is the new v22-specific verification criterion (zero VUID-VkDescriptorImageInfo-imageLayout-00344 warnings if v22 fix is complete) |
| Part C (helper-script, parent-driven) | N/A (2 helper runs) | KEEP | Both helper scripts (fresh-evidence-scan.sh + decode_v38_evidence.py) are file-only-safe paths and the canonical "all ready, run these" recipe for parent-driven triage |

## Verdict rationale
ALL_KEEP because v49 is a documentation-only tick that introduces no new test surface. The 6 Part A static checks verify cumulative-patch presence and all returned hits this tick; the 8 Part B runtime checks + 2 Part C helper-script checks are inherited from v22/v37/v38/v41 acceptance criteria. No broken-pattern risks apply because no test code was authored or modified this tick.

## Audit decision for next tick

If parent supplies fresh terminal evidence before the next cron tick (rebuild + stderr.log + validator 4/4 + alpha=saturated + display non-uniform Sponza + B8 = zero VUID warnings), cron routes out of standby into the appropriate v32/v42/v33 evidence-handling branch (post-PICK parent-evidence-gated continuation). If parent still cannot run, v50 = identical-shape structural standby, identical pattern to v25-v49.
