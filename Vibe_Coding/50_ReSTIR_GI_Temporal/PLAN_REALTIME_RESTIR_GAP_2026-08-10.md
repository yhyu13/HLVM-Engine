# PLAN — Closing the Gap to Real-Time ReSTIR GI (2026-08-10)

> Question answered in this doc: **is the current thing "ReSTIR in real time",
> or "path tracing with a ReSTIR speed-up"?** — It is the latter, and only
> partially: an offline multi-bounce path tracer whose radiance is then
> *reused* by a ReSTIR-flavored reservoir/resolve layer. This plan closes the
> gap to a true ReSTIR GI tracer (RealEngine-modeled), with real-time as the
> eventual target.

## 1. What it is today (measured, 2026-08-10)

### Pipeline

```
GBuffer (worldpos/normal/albedo+roughness/depth, D32 occlusion)
   ↓
FGIPass = full path tracer (GIPathTracing.hlsl)
   2 spp × up to 4 bounces, NEE sun + sky GI, Russian roulette
   → radiance texture + primary ray direction (u2)
   ↓
ReSTIR Generate/Temporal/Spatial (post-process on the path-traced radiance)
   reservoir = {radiance, hitT, M, W}  (no direction/Jacobian)
   temporal: camera-only reprojection; spatial: weighted-average resolve
   ↓
ReBLUR → GIAccumulate (ACES) → display/dump
```

### Measurements (RTX 3090, 800×600)

- **~0.7 s/frame** (2 spp, 4 bounces) — ≈1.4 fps, **not real-time**.
- The cost is the multi-bounce path tracer, not ReSTIR (the reservoir passes
  are cheap compute).
- ReSTIR here does **sample reuse of already-computed radiance**, not
  ReSTIR *sampling*: it never traces the reservoir's sample itself and never
  merges samples with visibility-aware weights. Temporal reuse also only
  handles camera motion — under the scene turntable, history is rejected
  (M stays ≈1.75 instead of accumulating).

### Honest verdict

**Path tracing with a ReSTIR-flavored smoothing/reuse layer.** It is not the
ReSTIR algorithm in the RealEngine sense, and it is not real-time. What IS
genuinely reusable for the real thing: the GBuffer + materials/textures, the
sun/sky model, ReBLUR, the dump/validator infrastructure, and the
temporal/spatial reservoir scaffolding (structure, not math).

## 2. The gap (current vs RealEngine `restir_gi`)

| Aspect | Current HLVM | RealEngine restir_gi | Gap |
|---|---|---|---|
| Sample source | full multi-bounce path tracer, 2 spp | **1 primary GI ray per pixel at half-res** per frame | sampling model |
| Reservoir sample | {radiance, hitT, M, W} | {radiance, **direction**, hitT, sumWeight, M, W} | direction/Jacobian missing |
| Target/PDF | luminance only, no Jacobian | luminance target × **Jacobian** for visibility-aware merge | math |
| Temporal | camera view-matrix reprojection only | velocity/object-aware reprojection | breaks under rotation |
| Spatial | weighted average resolve (biased) | true weighted reservoir merge (`Update`/`Merge`) | algorithm |
| Resolution | full-res tracing | **half-res trace + depth/normal upscale resolve** | perf |
| Per-frame cost | ~0.7 s | target ≪ 33 ms | 20–40× |

## 3. Plan to close the gap (phased, each with a gate)

### Phase A — ReSTIR-native sampling (the conceptual pivot) (2–3 days)

Change FGIPass from "path trace N spp per pixel" to "trace the **primary GI
sample** per pixel (1 ray, ~1 bounce + sky/NEE), store it in the reservoir":
- Keep the existing shading code (NEE sun + sky + one bounce) but run it once
  per pixel at **half resolution** (400×300) and write:
  `Reservoir0 = float4(radiance, hitT)`, `Reservoir1 = float4(M=1, W=1)`,
  `Direction = the actual ray direction` (the u2 output already exists).
- Delete the SPP loop / multi-bounce accumulation from the hot path (keep it
  behind a debug mode for validation).
- Gate: half-res 1-ray radiance ≈ full-res 1-ray radiance (downsample
  comparison); frame time drops ~2–4× (fewer rays).

### Phase B — Reservoir format + real merge math (2 days)

- Extend the reservoir to hold the **sample direction** (16 bytes more) and
  carry the Jacobian in the merge: `w = target(radiance)·W·M·J` per RealEngine
  `reservoir.hlsli`.
- Implement `Update(sample, w, rng)` and `Merge(hist, target_p, J, rng)` in the
  temporal/spatial shaders (replace the current box-average resolve with a
  proper weighted selection + W computation).
- Keep the biased average resolve only as the final denoise input.
- Gate: spatial merge with a fixed 2-sample case reproduces the exact
  closed-form W; M accumulates under a static camera (regression: M mean grows
  over frames as before).

### Phase C — Object-aware temporal reprojection (2 days)

- The turntable currently breaks temporal reuse because reprojection is
  camera-only. Add a **per-instance transform delta** (prev/current instance
  matrices, already cheap since all instances share the same rotation) and
  reproject the sample through `prevInstance·prevViewProj` instead of the view
  alone.
