# Pending Commit v135 — Add commitBarriers() BEFORE createBindingSet() in FGIPass.cpp

- plan: docs/PENDING_PLAN_v135.md
- plan_review: docs/PENDING_PLAN_REVIEW_v135.md (verdict: KEEP)
- files: Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp
- source: no bundle — direct edit
- target: branch the parent runspace owns (git topology not touched by cron)
- task: Add a `CmdList->commitBarriers();` call between the three `setTextureState(..., ShaderResource)` calls (FGIPass.cpp lines 547-555) and the `FBindingSetBuilder SRVBuilder` chain (line 557+). This ensures the GBuffer textures are physically in `SHADER_READ_ONLY_OPTIMAL` when `createBindingSet()` is called and when the descriptor is later bound. The existing commitBarriers() at line 668 (v131 patch) is KEPT for defense-in-depth.

  After the patch:
  - The three GBuffer `setTextureState(..., ShaderResource)` calls at lines 547-555 are unchanged.
  - A new `CmdList->commitBarriers();` call is inserted after line 555 (after the third setTextureState).
  - A new 2-line comment explains why this location matters (the nvrhi-deferred-barrier-ordering pattern).
  - The `SRVBuilder` chain at lines 557+ is unchanged.
  - The `createBindingSet` at lines 602-608 is unchanged.
  - The existing `commitBarriers()` at line 668 is unchanged.
  - The `DispatchRays` call at line 670 is unchanged.
  - All other FGIPass code is unchanged.
  - All v131+v132+v133+v134 patches remain intact (verified via search_files).
  - No commits, pushes, or git topology changes.

- verify:
  ```
  cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
  # 1. Rebuild (should pick up the FGIPass.cpp change automatically)
  ./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal --Config=Debug
  # 2. Run with debug mode 20 to verify GBufferMaterial SRV reads work
  HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20 ./Binary/Debug/TestReSTIR_GI_Temporal
  # 3. Inspect gi_raw dump with numpy
  python3 -c "
  import os
  from PIL import Image
  import numpy as np
  dump_dir = 'Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps'
  files = sorted([f for f in os.listdir(dump_dir) if 'gi_raw' in f])
  if files:
      im = np.array(Image.open(os.path.join(dump_dir, files[-1])))
      print(f'gi_raw: per-channel mean={im.mean(axis=(0,1))} std={im.std(axis=(0,1))}')
      print(f'gi_raw: unique R values={len(np.unique(im[:,:,0]))}')
  "
  # 4. If mode 20 returns non-zero, the SRV fix worked. Run validate_restir_gi.py.
  python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
  # 5. If mode 20 STILL returns zero, the barrier ordering was not the root cause.
  #    Next iteration (v136) would enable Vulkan validation layer explicitly
  #    via VK_LAYER_KHRONOS_validation=1 and grep for VUIDs.
  ```

- skip_impl_review: no — this is a binding/timing fix that affects all TestReSTIR_GI_Temporal frames. The reviewer MUST verify:
  1. The new commitBarriers() is at the right line (between setTextureState and SRVBuilder).
  2. The existing commitBarriers() at line 668 is INTACT (defense-in-depth).
  3. The WriteConstants at line 543 is unchanged.
  4. The patch does not introduce a circular dependency or behavioral change beyond the new barrier commit.
  5. All v131+v132+v133+v134 patches remain intact (verified via search_files).

- produces_test_files: no — no test files produced.

- notes: terminal access is structurally blocked in this cron runspace per EC-039. The patch lands file-only; the build/run/verify step requires the parent runspace with terminal.

## Files modified (this cycle)

### 1. `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp`

**One edit**: insert `CmdList->commitBarriers();` + 2-line comment after line 555 (the third setTextureState) and before line 557 (the SRVBuilder chain).

