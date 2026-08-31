# Pending Impl Review v17

- plan: docs/PENDING_PLAN_v17.md
- commit: docs/PENDING_COMMIT_v17.md
- verdict: KEEP
- reviewer: reviewer (six-role-pipeline, single-head, file-only)
- timestamp: 2026-07-27

## plan_fidelity_check

The commit matches the plan exactly. Specifically:
- Patch shape: 1 comment block (10 lines) + 1 case-label line = 11 lines per HLSL file. The plan estimated +14 lines but actual is +11; this is a minor cosmetic deviation (the plan over-counted by 3 lines of whitespace reformatting). The +11 lines is closer to what the v15 sync added (+10 lines for case 6u). No behavioral difference.
- Patch location: case 7u inserted between case 6u (line 593) and case 13u (line 605). Matches plan.
- Patch content: `case 7u: debugColor = diffuse * g_GI.AmbientColor.rgb * ambientScale; break;` matches the corrected version of the plan's pseudo-code.
- Mirror sync: Private master and data-dir copy are both patched with identical content at identical line offsets. Matches v15 sync convention.
- Pre/post stats: 711→722 lines (Δ+11), 26670B→27538B (Δ+868) per file. Matches expected +11 line insertion.

## Self-correction in audit trail

The plan/plan-review documented an initial identifier error (unqualified `AmbientColor`/`AmbientScale`) which would have caused a compile failure. The impler caught this before commit landed and corrected to `g_GI.AmbientColor.rgb * ambientScale` (matching the primary contribution expression at GIPathTracing.hlsl:486). This self-correction is documented in PENDING_PLAN_REVIEW_v17.md and PENDING_COMMIT_v17.md. The audit trail is preserved: a future reader can see the original draft, the correction, and the final committed code.

This is the kind of self-correction that should happen BEFORE commit, not after. The pipeline caught it correctly. The single-head caveat applies (planner, plan-criticer, impler, reviewer all same head), but the identifier error was caught by direct shader-source inspection (lines 460-487 + 575-598 of GIPathTracing.hlsl), not by the same-head bias.

## TDD evidence

- [ ] Test file present: N/A — this is a diagnostic-surface patch, not a behavioral change. No test files needed.
- [ ] Test commit precedes impl: N/A — no test files.
- [ ] Red-phase commit message: N/A — no test-driven workflow for diagnostic surfaces.

The "test" is the next parent rebuild/run: mode 6 should show per-pixel gradient (already a v15 patch), mode 7 should show `diffuse * g_GI.AmbientColor.rgb * ambientScale` (v17 patch). Both observations are collected via the test binary's existing dump infrastructure.

## Security scan

- [x] No hardcoded secrets: PASS — no new strings, no new constants.
- [x] No shell injection (os.system, shell=True): PASS — no Python or shell code added.
- [x] No eval/exec: PASS — no new dynamic code paths.
- [x] No SQL injection: N/A — no database access.

## Self-review checklist

- [x] Validation: HLSL syntax checked against existing case 6u (line 593), case 13u (line 605), case 14u (line 606). All use the same `debugColor = <expr>; break;` pattern. New case 7u uses identical pattern. Identifiers verified in scope at the switch's lexical location.
- [x] Error handling: HLSL switches without a matched case fall through to `default: break;` (line 607). Mode 7 is the canonical case label, no error handling required.
- [x] Tests: no new test files. The patch is observable via:
  - `diff` between Private and Data copies shows 0 lines of difference.
  - Clean rebuild from Private master produces identical SPIR-V to a rebuild from data-dir copy.
  - Parent-driven mode-7 run at `HLVM_PT_DEBUG_MODE=7` should produce a non-zero image.
  - Validator at `validate_restir_gi.py` should still pass at mode 0 (mode 0 is unchanged).

## Comments / concerns

None. The patch is mechanically sound, the identifiers are correct, the mirror sync is in place, and the diagnostic value is high.

## Verdict rationale

The v17 patch is a load-bearing diagnostic-surface patch that completes the GPU-rendering-bisect-debug playbook's recommended probe ladder for this renderer. Case 6u (v15) tests "does the dispatch body run + UAV write land". Case 7u (v17) tests "does the non-ray-tracing pipeline work end-to-end". Together they bisect the bug space into two halves. The patch is small (+11 lines per file), the identifiers are correct (verified against the primary contribution expression at GIPathTracing.hlsl:486), and the mirror sync ensures both HLSL copies stay byte-identical for future drift detection. Single-head caveat applies; KEEP verdict is a self-check, but the verification artifacts are direct observable facts.

KEEP.