# ReSTIR GI — New Plan (DeepSeek audit of the stuck state)

**Date:** 2026-08-01
**Author:** DeepSeek review pass

## 1.5 STATUS UPDATE — measured 2026-08-01 (this supersedes the old "black gi_raw" story)

I ran the current uncommitted binary and the picture changed. The old `gi_raw all-zeros`
was from an older binary; the current code fixes the GI tracer:

| Mode | What it proves | Result |
|---|---|---|
| 22 / 21 | GBufferWorldPos / Normal SRV reads inside GIPass | **work** (real, varying data) |
| 5 | first-hit distance | **varies 0→28.8 — primary rays hit geometry** |
| 3 | primary direct (NEE) | **varies 0→95 — direct lighting works** |
| 4 | indirect GI | varies 0→0.5 (dim, but present) |
| 0 | full result | **structured 0.9→96** (not constant) |

So `gi_raw` is now a real, structured image. Descriptors verified correct at three
levels: SPIR-V (`GBufferMaterial` Binding 3 Set 0), C++ layout, and runtime `[v23-diag]`
binding set (material handle matches RenderGBuffer, bound at slot 3). The per-channel
normalization in `DumpRGBA32FTexture` collapses *constant* images to black — do not read
"black gi_raw" as "SRV read failed" for uniform inputs.

**The current display blocker is downstream, not in FGIPass:**
- `denoised` (BilateralDenoise) = **near-white (236-250)** — the denoise blows out the
  signal. It also runs *before* ReSTIR, which is architecturally backwards vs. RealEngine
  (denoise runs after resolve).
- `spatial` (ReSTIR Spatial output) = **all zeros** — the fake ReSTIR produces black.
- `display` = uniform bright gray/pink — accumulate over broken spatial/denoise.

**Implication for the approved actions:** G1 (Phase 0 discriminator) is essentially done —
the GI tracer works. The "does not display" issue is the *fake ReSTIR* (black spatial) and
the misplaced denoise, which is exactly what **G3** fixes. G2 (validator) and G3 (real
ReSTIR) are now the critical path; G1's remaining work is only cosmetic (alive-sentinel
visibility, CPU albedo path) and can fold into G3.

## 1.6 G1 RESULT — the test now displays a real framed Sponza image (2026-08-01)

Two small changes landed and were verified by running the rebuilt binary:
1. **`HLVM_RGI_BYPASS=1` gate** in `TestReSTIR_GI_Temporal.cpp`: skips the broken
   bilateral-denoise + fake-ReSTIR passes and feeds `gi_raw` (OutputTexture) directly into
   the accumulate/display path. Gives an immediate correct image to validate against.
2. **Camera reframe** (`GetCameraPos=(0,6,24)`, target=(0,-1,-7), FOV 75): the old camera
   sat on the floor and framed ~6%; measured worldpos bounds are x[-15,15] y[-12,8]
   z[-14,0]. New camera frames ~44% with a clear building silhouette.

Verified output (mode 0, bypass):
- `gi_raw`: 2019 unique colors, mean 16.6, std 22.7 — real structured HDR.
- `display`: 1138 unique colors, mean [75.6], std 86.3, range 0-241 — coherent lit scene.
- No Vulkan validation errors; test completes.

