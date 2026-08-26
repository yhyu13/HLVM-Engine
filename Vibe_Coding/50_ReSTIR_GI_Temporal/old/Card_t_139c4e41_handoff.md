# Card t_139c4e41: HLVM_DUMP_RGI GBuffer dump + sentinel writes

**Commit:** `19a499a` on `rhi2`
**File:** `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` (103 lines added)
**Build:** Clean Debug, rc=0, 23.9s. Executable 182,519,248B.

## What landed

Following section 8 of
`Vibe_Coding/51_PathTraceGI_Debug/session-PathTraceGI_payload_debug.md`
(the Cornell-box debugging retrospective — same shape as TestReSTIR_GI_Temporal):

1. **`WriteGBufferSentinels()`** — new method (line 980). Uploads unique magic
   values to the 3 GBuffer channels via `Cmd->writeTexture`, with state
   transitions through `CopyDest` (write) and back to `RenderTarget` (for the
   raster pass). Pattern is identical to the existing `FillGBufferHardcoded()`.

   | Channel  | Sentinel (RGBA32F)               | Why this value                                            |
   | -------- | -------------------------------- | --------------------------------------------------------- |
   | WorldPos | `(-100, -200, -300, 1.0)`        | Far outside Sponza's scaled bbox (±0.06 in scaled units)  |
   | Normal   | `(0.111, 0.222, 0.333, 1.0)` (enc) | Decodes to magnitude 1.016 — not a unit vector → sentinel |
   | Material | `(0.999, 0.001, 0.500, 1.0)`     | No plausible PBR albedo looks like this                   |

   Invoked at the very top of `RenderGBuffer()` (line 1269). Per-frame upload
   cost ~5.76 MB (3 × 800×600×4 floats); unconditional.

2. **GBuffer dumps added to `DumpCurrentFrame()`** (lines 1498-1500).
   `gbuffer_worldpos`, `gbuffer_normal`, `gbuffer_material` channels. Same
   `HLVM_DUMP_RGI` env-var gate (line 607), same `dumps/` directory, same
   `timestamp_channel_frameN.png` naming convention. `validate_restir_gi.py`
   consumes them unchanged.

## How to verify (when a display is available)

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal
HLVM_DUMP_RGI=1 ./Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal
ls Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/*gbuffer_*
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
```

What you should see in the dumps:

- **Without the raster pass actually writing pixels** (current state until the
  Sponza drawcall is verified) — every pixel of every GBuffer dump is the
  sentinel value. WorldPos dump = flat gray encoded for `(-100,-200,-300)`.
- **With the raster pass working** — pixels inside the Sponza silhouette show
  real data; pixels outside (sky / off-mesh) keep the sentinel. The boundary
  is the proof that the raster pass touched the expected region.

## What this task did NOT do (out of scope by design)

- Did NOT modify any shader (GBufferPT_VS / GBufferPT_PS unchanged).
- Did NOT change any binding layout or pipeline state object.
- Did NOT actually verify the raster pass produces non-sentinel pixels —
  that is `t_0a6b647a`'s job. This card establishes the *machinery* (sentinel
  + dump) so the next task has a deterministic per-pixel oracle.

## Source-level verification (12/12 PASS)

```
PASS  V1 WriteGBufferSentinels() defined
PASS  V2 WriteGBufferSentinels() called early in RenderGBuffer
PASS  V3 WorldPos sentinel = (-100, -200, -300)
PASS  V4 Normal sentinel = (0.111, 0.222, 0.333)
PASS  V5 Material sentinel = (0.999, 0.001, 0.500)
PASS  V6 GBufferWorldPos dump
PASS  V7 GBufferNormal dump
PASS  V8 GBufferMaterial dump
PASS  V9 Dump still gated by HLVM_DUMP_RGI (bDumpRequested)
PASS  V10 Sentinel writes use writeTexture
PASS  V11 Three GBuffer channels dumped via same helper
PASS  V12 Sentinel transitions through CopyDest
```

## Display-blocked caveat (carried over from parent)

The executable fails at `GLFW3VulkanWindow` constructor before reaching
`Initialize()` — no X server in this sandbox. Same condition as the prior
3 cards in this lineage. The machinery is verifiable on disk + by clean
build; runtime verification of "rasterizer actually overwrites sentinel
pixels" requires a display-capable runner (covered by t_0a6b647a).
