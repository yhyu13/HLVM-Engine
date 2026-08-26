# Final State (REVISION) — TestReSTIR_GI_Temporal — 2026-08-09

> **Status: SUPERSEDES `final-state-2026-07-22.md` and `final-state-2026-07-23.md`.**
> Evidence base: the 2026-08-08 uncommitted working tree, the three same-day
> runs logged in `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal{,_1,_2}.log`,
> and the Aug-08 dump set in `TestReSTIR_GI_Temporal_Data/dumps/`.

## TL;DR

The pipeline is **alive end-to-end** in the current uncommitted tree: Sponza →
GBuffer → GI path trace → ReSTIR (Generate/Temporal/Spatial) → ReBLUR →
GIAccumulate → display, and `validate_restir_gi.py` passes 4/4 on the Aug-08
dumps. The last three commits/documents in this folder describe a dead or
magenta pipeline; they are historical, not current.

**UPDATE (same day — all four issues below were fixed; see
`FIX_LOG_2026-08-09.md`):** the final 20:45 run passes **6/6**, the display is
**0% black** with a blue sky, ReBLUR measurably denoises, and the ReSTIR
spatial pass performs a real weighted-average resolve.

1. **ReBLUR is a no-op** — `denoised` is pixel-identical to `spatial`
   in effect (HF reduction 0–2%; the "maxdiff 0.0" claim in this doc's draft
   was comparing two *black* bypass dumps — corrected). Root causes:
   `SpatialAlpha` was never sent, uninitialized GBuffer sky texels produced
   NaN weights, and the view-space plane rejection was garbage. **FIXED —
   Phase 1.**
2. **ReSTIR spatial output ≈ raw GI pass-through** — `spatial` has the same
   mean/max as `gi_raw`, `W` is exactly 1.000, and M only accumulates to ~4.6
   over 8 frames. **FIXED — Phase 4:** MIS-weighted average resolve +
   relative-depth rejection; spatial HF now drops below gi_raw on G/B.
3. **Display quality: 56% of pixels are black** — **FIXED — Phase 3:** RayGen
   writes `SampleSky(primary ray)` for no-geometry pixels; display black% is
   now 0.0%.
4. **Validator blind spots** — **FIXED — Phase 2:** 6 checks now include
   ReSTIR-channel liveness and denoise-effectiveness; bypass runs correctly
   fail unless `HLVM_VALIDATE_ALLOW_BYPASS=1`.

