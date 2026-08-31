# Pending Plan v1 — TestReSTIR_GI_Temporal GBuffer SRV binding fix

- task: GBuffer SRV binding fix (TestReSTIR_GI_Temporal)
- source: this pipeline session + test source at `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp`
- approach: (1) Read the GBuffer raster pass bindings + the GI/denoise/ReSTIR SRV bindings that read the GBuffer outputs back, identify the binding-index desync (the well-known `b/t/s/u` shift + `VulkanBindingOffsets.constantBufferOffset=256` default trap from the devops skill's C++ gotchas). (2) Patch the binding builder calls to use `Add*` (not `Set*`) and to call `BLB.SetBindingOffsets(zeroes)` so that GLSL binding 0 maps to Vulkan binding 0. (3) Add an `HLVM_PT_DEBUG_MODE` debug branch that dumps `GBufferMaterial` so criterion 7 is testable. (4) Pause for HUMAN_REQUIRED build/run/validation — file-only cron cannot run the binary.
- diff_estimate: +30 / -10 lines (one binding-layout builder, one debug-mode branch, possibly one binding-set wiring fix)
- skip_plan_review: no — binding offsets + debug mode are not trivial; plan-critique is worth the round
- test_strategy: tester role writes `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` (currently missing; 4-check structural validator per devops skill §"4-check structural validator > scalar mean-luma gate"); HUMAN_REQUIRED to actually run it
- risks:
  - `docs/DIAGNOSTIC_2026-07-30.md` is the user-claimed authoritative current-state but does not exist on disk. The plan is therefore grounded in the test source itself, not the retrospective. This may mis-identify the exact bug.
  - `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` is referenced in the test header (lines 41–42) but does not exist on disk. Tester role must create it; this adds scope.
  - `HLVM_PT_DEBUG_MODE=20` is not enumerated in the devops skill's debug-mode table (modes 1, 2, 3, 4, 9, 10, 12, 13, 14 are documented). The criterion may need to be re-stated as "an existing debug mode" once the planner reviews the actual `GIPathTracing.hlsl` source.
  - File-only cron mode: tester and testing-verifier roles cannot run the binary, cannot run the validator, cannot vision-inspect dumps. Their verdicts are structurally limited to code-level analysis. The acceptance gates that require fresh runtime evidence must be MANUAL_REQUIRED.

## Code anchors (verified against source at session start)

From `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp`:

- Line 265: `BindingCache.SetDevice(NvrhiDevice);` — binding cache initialization.
- Line 818: `BindingCache.Clear();` on resize — must keep cache coherent.
- Lines 670–677: `GBufferMaterial = nullptr; ... PrevGBufferMaterial = nullptr; PrevLinearDepth = nullptr;` — member declarations.
- Lines 847–872: GBuffer raster pass dispatches `Desc.GBufferNormal`, `Desc.GBufferMaterial`, `Desc.LinearSampler` to the raster pass. **This is where the GBuffer MRTs are produced.**
- Lines 980–985: GI dispatch desc wires `Gd.SampleInfoTexture = SampleInfoTexture;` (v210: x2 normal + pdf), `Gd.MaterialTexture = GBufferMaterial;` (v210: albedo for f), `Gd.WorldPosTexture = GBufferWorldPos;` — **this is the consumer side that needs the SRV.**

## Hypothesis (planner's best read of the symptom)

The devops skill's C++ gotchas list three binding-class bugs that match "downstream compute passes reading sentinel/empty values":

1. **`FBindingLayoutBuilder` uses `Add*`, not `Set*`** (devops skill §"`FBindingLayoutBuilder` is `Add*`, NOT `Set*`"). A new binding layout that uses `Set*` would fail at compile time, so this is unlikely in existing working code but possible in a recently-added GBuffer SRV binding set.
2. **`VulkanBindingOffsets.constantBufferOffset` defaults to 256** (devops skill §"`VulkanBindingOffsets.constantBufferOffset` defaults to **256** — override it"). If the binding layout was built without `SetBindingOffsets({0,0,0,0})`, then cbuffer b0 binds at Vulkan binding 256 while the shader expects binding 0 → the GI/denoise/ReSTIR CS shaders get wrong descriptors, often visibly as "all-zero" reads.
3. **Sentinel-then-overwrite footgun** (devops skill §"Sentinels-then-overwrite"): the raster pass writes sentinels before the visible raster, and a layout transition race can cause the SRV to read the sentinel instead of the raster output. The fix in that incident was closing+executing the sentinel command list and `waitForIdle()` before opening the raster list.

The most likely culprit for "SRV reads sentinel/empty values" is **#2** (binding offset default) combined with **#3** (transition race) — the raster pass's `WriteGBufferSentinels` pattern (if still present) interacting with the GI compute's `setComputeState` would produce exactly the symptom described.

## Plan steps (for the impler)

1. **Audit** `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` for the GBuffer raster pass's binding-layout builder. Verify `FBindingLayoutBuilder` calls use `AddTextureSRV(N)`, `AddConstantBuffer(N)`, etc., NOT `SetTextureSRV(N)` etc. If `Set*` verbs appear, rename to `Add*`. (`grep -nE "BLB\.Set(ConstantBuffer|TextureSRV|TextureUAV|Sampler)"` — but the file-only cron cannot grep; this is MANUAL_REQUIRED.)
2. **Audit** every place that constructs a `VulkanBindingOffsets` for the GBuffer pipeline. If any layout is built without `BLB.SetBindingOffsets({0,0,0,0})`, add the call. The expected pattern is:

   ```cpp
   nvrhi::VulkanBindingOffsets Offsets;
   Offsets.constantBufferOffset           = 0;
   Offsets.shaderResourceOffset           = 0;
   Offsets.samplerOffset                  = 0;
   Offsets.unorderedAccessViewOffset      = 0;
   FBindingLayoutBuilder BLB;
   BLB.AddConstantBuffer(0).AddTextureSRV(0)....;
   BLB.SetBindingOffsets(Offsets);
   ```

3. **Audit** the sentinel-upload pattern (if any) in the GBuffer raster pass. If a CPU-upload of sentinel values precedes the raster pass, ensure the upload command list is **closed + executed + waitForIdle** before the raster command list opens. The devops skill's `Sentinels-then-overwrite footgun` section documents this exact pattern.
4. **Add `HLVM_PT_DEBUG_MODE=20` branch** to `GIPathTracing.hlsl` (or rename criterion 7 to a debug mode the shader already implements — check the shader source for the actual enumerated modes; the devops skill's table is illustrative, not authoritative for this specific test). Either way, the debug branch should write the sampled `GBufferMaterial` value to the output for verification.
5. **Update `TestReSTIR_GI_Temporal.cpp` header comment** (lines 28–32) to document the new debug mode behavior so future runs know what to expect.

## Verification (MANUAL_REQUIRED — file-only cron cannot run)

The file-only cron cannot perform ANY of the verification commands below. Each is a HUMAN_REQUIRED gate after the impler commits.

```bash
# 1. Build
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal

# 2. Run with dumps
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 \
  ./Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal

# 3. Inspect log for Vulkan validation errors / command-list errors
grep -E "VUID|ERROR|validation" <logfile>

# 4. Run validator on newest dump group (validator must be created by tester)
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py \
  --dump-dir <latest-dump-dir>

# 5. Vision-inspect the display dump
#    (manual; the user opens the PNG in their image viewer)
```

## Plan Deviations

None yet (impler has not run).
