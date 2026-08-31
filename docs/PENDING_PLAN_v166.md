# Pending Plan v166
- task: TestReSTIR_GI_Temporal VUID-vkCmdTraceRaysKHR-None-08608 fix (nvrhi fork pDynamicStates patch)
- source: `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log` (262 lines, 2026-08-11 23:57:44) — 8 VUID errors → `Engine/Source/Runtime/Build/Debug/_deps/nvrhi-src/src/vulkan/vulkan-raytracing.cpp:1643-1649` (RT pipeline create-info missing `pDynamicStates`)
- approach: Patch the nvrhi fork's `vulkan-raytracing.cpp` to add `vk::PipelineDynamicStateCreateInfo` with `{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR}` to the RT pipeline creation. This mirrors the graphics pipeline builder in `vulkan-graphics.cpp:316-318` which already declares these dynamic states. Apply the same patch to the Debug and Release copies of the nvrhi fork so future configs match. After the patch, the operator must (a) re-run CMake configure on `Build/Debug` because the nvrhi fork is a FetchContent artifact in `_deps/`, (b) rebuild with `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild`, (c) re-run the binary with `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8`, and (d) `validate_restir_gi.py` on the new dump group. Acceptance is the 8 VUID errors disappear from the log and all 7 acceptance criteria from PICK card 6 pass.
- diff_estimate: +14 / -0 lines at `vulkan-raytracing.cpp:1643-1649` (array decl + dynamic state info + `setPDynamicState`); the release copy gets the same patch; no source-side behavioral changes — purely additive.
- skip_plan_review: no
- test_strategy: Operator-side terminal-blocked; the next role after impler is the reviewer who must re-read the patched nvrhi fork on disk AND the new `TestReSTIR_GI_Temporal.log` (post-rebuild-and-run) to confirm VUID gone. The tester role must run `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` against the new dump group AND grep the new log for `VUID` (expect 0 hits). The testing-verifier upgrades v161..v165 SOME_RELAX → ALL_KEEP once those criteria pass.
- risks:
  1. **Terminal blocked by tirith in cron runspace** — the file-only patch + diff verification can be done by the cron, but the rebuild + run + validate + vision-check MUST be done by the operator. The plan documents this in the verify step and the impler must write the verify recipe into PENDING_COMMIT_v166.md.
  2. **The `_deps/` directory is git-ignored** — patching it doesn't survive a clean `git clean -fdx` or a fresh FetchContent clone. The plan DOES NOT propose committing the patch; the operator's recipe must include re-applying it after any clean. This is a known limitation; not a blocker.
  3. **Multiple copies of nvrhi fork** — there are 3 copies (`Build/Debug/_deps/`, `Build/Release/_deps/`, `Build/RelWithDebInfo/_deps/`). The patch must be applied to all 3 to keep parity. The plan covers all 3.
  4. **The fix may not be the actual root cause** — VUID-vkCmdTraceRaysKHR-None-08608 says "pipeline doesn't set up dynamic state but caller uses dynamic state commands." If the caller (nvrhi's RT command-list state tracking at `setRayTracingState`) is incorrectly emitting `setViewport`/`setScissor` BEFORE the RT dispatch WITHOUT setting up dynamic states, the fix is to add the dynamic states. But the alternative is to NOT emit them. The plan picks the additive fix (option 1) because (a) `vulkan-graphics.cpp` already does this for graphics pipelines, (b) the nvrhi fork is local and patchable, (c) the alternative (path 2 in tick720 audit) changes user code which is fragile.
  5. **CMake FetchContent re-fetches** — if `./Build.sh --Rebuild` triggers a CMake re-configure, the FetchContent may re-clone the nvrhi fork and overwrite the patch. Need to verify the nvrhi fork source is fetched once and cached (typical FetchContent behavior). The plan's acceptance includes `stat -c '%y' _deps/nvrhi-src/src/vulkan/vulkan-raytracing.cpp` to show the patch mtime is preserved.

## Concrete patch (file-only, no commit)

Before line 1643 (`auto pipelineInfo = vk::RayTracingPipelineCreateInfoKHR()`), insert:

```cpp
        std::array<vk::DynamicState, 2> dynamicStates = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor
        };
        vk::PipelineDynamicStateCreateInfo dynamicStateInfo;
        dynamicStateInfo.setDynamicStates(dynamicStates);
```

Then add `.setPDynamicState(&dynamicStateInfo)` to the `pipelineInfo` chain at line 1643-1649. Final chain:

```cpp
        auto pipelineInfo = vk::RayTracingPipelineCreateInfoKHR()
            .setStages(shaderStages)
            .setGroups(shaderGroups)
            .setLayout(pso->pipelineLayout)
            .setMaxPipelineRayRecursionDepth(desc.maxRecursionDepth)
            .setPLibraryInfo(&libraryInfo)
            .setPDynamicState(&dynamicStateInfo)
            .setPNext(pNextChain2);
```

## Operator-side recipe (must run after patch + re-fetch)

This is the only step that requires terminal. The cron runspace cannot do this.

```bash
# Step 1: Verify the patch is on disk
grep -n 'setPDynamicState' Engine/Source/Runtime/Build/Debug/_deps/nvrhi-src/src/vulkan/vulkan-raytracing.cpp
# Expected: 1 hit at line ~1650

# Step 2: Force CMake reconfigure (FetchContent may need to re-detect)
cd Engine/Source/Runtime/Build/Debug && cmake -S ../../.. -B . -DCMAKE_BUILD_TYPE=Debug && cd -

# Step 3: Rebuild
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild

# Step 4: Run with the mode-20 discriminator (also resolves the v165 card 5)
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
2. `validate_restir_gi.py` PASS on newest dump group (4/4 checks)
3. Mode-20 gi_raw per-channel mean > 0 (the v165 card 5 acceptance)
4. Fresh display image shows Sponza (vision check)
5. No CommandList errors in the new log
6. Debug target builds cleanly
7. Pixel-statistics on `20260812_*gi_raw_frame*.png` are non-uniform

## Plan Deviations (impler fills this in if it deviated)

Empty by design. The plan is a single-file patch + operator-side rebuild. Any deviation must be reported here.

## Cross-references

- PENDING_PICK.md line 10 — the new VUID card
- PIPELINE_HEALTH_2026-08-12_cycle-stop-tick720.md — the fresh on-disk evidence (8 VUIDs in current log)
- vulkan-raytracing.cpp lines 1643-1649 — the patch site
- vulkan-graphics.cpp lines 316-318 — the existing graphics-pipeline dynamic-state pattern (the template)
- DIAGNOSTIC_2026-07-30.md — authoritative current-state from the operator
- v161..v165 chain — the prior cycles that produced the cfg flag and binding-set integrity evidence