The image is grayscale because the test loads flat white per-mesh albedo (Sponza texture
albedo isn't loaded) — expected, not a bug. This is the baseline; the G3 rewrite keeps this
working while replacing the fake ReSTIR.

## 1.7 G3 RESULT — real ReSTIR GI pipeline working end-to-end (2026-08-03)

The fake ReSTIR (screen-space luminance box-filter) was replaced with a RealEngine-modeled
reservoir pipeline. Verified with raw float readbacks (byte-clamped PNGs were misleading —
values ≥ 1.0 saturate to 255; the .f32 dump files + float stats logging are the source of
truth):

| Stage | What changed | Verified |
|---|---|---|
| GIPathTracing | emits `OutputDirection` (u2, space1) — primary ray direction | 255 unique dirs, no validation errors |
| GBuffer | MRT3 = linear view depth (R32F), framebuffer 4 attachments | PS outputs 4 MRTs (spirv-dis) |
| Generate | packages gi_raw (radiance+hitT) into reservoir M=1, W=1 | gen0 = gi_raw exactly |
| Temporal | exact NDC-z reprojection from depth+near/far; depth/normal validation; WRS merge; M≤MaxM | M accumulates 1→8 (mean 4.77), historyValid 55.6%, W≈1.0 |
| Spatial | 3×3 WRS merge with geometric rejection; output = selected*W | grayscale error 0.009, consistent with input |
| FReSTIRPass | **removed the bug-075 two-dispatch** (root cause of the yellow output: dispatch 2 re-read the aliased history texture overwritten with M/W) | single dispatch, 0 validation errors |
| Test | real view-proj matrices (was identity); spatial Reservoir0/1 follow the temporal ping-pong parity (was hardcoded → read M/W as radiance on odd frames); gate inverted: default bypass, `HLVM_RGI_RESTIR=1` enables ReSTIR; `HLVM_RGI_MINIMIZED=1` for headless CI | both modes: 0 validation errors, structured display |

Resulting data (RESTIR mode, 8 frames): display mean 0.42 max 0.97; spatial mean 0.69
grayscale; M mean 4.77 (temporal accumulation works); 0 Vulkan validation errors.

**Bugs found and fixed along the way (worth recording):**
1. **bug-075 double-dispatch → yellow (1,1,0) output.** The two-dispatch workaround let the
   second dispatch read the history texture AFTER the first had overwritten it with (M,W)
   (the ping-pong aliases Hist0/Out1 on the same texture) → merged (M,W,0) as radiance.
   Fix: single dispatch (per-thread read-before-write is then guaranteed).
2. **Spatial pass ping-pong parity.** After the temporal pass, Reservoir0/1 must be read
   with the same frame parity used for the temporal outputs, else the M/W texture is read
   as the sample radiance.
3. **Approximate ndc.z (0) reprojection** drifts prevPixel by a few pixels and kills the
   depth validation (M never accumulates). Fixed with exact ndcZ reconstruction from the
   linear depth and near/far (GLM RH-ZO).
4. **LinearDepthTexture was never populated** (created, never written) — all depth checks
   were meaningless. Fixed with MRT3.
5. **Byte-clamped PNGs hide values ≥ 1.0** — reservoir M (up to 30) reads as 1.0 in PNGs.
   Always use raw float readback for reservoir debugging.

**Remaining (G4):** remove the diagnostic scaffolding (reservoir/gi_dir dumps, .f32 files,
historyValid flag in reservoir.z, cerr/mode logging), wire ReBLUR after the resolve, set
`HLVM_RGI_RESTIR` as the default once visually confirmed on a real display, coherent
commits.

## 1.8 G3.5 — review round 2 (2026-08-03): all review items closed + a new root cause

The code-inspection review found: (a) a stale bug-075 two-dispatch comment, (b) unfinished
scaffolding cleanup, (c) ReBLUR not wired, (d) RESTIR not the default. All fixed AND a
latent validation-layer bug was found and fixed:

1. **Stale comment** in FReSTIRPass.cpp — rewritten to describe the split
   SRV(set0)/UAV(set1) layout + single dispatch.
2. **Scaffolding removed**: per-frame `cerr` log, reservoir/gi_dir dumps, `.f32` binary
   dumps, and the `historyValid` flag in reservoir1.z. Kept the raw-float stats log (the
   only non-clamped way to validate reservoir M/W — byte-clamped PNGs saturate ≥1.0).
3. **ReBLUR wired** after the spatial resolve (TestCornellBoxGI pattern): history pair +
   `commitBarriers()` before dispatch (the pass itself doesn't flush pending transitions,
   so the descriptor would otherwise record stale layouts). The spatial pass now writes
   the selected sample's hitT in alpha for ReBLUR. The redundant pre-ReSTIR bilateral
   pass was removed (ReBLUR replaces it); ReBLUR clamps fireflies (spatial max 3.26 →
   denoised max 2.42).
4. **RESTIR is now the default mode.** `HLVM_RGI_BYPASS=1` forces the raw gi_raw path.
5. **NEW ROOT CAUSE — temporal ping-pong aliasing:** the 2-texture ping-pong bound the
   SAME VkImage as SRV history (SHADER_READ_ONLY) and UAV output (GENERAL) in one
   dispatch every frame (the old comment claimed it didn't, but Hist0 always aliased
   Out1). Removing the bilateral dispatch shifted the barrier timing and the validation
   layer started reporting VUID-VkDescriptorImageInfo-imageLayout-00344 (10/run). Fixed
   with FOUR temporal reservoir textures (pairs 0/1 ↔ 2/3): history pair and output pair
   never overlap. **VUID count: 0.**

Final verified state (default mode = ReSTIR + ReBLUR, 8 frames):
- 0 Vulkan validation errors; test completes.
- Reservoir M accumulates 1→8 (mean 4.85), W=1.0.
- spatial/denoised grayscale (channel error 0.009), radiance consistent through the
  whole chain; display mean 0.42 max 0.97 with the framed Sponza silhouette.

## 1.9 review round 3 — numerical claims now verifiable from a PLAIN run's log

The reviewer's remaining gap: the numerical claims (M accumulation, W, grayscale error,
display range) only appeared with `HLVM_DUMP_RGI=1`. Fixed by always-on end-of-run stats:

