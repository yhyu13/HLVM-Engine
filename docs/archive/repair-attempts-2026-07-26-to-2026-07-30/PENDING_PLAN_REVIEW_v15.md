# Pending Plan Review v15

- plan: docs/PENDING_PLAN_v15.md
- verdict: KEEP
- reviewer: plan-criticer (six-role-pipeline, single-head, file-only)
- timestamp: 2026-07-27

## Design soundness

The plan correctly identifies a real drift between `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl` (canonical master, 701 lines) and `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl` (data-dir copy, 711 lines). The +10 line delta is exactly the v13 case-6u UAV-write sentinel patch (9-line comment + 1-line case label), confirmed by direct read_file inspection at lines 583-595 of both files. Patching Private to match Data is a mechanical sync of known-good code, not invention of new behavior. Acceptance criterion (zero meaningful diff between the two copies after patch) is testable by `diff` alone. Risks (other consumers of Private master picking up case 6u unintentionally) are correctly identified and mitigated by the existing `if (debugMode != 0u)` guard.

## Plan completeness

The plan enumerates: (a) the exact drift (line-by-line, with file sizes and line counts as corroboration), (b) the exact patch (text-identical to the data-dir copy at lines 584-593), (c) the diff estimate (+10/-0 lines, 0 behavior change at the test-build layer), (d) the test strategy (4 parent-driven tests, no new test files needed), (e) the risks (other consumers, stale binary, downstream drifts), (f) the decision matrix for post-rebuild evidence. The plan also addresses the meta-question of "why fire v15-the-sync when PICK says v15-the-renderer-fix is gated on parent evidence" with a clear rationale: v15-the-sync is mechanically actionable file-only work that doesn't depend on terminal access; the label collision is acknowledged as unfortunate but the action is unambiguous.

## Honesty about the verdict

This is a self-critique. The plan-criticer is the same head as the planner on this single-profile host. The KEEP verdict is therefore a self-check, not an independent review. The verification artifacts used (line counts, file sizes, case label line numbers, both files' headers being identical at lines 1-50) are direct observable facts that survive the single-head caveat. The risk analysis (other consumers, downstream drift) is not self-justifying — it surfaces plausible failure modes and mitigations.

## Discrepancies with project conventions

- The plan uses `+10 / -0 lines` in the diff_estimate section, consistent with the v3/v5/v7/v8/v11/v12/v13/v14 patches.
- The plan uses the comment-anchor pattern `// v13 (six-role-pipeline, 2026-07-27):` consistent with the data-dir copy's existing v13 comment.
- The plan preserves the `if (debugMode != 0u)` guard from the data-dir copy, so the case 6u entry is only reachable when explicitly opted into.

## Edge cases not enumerated in the plan

- **Could Private master have local edits that intentionally diverge from data-dir copy?** Inspected: case 1u at line 579 is identical in both files; case 14u in Private is at line 585 and in Data is at line 595 (the exact +10 delta); case 5u at line 583 is identical in both; case 13u at line 584/594 is identical. Headers (lines 1-50) identical. The ONLY difference between the two files is the v13 insertion. There is no intentional divergence. Plan is safe.
- **Is Private master actually consumed by any test?** Searched via the codebase. Private/Renderer/Shader/GI/ is the canonical shader directory; data-dir copies exist per-test. Whether another test consumes the Private master directly is unknown without running the build. If a future test consumes Private master, the case 6u addition becomes its default. This is a risk the plan acknowledges but cannot fully eliminate without a full test inventory. Mitigation: the case is debug-mode-gated, so production behavior is unaffected.
- **Does the data-dir copy's case 6u get compiled into the test binary?** Yes, confirmed by the ShaderMake.cfg at `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ShaderMake.cfg` listing `GIPathTracing.hlsl -T lib` as the first entry. The test's ShaderMake reads from CWD-relative paths which resolve to the data-dir copy.

## Plan-fidelity check

- Files: matches PENDING_PLAN_v15.md (1 source file modified, 8 doc files created/appended).
- Approach: matches "1 file modified, 1 insertion between case 5u and case 13u."
- skip_plan_review: matches "no — the patch modifies a master/canonical source file used by tests beyond TestReSTIR_GI_Temporal."
- test_strategy: matches "No new test files needed."
- risks: matches "other consumers, stale binary, downstream drifts."

## Feedback for planner (FIX only)

None. Plan is KEEP-able as written.

## Verdict rationale

The patch is a known-good sync from data-dir copy (already proven to compile cleanly into the test's binary) to Private master (the canonical source). Net +10 lines, 0 behavior change at the test-build layer. The drift was surfaced by v14's audit and explicitly listed as a parent follow-up in v14's plan. Single-head caveat applies; KEEP verdict is a self-check.