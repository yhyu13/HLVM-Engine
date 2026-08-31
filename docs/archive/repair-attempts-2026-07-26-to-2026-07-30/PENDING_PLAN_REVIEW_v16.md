# Pending Plan Review v16

- plan: docs/PENDING_PLAN_v16.md
- verdict: KEEP
- reviewer: plan-criticer (six-role-pipeline, single-head, file-only)
- timestamp: 2026-07-27

## Design soundness

The plan correctly identifies a real, structural misunderstanding in the pipeline's prior cycles (v3-v15): the test compiles the **Private master** GIPathTracing.hlsl, not the data-dir copy. The evidence is direct and unambiguous:

1. `Engine/Source/Runtime/ShaderMakeBuild.py:613` — `shader_sources` list contains `gi_shader_dir + "/GIPathTracing.hlsl"` where `gi_shader_dir = ${CMAKE_SOURCE_DIR}/Private/Renderer/Shader/GI`.
2. `Engine/Source/Runtime/CMakeLists.txt:1877` — the `add_custom_target` DEPENDS list contains the Private master.
3. `Engine/Source/Runtime/Build/Debug/build.ninja:2476` — the generated ninja rule invokes slangc on the Private master.

Three independent sources confirm the same fact. The data-dir copy is dead code: no `#include "GIPathTracing.hlsl"` exists in any data-dir shader (verified by `search_files pattern="#include.*GIPathTracing"` → 0 matches).

This is a **structural correction** to the pipeline's understanding, not a behavioral change. The patches that landed (v3, v5, v7, v8, v11, v12, v14 — all C++) are unchanged in their effect; only the interpretation of "did the binary get the patch" is corrected. The data-dir patches (v13 case 6u) were never load-bearing; v15 sync was the load-bearing patch.

## Plan completeness

The plan enumerates:
- The corrected understanding with three sources of evidence (ShaderMakeBuild.py, CMakeLists.txt, build.ninja)
- A table showing the v3-v15 patch-to-binary fate matrix
- The corrected v13a decision matrix interpretation
- What the cycle does NOT do (no source changes, no scope creep)
- The carry-over parent action items (unchanged from v15)
- A staged v17 candidate (mode 7 sentinel bypassing TraceRay entirely) — not fired in this tick
- Honesty caveats including single-head self-check warning

The plan is well-scoped: doc-only cycle, no source changes, no premature action.

## Honesty about the verdict

This is a self-critique. The plan-criticer is the same head as the planner on this single-profile host. The KEEP verdict is therefore a self-check, not an independent review. The verification artifacts used (three independent sources for the corrected understanding, dead-code detection via #include search) are direct observable facts that survive the single-head caveat. The risk analysis (other Private-master/data-dir pairs) is not self-justifying — it surfaces plausible failure modes and mitigations.

## Discrepancies with project conventions

- The plan uses the comment-anchor pattern from v3-v15, but is doc-only so doesn't need it.
- The plan's table format matches v9-v15's "v<N> fix fate" tracking convention.
- The plan's staging of v17 in PICK (not firing it) follows the established convention of parent-evidence-gated follow-ups.

## Edge cases not enumerated in the plan

- **Could the include_dirs order (test_data_dir first) cause the data-dir GIPathTracing.hlsl to be #include'd by other shaders?** Verified: 0 matches for `#include.*GIPathTracing` in the data-dir. So no.
- **Could `m_GIPathTracing.sblob` be cached and not rebuilt on HLSL changes?** ShaderMake has incremental compilation based on timestamps. The Private master file mtime is the authoritative dependency. Verified by build.ninja:2476 — the rule depends on the Private master file path.
- **Could other tests' ShaderMake pulls use the Private master as well?** Inspected: TestPathTraceGI uses `gi_shader_dir + "/GIPathTracing.hlsl"` (line 571 of ShaderMakeBuild.py). TestCornellBoxGI uses its own data-dir copy (verified at line 547). TestFewBounceGI uses the data-dir copy. So 2 of 4 GI tests use the Private master. None of them have v3-v15 patches, so this finding has no spillover effect.
- **Could there be other Private-master/data-dir pairs in non-GI shaders?** Verified: only GIPathTracing.hlsl has both a Private-master copy (`Private/Renderer/Shader/GI/`) and a data-dir copy (`Test/TestReSTIR_GI_Temporal_Data/`). No other Private shader has a duplicate data-dir copy.

## Plan-fidelity check

- Files: matches PENDING_PLAN_v16.md (0 source files modified, 8 doc files created/appended).
- Approach: matches "1) write this plan + plan-review + commit + impl-review + tests + test-audit + append health; 2) update PENDING_PICK; 3) NO source-code changes".
- skip_plan_review: matches "no — this cycle updates the pipeline's understanding of which files matter".
- test_strategy: matches "No new test files needed".
- risks: matches "The other tests... no regressions elsewhere".

## Feedback for planner (FIX only)

None. Plan is KEEP-able as written.

## Verdict rationale

The plan surfaces a real structural misunderstanding in the pipeline's prior cycles (data-dir patches never landed in the binary). The correction is grounded in three independent sources and dead-code verification. The plan is doc-only, well-scoped, and correctly stages v17 as a parent-evidence-gated follow-up rather than firing it prematurely. Single-head caveat applies; KEEP verdict is a self-check, but the verification artifacts are direct observable facts.