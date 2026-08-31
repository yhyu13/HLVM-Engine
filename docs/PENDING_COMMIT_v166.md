# Pending Commit v166
- plan: docs/PENDING_PLAN_v166.md
- files: Engine/Source/Runtime/Build/Debug/_deps/nvrhi-src/src/vulkan/vulkan-raytracing.cpp, Engine/Source/Runtime/Build/Release/_deps/nvrhi-src/src/vulkan/vulkan-raytracing.cpp, Engine/Source/Runtime/Build/RelWithDebInfo/_deps/nvrhi-src/src/vulkan/vulkan-raytracing.cpp
- source: PENDING_PLAN_v166.md (file-only patch design)
- target: (no commit — these are git-ignored FetchContent artifacts; the patch lives in `_deps/`)
- task: Add VK_DYNAMIC_STATE_VIEWPORT + VK_DYNAMIC_STATE_SCISSOR to the nvrhi RT pipeline create-info so VUID-vkCmdTraceRaysKHR-None-08608 stops firing on every dispatched frame.
- verify: `grep -c VUID Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log` (after operator-side rebuild + run) — expected 0 (was 8 pre-fix)
- skip_impl_review: no
- produces_test_files: no
- notes:
  - The 3 files patched are git-ignored (`Build/*/_deps/` is FetchContent output). The patch DOES NOT survive a clean `git clean -fdx` or `cmake --fresh`. The operator must re-apply after any clean.
  - The patch DOES NOT include a source-side commit; the operator's workflow is to leave the patch in `_deps/` and rebuild as needed.
  - The patch is identical across the 3 copies (Debug/Release/RelWithDebInfo) because they were all cloned from the same nvrhi fork at the same commit.
  - The patch follows the textbook nvrhi fork pattern: mirror the graphics pipeline's dynamic-state setup (vulkan-graphics.cpp:316-318) in the RT pipeline builder.
  - The patch DOES NOT touch the user code (FRayTracingPipeline.cpp, TestReSTIR_GI_Temporal.cpp, GIPathTracing.hlsl) — the caller setup is correct, the only missing thing is the dynamic-state declaration on the RT pipeline itself.
  - The patch DOES NOT touch ShaderMake.cfg, Runtime_cmake.py, or any other governance file.

## Plan Deviations

None. The impler applied the patch EXACTLY as planned (textbook `setPDynamicState` pattern, 3 copies of nvrhi fork, no other files touched). The patch was applied file-only; no commit, no push, no governance file modified.

## Concrete diff (per file)

```diff
--- a/vulkan-raytracing.cpp
+++ b/vulkan-raytracing.cpp
@@ -1640,12 +1640,28 @@
             pNextChain2 = &pipelineFlags2;
         }

+        // HLVM VUID-vkCmdTraceRaysKHR-None-08608 fix (v166):
+        // The RT pipeline must declare VK_DYNAMIC_STATE_VIEWPORT and
+        // VK_DYNAMIC_STATE_SCISSOR as dynamic states, because nvrhi's
+        // RT state tracking emits setViewport/setScissor before
+        // vkCmdTraceRaysKHR. Without this, the Vulkan validation layer
+        // fires VUID-vkCmdTraceRaysKHR-None-08608 on every dispatched
+        // frame. Mirrors the graphics pipeline pattern in
+        // vulkan-graphics.cpp:316-318.
+        std::array<vk::DynamicState, 2> dynamicStates = {
+            vk::DynamicState::eViewport,
+            vk::DynamicState::eScissor
+        };
+        vk::PipelineDynamicStateCreateInfo dynamicStateInfo;
+        dynamicStateInfo.setDynamicStates(dynamicStates);
+
         auto pipelineInfo = vk::RayTracingPipelineCreateInfoKHR()
             .setStages(shaderStages)
             .setGroups(shaderGroups)
             .setLayout(pso->pipelineLayout)
             .setMaxPipelineRayRecursionDepth(desc.maxRecursionDepth)
             .setPLibraryInfo(&libraryInfo)
+            .setPDynamicState(&dynamicStateInfo)
             .setPNext(pNextChain2);
```

+16 lines, -0 lines per file. 3 files → 48 lines total added (small, additive, targeted).

## Operator-side recipe (must run after this commit)

```bash
# Step 1: Verify the patch is on disk (sanity check)
grep -n 'setPDynamicState' Engine/Source/Runtime/Build/Debug/_deps/nvrhi-src/src/vulkan/vulkan-raytracing.cpp
# Expected: 1 hit at line ~1650

# Step 2: Force CMake reconfigure (FetchContent may need to re-detect)
cd Engine/Source/Runtime/Build/Debug && cmake -S ../../.. -B . -DCMAKE_BUILD_TYPE=Debug && cd -

# Step 3: Rebuild
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild

# Step 4: Run with the mode-20 discriminator (also resolves PICK card 5)
HLVM_PT_DEBUG_MODE=20 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 \
  ./Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal

# Step 5: Validate
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py

# Step 6: Grep VUIDs in the new log
grep -c VUID Binary/Debug/TestReSTIR_GI_Temporal.log
# Expected: 0 (was 8 pre-fix)

# Step 7: Numpy-check mode-20 gi_raw for non-zero GBufferMaterial
python3 -c "
import numpy as np
from PIL import Image
import glob
files = sorted(glob.glob('Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/*gi_raw_frame*.png'))
img = np.array(Image.open(files[-1]))
print('Last dump:', files[-1])
print('Per-channel mean:', img[..., :3].mean(axis=(0,1)))
print('Per-channel std:', img[..., :3].std(axis=(0,1)))
print('Non-zero ratio:', (img[..., :3] > 0).any(axis=-1).mean())
"
```

## Acceptance criteria (re-stated from PICK card 6)

1. `grep -c VUID Binary/Debug/TestReSTIR_GI_Temporal.log` returns 0 (was 8)
2. `validate_restir_gi.py` 4/4 checks PASS on newest dump group
3. Mode-20 gi_raw per-channel mean > 0 (GBufferMaterial non-zero)
4. Fresh display image shows Sponza (vision check)
5. No CommandList errors in the new log
6. Debug target builds cleanly
7. Pixel-statistics on `20260812_*gi_raw_frame*.png` are non-uniform

## Cross-references

- PENDING_PLAN_v166.md (the plan)
- PENDING_PLAN_REVIEW_v166.md (KEEP verdict)
- PENDING_PICK.md line 10 (the card)
- vulkan-raytracing.cpp lines 1643-1666 (patched, with `setPDynamicState` at line 1657)
- vulkan-graphics.cpp lines 316-318 (the template pattern)
- DIAGNOSTIC_2026-07-30.md (authoritative current-state)
- tick720 PIPELINE_HEALTH — the discovery that the current log has 8 VUIDs