- Gate: with `HLVM_RGI_SCENE_RPS>0`, M now accumulates (was ≈1.75); the
  rotating scene stays converged instead of blurring.

### Phase D — Half-res + upscale resolve + real-time budget (2–3 days)

- Trace at 400×300; resolve to 800×600 with depth/normal-weighted upscale
  (RealEngine resolve) feeding ReBLUR.
- Measure frame time with the existing dump/log infra: target **< 33 ms/frame
  (30 fps)** on the RTX 3090 at 800×600; profile to find the remaining cost
  (TLAS build per frame, half-res trace, resolve).
- Gate: validator 6/6 on a fixed-yaw still; frame-time number logged per run
  (add a `HLVM_RGI_LOG_FRAMETIME` diagnostic).

### Phase E — Quality + validation (2 days)

- Compare half-res 1-ray ReSTIR GI vs the current full-res 2-spp path tracer
  on the same scene/camera (structural metrics + screenshots) — the gap should
  be small after spatial/temporal reuse.
- Keep the per-texel materials (already done), sun/sky, and ReBLUR untouched.
- Update the validator: add a **frame-time check** (real-time gate) and keep
  the existing 6 checks for stills.

## 4. What stays untouched (reuse, don't redo)

- GBuffer + D32 depth, per-texel materials, roughness, sun/sky, camera
  turntable, dump/validator, ReBLUR, ACES accumulate.
- The reservoir *scaffolding* (Generate/Temporal/Spatial pass wiring, ping-pong
  pairs, layout splits from bug-075/v151).

## 5. Acceptance criteria (definition of done)

- [ ] 1 primary GI ray/pixel at half-res (no per-pixel SPP loop in the hot path).
- [ ] Reservoir holds radiance + direction + M/W; merge uses Jacobian-weighted
      `Update`/`Merge` (not a box average).
- [ ] Temporal reuse survives the turntable (M accumulates with rotation).
- [ ] **Frame time < 33 ms** at 800×600 on the RTX 3090 (logged per run).
- [ ] Visual quality within a documented tolerance of the current full-res
      path tracer (screenshots + structural metrics).
- [ ] Validator 6/6 on stills + new frame-time gate.

## 6. Risks

- Half-res + upscale can soften fine detail (curtains, columns) — the
  depth/normal upscale must be tuned.
- Jacobian/PDF mistakes produce bias — Phase B needs a closed-form unit test.
- TLAS per-frame rebuild (turntable) adds cost; consider baking the rotation
  into the instance matrices only when it changes (already cheap, but measure).
- The temporal reprojection upgrade touches the same code that was fixed in
  bug-075 — keep the layout splits intact.

---

## Progress (2026-08-10)

- **Phase A — DONE.** TestReSTIR traces exactly **1 primary GI ray per pixel**
  (was 2 spp); the reservoir IS the sample (Generate copies radiance+hitT with
  M=1/W=1, direction written to u2). Full-res 1-ray: ~370 ms/frame (2.3×
  faster than 2-spp ~850 ms); validator 6/6.
- **Phase B — DONE.** Reservoirs now carry the sample **direction**
  (octahedral-encoded into Reservoir1.zw): Generate reads `u2` (bound as t4 in
  the generation layout), Temporal decodes and carries the selected direction
  through the weighted merge. M=2.76, W=0.982, validator 6/6 (image unchanged —
  direction is reprojection metadata).
- **Phase C — DONE.** **Object-aware temporal reprojection**: the temporal pass
  rotates the reprojected world position by `R_y(PrevSceneYaw−SceneYaw)` before
  `PrevViewProj`, so the scene turntable reuses history. Under rotation M rose
  from 1.75 → 2.23 (max 9 → 17); fixed yaw unchanged.
- **Phase D — DONE (pipeline + measure).** ReSTIR GI now **traces at half
  resolution (400×300)**; a new `Resolve_cs.hlsl` depth/normal-weighted
  upscale brings it to 800×600 for ReBLUR/accumulate/display. All dumps are
  full-res; the readback/stats paths were made size-aware (they previously
  hung on half-res textures). **Measured ~64 ms/frame (≈15.6 fps)** — 5.8×
  faster than full-res 1-ray, ~13× vs the original 2-spp tracer. Validator 6/6.

### Remaining to Phase D's 33 ms target

- Bounce count is NOT the bottleneck (4→2→1 bounces: 64–71 ms, flat).
- **Frame overlap is the big lever**: removing the per-frame `waitForIdle()`
  in `RunMessageLoop` measured **42 ms/frame (24 fps)**, but exposed a real
  engine bug — acquire semaphores are reused before their pending signal
  completes (`VUID-vkAcquireNextImageKHR-semaphore-01779`). Fix that
  (per-index acquire event query) before re-enabling overlap.
- Then profile per pass (GBuffer, TLAS rebuild, trace, resolve, ReBLUR).
- Phase E (quality vs full-res tracer + validator frame-time gate) is still
  open.

Screenshots: `evidence/realtime_halfres/`.
