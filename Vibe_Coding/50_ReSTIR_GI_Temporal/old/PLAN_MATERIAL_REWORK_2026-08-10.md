# PLAN — Material System Rework for TestReSTIR_GI_Temporal (2026-08-10)

> Status: PLAN ONLY — no implementation yet. Long-task plan requested before
> any GI rework. Companion to `FIX_LOG_2026-08-09.md` and
> `SCREENSHOTS_2026-08-10.md`.

## 1. Problem statement (why GI is "not correct" and pillars are colorful)

User observation: *"Sponza should have gray pillars, not colored pillars; GI is
not correct — seems not original from Sponza; material system needs rework
before we do GI again."*

Confirmed root cause — the renderer never uses Sponza's real materials:

1. **Palette-hash albedo hack** (`TestReSTIR_GI_Temporal.cpp:195`
   `GetMeshAlbedo`): when a GLTF material's albedo is neutral (gray), the code
   substitutes one of 8 arbitrary colors chosen by a **hash of the mesh name**.
   Sponza's stone columns/arches/pillars are neutral → they get blue/red/green
   by name hash. This is the pillar-color bug, and it also poisons every GI
   bounce (bounce albedo comes from the same palette).
2. **No texture sampling in the GBuffer**: `GBufferPT_PS.hlsl` writes
   `MRT2 = AlbedoColor` from a per-instance constant. `FInstanceInfo`
   has `AlbedoTextureIndex` but it is hardcoded to 0 and never bound.
   UV/TANGENT were even removed from the input layout (2026-08-09 attribute
   cleanup) — UV must come back for textures.
3. **No roughness/metallic anywhere**: the GI shader uses a Lambert diffuse
   (`GBufferMaterial.rgb` for primary, `RTInstanceInfo.AlbedoColor` for
   bounces); roughnessFactor/metallic from the GLTF are ignored.
4. Sponza's real data is available: `Sponza01.gltf` has **25 materials, all
   with `baseColorTexture` (.ktx albedo: `sponza_column_a_diff.ktx`,
   `spnza_bricks_a_diff.ktx`, …) and `roughnessFactor` 0.588** (metallic 0).
   The engine already loads such textures in `TestRTShadowsGBuffer` via
   `FAsyncTextureLoader` + `FPBRMaterial::GetGPUTexture(Albedo)` + a per-mesh
   `DiffuseTexture` binding — a proven reference path to copy.

Conclusion: GI cannot be "correct" until the material inputs are correct.
This plan stages the material-system rework, then re-validates GI.

## 2. Goals / non-goals

**Goals**
- Render Sponza with its real albedo textures (gray pillars, red bricks,
  correct arch/ceiling/leaf colors).
- Carry per-pixel albedo **and roughness** (metallic where the gltf defines
  it) through the GBuffer into the GI shader (primary and bounce).
- Remove the palette-hash hack; keep a clean, documented untextured fallback.
- Re-validate the full ReSTIR GI pipeline after the rework (validator 6/6 +
  screenshot suite + numeric albedo checks).

**Non-goals (future, noted)**
- Normal maps / tangent-space shading (this gltf export has no normal
  textures; add later if a textured-normal asset is available).
- Specular/GGX GI (current tracer is diffuse-only; roughness will drive
  indirect sample distribution and any future specular lobe, not a full
  specular pass).
- ReSTIR reservoir math rework (separate plan per `deepseek/PLAN.md`).

## 3. Architecture: current vs target

### Current (broken) material path

```
Sponza01.gltf → FScene3DLoader (materials exist)
      ↓ GetMeshAlbedo() palette hash (neutral → arbitrary color)
FInstanceInfo.AlbedoColor + AlbedoTextureIndex=0 (unused)
      ↓
GBufferPT_PS: MRT2 = AlbedoColor            (no texture, no roughness)
      ↓
GI: primary diffuse = GBufferMaterial.rgb    (palette)
    bounce albedo  = RTInstanceInfo.AlbedoColor (palette)
```

### Target material path

```
Sponza01.gltf → FPBRMaterial (per-mesh) → FAsyncTextureLoader → GPU albedo
      ↓ per-mesh SRV binding (reference: TestRTShadowsGBuffer:920-947)
GBufferPT_VS: POSITION/NORMAL/UV (+TANGENT later) — UV re-added
GBufferPT_PS: MRT2 = float4(albedoTex.Sample(uv).rgb, roughness)
      ↓
GI primary:  albedo = GBufferMaterial.rgb, roughness = GBufferMaterial.a
GI bounce:   closest-hit interpolates UV → per-instance albedo SRV
             (RTInstanceInfo.AlbedoTextureIndex + texture array/per-instance set)
```

