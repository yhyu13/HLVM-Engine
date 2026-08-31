# Pending Plan v160 (NON-BINDING RECOMMENDATION — NOT a state-machine advance)

> ⚠️ **This is a recommendation for the operator runspace, NOT a state-machine advance.**
> Per `six-role-pipeline §Anti-pattern #6` (cycle-stop anti-pattern) and the v159 cycle's
> KEEP verdict, the state machine is correctly halted at v159. The v160 plan is the next
> decisive single experiment the operator runspace can run; it does not create a v160
> cycle in the state machine. Tick73 is the audit that produced this recommendation.

- task: TestReSTIR_GI_Temporal acceptance verification — propose the v131 mode-31 discriminator run as the next decisive single experiment
- source: no bundle — verification-only cycle against the existing v22, v131, v137, v140, and v151 fixes
- approach: The bisect has narrowed from 4 to 2 hypotheses (tick73 finding). The remaining 2 are: (1) slangc dead-strip on the GIPathTracing.hlsl debug-mode switch, vs (2) image layout transition wrong / binding works but `.Load()` returns zero. The v131 mode-31 discriminator in the source at `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl:716-733` (data-dir copy at `Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl:716-735`) discriminates between them in a single operator-side run. **Mode 31 is the right experiment; the v159 audit's proposed "OpSwitch liveness via spirv-cross" path is ruled out by sblob format inspection (NVSP packed, not raw .spv).** The v160 plan proposes running mode 31 (and mode 20 for direct comparison) on the v137+v140+v151-linked binary in a parent runspace with terminal+vision+python3+numpy.
- diff_estimate: +0 / -0 production lines for verification; the discriminator is already in the source. Any evidence-driven fix requires a subsequent reviewed plan.
- skip_plan_review: yes — this is a single-experiment proposal with a known discriminator, no design to critique; per `six-role-pipeline §Anti-pattern #6` (cycle-stop anti-pattern) the plan-critique step is overhead for a one-line experiment.
- test_strategy: Build `TestReSTIR_GI_Temporal` in Debug; run the non-bypass target with `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8`; then run with `HLVM_PT_DEBUG_MODE=31` (the v131 discriminator); then run with `HLVM_PT_DEBUG_MODE=20` (for direct comparison with the 2026-07-30 finding); inspect the 3 newest dump groups with numpy per-pixel/channel statistics; vision-check the display PNG; require mode 31 to discriminate unambiguously: non-uniform color → binding works AND slangc keeps the read (hypothesis #1, #2 both ruled out, bug is downstream); uniform blue (0,0,1) → SRV read alive but value is zero (hypothesis #2 confirmed: layout/descriptor issue, not slangc); uniform gray (0.5, 0.5, 0.5) → slangc dead-stripped the case entirely (hypothesis #1 confirmed).
- risks: This scheduled runspace's terminal request is blocked again this tick with `status: pending_approval` / `pattern_key: tirith:unknown` for `git status; pwd; ls; python3; echo; date; wc -c; xxd; spirv-cross; rg` and all earlier probes. The v160 evidence channel is a single runtime probe that requires terminal+vision+python3+numpy. None of this is addressable from a file-only runspace. Do not modify production code, mark the PICK card complete, or claim discriminator verdict without fresh execution evidence.
- bisect_narrowing: tick73 confirmed that hypothesis #3 (nvrhi silently dropping the second binding set) is FALSIFIED at the binding-set level (v23-diag dump in 17:30 log shows 11/11 binding-set items match 11/11 layout items, all handles valid, 8/8 frames). Hypothesis #4 (texture handles mismatched) was FALSIFIED at v158. Remaining: #1 (slangc dead-strip) and #2 (image layout). Mode 31 discriminates them.

## What this plan does NOT do

- Does NOT advance the state machine from v159. The state machine is correctly halted.
- Does NOT modify any source file. The discriminator is already in the source.
- Does NOT close PICK card 3. Card 3 requires `validate_restir_gi.py 4/4` + vision check + mode-31 verdict, all of which require terminal+vision+python3+numpy in an operator runspace.
- Does NOT mark any cycle complete. The v159 cycle is already complete with SOME_RELAX.

## Operator recipe (v160, refined this tick)

```bash
# 1. Build (rebuilds the v137+v140+v151-linked binary with the v131 mode-31 discriminator)
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test

# 2. Run the non-bypass baseline (mode 0; gi_raw goes through path trace, denoised, display)
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 \
  ./Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal

# 3. Run the mode-31 discriminator (v131 Candidate A probe)
HLVM_PT_DEBUG_MODE=31 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 \
  ./Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal

# 4. Run mode 20 for direct comparison with the 2026-07-30 finding
HLVM_PT_DEBUG_MODE=20 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 \
  ./Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal

# 5. Inspect the 3 newest dump groups with numpy per-pixel/channel statistics
python3 -c "
import numpy as np
from PIL import Image
import glob
for path in sorted(glob.glob('Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/2026*_gi_raw_frame8.png'))[-3:]:
    img = np.array(Image.open(path))
    print(f'{path}: shape={img.shape} mean={img.mean(axis=(0,1))} std={img.std(axis=(0,1))}')
    print(f'  unique values per channel: R={len(np.unique(img[..., 0]))} G={len(np.unique(img[..., 1]))} B={len(np.unique(img[..., 2]))}')
"

# 6. Validator on the newest non-bypass dump group
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py

# 7. Vision-check the display PNG for recognizable Sponza
# (vision_analyze or human eyeball)

# 8. Verdict mapping:
#    mode 31 non-uniform color  → binding works, slangc keeps read (hypotheses #1, #2 ruled out; bug downstream of debug switch)
#    mode 31 uniform blue       → SRV read alive but zero (hypothesis #2 confirmed: layout/descriptor)
#    mode 31 uniform gray       → slangc dead-stripped the case (hypothesis #1 confirmed)
```

## Why this is the right next experiment

The v159 audit proposed `spirv-cross --reflect GIPathTracing.spv | grep OpSwitch` as v160's evidence channel. Tick73's sblob format inspection ruled that out (the sblob is NVSP packed, not raw SPIR-V; 0 matches for SPIR-V magic / OpSwitch / OpSelectionMerge / `case` in the sblob via `search_files`). The v131 mode-31 discriminator was added to the source on 2026-07-30 as Candidate A but never run; it is functionally equivalent to the spirv-cross check but is a runtime probe (works on the actual linked binary) rather than a static binary inspection. The discriminator is the right experiment because:

1. **It's already in the source** — no new code needed.
2. **It's a runtime probe** — exercises the actual slangc compilation, nvrhi binding, and Vulkan dispatch chain, not a static binary inspection.
3. **It discriminates in one run** — three-color verdict (non-uniform / blue / gray) maps directly to the two remaining hypotheses plus the no-bug-found branch.
4. **It's faster than spirv-cross** — single dispatch, no extra toolchain dependency.

## Verdict

If the operator runspace runs mode 31 and gets:
- **non-uniform color** → the discriminator says binding works; the bisect moves downstream (path-trace math, payload handling, or post-TraceRay logic). The next experiment is a different debug mode (e.g., mode 13 for `RTInstanceInfo[0].AlbedoColor` SRV sanity read, which already returns 0.85 per tick72).
- **uniform blue** → the discriminator says the SRV read is alive but the value is zero; the bug is in image layout transition or descriptor indexing. The fix is a 1-line change in `FGIPass::DispatchRays` to add an explicit `cmdList->setTextureState(GBufferMaterial, nvrhi::ResourceStates::ShaderResource)` before the dispatch. The 2026-07-30 diagnostic's hypothesis #2 path.
- **uniform gray** → the discriminator says slangc dead-stripped the case; the bug is in the slangc invocation or the HLSL switch structure. The fix is to either (a) replace the switch with an if-else chain (slangc doesn't dead-strip if-else), (b) add `[[dont_strip]]` equivalents (HLSL doesn't have this; slangc-specific attribute), or (c) verify the slangc invocation passes `-no-dead-strip` or equivalent. The 2026-07-30 diagnostic's hypothesis #1 path.
