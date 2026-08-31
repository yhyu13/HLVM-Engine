# Pending Commit v15

- plan: docs/PENDING_PLAN_v15.md
- files: Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl
- source: no bundle — direct edit (sync from data-dir copy)
- target: master (working tree, parent commits on next session)
- task: bring canonical Private master GIPathTracing.hlsl into sync with data-dir copy by inserting v13 case-6u UAV-write sentinel between case 5u and case 13u
- verify: `diff -u Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl` — should show 0 lines of difference outside header comments
- skip_impl_review: no — patch modifies master source consumed by tests beyond TestReSTIR_GI_Temporal
- produces_test_files: no
- notes: The patch is text-identical to the existing data-dir copy at lines 584-593. The case-6u sentinel is gated behind `if (debugMode != 0u)`, so production (debugMode=0) behavior is unaffected. The patch only affects behavior if some other test consumer sets HLVM_PT_DEBUG_MODE=6 and reads from Private master instead of data-dir copy — extremely unlikely per the v15 plan's risk analysis.

## Implementation summary

Applied patch via `patch` tool. The 10-line insertion (9-line comment + 1-line case label) was inserted between the existing case 5u (line 583) and case 13u (was line 584, now line 594). Diff:

```
@@ -581,6 +581,16 @@
             case 3u:  debugColor = primaryDirect; break;
             case 4u:  debugColor = indirect / max(float(spp), 1.0f); break;
             case 5u:  debugColor = float3(avgFirstHitDist, avgFirstHitDist, avgFirstHitDist) * 0.1f; break;
+            // v13 (six-role-pipeline, 2026-07-27): UAV-write sentinel. Writes
+            // a UNIQUE, recognizable per-pixel constant (1.0, 0.0, 1.0) to
+            // OutputTexture at the very start of the write, BEFORE any other
+            // code. If gi_raw with HLVM_PT_DEBUG_MODE=6 shows magenta-like
+            // values, the dispatch body is running and the UAV write is
+            // landing in the texture. The bug is then in the lighting/payload
+            // math downstream of this line. If gi_raw with mode=6 shows 0,
+            // the dispatch is not running or the UAV write is being dropped
+            // (desc-barrier, descriptor mismatch, no dispatch at all).
+            case 6u:  debugColor = float3(float(pixel.x) / 256.0, 0.0, float(pixel.y) / 256.0); break;
             case 13u: debugColor = RTInstanceInfo[0].AlbedoColor; break;         // SRV sanity read
             case 14u: debugColor = RTVertices[0].Position * 0.25f + 0.5f; break; // SRV sanity read
             default: break;
```

File went from 701 to 711 lines (+10 net). File size from 25881 to 26670 bytes (+789 net, consistent with +10 lines averaging ~79 bytes/line). Verified by `read_file` post-patch at offset 578-602.

## What this commit does NOT do

- Does NOT change any runtime behavior. The test build was already compiling the data-dir copy, which had the patch.
- Does NOT require a rebuild to be useful. The patch's value is downstream: future rebuilds from Private master produce identical SPIR-V to rebuilds from data-dir copy.
- Does NOT touch TestReSTIR_GI_Temporal.cpp, FGIPass.cpp, or any other source file. Single-file additive sync.
- Does NOT introduce case 6u behavior into any new code path. The case is debug-mode-gated.

## What parent must do

1. Run the verify command above to confirm zero meaningful diff between the two HLSL copies.
2. (Optional) Run a clean rebuild from Private master and compare the resulting `.sblob` against the existing data-dir compile — SPIR-V identity confirms the sync at the binary level.
3. Continue v12+v13+v15 evidence collection on the next test run.
4. Commit v15 to master on next session.