- Refactored `DumpRGBA32FTexture` into `ReadbackTextureFloats` (staging copy) +
  `LogFloatStats` (per-channel min/max/mean/std, NaN-guarded) + the PNG writer.
- Added `LogFinalFrameStats()` — runs on the last frame unconditionally, readbacks
  display/spatial/denoised/gi_raw + the 4 temporal reservoirs, and logs a derived
  summary line (reservoir M mean/max across both pairs, W mean of the active pair,
  spatial grayscale channel error over lit pixels).

Plain run (no env vars) now emits, e.g.:

```
stats reservoir_MW_A floats: R[1.0000,8.0000] G[1.0000,1.0000] mean=[4.8459,1.0000,0.0000]
ReSTIR summary: reservoir M mean=4.57 max=8.0 (MaxM=30) | W mean=1.000 | spatial grayscale err=0.0125
stats display floats: ... max=0.9726 mean=[0.4157,0.4159,0.4161]
stats spatial floats: ... max=3.2618 | stats denoised floats: ... max=2.4232  (fireflies clamped)
```

Bypass mode logs the same (reservoir M=0 expected — ReSTIR skipped). Both modes: exit 0,
0 VUID.

## 2.0 multimodal structural judgment (2026-08-08) — the picture, not the numbers

The reviewer's sharpest point: scalar stats (M, W, means) cannot distinguish "correct
Sponza" from "smooth gray blob". Two gaps closed and a NEW judge introduced:

