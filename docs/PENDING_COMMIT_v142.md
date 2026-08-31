# Pending Commit v142
- plan: docs/PENDING_PLAN_v142.md
- plan_review: docs/PENDING_PLAN_REVIEW_v142.md (verdict: KEEP)
- files:
  - Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp
- source: docs/PIPELINE_HEALTH_2026-08-01_tick534.md (authoritative v25-diagnostic-correction finding showing v141 collapsed per-pixel dynamic range from 95× to 2×) + the three log files at `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal{,_1,_2}.log` line 320-321 + re-read of `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:353-396` (4-light NEE infrastructure)
- target: branch `six-role-pipeline/restir-gi-binding-fix` (or current working branch if dev is iterating freely)
- task: Revert v141's `Desc.AmbientScale = 0.25f` back to `1.5f` and replace the v141 REFINED DIAGNOSIS comment with a v25-aligned comment, while keeping v140's `Desc.AmbientColor = (1, 1, 1, 0)` override. After v142, the test should produce a Sponza image with per-pixel NEE variation visible (pre-v141 evidence: `R[0.9, 96.2]` 95× dynamic range).
- verify: file-only integrity checks (this file's `## File-only verification` section); for behavioral verification the parent runspace executes:
  ```bash
  cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
  ./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal --Config=Debug
  HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal
  python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.py \
      --data-dir Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data
  python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
  ```
  Expected post-v142:
  - Build SUCCESS (TestReSTIR_GI_Temporal.cpp single-line revert; no other files touched)
  - gi_raw log line: `R[<min>,<max>] G[<min>,<max>] B[<min>,<max>]` with `min<max` and dynamic range > 5× (pre-v141 was 95×; v141 was 2×; v142 should restore >5×)
  - dump_pixelstats: gi_raw per-channel mean > 5.0 (per validate_restir_gi.py check 1)
  - validate_restir_gi.py: passes non_black_channel_mean, spatial_std, cell_variance, alpha_sentinel (4/4)
  - Vision check: recognizable Sponza with directional shading and sane exposure
- skip_impl_review: yes (per HARD INVARIANT #2: only honored because `produces_test_files: no` — this is a single-line revert in production test code, no new test files)
- produces_test_files: no
- notes:
  - v142 is a surgical revert. The pre-v141 binary at 19:46:53 produced 95× dynamic range; v141 collapsed it to 2×; v142 should restore it.
  - **Test determinism caveat**: tick 534 identified that the post-v141 binary at 23:15:39 produced 95× range, while the 23:17:02 run (90 seconds later) produced 2× range. v142 reverts AmbientScale but does NOT address the determinism issue. If post-v142 produces inconsistent results between runs, the determinism issue must be investigated separately (likely in the GPU temporal ReSTIR pass or accum buffer state initialization).
  - **Vision exposure caveat**: with `AmbientScale=1.5` + `AmbientColor=(1,1,1)`, per-pixel `primaryAmbient` can saturate to 1.5 in lit areas. The dump clamps to [0, 1] so the displayed PNG will saturate in lit regions but show per-pixel variation in non-saturated regions. This is acceptable for the acceptance criteria.

## Patch summary (1 file)

### `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` (-16 / +8 lines)

Replaced lines 447-464 (18-line "Lighting setup" block: 16-line v141 REFINED DIAGNOSIS comment + `Desc.AmbientScale = 0.25f`) with an 8-line v142 REVERT v141 comment + `Desc.AmbientScale = 1.5f`. The v140 AmbientColor comment + `Desc.AmbientColor[0..3] = (1, 1, 1, 0)` override at the original lines 461-464 (now 456-464 after patch) is preserved unchanged.

Net: -16 / +8 lines = -8 lines net, single file. Diff: ~8 lines, surgical.

## File-only verification (this runspace already executed)

| # | Check | Expected | Actual | Pass |
|---|-------|----------|--------|------|
| 1 | `TestReSTIR_GI_Temporal.cpp:455` contains `Desc.AmbientScale      = 1.5f;` | yes | ✓ (1 match at line 455 with value `1.5f`) | ✓ |
| 2 | `TestReSTIR_GI_Temporal.cpp:455` does NOT contain `Desc.AmbientScale ... 0.25f` | 0 matches in assignment | ✓ (line 452's "v141's AmbientScale=0.25f" is in a comment explaining the OLD value, not the assignment) | ✓ |
| 3 | `TestReSTIR_GI_Temporal.cpp:447` contains v142 REVERT v141 comment marker | yes | ✓ ("v142 (six-role-pipeline): REVERT v141." at line 447) | ✓ |
| 4 | `TestReSTIR_GI_Temporal.cpp` does NOT contain the old v141 REFINED DIAGNOSIS comment block | 0 matches for `REFINED DIAGNOSIS` | ✓ (0 matches — replaced by v142 REVERT v141 comment) | ✓ |
| 5 | `TestReSTIR_GI_Temporal.cpp:461-464` retains v140 AmbientColor override | 4 matches for `Desc.AmbientColor[` | ✓ (4 matches at lines 461-464 with values `(1.0f, 1.0f, 1.0f, 0.0f)`) | ✓ |
| 6 | FGIPass.h:58-62 v140 AmbientColor field intact (default `{ 0.6f, 0.6f, 0.65f, 0.0f }`) | yes | verified pre-patch, no change in this cycle | ✓ |
| 7 | FGIPass.cpp:447-449 + 463 v140 AmbientColorPtr indirection intact | yes | verified pre-patch, no change in this cycle | ✓ |
| 8 | All 11 prior patches (v22 split + v131-v140) still intact | yes | spot-check via search_files (v140 `AmbientColor` field at FGIPass.h:58-62 INTACT; v138 `bypassEarlyReturn` at GIPathTracing.hlsl:486 INTACT; v137 `setBindingOffsets` at FGIPass.cpp:318 INTACT; v139 `createValidationLayer` at DeviceManagerVk4_LifeCycle.cpp:118 INTACT; v135 `commitBarriers` at FGIPass.cpp:578/579/683/686/692 INTACT) | ✓ |
| 9 | CMakeLists.txt UNCHANGED | yes | v142 modifies only `TestReSTIR_GI_Temporal.cpp`; no new source files; no cmake edits needed | ✓ |
| 10 | v142 REVERT v141 comment block length is 8 lines (replacing v141's 16 lines) | yes | ✓ (lines 447-454 = 8 lines) | ✓ |

## Plan Deviations

None. The impler applied the plan precisely:
- Reverted `Desc.AmbientScale = 0.25f` to `1.5f` (the v140 value, as planned)
- Replaced the 16-line v141 REFINED DIAGNOSIS comment with an 8-line v142 REVERT v141 comment (planned as 4 lines, actual is 8 lines for clarity — slightly longer but well within the "surgical" scope; the extra 4 lines document the 4-light NEE infrastructure per `FGIPass::UploadLights()` at line 353-396 as called for in the plan's `## risks` step 3)
- Preserved v140 AmbientColor override at lines 461-464 unchanged
- All other source files unchanged

**Minor deviation note**: the v142 comment is 8 lines (vs the plan's 4-line target). This is a 4-line increase in the comment block, justified by the plan's `## risks` step 3 ("the 4-line comment should reference `FGIPass::UploadLights()` at line 380+390-395 explicitly so future maintainers can verify the light infrastructure"). The extra 4 lines document the 4-light NEE infrastructure + the 3-log evidence chain. Net diff: -16 / +8 = -8 lines net (still a substantial reduction from v141's +11 lines).

## Notes

- This is a file-only patch (no behavioral verification in this runspace — terminal/vision/python3 blocked by tirith).
- The patch is **necessary and likely sufficient** for the user's "recognizable Sponza with sane exposure" acceptance criterion IF the test determinism issue (23:15 vs 23:17 dynamic range collapse) is independent of AmbientScale. Per tick 534's analysis, the determinism issue is most likely a separate bug in the GPU temporal ReSTIR pass or accum buffer state initialization, not caused by v141.
- If post-v142 the image is recognizable Sponza, the bisect closes (4/4 validator checks pass, vision check confirms Sponza, mode 20 returns non-zero per-pixel data).
- If post-v142 the image is uniform or unrecognizable, v143 must investigate the test determinism issue separately.
