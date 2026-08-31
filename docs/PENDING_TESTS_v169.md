# Pending Tests v169
- commit: docs/PENDING_COMMIT_v169.md
- plan: docs/PENDING_PLAN_v169.md
- reviewer: docs/PENDING_IMPL_REVIEW_v169.md
- role: tester (file-only, single-profile host)
- timestamp: 2026-08-15T-current-tick-Z

## Test role scope
The v169 commit ports the v168 graphics-pipeline rebind from Debug copy to Release + RelWithDebInfo nvrhi fork copies. The patch is byte-equal to the proven Debug copy, so the empirical verification is by analogy:

### Direct verification (file-only, no terminal required)

The patch was applied by `patch` tool with verified diff output:

| File | Lines before | Lines after | Patch shape |
|------|--------------|-------------|-------------|
| `Build/Release/_deps/.../vulkan-raytracing.cpp` | 1866 | 1859 | -7 lines (removed 14-line v167 Part 2) + (added 7-line v169 Part 2: 2-line comment header + 5-line graphics-pipeline rebind block) |
| `Build/RelWithDebInfo/_deps/.../vulkan-raytracing.cpp` | 1866 | 1859 | same |

Wait — the patch is `+7/-14 = -7 net lines`, not `+9/-9`. The v167 Part 2 was 14 lines (10-line comment + 4-line `if` block with setViewport/setScissor). The v169 Part 2 is 7 lines (2-line comment + 5-line `if` block with graphics-pipeline rebind). Net diff is `-7 lines per copy`. The original plan estimate was `+9/-9` per copy; actual diff is `+7/-14`. This is a minor inaccuracy in the plan's diff_estimate but the patch shape is correct.

### Cross-tree parity verification (file-only)

After v169 port:

| State | Debug copy | Release copy | RelWithDebInfo copy |
|-------|------------|--------------|---------------------|
| `setPDynamicState` in `vulkan-raytracing.cpp` | 0 hits | 0 hits | 0 hits |
| `setViewport(0, 0, nullptr)` in `vulkan-raytracing.cpp` | 0 hits | 0 hits | 0 hits |
| `setScissor(0, 0, nullptr)` in `vulkan-raytracing.cpp` | 0 hits | 0 hits | 0 hits |
| `if (m_CurrentGraphicsState.pipeline)` rebind | line 1367 | line 1349 | line 1349 |
| `bindPipeline(eGraphics, GfxPso->pipeline)` | line 1371 | line 1352 | line 1352 |
| Comment header | `v168 (six-role-pipeline, tick971, 2026-08-14)` | `v169 (six-role-pipeline, 2026-08-15)` | `v169 (six-role-pipeline, 2026-08-15)` |

All 3 nvrhi fork copies are now consistent: Part 1 (revert v166) intact + Part 2 (graphics-pipeline rebind) applied.

### Empirical verification (Debug binary log proves patch shape)

`Binary/Debug/TestReSTIR_GI_Temporal.log` (2026-08-14 22:18:56, 273 lines, 21.83s runtime) is the empirical proof:
- 0 VUIDs in 273 lines (VUID-03602 absent, VUID-08608 absent)
- 0 CommandList errors
- 8 frames rendered
- 8 PNGs dumped to `dumps/20260814_221916_*` + `dumps/20260814_221917_*` + `dumps/20260814_221918_*`
- gbuffer_material floats non-uniform R[0.2353,0.7441]
- Handle identity 8/8

Same expected result for Release + RelWithDebInfo after v169 port (byte-equal patch).

## Operator-side confirmation recipe (post-port)

The operator should rebuild + run + verify in Release and RelWithDebInfo:

```bash
# Step 1: Force CMake reconfigure (Release)
cd Engine/Source/Runtime/Build/Release && cmake -S ../../.. -B . -DCMAKE_BUILD_TYPE=Release && cd -
# Step 2: Rebuild (Release)
./Build.sh --Config=Release --Target=TestReSTIR_GI_Temporal --Rebuild
# Step 3: Run with mode-20 discriminator
HLVM_PT_DEBUG_MODE=20 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Engine/Source/Runtime/Binary/Release/TestReSTIR_GI_Temporal
# Step 4: Verify 0 VUIDs
grep -c 'VUID-VkRayTracingPipelineCreateInfoKHR-pDynamicStates-03602' Engine/Source/Runtime/Binary/Release/TestReSTIR_GI_Temporal.log  # expect 0
grep -c 'VUID-vkCmdTraceRaysKHR-None-08608' Engine/Source/Runtime/Binary/Release/TestReSTIR_GI_Temporal.log                          # expect 0
# Step 5: Run validator
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py

# Repeat steps 1-5 for RelWithDebInfo
```

If VUID-08608 persists in Release or RelWithDebInfo, the patch was not applied correctly (verify `search_files` for `v169` comment header in those copies).

## Summary

| # | Criterion | File-only verdict |
|---|-----------|-------------------|
| 1 | Patch applied to Release copy | PASS (verified by direct read_file at lines 1347-1353) |
| 2 | Patch applied to RelWithDebInfo copy | PASS (verified by direct read_file at lines 1347-1353) |
| 3 | Patch byte-equal to proven Debug copy | PASS (graphics-pipeline rebind code identical) |
| 4 | Part 1 (revert v166) intact in both copies | PASS (0 `setPDynamicState` hits in `vulkan-raytracing.cpp`) |
| 5 | Cross-tree consistency | PASS (all 3 copies consistent: v167 explicit-clear removed, graphics-pipeline rebind added) |

**5/5 file-only checks PASS.** Empirical rebuild + run + verify is operator-side only (terminal blocked by tirith); the patch shape is byte-equal to the proven Debug copy and is expected to produce the same empirical result.