1. **The scene was flat-white** (Sponza's colors live in unloaded textures) — the
   CHROMATIC modality could never be judged. Added `GetMeshAlbedo()`: deterministic
   per-mesh palette (8 colors, FNV-1a on mesh name) when the loaded material has no
   chromatic content; used consistently in BOTH the RT instance buffer and the GBuffer
   pass so the GBuffer material texture remains the per-pixel ground truth.
2. **`validate_multimodal.py`** (replaces the mean/std/variance `validate_restir_gi.py`
   class of checks): four independent structural modalities, ALL must pass, plus a
   mandatory human-review rendering. Ground truth = GBuffer (normal dump for the
   geometry mask — the worldpos dump is per-channel NORMALIZED, so sky pixels rescale
   to mid-range and must NOT be used as a mask; that bug made the first run fail
   M1/M2 with F1/IoU 0.00/0.44 before the fix).

Result — DEFAULT ReSTIR+ReBLUR mode:
```
M1 geometry/edge alignment: edge F1 = 1.000 -> PASS   (display edges == geometry silhouette)
M2 spatial layout:          lit-mask IoU = 1.000 -> PASS (occupancy matches exactly)
M3 chromatic regions:       7/7 albedo clusters show their hue -> PASS
M4 channel sanity:          no zero channel, 18 distinct hues -> PASS
M5 human review:            color map reads as a coherent building facade with
                            arch/door openings and colored facade details
```
Bypass mode: identical 4/4 PASS. Both modes exit 0, 0 VUID.

The scalar record (for the log, NOT a gate): M mean 4.57 max 8.0, W=1.000, display
max 0.97, spatial max 3.26 → denoised max 2.42, grayscale err 0.52 now (image is
intentionally chromatic).

---

## 0. Validation metric policy (override the current validator)

**Rejected metrics — never use these as a pass gate:**
- mean luminance / mean per-channel color (a global scalar — the Cornell box passed its
  mean-luma gate while showing pure gray noise; see 51 §1/§8).
- spatial std-dev / variance (uniform-but-noisy images pass variance).
- pixel noise ratio / firefly ratio (a transport artifact, not correctness).
- the alpha "alive-sentinel" (besides being a transport marker, it is **broken here**:
  `DumpRGBA32FTexture` forces `Pixels[..3] = 1.0` at line 1747, so every PNG is written
  with alpha=255 and `check_alpha_sentinel` can never FAIL — a false-PASS metric).

**The only valid gate: multimodal structural validation.** "Multimodal" = several
*independent, structurally meaningful* detectors, each observing a *different modality*
of the image, and **all** must pass jointly. A single scalar that "looks alive" is not
evidence; a set of independent structural signatures agreeing is.

Required modalities (all must PASS together):

1. **Edge/structure match (modality: geometry).** Sobel/Canny edge map of the display
   must match the expected Sponza silhouette — the arch columns, the door cut-out, the
   wall/floor/ceiling horizon. Scored by edge-layout correlation against a reference
   (raster GBuffer silhouette, or a CPU render), **not** by a mean. Flat/magenta frames
   have no edges → fail.
2. **Chromatic region separation (modality: color/materials).** Sponza's distinct albedo
   regions (the colored walls) must each be dominated by their expected channel in their
   region. Per-region *classifier* (each known wall/floor region maps to its expected
   hue), **not** a global color mean.
3. **Per-region occupancy (modality: spatial layout).** Label known screen regions
   (left wall / right wall / floor / ceiling / door). Each must contain its expected
   content — no region may be uniformly one color, and the region set must be
   distinguishable. This is region *classification*, not cell-variance of means.
4. **Channel independence (modality: multi-channel).** Red/green/blue channels must
   diverge where materials diverge and agree where they should; uniform RGB correlation
   across the whole frame is a failure signature.
5. **Mandatory human visual review.** An image test without a human looking at a dump is
   a weak test (51 §8). Every "PASS" requires a reviewed dump, not just an automated
   scalar.

The current `validate_restir_gi.py` (non-black channel mean / spatial std / cell
variance / alpha sentinel) must be **replaced**, not tuned. Checks 1–4 of that file are
the exact rejected metric classes; check 5 (alpha) is broken by the dumper. New
validator: multimodal detectors + exit only if a human confirms structure.

---

## 1. Judge of what is right / wrong in the current code and process

### RIGHT (keep, do not regress)

1. **GBuffer raster is now real and working.** `GBufferPT_VS/PS` + per-mesh `drawIndexed`
   produces real Sponza positions (`gbuffer_worldpos` range proves it). This was the
   correct direction vs. the old hardcoded fill. Keep `RenderGBuffer` leaving the CL open
   and submitting once at end-of-Render (the v1 HLVM-bypass regression must stay dead).
2. **Sponza 0.01 scaling is applied consistently** to TLAS transform, raster ModelMatrix,
   and camera. This is why raster and RT agree in space. Good.
3. **bug-088 (missing `executeCommandList`) was correctly root-caused and fixed.** The
   `close(); executeCommandList();` at end of `Render()` is correct.
4. **Following `Vibe_Coding/51_PathTraceGI_Debug`:** debug modes to amputate the chain,
   per-channel-normalized dumps, handle-id probes, alive-sentinels. This methodology is
   the right *process* — the same methodology that fixed Cornell Box.
5. **64-byte fully-used payload + NEE inside ClosestHit** (from 51) is carried into
   `GIPathTracing.hlsl`. This is the proven shape; do not reintroduce payload dead-strip.

### WRONG (why you are stuck)

1. **The "ReSTIR" implementation is not ReSTIR at all.** It is a screen-space luminance
   filter dressed up as ReSTIR (details in §4). This is the fundamental wrong, and it is
   *not* the immediate display blocker — it is the long pole that will bite after you
   get an image.
2. **Temporal reprojection is disabled/hardcoded.** `TestReSTIR_GI_Temporal.cpp:558-563`
   fills `InverseCurrViewProj` and `PrevViewProj` with **identity**. Temporal merge then
   reprojects every pixel to itself: no history reuse, no validation. Fixable, but it is
   currently a no-op.
3. **Measurement fatigue / scaffolding in the hot path.** 30+ debug modes, a `cerr` on
   every `Render()`, per-frame handle-id logs, and an alpha "alive-sentinel". Per 51's own
   checklist (#7, #8): workarounds and probes that hide symptoms delay the root cause.
   The probes exist but **the decisive run (debug modes 20/21/22/30/31 → dump `gi_raw`) has
   not been recorded** — so the correct measurement is missing.
4. **"Uniform magenta = pipeline runs" is a false PASS.** The docs treat magenta as
   success ("Output is uniform magenta, not Sponza structure yet"). Per 51 checklist #8,
   a uniform image with a saturated alpha is exactly the `RGB=0 + alpha alive-sentinel`
   signature of the GI pass *not lighting anything*. Uniform ≠ alive.
5. **`space1` UAV / two-dispatch split (bug-075) is a symptom patch.** The single-set
   layout should work; the workaround dispatches the temporal shader twice. Fix the root
   descriptor/layout mismatch instead of doubling the work.
6. **Patching NVRHI's validation wrapper to remove a `return`** (the final-state doc)
   disables a real double-immediate-open safety check rather than fixing the double-open.
   Fragile and un-rebuildable (ninja reconfigure fails). Avoid relying on it.
7. **No CPU reference oracle for Sponza.** 51's strongest tool was the CPU reference
   render. `validate_restir_gi.py` has structural checks but no scene/camera/light oracle,
   so you cannot separate "scene wrong" from "GPU chain wrong".
8. **Messy working tree.** ~35 modified files incl. docs, dumps, stale `Document/`. The
   churn hides the real diff. Commit in coherent steps (get-green → display → real ReSTIR).

---

## 2. The current blocker (precise)

Log (2026-07-30 frame 8):

- `gi_raw` normalized per-channel: `R[0.000,0.000] G[0.000,0.000] B[0.000,0.000]` → the
  RT pass writes `0.0` RGB for every pixel.
- `gbuffer_worldpos`: `R[-15.228,15.264] …` → GBuffer has real data.
- `display`/`spatial`/`denoised`: uniform (they consume the zero `gi_raw`).

Note: `DumpRGBA32FTexture` **forces alpha to 1.0** (line 1747), so the shader's
`Output.w = max(…, 0.99994)` alive-sentinel is **invisible in the dump** — you cannot use
the alpha to tell early-return vs. zero-read. That sentinel must be tested in a *different*
channel (e.g. pack into a color channel under a debug mode), or dump raw before forcing alpha.

Two compatible hypotheses for `gi_raw == 0`:

- **H-A:** `length(worldPos) < 0.001` fires for every pixel → the `GBufferWorldPos` SRV
  read inside RayGen returns ~0 (early-return at line 481).
- **H-B:** SRV reads return 0 for material/albedo (or AmbientColor/AmbientScale reach
  zero), so `primaryAmbient = diffuse*Ambient*scale = 0` and `primaryDirect = 0` (no light
  or NEE off).

Both reduce to **"the GIPass GBuffer SRV reads / lighting uniforms are not what the CPU
dump says they should be."** The discriminator already exists (modes 20/21/22/30/31) — it
just has not been run and its output recorded.

---

## 3. Fix order (get signal first, then quality — 51's rule)

### Phase 0 — settle H-A vs H-B (one run, no code change)

Run and dump `gi_raw` (or `display`) under each mode, record the image + the min/max log:

```bash
cd Engine/Source/Runtime/Binary/Debug
for m in 0 20 21 22 30 31 7 12; do
  VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.json \
  HLVM_DUMP_RGI=1 HLVM_PT_DEBUG_MODE=$m timeout 120 ./TestReSTIR_GI_Temporal > /tmp/rgi_$m.log 2>&1
  cp Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/*gi_raw*.png /tmp/rgi_$m.png
done
```

Interpretation:
- modes **20/21/22/30/31** show real GBuffer data in `gi_raw` → SRV reads work; bug is
  H-B (lighting / uniforms / NEE). If they show black → SRV binding broken (H-A / binding).
- mode **12** (AmbientColor) and mode **7** (`diffuse*Ambient*scale`): if 7≈0 while 12≈1
  and mode 1 (diffuse) ≈ real albedo, the product path is fine and the culprit is
  `primaryDirect` being zero (lights not bound / LightCount=0 / NEE flag off).

### Phase 1 — get a non-zero, actually-lit image (the display fix)

Do these, one at a time, dump after each:

1. **Fix the alive-sentinel** so it is dump-visible: put the "did I reach the end" marker
   in a color channel under mode 0 (e.g. add a tiny `+ (mode==0 ? (1e-3,0,0) : 0)`), or
   change the dumper to not force alpha. Otherwise you are flying blind.
2. **Verify lights are actually bound and counted.** `g_GI.Params3.w` (LightCount) must
   be >0 and `Params4.x` (EnableNEE) >0.5, and `Lights[]` SRV must be the real buffer.
   Commit `1773749` added default point lights inside Sponza — confirm the C++ fills
   `Desc` → `FGIPass::WriteConstants` and the shader sees `LightCount`. If `primaryDirect`
   is the only intended illuminator and it's 0, add a *guaranteed* ambient fallback until
   the image appears (do **not** trust it as final).
3. **Confirm the GBuffer SRV descriptors match the RT binding layout** (space0 t1/t2/t3 vs
   `Output` at space1 u0). Compare the `[handle-id]` logs from `RenderGBuffer` (line 1540)
   against `FGIPass::DispatchRays` (line 556). If handles match and modes 20/22 still read
   zero, the layout/descriptor is the problem — inspect with `spirv-reflect` on the sblob
   and fix the single-set layout rather than adding a `space1` workaround.
4. **Camera sanity:** after an image appears, confirm Sponza is framed (75° FOV, camera
   at (0,0.03,0.08) looking at (0,0.02,0)) — mirrors 51's "camera must frame the scene".

Definition of Phase 1 done: the **multimodal** gate in §0 passes (edges match Sponza
silhouette AND chromatic regions separate AND per-region occupancy holds AND a human
confirms structure under mode 0). Not a mean, not a variance, not uniform-but-bright.

### Phase 2 — replace the fake ReSTIR with a correct reservoir (model on RealEngine)

The current `ReSTIR_Generate/Temporal/Spatial` shaders are not ReSTIR (see §4). Replace
them following the reference at
`/home/hangyu5/Documents/Gitrepo-Other/Graphics/raytracing/RealEngine/shaders/restir_gi/`
(`reservoir.hlsli`, `initial_sampling.hlsl`, `temporal_resampling.hlsl`,
`spatial_resampling.hlsl`, `restir_resolve.hlsl`).

Concrete mapping for HLVM (single-source, no ResourceDescriptorHeap — bind explicit SRVs):

- **Generate** = RealEngine `initial_sampling.hlsl`: for each pixel, sample ONE hemisphere
  ray (pdf = 1/2π), trace, and store `(radiance, hitT, rayDirection)` into the reservoir.
  Do **not** sample the already-denoised radiance texture. If you keep the denoise step,
  it runs *after* ReSTIR resolve, not before.
- **Temporal** = `temporal_resampling.hlsl`: carry `sample.radiance + sample.hitT +
  sample.rayDirection + depthNormal(M/W)`, reproject with the **real** inverse-current
  view-proj and prev view-proj (stop using identity), validate depth/normal, then
  `R.Update(S, target_p_q, rng)` and clamp `M≤30`.
- **Spatial** = `spatial_resampling.hlsl`: proper `Merge` with `target_p` and random, not
  the naive `Σ(w_i*rad_i)/Σ(M_i*p_i)` box-mean.
- **Reservoir** = copy `reservoir.hlsli` verbatim (`Update`, `Merge`, `W = sumW/(M*target)`).
- **Resolve** = upscale from half-res with bilinear + custom depth/normal weights (for the
  denoiser input). Run the GI denoiser (ReBLUR is already present) on the resolve output.

### Phase 3 — denoiser / temporal quality (separate, large)

NRD/ReBLUR and proper temporal accumulation on the resolved irradiance. Out of scope for
the "does it display" fix; keep as a follow-up card.

---

## 4. Why the current HLVM ReSTIR is not ReSTIR (technical)

| Aspect | RealEngine (correct) | HLVM current (wrong) |
|---|---|---|
| Sample domain | hemisphere direction, pdf 1/2π, traced ray | a **screen pixel index** into the denoised radiance texture |
| Reservoir holds | `sample{radiance,hitT,rayDir}` + `M,W` | `y` (pixel coord), `w_sum`, `M`, `W` |
| Target function | `f·Le/pdf` (BSDF×emission) | `Luminance(radiance)` of a neighbor pixel |
| Jacobian / visibility | included (or flagged ignored) | silently set to 1.0 with no justification |
| W estimator | `sumW/(M·target)` with correct target | `w_sum/(M·Luminance(y))` → not unbiased |
| Temporal | real velocity + depth/normal validation, `Merge` | identity matrices → no-op reprojection |
| Spatial | `Merge` (weighted reservoir sampling) | `Σ(w_i r_i)/Σ(M_i p_i)` = box blur |
| Output | resolve half-res → SH/irradiance → denoiser | directly an RGBA texture, then accumulated |

A "reservoir" whose sample is a *neighbor pixel* and whose target is *luminance of that
pixel* is a bilateral/box filter, not ReSTIR. It cannot produce unbiased GI or reuse rays
temporally/spatially. This is why "tuning ReSTIR" has not produced Sponza structure: there
is no ReSTIR to tune. **The 51 learning that does carry over is the GI tracer itself** —
keep `GIPathTracing.hlsl` as-is; only the ReSTIR layers above it are a false start.

---

## 5. Are we too far from fix?

- **Display bug (`gi_raw` black): NO — close.** The GBuffer works; the remaining issue is
  one read/lighting/binding discriminator (Phase 0 → Phase 1). This is hours-to-a-day,
  and Phase 0 needs zero code changes.
- **Real ReSTIR GI: not "too far" but it is a rewrite, not a tuning.** The current ReSTIR
  math is a dead end (a filter mislabeled ReSTIR). The good news: the RT foundation, the
  GBuffer, the pass plumbing, and a correct reference implementation (RealEngine) all
  exist. Phase 2 is real engineering (days), but it is mechanical once Phase 1 shows a
  real image, because the reference is small (~300 lines of HLSL) and the reservoir
  struct is trivial.

**Priority order:** Phase 0/1 (display) first — you cannot debug ReSTIR on a black input.
Then Phase 2 rewrite. Do **not** sink more time tuning the current fake ReSTIR.

---

## 6. Concrete action list (smallest steps, each leaves a build green)

- [ ] A0: Run modes 0/20/21/22/30/31/7/12; record dumps for **visual review only**
      (never auto-gate on mean/variance/alpha — see §0).
- [ ] A1: Make the alive-sentinel dump-visible (color channel) OR stop forcing alpha in
      `DumpRGBA32FTexture` (currently makes alpha metrics trivially pass).
- [ ] A2: Verify LightCount>0, EnableNEE>0.5, `Lights[]` bound; log them in RayGen mode 15.
- [ ] A3: Compare RenderGBuffer vs FGIPass handle-id logs; `spirv-reflect` the sblob to
      confirm t1/t2/t3 + u0(space1) layout.
- [ ] A3b: **Replace `validate_restir_gi.py`** with the §0 multimodal structural validator
      (edge-match + chromatic-region + per-region occupancy + human review gate); delete
      the mean/std/cell-variance/alpha checks.
- [ ] A4: Get a real Sponza image under mode 0 (multimodal gate passes + human review).
      Commit. (Phase 1 done.)
- [ ] A5: Add a CPU Sponza reference oracle (mirror 51) — lights, camera, first-hit.
- [ ] A6: Rewrite Generate/Temporal/Spatial reservoirs from RealEngine reference (§4).
- [ ] A7: Fix temporal matrices to real view-proj (remove identity).
- [ ] A8: Replace bug-075 two-dispatch hack with a correct single layout.
- [ ] A9: Restore/verify ReBLUR on the resolve output; commit.
- [ ] A10: Clean working tree; coherent commits; retire stale scaffolding.

---

## 7. Reference files

- HLVM GI tracer (keep): `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl`
- HLVM GBuffer pass (keep): `GBufferPT_VS.hlsl` / `GBufferPT_PS.hlsl`
- HLVM driver: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp`
- HLVM pass: `Engine/Source/Runtime/Private/Renderer/PostProcess/FReSTIRPass.cpp`
- HLVM ReSTIR (replace): `TestReSTIR_GI_Temporal_Data/ReSTIR_{Generate,Temporal,Spatial}_cs.hlsl`
- **Reference ReSTIR (model on this):**
  `…/Gitrepo-Other/Graphics/raytracing/RealEngine/shaders/restir_gi/{reservoir.hlsli, initial_sampling.hlsl, temporal_resampling.hlsl, spatial_resampling.hlsl, restir_resolve.hlsl}`
  and `…/RealEngine/source/renderer/lighting/restir_gi.{h,cpp}`
- Debug methodology (apply): `Vibe_Coding/51_PathTraceGI_Debug/session-PathTraceGI_payload_debug.md`
