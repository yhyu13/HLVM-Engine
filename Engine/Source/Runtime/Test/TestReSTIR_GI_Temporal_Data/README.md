# TestReSTIR_GI_Temporal

End-to-end verification of the ReSTIR GI pipeline on the Sponza scene:

```
glTF Sponza (Samples/Assets/sponza/Sponza01.gltf)
   ↓
GBuffer pass → WorldPos / Normal / Material
   ↓
GI ray tracing (FGIPass w/ shared GIPathTracing.hlsl — 64-byte payload)
   ↓
Bilateral Denoise → ReSTIR Generate → ReSTIR Temporal
   → ReSTIR Spatial (pairwise MIS) → ReBLUR (temporal accumulate)
   ↓
GIAccumulate (ACES tonemap, sRGB gamma)
   ↓
Blit to swapchain
```

This pipeline was assembled from the proven TestPathTraceGI ray-tracing
core (path-trace debug fixes inherited) plus the legacy phase-1 ReSTIR
compute shaders (already debugged separately). See
`Vibe_Coding/50_ReSTIR_GI_Temporal/finish_2026-07-20.md` for the
rebuild rationale and the cross-reference to the path-trace debug
session.

## Running

```bash
./Engine/Source/Runtime/Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test
HLVM_DUMP_RGI=1 ./Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
```

## What the validator checks

Unlike the original phase-1 validation (which used a single
mean-luminance threshold and trusted PASS/FAIL), this validator runs
four independent structural checks:

1. **Black-pixel ratio** < 5% (some architectural shadows are expected)
2. **Color variance** across the image > some threshold (uniform gray is
   wrong; uniform noise is also wrong)
3. **Frame-to-frame stability** (mean Δ-E < some threshold; we expect the
   ReSTIR temporal pass to converge, not flicker)
4. **Recognizable structure** — bucket the image into 4x4 cells and
   verify the per-cell luminance pattern is *not* uniform (random-noise
   images pass all three checks above but fail this one)

The Python script exits non-zero on any failure.

## Debug visualization

`HLVM_PT_DEBUG_MODE=N` from the GIPathTracing.hlsl debug code (modes
0..5, 13, 14) is honored — see the path-trace debug session for the
meaning of each mode. Useful for bisecting a future regression.
