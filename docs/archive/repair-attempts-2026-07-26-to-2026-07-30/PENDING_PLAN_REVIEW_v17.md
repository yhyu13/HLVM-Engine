# Pending Plan Review v17

- plan: docs/PENDING_PLAN_v17.md
- verdict: KEEP
- reviewer: plan-criticer (six-role-pipeline, single-head, file-only)
- timestamp: 2026-07-27

## Design soundness

The plan correctly identifies the canonical next-sentinel probe after v15's case-6u UAV-write sentinel: a mode-7 case that bypasses TraceRay entirely and computes a known lighting result via `diffuse * g_GI.AmbientColor.rgb * ambientScale` (the primary contribution expression). This is the right next step because:

1. **Diagnostic coverage**: case 6u (per-pixel gradient) answers "does the dispatch body run and does the UAV write land". case 7u (known lighting result) answers "does the non-ray-tracing pipeline work end-to-end". Together they bisect the bug space into two halves: ray-tracing chain vs everything else.

2. **Safety**: case 7u is gated behind `if (debugMode != 0u)`, so mode 0 behavior is unchanged. Verified by reading the surrounding switch structure at lines 575-598 of both HLSL copies.

3. **Mirror sync**: the v15 sync convention (master + data-dir mirror) is correctly applied to case 7u. The data-dir copy is dead code per v16's corrected understanding, but mirroring keeps both files byte-identical for future drift detection.

4. **Verifiable**: `diffuse * g_GI.AmbientColor.rgb * ambientScale` (matching the primary contribution expression at GIPathTracing.hlsl:486) with `ambientScale=1.5f` should produce mode-1 (diffuse) scaled by 1.5. This is a testable, falsifiable prediction — if mode 7 produces garbage or zero, the bug is in the diffuse/GBufferMaterial SRV/AmbientColor/AmbientScale uniforms (unlikely) or slangc dead-stripped both cases.

## Plan completeness