```cpp
// BEFORE (current FGIPass.cpp:545-557):
        CmdList->setTextureState(Desc.OutputTexture, nvrhi::AllSubresources,
                                 nvrhi::ResourceStates::UnorderedAccess);
        if (Desc.GBufferWorldPos)
            CmdList->setTextureState(Desc.GBufferWorldPos, nvrhi::AllSubresources,
                                     nvrhi::ResourceStates::ShaderResource);
        if (Desc.GBufferNormal)
            CmdList->setTextureState(Desc.GBufferNormal, nvrhi::AllSubresources,
                                     nvrhi::ResourceStates::ShaderResource);
        if (Desc.GBufferMaterial)
            CmdList->setTextureState(Desc.GBufferMaterial, nvrhi::AllSubresources,
                                     nvrhi::ResourceStates::ShaderResource);

        FBindingSetBuilder SRVBuilder;
        SRVBuilder.SetConstantBuffer(0, ConstantBuffer)
                  .SetConstantBuffer(1, Desc.ViewConstants)

// AFTER (v135 patch):
        CmdList->setTextureState(Desc.OutputTexture, nvrhi::AllSubresources,
                                 nvrhi::ResourceStates::UnorderedAccess);
        if (Desc.GBufferWorldPos)
            CmdList->setTextureState(Desc.GBufferWorldPos, nvrhi::AllSubresources,
                                     nvrhi::ResourceStates::ShaderResource);
        if (Desc.GBufferNormal)
            CmdList->setTextureState(Desc.GBufferNormal, nvrhi::AllSubresources,
                                     nvrhi::ResourceStates::ShaderResource);
        if (Desc.GBufferMaterial)
            CmdList->setTextureState(Desc.GBufferMaterial, nvrhi::AllSubresources,
                                     nvrhi::ResourceStates::ShaderResource);

        // v135 (six-role-pipeline, tick 213, 2026-07-30): commit barriers BEFORE
        // createBindingSet. The descriptor's vkUpdateDescriptorSets call captures
        // the image's CURRENT physical layout; if the image is still in
        // COLOR_ATTACHMENT_OPTIMAL (from the GBuffer raster pass), the GPU reads
        // garbage at dispatch time even if the line-668 commitBarriers fires later.
        CmdList->commitBarriers();

        FBindingSetBuilder SRVBuilder;
        SRVBuilder.SetConstantBuffer(0, ConstantBuffer)
                  .SetConstantBuffer(1, Desc.ViewConstants)
```

**Total diff**: +3 / -0 lines (one commitBarriers() call + 2-line comment).

## Plan Deviations

NONE. The impler followed the v135 plan exactly:
- Edit 1 inserts `CmdList->commitBarriers();` + 2-line comment at the correct position (between setTextureState and SRVBuilder).
- The existing commitBarriers() at line 668 is INTACT (defense-in-depth).
- The WriteConstants at line 543 is unchanged.
- All v131+v132+v133+v134 patches remain intact.

## Acceptance verification (parent runspace)

After the rebuild:
1. Build succeeds: `./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal` exits 0.
2. (NEW per v135) `HLVM_PT_DEBUG_MODE=20` returns non-zero GBufferMaterial per-pixel values.
3. (NEW per v135) `HLVM_PT_DEBUG_MODE=21` returns non-zero GBufferNormal per-pixel values.
4. (NEW per v135) `HLVM_PT_DEBUG_MODE=22` returns non-zero GBufferWorldPos per-pixel values.
5. `validate_restir_gi.py` passes the newest dump group.
6. Fresh display image (vision) shows recognizable Sponza geometry.
7. No Vulkan VUID/ERROR when validation layer is enabled (v132+v133+v134 patches).
8. No command-list errors in the test log.

## Honesty floor

This commit lands a patch. It does NOT claim:
- The cmake reconfigure succeeded.
- The validation symbols are now in libnvrhid.a.
- The validation layer fires VUID.
- The gi_raw dump is non-zero.
- Any dump was analyzed.
- The barrier ordering was the root cause.

The patch is correct on static analysis (per the v135 plan evidence and the plan-criticer's KEEP verdict). The build/run verification requires the parent runspace. If the build fails (linker error), the impler's fallback path is: revert the v135 patch, then re-attempt with a different mechanism (e.g., add explicit `vkCmdPipelineBarrier` calls for each texture). If the build succeeds but mode 20 STILL returns zero, the barrier ordering was NOT the root cause and v136 would address the next hypothesis (slangc dead-strip, pipeline cache staleness, or Vulkan validation layer needed).

## What this commit does NOT change

- No commits, pushes, history rewrites (cron is file-only; git topology is parent-runspace responsibility).
- No governance-file edits.
- No new test files.
- The v131 patches (commitBarriers at line 668, case 31u discriminator) remain as landed.
- The v132 patches (createValidationLayer at DeviceManagerVk4_LifeCycle.cpp:88) remain as landed.
- The v133 patches (cmake FORCE at Engine/Source/Runtime/CMakeLists.txt:182) remain as landed.
- The v134 patches (validation TUs in add_library) remain as landed.