## 4. Phases & tasks (execution order, each with a verification gate)

### Phase 0 — Instrument & baseline (½ day)

Tasks:
1. Add a material debug output: dump `gbuffer_material` + per-mesh albedo
   (mesh name → palette color vs gltf `baseColorFactor`).
2. Verify what the loader actually provides for Sponza:
   `Scene->MeshMultiMaterialMap[mesh][0]->GetGPUTexture(Albedo)` — does it
   return a texture for this scene today (or only after explicit loading)?
3. Capture "before" screenshots (interior + exterior) and per-pixel albedo
   stats (e.g., pillar-center pixel RGB) as the regression baseline.

Gate: a table of 8–10 representative meshes (name, gltf baseColor, current
palette color, GPU texture present?) that Phase 1 must fix.

### Phase 1 — Textured albedo in the GBuffer pass (the color fix) (1–2 days)

Tasks:
1. Re-add `TEXCOORD0` to `GBufferPT_VS.hlsl` and to the input layout
   (FVertex UV at offset 32, stride 64); pass UV through to the PS.
2. Load Sponza albedo textures with the existing
   `FAsyncTextureLoader::LoadMaterialTexturesAsync` (albedo type), one
   placeholder for missing textures (reference: TestRTShadowsGBuffer).
3. Per-mesh draw: bind `PBRMat->GetGPUTexture(Albedo)` as `t0` in
   `RenderGBuffer`; keep `AlbedoColor` only as the untextured fallback.
4. `GBufferPT_PS`: `MRT2.rgb = albedoTex.Sample(sampler, UV).rgb` (fallback to
   `AlbedoColor` when the mesh has no texture).
5. Keep the rest of the pipeline untouched; run the existing validator.

Gate: **pillars/columns render gray** (albedo from `sponza_column_a_diff.ktx`),
bricks warm red, leaf/arch/ceiling match Sponza reference images; validator
6/6 (or a documented, calibrated change); before/after screenshots.

### Phase 2 — Roughness/metallic through the GBuffer into GI (1 day)

Tasks:
1. Extend `FInstanceInfo` (48 B layout is shared with the RT shader): use the
   reserved `Padding` for `Roughness`/`Metallic` (gltf roughnessFactor 0.588,
   metallic 0; per-material factor).
2. `GBufferPT_PS`: `MRT2 = float4(albedo.rgb, roughness)` (MRT stays
   RGBA32F).
3. GI shader: read `roughness = GBufferMaterial.a` for primary shading; pass
   roughness into the indirect sample distribution (cosine → roughness-
   weighted) and future BRDF.
4. Update the GBufferSponzaPS/other consumers of the material MRT if shared.

Gate: `gbuffer_material.a` ≈ 0.588 on textured surfaces; changing roughness
visibly changes indirect spread in a controlled A/B (debug CVar).

### Phase 3 — Bounce shading uses real materials (closest-hit) (1–2 days)

Tasks:
1. `RTInstanceInfo` already carries `AlbedoTextureIndex`; add a texture array
   (or per-instance SRV set) bound to the RT pipeline, indexed by instance.
2. Closest-hit: interpolate UV from `RTVertices` (FRTVertex already stores UV),
   sample the instance's albedo texture, multiply by fallback color.
3. Verify: red carpet / colored wall bounces tint the indirect light (warm
   color bleeding), gray pillars bounce neutral.

Gate: indirect (mode 4 / gi_raw without NEE) shows texture-driven color
bleeding; numeric: bounce radiance from a red surface > from gray surface.

### Phase 4 — Material-system hygiene (the "rework" beyond this test) (1 day)

Tasks:
1. Delete/disable `GetMeshAlbedo` palette hack; leave a single documented
   untextured fallback (`baseColorFactor` or 0.7 gray).
2. Extract the per-mesh texture binding into a reusable helper (shared with
   TestRTShadowsGBuffer) instead of duplicating the loop.
3. Document the GBuffer MRT packing (albedo.rgb + roughness.a), texture
   conventions (sRGB albedo → linear), and the material API
   (`IMaterial::ETextureType`, `FPBRMaterial::GetGPUTexture`) in a
   `docs/MATERIAL_CONVENTIONS.md`.
4. Optionally backfill `docs/` with the loader/color-space findings from
   Phase 0.

Gate: no palette colors remain in the renderer; a mesh without textures still
renders with its `baseColorFactor`.

### Phase 5 — GI re-validation (1 day)

Tasks:
1. Full ReSTIR pipeline run (interior sunlight + exterior camera).
2. Validator 6/6 (recalibrate thresholds only if the material change moves
   statistics in a documented way).