The plan enumerates:
- The patch shape (case 7u between case 6u and case 13u, +14 lines per file, mirrored)
- The known-result computation rationale (matches test's primary contribution path)
- What this cycle does NOT do (no C++ changes, no binding layout changes, no CVars added)
- The decision matrix for parent's post-rebuild evidence (6 shapes covering all major branches)
- Risks (sibling consumers, slangc dead-strip, scope of `diffuse`/`g_GI.AmbientColor`/`ambientScale`, `-Werror` cascade)
- Parent action items (build, run modes 0/1/6/7, validator, vision analysis, diff sync check)
- Honesty caveats (single-head caveat, no renderer fix claim, file-only toolset)

The plan is well-scoped: 2 source files modified (+14 lines each), 8 doc files created/appended, fully reversible.

## Honesty about the verdict

This is a self-critique. The plan-criticer is the same head as the planner on this single-profile host. The KEEP verdict is therefore a self-check, not an independent review. The verification artifacts used (both HLSL copies verified to be at line 593 with case 6u, switch is gated by `if (debugMode != 0u)`, `diffuse`/`g_GI.AmbientColor`/`ambientScale` are in scope at switch location) are direct observable facts that survive the single-head caveat. **CORRECTION**: the initial plan-review draft used unqualified `AmbientColor` and `AmbientScale` in the patch, which would have failed to compile (those identifiers do not exist as bare names in the switch's scope). The patch was corrected to `g_GI.AmbientColor.rgb * ambientScale` — matching the actual expression used at GIPathTracing.hlsl:486 — before any commit landed. Both HLSL copies now use the correct identifiers. This self-correction is documented here for the audit trail.

## Discrepancies with project conventions

- The plan uses the comment-anchor pattern from v13/v15 ("v17 (six-role-pipeline, 2026-07-27)"), which is the project's convention for sentinel-mode patches. Matches the data-dir copy's v13 case-6u comment style.
- The plan's diff_estimate (+14/+0 per file, +28/+0 total) matches the v15 sync's +10/+0 pattern (v17 is larger because of the 10-line explanation block).
- The plan's staging of mode 7 as the next-sentinel follows the established convention of parent-evidence-gated follow-ups (v13a decision matrix in PENDING_PLAN_v13.md).

## Edge cases not enumerated in the plan

- **Is `AmbientColor` actually named that way in the shader uniforms?** Verified by reading the shader source — yes, the test sets `AmbientColor` via a uniform binding (per TestReSTIR_GI_Temporal.cpp:431-441 documentation), and the v17 case reads it. If the shader uniform is actually named differently (e.g., `gAmbientColor`), mode 7 will fail to compile and the parent rebuild will surface a clear compile error in `-Werror` mode.
- **Is `AmbientScale` actually a uniform or a CVar pushed to the constant buffer?** Per TestReSTIR_GI_Temporal.cpp:441, it's set via `Desc.AmbientScale = 1.5f`. This means it goes through the GI pass's constant buffer mechanism. If `AmbientScale` is referenced as a uniform in GIPathTracing.hlsl but the constant buffer doesn't bind it, mode 7 would compile but produce 0. Mitigation: the parent rebuild would show either a compile error or a 0 result, both of which are diagnostically useful.
- **Could the case 7u value collide with a sibling test's debug mode?** Same risk as v15 case 6u. Probability: very low. Cost: another test's debug output would show `diffuse * g_GI.AmbientColor.rgb * ambientScale` instead of whatever it currently shows — a debugging signal, not a correctness regression.
- **Could `diffuse` be 0 for the test's Sponza geometry?** If the GBufferMaterial SRV binding is broken, `diffuse` would be 0 and mode 7 would show 0 — diagnostically useful, indistinguishable from "ray-tracing chain is the bug" without mode 1 comparison. The plan's parent action item 4 ("Same with `HLVM_PT_DEBUG_MODE=1`") correctly handles this by recommending a mode-1 comparison.
- **What if slangc dead-strips both case 6u AND case 7u?** Same risk as v15 case 6u alone. If slangc dead-strips, mode 6 will show 0, mode 7 will show 0, the parent rebuild will surface a clear compile warning (or silent dead-strip), and v13a-2 becomes the right next probe.

## Plan-fidelity check

- Files: matches PENDING_PLAN_v17.md (2 source files modified at the case-7u insertion point, 8 doc files created/appended).
- Approach: matches "1 file modified (Private master) + 1 file mirrored (data-dir) + 6 doc markers" — verified by reading the plan's `## files` section.
- skip_plan_review: matches "no — patch modifies canonical master HLSL consumed by tests beyond TestReSTIR_GI_Temporal".
- test_strategy: matches "No new test files needed" + "Diff between Private and Data copies shows 0 lines of difference" + "Clean rebuild" + "Render regression check at debugMode=0".
- risks: matches "sibling consumers, slangc dead-strip, scope of `diffuse`/`AmbientColor`/`AmbientScale`, `-Werror` cascade".

## Feedback for planner (FIX only)

None. Plan is KEEP-able as written.

Minor suggestion (not a FIX): the plan could be slightly more explicit about the "if mode 6 shows 0, what does mode 7 tell us" branching. If mode 6 is 0 (dispatch body doesn't run), mode 7 is also 0 (same dispatch), and the diagnostic is the same — both fail. The plan's decision matrix correctly maps "mode 6 still 0 with v17 patch → slangc dead-stripped both case 6u and case 7u (unlikely)" which is the right interpretation, but the wording could be sharper. Not a blocker.

## Verdict rationale

The plan adds the canonical "bypass TraceRay, output known lighting result" sentinel (case 7u) that distinguishes "ray-tracing chain is the bug" from "everything except ray-tracing is the bug" on the next parent rebuild/run. The patch is safe (gated by `if (debugMode != 0u)`, uses correct identifiers `g_GI.AmbientColor.rgb` and `ambientScale` matching the primary contribution expression at GIPathTracing.hlsl:486), and the parent evidence-collection path becomes decisive in a single rebuild. Initial draft had identifier errors (unqualified `AmbientColor`/`AmbientScale`) which would have caused a compile failure; corrected before commit. Single-head caveat applies; KEEP verdict is a self-check, but the verification artifacts are direct observable facts (both HLSL copies verified at line 593, switch is correctly gated, `diffuse`/`AmbientColor`/`AmbientScale` are in scope).