The entire fix set (Aug-07/08 fixes + today's four phases) is still
**uncommitted** — commit before further work.

## What works (measured, 2026-08-08 17:28 run — non-bypass)

Run evidence: `TestReSTIR_GI_Temporal_1.log`; dumps `20260808_172853/54/55_*`
(rotated out of `dumps/` by the later run — the identical stats are
reproduced in the log itself).

| Stage | Evidence | Verdict |
|-------|----------|---------|
| Sponza load + GBuffer raster | 24 meshes, 188,568 verts, 786,723 indices; worldpos range [-19.2, 18.0], depth max 40.38, material albedo max (0.72, 0.63, 0.68) | **Works** — real geometry, correct coordinate ranges |
| GI path trace (FGIPass) | `gi_raw` mean (0.18–0.29), max 1.62, structured | **Works** — first-hit, direct + indirect visible |
| ReSTIR Generate/Temporal/Spatial | reservoir M mean 4.57 / max 8 (MaxM=30), W mean 1.000; `spatial` non-zero, same mean as gi_raw | **Alive** — dispatches execute, reservoirs fill; quality is pass-through (below) |
| GIAccumulate + display | display mean (0.37, 0.34, 0.31), std 0.39, 4×4 cell-mean std 73, alpha sentinel 100% | **Works** — tonemapped, structured image |
| Validator | 4/4 checks PASSED (exit 0) | **Works on the Aug-08 dump set** |

The Aug-08 17:30 dumps in `dumps/` show `spatial`/`denoised` as pure black —
that run was launched with **`HLVM_RGI_BYPASS=1`** (log line: "displaying
gi_raw directly (ReSTIR skipped)"). Black spatial/denoised there is expected
bypass behavior, **not** a pipeline regression.

## What was fixed on 2026-08-09 (all four issues)

1. **ReBLUR denoises** — `SpatialAlpha` now comes from `BlurParams`, the 4
   GBuffer MRTs are cleared each frame (kills the NaN sky-texel weights), the
   broken view-space plane rejection became a relative-depth bilateral, and
   defaults are BlurRadius 8 / NormalWeight 2 / PlaneWeight 12 / alpha 1.0.
   HF −4.8%/−5.3%/−14.2%, zero NaN. (`FReBLURPass.cpp`, `FReBLURPass.h`,
   `ReBLUR_cs.hlsl`, `TestReSTIR_GI_Temporal.cpp`)
2. **Validator is 6 checks** — `check_restir_alive` (spatial/denoised must be
   non-black) + `check_denoise_effective` (MAE > 0.5 and HF ratio < 0.99),
   both skippable via `HLVM_VALIDATE_ALLOW_BYPASS=1`; display-std threshold
   recalibrated 30→20 for the new sky. Bypass runs now FAIL.
3. **Sky background** — RayGen writes `SampleSky(primary ray)` for
   no-geometry pixels; ReBLUR passes sky through instead of black. Display
   black% 56% → 0.0%. (`GIPathTracing.hlsl` test-data copy, `ReBLUR_cs.hlsl`)
4. **Spatial resolve** — `ReSTIR_Spatial_cs.hlsl` now outputs the
   MIS-weighted average `Σ(w_i r_i)/Σ(w_i)` with relative-depth rejection;
   G/B HF drops below gi_raw, per-pixel MAE vs gi_raw is 41–60.

Remaining known limits (not regressions): camera is static (identity temporal
reprojection), the reservoir lacks a ray direction/Jacobian (architecture
limit per `deepseek/PLAN.md`), and the resolve is the biased weighted-average
form (the unbiased `selected·W` remains available).

## Why the historical bugs happened (lessons already paid for)

| Bug | Symptom | Root cause | Fix (in uncommitted tree) |
|-----|---------|------------|---------------------------|
| bug-073 | all-black dumps, exit 0, validator "passed" | NVRHI validation wrapper `open()` returned early on immediate-command-list counter overflow → every recorded command became a no-op | NVRHI patch: log warning instead of returning; validator trust rebuilt |
| bug-088 | raster pass produced no fragments | per-frame CommandList closed but never submitted; next `open()` discarded recorded work | execute the per-frame CL before dump (commit 9a09df2) |
| bug-074 | GBuffer dumps black even with sentinels | RGBA32F readback normalization / wrong state | per-channel dump normalization (commit 2fab7d6); note: normalization maps *constant* images to black — a trap for future analysis |
| bug-075 + v151 | temporal/generate dispatch VUIDs + garbage | mixed SRV+UAV in one descriptor set; nvrhi binds sets before barriers land → wrong image layouts | split layouts (set 0 SRV-only, set 1 UAV-only) + `space1` UAV declarations; **generation split (v151) is the Aug-07 uncommitted fix** |
| bug-069 | validator green on black frames | validator accepted "any file in dumps/" | prune dumps per run / require latest stamp group (still not enforced — see #4) |
| — (17:27 run) | whole chain black + `VUID-vkCmdDispatch-None-08600` "set (1) out of bounds (1 bound)" | old binary with unsplit generation layout (`VUID-VkComputePipelineCreateInfo-layout-07988` at pipeline create) | rebuilt at 17:28 with the v151 split; no VUIDs in the 17:28/17:30 runs |

## Operational hazards found during this revision

1. **`HLVM_RGI_BYPASS=1` silently changes dump semantics.** The current
   validator now detects bypass runs (check 5) unless
   `HLVM_VALIDATE_ALLOW_BYPASS=1`; still, bake the mode into dump filenames
   for clarity.
2. **The Aug-08 fixes are uncommitted.** `git status` shows 44 modified files,
   ~1,100+ inserted lines, plus deleted old dumps (and today's round-2 fixes).
   Commit this before further work.
3. **`.wolf/buglog.json` has 92 entries; bug-077…092 are auto-generated
   placeholders** ("Significant refactor of …", no root cause/fix). Today's
   fixes added bug-093…097; quarantine the placeholders before the next cron
   review.
4. ~~GBuffer input layout UV/Tangent warnings~~ — **fixed 2026-08-09**
   (round 2.1).
5. ~~Vulkan validation layer hardcoded~~ (bug-076) — **verified already
   CVar-gated** (`UseValidationLayers` / `UseDebugRuntime`); no hardcode in
   the current tree.
6. **GIPathTracing.hlsl exists in three places** — synced to identical
   content on 2026-08-09 (round 2.2); keep them in sync when editing.

## Suggested next steps (in order)

1. Commit the current uncommitted tree (the working state is good; don't lose it).
2. Keep the ReBLUR/ReSTIR params in CVars so the tuning is tweakable at runtime.
3. Revisit ReSTIR reservoir math (RealEngine-aligned sample representation) —
   per the Aug-01 PLAN.md — once the current tuned state is accepted.
