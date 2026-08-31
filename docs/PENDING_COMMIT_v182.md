# Pending Commit v182

- plan: docs/PENDING_PLAN_v182.md
- files: Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl,
  Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl,
  Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/v176-recipe.sh
- source: no bundle
- target: (uncommitted working tree — cron does not commit per job instruction)
- task: align GBuffer debug probes (modes 20/21/22/31) from dispatch-space
  `pixel` to full-res GBuffer-space `gbPixel`
- verify: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild`
  then `cd Engine/Source/Runtime/Binary/Debug && HLVM_PT_DEBUG_MODE=20
  HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal`
- skip_impl_review: no
- produces_test_files: no
- notes: production render path untouched; only HLVM_RGI_DEBUG_VIS-gated probes.

## Change detail

4 sites per shader copy, byte-equal between the two copies:

| line | before | after |
|------|--------|-------|
| 764 (case 20u) | `GBufferMaterial.Load(int3(pixel, 0))` | `...int3(gbPixel, 0)` |
| 765 (case 21u) | `GBufferNormal.Load(int3(pixel, 0))` | `...int3(gbPixel, 0)` |
| 766 (case 22u) | `GBufferWorldPos.Load(int3(pixel, 0))` | `...int3(gbPixel, 0)` |
| 793 (case 31u) | `GBufferMaterial.Load(int3(pixel, 0))` | `...int3(gbPixel, 0)` |

Plus a 9-line explanatory comment above case 20u and a stale-comment fix at
`v176-recipe.sh:339`.

## Evidence the probes were reading the wrong address

- `GIPathTracing.hlsl:496-499`: "Phase D: the tracer dispatches at HALF
  resolution; scale the dispatch pixel back to the full-res GBuffer before
  reading geometry." `gbScale = g_View.RenderTargetSize.xy /
  DispatchRaysDimensions().xy`; `gbPixel = int2((float2(pixel)+0.5)*gbScale)`.
- `:501-503` production reads use `gbPixel`. The probes used `pixel`.
- Freshest log (2026-08-14 22:19), byte-read this tick:
  - `FGIPass.cpp:573` → `OutputW=400 OutputH=300` (dispatch dims)
  - `TestReSTIR_GI_Temporal.cpp:2249` → `RenderGBuffer frame 1: drew 24 meshes,
    viewport 800x600` (GBuffer dims)
  - ⇒ gbScale = 2.0. The probe sampled the top-left quadrant of the GBuffer at
    half the intended stride — a strictly different texel set than production.

## Why this closes the card rather than re-litigating it

`DIAGNOSTIC_2026-07-30.md` §"Root cause" concluded "GI shader's GBuffer SRV
bindings are not actually bound" from mode 20/21/22 returning solid black. Ticks
526-528 proved the binding chain correct (shader t3 decl ↔ `AddTextureSRV(3)` ↔
`SetTextureSRV(3, Desc.GBufferMaterial)` ↔ handle-identity with RenderGBuffer)
but never explained the black. This patch explains it: the probes were
instrumentation bugs, not binding evidence. Both facts are now consistent.

Caveat recorded honestly: because the old probes measured the wrong texel, the
mode-20 black is explained but NOT yet re-measured. Confirming that mode 20 now
returns per-material variation requires a build+run (terminal), which this
runspace cannot perform — see the blocker section in the audit.

## Plan Deviations

None. Implemented exactly as planned (2 shader copies + 1 stale recipe comment).
