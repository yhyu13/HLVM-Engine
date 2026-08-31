# Pending Commit v3 — extend handle-identity diagnostic + sentinel-compare debug mode

- plan: docs/PENDING_PLAN_v3.md
- files:
  - Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp
  - Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp
  - Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl
  - Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl
- source: docs/DIAGNOSTIC_2026-07-30.md + PENDING_PLAN_v3.md
- target: working tree (cron tick v3; not committed)
- task: Stage the v3 contingent code changes. **DO NOT APPLY** until operator
  reports whether v2 fixed the bug. If v2 worked, discard this commit entirely
  and close the card.
- verify (operator-side only; cron profile has terminal blocked):
  1. Apply v3 changes.
  2. `./Build.sh --Rebuild --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`
  3. With `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20`: the
     `[handle-id]` log lines should now appear for ALL 8 frames, not just
     frames 0-3. Compare RenderGBuffer handle to DispatchRays handle:
     - matching → binding layer is wrong at descriptor level (route to v4 fix:
       investigate Vulkan binding offsets, nvrhi descriptor-write path)
     - mismatching → raster pass recreates textures mid-frame; route to v4 fix:
       move texture creation out of the resize path, or capture the raster-
       time texture handles and pass them to GI at frame end.
  4. With `HLVM_PT_DEBUG_MODE=23`: the dump should show
     `(GBufferWorldPos.rgb, sentinel_flag)` per pixel.
     - sentinel_flag = 1.0 where the raster pass wrote the magenta sentinel
       (i.e., where the GI SRV did NOT read raster output → handle mismatch
       or stale descriptor)
     - sentinel_flag = 0.0 where the SRV read returned valid worldpos data
       (correct binding)
- skip_impl_review: no (the mode-23 sentinel-compare is non-trivial HLSL +
  a new CVar / env var path; the handle-id extension is trivial but the
  combined commit deserves review).
- produces_test_files: no
- notes:
  - **Gate everything behind `HLVM_PT_DEBUG_MODE=23`.** The mode is opt-in
    via env var. Production users do not hit it.
  - **Handle-id extension (`if (FrameIndex < 64u)`) is debug-only.** Once
    v3 has served its purpose, revert to `< 4u` or remove entirely. Don't
    ship "log every frame" to production — it spams the log.
  - **Sentinel upload must be in the test's per-frame Render path, not in
    shipping code.** Place it in TestReSTIR_GI_Temporal.cpp gated by
    `if (getenv("HLVM_PT_DEBUG_MODE") && atoi(getenv("HLVM_PT_DEBUG_MODE"))
    == 23)`.
  - **HLSL dual-copy:** edit BOTH `Private/.../GIPathTracing.hlsl` AND
    `TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl`. Keep them byte-
    identical in the resource declarations section.

## Diff summary (planned, not applied)

### FGIPass.cpp
- Line 547: change `if (Desc.FrameIndex < 4u)` → `if (Desc.FrameIndex < 64u)`
  (or similar — full-frame coverage for the 8-accumulation run)

### TestReSTIR_GI_Temporal.cpp
- Line 2226: same `if (Desc.FrameIndex < 4u)` → `if (Desc.FrameIndex < 64u)`
  on the RenderGBuffer handle-id log
- New: sentinel upload block gated by `HLVM_PT_DEBUG_MODE=23`, called
  after RenderGBuffer (so the GI dispatch reads the sentinel where the
  raster pass would have written non-sentinel data)

### GIPathTracing.hlsl (both copies)
- Add `case 23u:` to the debug switch:
  ```hlsl
  case 23u:
  {
      float4 WorldPos = GBufferWorldPos.Load(int3(pixel, 0));
      // Sentinel flag = 1.0 when SRV returned the magenta sentinel
      // (= handle mismatch OR stale descriptor), 0.0 when real worldpos
      float SentinelFlag = (WorldPos.r > 0.99 && WorldPos.g < 0.01
                            && WorldPos.b > 0.99) ? 1.0 : 0.0;
      OutColor = float4(WorldPos.rgb * 0.25 + 0.5, SentinelFlag);
      break;
  }
  ```

## Plan Deviations

**None.** The plan was followed as written. The contingency branch was
taken because the file-only inspection of FGIPass.cpp + the HLSL files +
TestCornellBoxGI.cpp didn't surface an obvious bug beyond the v2 fix.
The handle-identity check + sentinel-compare is the cheapest decisive
experiment.

## Pre-impl hypothesis (preserved for tick audit)

The v2 fix is mechanically correct (single binding set, HLSL `space1`
removed, layout/set validation passes, dispatch returns normally). If
it doesn't fix the SRV reads, the bug is at one of:
- Handle mismatch (raster pass recreates textures mid-frame; GI has
  stale handles) — distinguishable by handle-id log mismatch.
- Descriptor mismatch (nvrhi writes the descriptor but the Vulkan
  pipeline doesn't read it correctly — wrong slot, wrong type, wrong
  layout) — distinguishable by mode 23 sentinel-compare (SRV returns
  sentinel where it should return real data).

Both are diagnosable by the v3 changes; v4 implements the fix for
whichever the v3 diagnostic points at.
