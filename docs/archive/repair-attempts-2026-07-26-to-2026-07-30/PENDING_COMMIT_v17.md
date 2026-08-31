# Pending Commit v17

- plan: docs/PENDING_PLAN_v17.md
- files:
  - `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl` (+11 / -0 lines net: 1 comment block of 10 lines + 1 case-label line)
  - `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl` (+11 / -0 lines net: mirrored from Private master per v15 sync convention)
- source: docs/PENDING_PLAN_v17.md (patch shape) + on-disk inspection of both HLSL copies
- target: not-applicable (file-only diagnostic patch; not a renderer fix)
- task: add `case 7u` TraceRay-bypass sentinel to GIPathTracing.hlsl that computes `diffuse * g_GI.AmbientColor.rgb * ambientScale` (the primary contribution expression at GIPathTracing.hlsl:486) regardless of TraceRay result. Distinguishes "ray-tracing chain is the bug" from "everything except ray-tracing is the bug" on the next parent rebuild/run.
- verify: `diff -u Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl` — expected: 0 lines of difference outside header comments.
- skip_impl_review: no — patch modifies canonical master HLSL consumed by tests beyond TestReSTIR_GI_Temporal; full reviewer check required.
- produces_test_files: no.
- notes:
  - The patch is text-identical between the two HLSL copies. Same insertion site (between case 6u and case 13u), same comment block, same case-label line.
  - The patch was self-corrected during impl: initial draft used unqualified `AmbientColor` and `AmbientScale` which are NOT in scope at the switch statement's location. Verified by reading GIPathTracing.hlsl lines 460-487 and 575-598: the actual expression at line 486 is `diffuse * g_GI.AmbientColor.rgb * ambientScale` (cbuffer field via `g_GI.AmbientColor`, local variable `ambientScale` from line 475). Patch was corrected to use these identifiers before any commit landed.
  - The 11-line delta matches the plan's +14 line estimate minus 3 lines of trailing whitespace reformatting (the plan's diff_estimate was generous; actual insertion is 11 lines per file).
  - Both HLSL files are 722 lines, 27538 bytes after the patch (verified by read_file at line offsets). Diff between them is empty outside header comments.

## Self-correction documented in plan-review

PENDING_PLAN_REVIEW_v17.md documents the initial identifier error and the mid-flight correction. The audit trail is preserved: the plan/plan-review/commit/test-audit all reference the corrected identifiers (`g_GI.AmbientColor.rgb * ambientScale`).

## Plan Deviations (impler fills this in if it deviated)

NONE. The patch matches the plan exactly after the in-flight identifier correction. The corrected identifiers were applied before commit landed.

## What landed

```
case 6u:  debugColor = float3(float(pixel.x) / 256.0, 0.0, float(pixel.y) / 256.0); break;
// v17 (six-role-pipeline, 2026-07-27): TraceRay-bypass sentinel.
// If case 6u shows per-pixel gradient AND case 7u shows non-zero
// scene-shape output, the entire non-ray-tracing pipeline works.
// Bug is then constrained to TraceRay / payload / SRV-read chain.
// If case 7u shows 0 or garbage, bug is in the post-TraceRay code
// path (lighting math, payload write, accumulate). Uses the same
// diffuse * g_GI.AmbientColor.rgb * ambientScale expression the
// primary contribution uses (GIPathTracing.hlsl:486), so a non-zero
// result is meaningful: it shows what the shader produces when
// ray-tracing is bypassed. Predicted: mode 7 = mode 1 * 1.5.
case 7u:  debugColor = diffuse * g_GI.AmbientColor.rgb * ambientScale; break;
case 13u: debugColor = RTInstanceInfo[0].AlbedoColor; break;         // SRV sanity read
```

Insertion at line 593 of both HLSL copies. Verified by `patch` tool diff and post-patch `read_file` at line offsets 591-615.

## Verification status

- Pre-patch: Private=711 lines/26670B, Data=711 lines/26670B (post-v15 sync state, drift=0)
- Post-patch: Private=722 lines/27538B, Data=722 lines/27538B (Δ=+11 lines/+868B per file, drift=0)
- Both files have case 6u at line 593 and case 7u at line 604 and case 13u at line 605 (verified by search_files)
- The case 7u expression uses identifiers (`diffuse`, `g_GI.AmbientColor.rgb`, `ambientScale`) that are confirmed in scope at the switch's lexical scope (lines 460-487 declare them all).

## Files NOT modified

- No C++ files modified. v3, v5, v7, v8, v11, v12, v14 patches are already correct and load-bearing.
- No CMake files modified. The Private master HLSL is what slangc compiles (verified by v16's three-source evidence chain).
- No ShaderMake.cfg modified. The data-dir copy remains in the build path as before.
- No CVars or env vars added. The patch reuses the existing `HLVM_PT_DEBUG_MODE` and `r.GI.DebugMode` infrastructure.

## Reversibility

The patch is fully reversible. `git checkout HEAD~1 -- <file>` (or equivalent) restores both HLSL files to their pre-v17 state. The patch does not introduce any persistent state, does not modify build infrastructure, and does not affect the runtime behavior at debugMode=0.