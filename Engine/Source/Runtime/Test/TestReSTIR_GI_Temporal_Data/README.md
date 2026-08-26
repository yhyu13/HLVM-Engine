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

`HLVM_PT_DEBUG_MODE=N` from the GIPathTracing.hlsl debug code is
honored and gives a stable, parent-friendly bisection surface. The
cumulative diagnostic surface (22 patches, 2026-07-27) is:

| Mode | Sentinel                     | Bisects                          | Patch |
|------|------------------------------|----------------------------------|-------|
| 1    | GBufferMaterial[0].Albedo    | GBufferMaterial SRV              | pre-existing |
| 2    | normal * 0.5 + 0.5           | GBuffer normal SRV               | pre-existing |
| 3    | primaryDirect                | Light/visibility                 | pre-existing |
| 4    | indirect / spp               | GI bounce / payload              | pre-existing |
| 5    | avgFirstHitDist * 0.1        | First-hit distance               | pre-existing |
| 6    | per-pixel gradient           | "dispatch + UAV write"          | v13 |
| 7    | diffuse * AmbientColor * ambientScale | "non-RayGen path"    | v17 |
| 8    | TraceRay + discard payload   | "TraceRay setup"                | v18 |
| 9    | diffuse * 1.5                | "GBufferMaterial SRV alone"      | v18 |
| 10   | debugMode / 256              | "GI cbuffer reach"              | v18 |
| 11   | FrameIndex / 256             | "View cbuffer reach"            | v18 |
| 12   | AmbientColor.rgb             | "AmbientColor uniform bind"     | v19 |
| 13   | RTInstanceInfo[0].AlbedoColor| "RT SRV sanity"                 | pre-existing |
| 14   | RTVertices[0].Position*0.25+0.5 | "RT SRV sanity (vertex)"     | pre-existing |
| 15   | debugMode raw                | Sanity check on mode 10         | v19 |
| 99   | debugMode % all-cases        | Catch-all sentinel (gray)       | v19 |

Plus an unconditional alpha-channel alive-sentinel (v28) that writes
0.99994f to the `.w` channel of every pixel the dispatch reaches. On
the next `display_frame8.png`, alpha saturated 254-255 ⇒ dispatch ran;
alpha uniformly 0 ⇒ bug is upstream (command-list, binding-layout,
or no dispatch at all).

For the meaning of each mode and the evidence-shape decision matrix
that maps each mode to the next debugging step, see
`docs/PIPELINE_HEALTH_2026-07-28.md` and the decision matrices in
`docs/PENDING_PLAN_v32.md` / `v33.md` / `v42.md` / `v13a.md`.

## Helper scripts

Beyond the validator, the test data dir ships these reusable
diagnostic helpers (all file-only, no rebuild required):

| Script                          | Purpose                                                   |
|---------------------------------|-----------------------------------------------------------|
| `validate_restir_gi.py`         | 4-check structural validator + alpha sentinel verifier  (v37) |
| `dump_pixelstats.py`            | Fast first-look on stale/fresh dumps (alpha-aware, v40)   |
| `decode_v38_evidence.py`        | Auto-classifies the v38 cerr-line evidence shape         |
| `fresh-evidence-scan.sh`        | Read-only patch-presence inventory (27 entries, v43)     |
| `run_rgi_diagnostic.sh`         | One-shot 10-mode evidence-capture runner (v23 fixed)    |

Canonical usage:
```bash
# Validate fresh dumps after a rebuild + run
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py

# Inspect alpha channel of a stale dump without rebuilding
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.py \
    Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/display_frame8.png

# Auto-classify the v38 cerr-line evidence from a parent rebuild
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/decode_v38_evidence.py \
    --cerr-file stderr.log

# Verify all 22 patches are still intact in source
bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan.sh

# One-shot multi-mode diagnostic run
bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/run_rgi_diagnostic.sh
```

## References

- [Vibe_Coding/51_PathTraceGI_Debug/session-PathTraceGI_payload_debug.md](../../../../../Vibe_Coding/51_PathTraceGI_Debug/session-PathTraceGI_payload_debug.md) —
  the path-trace payload debugging session that preceded this test's ReSTIR
  rework. TL;DR: slangc compiles each RT entry point independently and can
  dead-strip payload fields an entry never uses, desyncing the raygen /
  closest-hit payload layout; the fix is to fully initialize every payload
  field on both sides of TraceRay. That session is why GIPathTracing.hlsl
  keeps its payload compact and symmetric.
