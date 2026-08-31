# Pending Plan v135 — Move `CmdList->commitBarriers()` BEFORE `createBindingSet()` in FGIPass.cpp (file-only fix for the nvrhi-deferred-barrier-ordering root cause of zero GBuffer SRV reads)

- task: Move `CmdList->commitBarriers()` from FGIPass.cpp:668 (after binding set creation, before dispatch) to a new location AFTER `setTextureState(..., ShaderResource)` calls (lines 547-555) and BEFORE `SRVBuilder` chain (line 557+). This closes the nvrhi-deferred-barrier-ordering gap that v131 missed: the v131 patch put `commitBarriers()` AFTER the descriptor sets were created, but the descriptor set's `vkUpdateDescriptorSets` call captures image layout at update time. If the image is in `COLOR_ATTACHMENT_OPTIMAL` (post-raster) when the descriptor is updated, and the barrier hasn't fired yet, the descriptor's recorded `imageLayout = SHADER_READ_ONLY_OPTIMAL` is correct in metadata, but the imageView's contents at descriptor-use time are UB per Vulkan spec. Empirical evidence: `HLVM_PT_DEBUG_MODE=20/21/22` (GBufferMaterial/Normal/WorldPos SRV reads) return zero for every pixel even though the textures contain real Sponza data (gbuffer_worldpos dump: R[-15.228,15.264] G[-11.811,8.193] B[-14.291,0.025]; gbuffer_material: 254/255 across all pixels). The shader's cbuffer reads (mode 10/11/15) also return zero, which is consistent with the cbuffer descriptor having the same problem.
- source: no bundle — direct edit to `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp`
- approach: Add a `CmdList->commitBarriers();` call between the three `setTextureState(..., ShaderResource)` calls (lines 547-555) and the `FBindingSetBuilder SRVBuilder` chain (line 557). Keep the existing `commitBarriers()` at line 668 for defense-in-depth. The new commitBarriers at the new location ensures the GBuffer textures are physically in `SHADER_READ_ONLY_OPTIMAL` when `createBindingSet()` is called and when the descriptor is later bound. This is the canonical fix for the nvrhi-deferred-barrier-ordering pattern documented in `references/nvrhi-deferred-barrier-ordering.md` (mentioned in the v131 patch comments) and the AGENTS.md "nvrhi's auto-barrier ordering is fragile" gotcha.
- diff_estimate: +3 / -0 lines (one new commitBarriers() call + 2-line comment explaining why this location matters)
- skip_plan_review: no — this is a binding/timing fix that affects all TestReSTIR_GI_Temporal frames; the reviewer MUST verify the placement is correct and the chain still functions.
- test_strategy: file-only tests:
  - Test 1: confirm new `CmdList->commitBarriers();` is at the right line (between the three setTextureState calls and the SRVBuilder chain).
  - Test 2: confirm the existing commitBarriers at line 668 is INTACT (defense-in-depth).
  - Test 3: confirm WriteConstants at line 543 is unchanged (writeBuffer doesn't commit barriers, so no behavioral change).
  - Test 4: confirm GBuffer textures in test log show real Sponza data (per-channel stats from dump file group 20260730_081242_*: worldpos R[-15.228,15.264]).
  - Test 5: confirm the gi_raw dump is non-zero after rebuild (deferred to parent runspace — requires terminal+run).
  - Test 6: confirm validator_restir_gi.py passes after rebuild (deferred to parent runspace).
  - Test 7: confirm HLVM_PT_DEBUG_MODE=20 returns non-zero GBufferMaterial (deferred to parent runspace).
  - Test 8: confirm no Vulkan VUID/ERROR after enabling validation layer via v132 (deferred to parent runspace).
  - Test 9: confirm the v131 commitBarriers() at line 668 is still there for defense-in-depth.
  - Test 10: confirm the v22 split binding layout (SRV-only + UAV-only) is still intact (no regression to single-binding-set pattern).
- risks:
  1. **The actual root cause may be different.** The v24 diagnostic suggested "image layout transition from RENDER_TARGET to SHADER_READ_ONLY_OPTIMAL might be missing." If the barrier is already firing (v131 commitBarriers at line 668), then the textures ARE in SHADER_READ_ONLY_OPTIMAL by dispatch time. The fix of adding a commitBarriers earlier is defense-in-depth, but might not change observable behavior. **Mitigation**: this fix is low-risk (additive; doesn't remove existing code). If it doesn't fix the bug, the next iteration addresses the next hypothesis.
  2. **Two commitBarriers() calls in one dispatch may cause redundant barriers.** nvrhi's barrier queue is deduplicated internally — calling commitBarriers twice in a row with no new barriers between should be a no-op. **Mitigation**: not a regression risk.
  3. **The SRV binding set's createBindingSet at line 602-608 might capture stale texture state if the new commitBarriers doesn't actually transition images.** This is the canonical nvrhi bug — nvrhi's `setTextureState` only QUEUES a transition, doesn't apply it until commitBarriers. **Mitigation**: by calling commitBarriers BEFORE createBindingSet, we ensure the transition has applied when the descriptor is created.
  4. **The fix might NOT be the root cause.** The actual root cause could be (a) slangc dead-stripping (ruled out by mode 31 discriminator returning zero), (b) descriptor pointing to wrong image (ruled out by handle-id diagnostic), (c) descriptor pointing to a separate "shadow" texture (none), (d) UAV/SRV conflict in same descriptor set (ruled out by v22 split). After v135, the remaining hypotheses are: slangc removing the Texture2D load entirely (would need spirv-dis verification), or pipeline cache returning a stale pipeline. **Mitigation**: v136 would address these if v135 doesn't fix the bug.
  5. **The patch breaks if the pipeline library cache holds an old pipeline layout.** nvrhi's pipeline cache (keyed on shader hash + binding layout) might return a cached pipeline that was compiled with the old barrier ordering. **Mitigation**: pipeline cache rebuild is automatic on binding layout change (no manual clear needed in this patch).
  6. **CI doesn't have a Vulkan validation layer installed.** The validation layer would catch the SRV-read-zero pattern with a VUID; without it, we're flying blind. **Mitigation**: v132+v133+v134 patches enable the validation layer at build time; after rebuild, the validation layer should fire VUIDs that name the exact issue.

## Why this fix is high-probability-correct

The v131 patch put `commitBarriers()` at FGIPass.cpp:668 with the rationale "nvrhi's setComputeState binds descriptor sets BEFORE commitBarriers, so the Vulkan validation layer sees the descriptors with the WRONG image layout... Without this explicit commitBarriers() call, the GPU may dispatch with stale layouts and return zero for SRV reads."

This rationale is correct — but the v131 fix is **in the wrong location**. The descriptor sets are CREATED (not bound) at lines 602-608. `createBindingSet()` calls `vkUpdateDescriptorSets` which records the imageLayout parameter (SHADER_READ_ONLY_OPTIMAL). nvrhi passes the current expected layout (which is SHADER_READ_ONLY_OPTIMAL because the SRV requires it), NOT the actual current physical layout. So the descriptor's metadata says SHADER_READ_ONLY_OPTIMAL.

**The bug is subtle**: when `vkUpdateDescriptorSets` is called with imageLayout=SHADER_READ_ONLY_OPTIMAL, but the image is physically in COLOR_ATTACHMENT_OPTIMAL, the GPU at dispatch time needs the image to be in SHADER_READ_ONLY_OPTIMAL. The barrier at line 668 transitions it. By dispatch time, the image is in SHADER_READ_ONLY_OPTIMAL — the GPU should read correctly.

**HOWEVER**: `createBindingSet` is implemented by nvrhi to call `vkUpdateDescriptorSets` with the image layout it expects. If the image is currently in a different layout, **nvrhi may insert the appropriate barrier BEFORE the vkUpdateDescriptorSets call** (this is what `insertComputeResourceBarriers` does in nvrhi's setComputeState). But the v131 patch put the explicit commitBarriers BEFORE the dispatch (line 668), AFTER the createBindingSet. **By the time line 668's commitBarriers fires, the descriptor set is already updated with the image layout. But the IMAGE wasn't in that layout when the descriptor was updated — it WAS in COLOR_ATTACHMENT_OPTIMAL.** The descriptor records the image's VkImageView handle + expected layout. When the GPU does `OpImageFetch` at dispatch time, it looks up the image (via VkImageView) and checks the current layout. If the layout is SHADER_READ_ONLY_OPTIMAL (because the barrier fired), the read proceeds. **This SHOULD work.**

So the barrier ordering per Vulkan spec is actually fine. The descriptor creation order shouldn't matter for SRV correctness.

**The actual root cause may be elsewhere.** The most likely candidates, in order of likelihood given the evidence:

1. **The descriptor's imageView points to a different image than expected.** This is what the v23-diag handle-id log was designed to check — and it shows matching handles. Ruled out.
2. **The image was recreated between RenderGBuffer and FGIPass.** Also ruled out by handle-id.
3. **The texture isn't actually in SHADER_READ_ONLY_OPTIMAL at dispatch time.** Possible if the barrier at line 668 doesn't fire before the dispatch's setComputeState.
4. **slangc removed the texture loads.** Mode 31 discriminator reads `GBufferMaterial.Load(int3(pixel, 0)).rgb * 0.5f + 0.1f` — if this returns zero, the texture load is dead-stripped. Ruled out only if mode 31 actually shows non-zero, which the v24 diagnostic shows it does NOT (mode 31 also returns all-zero).
5. **The pipeline was built with the old shader and cached.** Possible if slangc-emitted SPIR-V is cached and doesn't reflect the latest source changes. **Mitigation**: pipeline cache rebuild.

**Mode 31 also returning zero is the critical clue.** Mode 31's logic:
```hlsl
case 31u:
{
    float3 aliveSentinel = GBufferMaterial.Load(int3(pixel, 0)).rgb * 0.5f + 0.1f;
    if (any(aliveSentinel > float3(0.1, 0.1, 0.1))) {
        debugColor = aliveSentinel;
    } else {
        debugColor = float3(0.0, 0.0, 1.0); // blue
    }
    break;
}
```

If `GBufferMaterial.Load(int3(pixel, 0)).rgb = (0,0,0)`, then `aliveSentinel = (0.1, 0.1, 0.1)`. The condition `any(aliveSentinel > (0.1, 0.1, 0.1))` is FALSE (they're equal, not greater). So `debugColor = (0, 0, 1)` — pure blue. But the dump shows all-zero, not blue. **This means the case 31 branch is being SKIPPED entirely**, not just returning blue.

How can case 31 be skipped if it's in the bypass list (lines 475-479) and the shader is reached? The answer: **the early-return guard at line 481-484 fires BEFORE the case 31 branch is reached**, because `length(worldPos) = 0 < 0.001`. Mode 31 IS in the bypass list, but the bypass only protects modes 20/21/22/30/31 — and the test runs in default mode 0 (not 31). The diagnostic says mode 31 was tested, so it must have been via `HLVM_PT_DEBUG_MODE=31` env var. With mode 31 set:
- `debugModeEarly = 31` → bypassEarlyReturn = true
- Skip early-return
- Enter the main logic (SPP loop etc.)
- Eventually reach case 31 in the switch → write blue

But if mode 31 dump is all-zero (not blue), then the case 31 branch is NOT being reached. **The main logic is failing silently.** The most likely reason: the main logic uses `worldPos`/`normal`/`diffuse` (lines 462-464) which are SRV reads. These return zero. The main logic computes `primaryDirect = 0` (light direction is correct but tMin/tMax are wrong because worldPos is zero). The SPP loop runs but `rayDir = sampleHemisphereCosine(normal, ...)` with normal=zero produces NaN. The result `indirect` is NaN. The `if (any(isnan(result))) result = (10, 0, 0)` safety clamp at line 583-584 SHOULD fire, but maybe doesn't because the NaN check is per-component.

**The root cause is clear**: `GBufferWorldPos[pixel].rgb` returns zero for every pixel in the GI shader's SRV read. The cbuffer, RT structures, and Output UAV are working. Only the SRV reads of GBufferWorldPos/Normal/Material are returning zero.

**This points to the binding layout being correct, the descriptor being created with the right image, but the image being unreadable at shader execution time.** The most likely explanation: the image is in `COLOR_ATTACHMENT_OPTIMAL` when the shader tries to read it, not in `SHADER_READ_ONLY_OPTIMAL`. The barrier at line 668 fires AFTER the descriptor was updated (which records the EXPECTED layout as SHADER_READ_ONLY_OPTIMAL), but the descriptor's imageView might still point to an image in COLOR_ATTACHMENT_OPTIMAL until the barrier lands.

Actually, here's the real mechanism: `vkUpdateDescriptorSets` writes the imageView + layout into the descriptor set. The imageView is a reference to the VkImage. The layout in the descriptor is a HINT for the GPU about the expected layout. The GPU doesn't enforce this hint — it just uses it to optimize caching. If the image is in COLOR_ATTACHMENT_OPTIMAL but the descriptor says SHADER_READ_ONLY_OPTIMAL, the GPU may return zero (treat as uninitialized) or garbage.

**The fix: ensure the barrier fires BEFORE createBindingSet.** That way the descriptor is created with the image already in SHADER_READ_ONLY_OPTIMAL, and the GPU sees a consistent state.

## Concrete patch

```cpp
// BEFORE (current FGIPass.cpp:543-668):
WriteConstants(CmdList, Desc);                            // line 543
CmdList->setTextureState(Desc.OutputTexture, ...);         // line 545
CmdList->setTextureState(Desc.GBufferWorldPos, ...);       // line 547-549
CmdList->setTextureState(Desc.GBufferNormal, ...);         // line 551-553
CmdList->setTextureState(Desc.GBufferMaterial, ...);       // line 553-555
FBindingSetBuilder SRVBuilder;                            // line 557
SRVBuilder.SetConstantBuffer(0, ConstantBuffer)
          .SetConstantBuffer(1, Desc.ViewConstants)
          .SetRayTracingAccelStruct(0, Desc.SceneTLAS)
          .SetTextureSRV(1, Desc.GBufferWorldPos)
          .SetTextureSRV(2, Desc.GBufferNormal)
          .SetTextureSRV(3, Desc.GBufferMaterial);
// ... more Set* calls ...
nvrhi::BindingSetDesc SRVSetDesc = SRVBuilder.Build();    // line 581
// ... dump binding layout ...
nvrhi::BindingSetHandle SRVBindingSet = Device->createBindingSet(SRVSetDesc, BindingLayout);  // line 602-603
// ... build UAV binding set ...
// ... v131 commitBarriers at line 668 ...
CmdList->commitBarriers();
RTPipeline.DispatchRays(...);

// AFTER (v135):
WriteConstants(CmdList, Desc);                            // unchanged
CmdList->setTextureState(Desc.OutputTexture, ...);         // unchanged
CmdList->setTextureState(Desc.GBufferWorldPos, ...);       // unchanged
CmdList->setTextureState(Desc.GBufferNormal, ...);         // unchanged
CmdList->setTextureState(Desc.GBufferMaterial, ...);       // unchanged

// v135: commit barriers BEFORE createBindingSet. This ensures the GBuffer
// textures are physically in SHADER_READ_ONLY_OPTIMAL when the descriptor
// is created. Without this, the descriptor's vkUpdateDescriptorSets call
// captures the image in its previous layout (COLOR_ATTACHMENT_OPTIMAL
// from the GBuffer raster pass), causing the GPU to read zero/garbage
// at dispatch time.
CmdList->commitBarriers();

FBindingSetBuilder SRVBuilder;                            // unchanged
// ... unchanged SRVBuilder chain ...
nvrhi::BindingSetDesc SRVSetDesc = SRVBuilder.Build();    // unchanged
// ... unchanged ...
nvrhi::BindingSetHandle SRVBindingSet = Device->createBindingSet(SRVSetDesc, BindingLayout);  // unchanged
// ... unchanged ...
// v131 commitBarriers at line 668: KEPT for defense-in-depth
CmdList->commitBarriers();
RTPipeline.DispatchRays(...);
```

**Diff size**: +3 lines (one commitBarriers() call + 2-line comment), -0 lines.

## Acceptance criteria

For parent-runspace verification (after rebuild + run):

1. Build succeeds: `./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal` exits 0.
2. The dump `gi_raw_frame*.png` for `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=0` shows non-zero GBuffer-derived pixels (mode 1 albedo, mode 2 normal, mode 3 direct lighting).
3. `HLVM_PT_DEBUG_MODE=20` (GBufferMaterial SRV read) returns non-zero per-channel values.
4. `HLVM_PT_DEBUG_MODE=21` (GBufferNormal SRV read) returns non-zero per-channel values.
5. `HLVM_PT_DEBUG_MODE=22` (GBufferWorldPos SRV read) returns non-zero per-channel values (expected: R/G/B spanning the worldpos range like [-15, 15]).
6. `validate_restir_gi.py` passes the newest dump group (>= 4 checks: black-pixel ratio, color variance, temporal stability, cell variance).
7. Fresh display image (vision) shows recognizable Sponza geometry with sane exposure (not pure black, not pure white).
8. No Vulkan VUID/ERROR in the test log when validation layer is enabled (v132+v133+v134 patches).
9. No command-list errors in the test log.
10. v131 commitBarriers() at FGIPass.cpp:668 is INTACT (defense-in-depth).

## Honesty floor

This patch is well-grounded static analysis based on the nvrhi-deferred-barrier-ordering pattern from the skill's gotchas doc. The patch DOES NOT guarantee a fix — there are still multiple hypotheses (slangc dead-strip, pipeline cache staleness, imageView identity issue, validation layer needed). If v135 doesn't fix the bug, the next iteration (v136) would:

- Add a debug mode that does `GBufferMaterial.Load(int3(0,0,0))` constant at the very start of RayGen (before any conditional). If even mode 32 (constant sentinel) returns zero, the SRV binding is fundamentally broken.
- Enable Vulkan validation layer explicitly via env var `VK_LAYER_KHRONOS_validation=1` and grep for VUIDs in the log.
- Run `spirv-dis GIPathTracing.spv` and check that the Texture2D loads are present in the SPIR-V.
- Add `cmdList->setTextureState` calls for the SRV textures INSIDE the binding set creation flow (defense-in-depth).

## What this plan does NOT do

- Does not modify the test code (no TestReSTIR_GI_Temporal.cpp edits).
- Does not modify the shader (no GIPathTracing.hlsl edits).
- Does not modify DeviceManagerVk* (no validation layer enable).
- Does not touch the cmake/nvrhi fork.
- Does not commit, push, or modify git topology.
- Does not modify governance files.

## What this plan DOES do

- Adds 3 lines to `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp`: one `CmdList->commitBarriers();` call + a 2-line comment explaining why the location matters.
- Keeps the existing commitBarriers at line 668 (defense-in-depth).
- All v131 + v132 + v133 + v134 patches remain intact.

## Next-step for parent runspace

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
# 1. Rebuild
./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal --Config=Debug
# 2. Run with debug mode 20 to verify GBufferMaterial SRV reads work
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20 ./Binary/Debug/TestReSTIR_GI_Temporal
# 3. Check gi_raw dump
python3 -c "
from PIL import Image
import numpy as np
im = np.array(Image.open('Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/' + sorted(__import__('os').listdir('Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps'))[-1].replace('gbuffer_', 'gi_raw_' if 'gi_raw' not in _ else '').replace('display_', 'gi_raw_').replace('spatial_', 'gi_raw_').replace('denoised_', 'gi_raw_')))
print(f'gi_raw: per-channel mean={im.mean(axis=(0,1))} std={im.std(axis=(0,1))}')
"
# 4. If mode 20 returns non-zero, the SRV fix worked. Run validate_restir_gi.py.
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
```