3. Screenshot suite update (`evidence/`) + numeric albedo spot checks
   (pillar ≈ gray, brick ≈ red, gltf baseColor vs renderer albedo within a
   tolerance).
4. Update `FIX_LOG`/`final-state`/`SCREENSHOTS` docs.

Gate: screenshots show recognizable Sponza materials; GI bounces match
material colors; validator green.

## 5. Risks & open questions

1. **Does the loader already give GPU textures for Sponza?** Phase 0 answers
   this. If not, the KTX load path (`FAsyncTextureLoader`) must be wired to
   this scene — the RTShadowsGBuffer test proves the machinery exists.
2. **sRGB handling**: KTX albedo textures must be decoded to linear before
   lighting (check `FTextureCache`/loader color-space handling). Wrong gamma
   makes everything look off — a common silent bug.
3. **Multi-material meshes**: `MeshMultiMaterialMap` may have >1 material per
   mesh; start with material [0] (same simplification as TestRTShadowsGBuffer).
4. **FInstanceInfo layout is shared** with `GIPathTracing.hlsl`
   (`RTInstanceInfo`) and the 48-byte std430 layout — extend the reserved
   padding, keep the static_assert, update the HLSL mirror together.
5. **UV re-add** conflicts with the earlier 2-attribute input layout — only
   UV is required now; TANGENT stays out until normal mapping.
6. **Texture array vs per-instance sets** for bounce sampling: Vulkan
   descriptor indexing is enabled (`VK_EXT_descriptor_indexing`), so an
   unbounded texture array indexed by `AlbedoTextureIndex` is viable.
7. **Validator thresholds**: correct materials change means/std — recalibrate
   with a documented rationale, not silently.

## 6. Definition of done

- [ ] Pillars/columns/arches render gray from `sponza_*_diff.ktx`, bricks red,
  leaf/ceiling correct — no hash-palette colors in any output.
- [ ] GBuffer material MRT carries albedo + roughness; GI reads both.
- [ ] Bounce shading samples the instance's real albedo texture.
- [ ] `GetMeshAlbedo` hack removed; documented fallback path remains.
- [ ] Validator 6/6; screenshot suite + numeric albedo checks updated.
- [ ] Material conventions documented; the per-mesh binding helper is shared.

## 7. Suggested execution

Order strictly Phase 0 → 5; each phase ends with its gate. Phases 1 and 2 are
the minimum for the visible pillar/GI-color fix; Phase 3 completes physical
correctness; Phases 4–5 close the "material system rework" and re-validate GI.
Estimated total: ~5–7 focused work days.

---

## Progress (2026-08-10)

- **Phase 0 — DONE.** 24/24 Sponza albedo textures decode/upload
  (`FAsyncTextureLoader`, `.ktx2`); inventory confirms real paths + factors;
  palette-hash root cause captured in logs.
- **Phase 1 — DONE.** UV re-added; per-mesh texture binding in the GBuffer;
  `MRT2` samples real albedo. Pillars: warm stone (0.62, 0.58, 0.51) vs palette
  blue; material saturation 0.475 → 0.20; validator 6/6.
- **Phase 2 — DONE.** `FInstanceInfo` carries Roughness/Metallic (48B kept);
  `GBufferMaterial.a` = 0.898 roughness; GI reads it (roughness lobe in RayGen
  + closest-hit). Also fixed `ReadbackTextureFloats` forcing alpha=1.0 (bug-101,
  masked the roughness and the validator's alpha sentinel).
- **Phase 3 — DONE (average-albedo).** Per-mesh linear average albedo from the
  texture's last KTX2 mip → `RTInstanceInfo.AlbedoColor` → bounce shading uses
  real material colors (column_a gray, fabric_a red → correct color bleeding).
  Per-texel bounce sampling remains the documented future upgrade.
- **Phase 4 — DONE.** Palette/name-hash removed from `GetMeshAlbedo`
  (fallback = factor or 0.7 gray); material conventions documented in
  `docs/MATERIAL_CONVENTIONS.md`.
- **Phase 5 — DONE.** Interior + exterior runs both 6/6 validator PASS with
  real materials; screenshots in
  `evidence/material_rework_interior/` + `evidence/material_rework_exterior/`;
  spot-check table + before/after in `SCREENSHOTS_2026-08-10.md`.

**Material rework complete — including per-texel bounce sampling (Phase 3b,
2026-08-10).** The ray tracer now samples the hit point's real albedo texture
(descriptor array at t9, indexed by `AlbedoTextureIndex`), so GI bounces carry
actual texture detail. Remaining future work: normal maps (the current gltf
export has none) and a bindless refactor of the fixed 32-slot array.
