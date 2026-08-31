# Pending Commit v18

- plan: docs/PENDING_PLAN_v18.md
- files:
  - `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl` (+51 / -0 lines net: 4 case-block insertions + 4 comment blocks)
  - `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl` (+51 / -0 lines net: mirrored from Private master per v15/v17 sync convention)
- source: docs/PENDING_PLAN_v18.md (patch shape) + on-disk inspection of both HLSL copies
- target: not-applicable (file-only diagnostic patch; not a renderer fix)
- task: add `case 8u` (TraceRay-only sentinel), `case 9u` (diffuse × 1.5), `case 10u` (debugMode cbuffer reach), `case 11u` (View cbuffer reach) to GIPathTracing.hlsl. Advances diagnostic surface from 2 probes (modes 6, 7) to 6 probes (modes 6, 7, 8, 9, 10, 11). Each new case is gated behind `if (debugMode != 0u)` and produces a recognizable per-pixel output that distinguishes a specific hypothesis.
- verify: `diff -u Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl` — expected: 0 lines of difference outside header comments.
- skip_impl_review: no — patch modifies canonical master HLSL consumed by tests beyond TestReSTIR_GI_Temporal; full reviewer check required.
- produces_test_files: no.
- notes:
  - The patch is text-identical between the two HLSL copies. Same insertion site (between case 7u at line 604 and case 13u at line 605 in pre-v18, now lines 614-655 in post-v18), same comment blocks, same case labels.
  - **Mid-flight correction**: the plan's initial draft included a cosmetic change to `case 13u: debugColor = RTInstanceInfo[0].AlbedoColor;` (adding `.rgb` for explicit `float3` access). On review, the original line was correct: `FInstanceInfo.AlbedoColor` is already declared as `float3` at HLSL line 125; adding `.rgb` would have been redundant. The impl dropped this cosmetic change to keep the patch as minimal-diff-as-possible.
  - **Mid-flight structural change**: the plan's initial draft placed all four case labels inline (single statement per case). The actual implementation uses a `{ ... }` block scope for case 8u because it declares local variables (`GIPayload tracePayload`, `RayDesc traceRay`). This is correct HLSL: case labels with local variable declarations require a block scope to avoid "jump skips variable initialization" errors. Case 9u, 10u, 11u don't declare local variables and can stay single-statement.
  - **Diff delta**: +51 lines per file (plan estimated +44; the case 8u block-scope adds 4 lines for the braces, and the case 8u comment is 10 lines vs the plan's 9-line estimate for similar comments elsewhere). The +51 number is the accurate delta; the plan's estimate was slightly conservative.
  - Both HLSL files are 773 lines, 30470 bytes after the patch. Verified by read_file at line offsets and line count.

## Self-correction documented in plan-review

The mid-flight decision to (a) drop the case 13u cosmetic change and (b) add a block scope for case 8u were both made BEFORE commit landed. PENDING_PLAN_REVIEW_v18.md acknowledges these as correct refinements.

## Plan Deviations (impler fills this in if it deviated)

- **Cosmetic change to case 13u dropped** (plan: add `.rgb`; actual: leave as `RTInstanceInfo[0].AlbedoColor`). Rationale: `FInstanceInfo.AlbedoColor` is already `float3`, no conversion needed. Minimal-diff principle.
- **Block scope added for case 8u** (plan: inline; actual: `{ ... }` block). Rationale: case 8u declares local `GIPayload tracePayload` and `RayDesc traceRay`; HLSL requires block scope to avoid "jump skips variable initialization" compile error. Block scope is the correct pattern for any case that declares locals.
- **+51 vs +44 line estimate**: the actual patch is 7 lines longer than the plan's diff_estimate, all in the case 8u block (4 lines for `{`/`}` + 3 lines of comment expansion). Plan's estimate was slightly conservative.

These deviations are all mechanical refinements, not design changes. The patch's intent (advance the diagnostic surface from 2 to 6 probes) is preserved exactly.

## What landed

```
case 7u:  debugColor = diffuse * g_GI.AmbientColor.rgb * ambientScale; break;
// v18 (six-role-pipeline, 2026-07-27): TraceRay-only sentinel.
// [comment]
            case 8u:
            {
                GIPayload tracePayload;
                [12 lines of payload + ray setup]
                TraceRay(SceneBVH, RAY_FLAG_FORCE_OPAQUE, 0xFF, 0, 0, 0, traceRay, tracePayload);
                debugColor = float3(tracePayload.hitDistance > 0.0f ? 1.0f : 0.0f,
                                    tracePayload.hitDistance * 0.1f,
                                    float(tracePayload.flags) / 8.0f);
                break;
            }
// v18: diffuse-only sentinel (mode 9 = mode 1 * 1.5). [comment]
            case 9u:  debugColor = diffuse * 1.5f; break;
// v18: debugMode cbuffer reach sentinel. [comment]
            case 10u: debugColor = float3(g_GI.Params5.x / 256.0f, 0.0f, 0.0f); break;
// v18: View cbuffer reach sentinel. [comment]
            case 11u: debugColor = float3(g_View.FrameIndex / 256.0f, g_View.FrameIndex / 256.0f, g_View.FrameIndex / 256.0f); break;
            case 13u: debugColor = RTInstanceInfo[0].AlbedoColor; break;         // SRV sanity read
```

Insertion at line 605 of both HLSL copies (post-v17 line 604 was case 7u, so the new cases start at line 605). Verified by `patch` tool diff and post-patch `read_file` at line offsets.

## Verification status

- Pre-patch: Private=722 lines/27538B, Data=722 lines/27538B (post-v17 sync state, drift=0)
- Post-patch: Private=773 lines/30470B, Data=773 lines/30470B (Δ=+51 lines/+2932B per file, drift=0)
- Both files have case 7u at line 604, case 8u at line 614, case 9u at line 642, case 10u at line 650, case 11u at line 655, case 13u at line 656 (verified by search_files)
- All identifiers used in the new cases are confirmed in scope at the switch's lexical location:
  - `GIPayload`: declared at the top of the file; used in the main loop at line 502 with identical field-set pattern
  - `RayDesc`: declared at the top of the file; used in the main loop at line 522 with identical field-set pattern
  - `SceneBVH`: declared at line 94; used in the main loop at line 529
  - `RAY_FLAG_FORCE_OPAQUE`: defined in nvrhi/slang header; used in the main loop at line 530
  - `g_GI.Params2.y/.z`: declared in GIConstants:66; used in the main loop at lines 525-526
  - `g_GI.Params5.x`: declared in GIConstants:69; used at line 575 to read debugMode
  - `g_View.FrameIndex`: declared in ViewConstants:77; used at line 477 to compute pixelSeed
  - `diffuse`: declared at line 464 (`float3 diffuse = GBufferMaterial[pixel].rgb;`)
  - `pixelSeed`: declared at line 477
  - `rayOrigin`, `rayDir`: declared at lines 511-512

## Files NOT modified

- No C++ files modified. v3, v5, v7, v8, v11, v12, v14 patches are already correct and load-bearing.
- No CMake files modified. The Private master HLSL is what slangc compiles (verified by v16's three-source evidence chain).
- No ShaderMake.cfg modified. The data-dir copy remains in the build path as before.
- No CVars or env vars added. The patch reuses the existing `HLVM_PT_DEBUG_MODE` and `r.GI.DebugMode` infrastructure.

## Reversibility

The patch is fully reversible. `git checkout HEAD -- <file>` (or equivalent) restores both HLSL files to their pre-v18 state. The patch does not introduce any persistent state, does not modify build infrastructure, and does not affect the runtime behavior at debugMode=0.