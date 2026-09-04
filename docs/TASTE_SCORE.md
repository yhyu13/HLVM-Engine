# HLVM-Engine Taste Score — Rubric & Spec

**Dated 2026-09-02 — Phase 4d of the four-phase autonomous run.**
**Source of truth:** `docs/FEATURE_ROADMAP_2026-09-01.md` + `docs/P0_PLAN.md`.
**Purpose:** A numeric, reproducible score (0–100) that grades a frame dump
on render *quality*, not test pass/fail. Lets multiple agents iterate
overnight, competing to push the score higher against a fixed reference
scene (Cornell Box, San Miguel, Sponza, Bistro).

---

## 1. What this is NOT

- **Not a validator.** Validators check *correctness* (does the trace
  fire, does BLAS build). Taste score checks *does it look good*.
  A frame can pass every validator and still be ugly (uniform gray
  GI, hot pixels, banding). The rubric catches the gap.
- **Not a single number.** It is **6 dimensions**, each 0–10, summed
  with weights. The breakdown tells you *why* the score moved, which
  is what makes iteration possible. A +2 lift on `light_transport`
  is more diagnostic than `+2 on total`.
- **Not subjective.** Each dimension has computable sub-metrics
  (LUT distance, gradient ratio, silhouette entropy, SSIM vs
  reference). The agent that produces a frame does not score its own
  frame; an independent scorer reads the dump + reference and emits
  numbers. Score can move only by 0.5 increments (anchored to
  measurement noise).

---

## 2. The six dimensions

### D1. PBR correctness (weight 1.5)
**"Does this obey physics on a flat surface?"**
- Sub-metrics:
  - `BRDF_sanity`: a known material sphere lit by a known envmap
    should reproduce the IBL specular lobe within ±5% of a
    pre-baked reference. Score 0–10 by LUT distance.
  - `Energy_conservation`: a white diffuse plane in direct sun
    should never exceed `albedo * cosTheta` luminance. Test with
    `TestPathTraceGI_Data/energy_test.json`.
  - `Gamma_end_to_end`: sRGB output matches linear-light input
    squared (5% tolerance).
- Score 10: textbook PBR, indistinguishable from Cycles path-traced
  ground truth.
- Score 5: plausible but wrong tone (over-saturated highlights,
  crushed blacks).
- Score 0: physically broken (albedo visible in shadow, neon
  indirect).

### D2. Light transport (weight 2.0 — highest)
**"Does the lighting in the image tell a coherent story?"**
- Sub-metrics:
  - `Direct_indirect_balance`: ratio of direct to indirect
    luminance. Cornell Box ceiling at y=2.99, light at y=0.99:
    wall-to-wall indirect should be 30–60% of direct lit wall.
  - `Color_bleeding`: red wall on white floor should show red bounce
    within the visible spectrum. Δ-E2000 vs reference > 5 = 0 pts.
  - `Skybounce_validity`: outdoor scene with sun should have
    measurable sky contribution on shadow side (not 0, not
    clamped). Confirms the v236 fix held (see
    `DIAGNOSTIC_2026-07-30.md`).
  - `Shadow_softness`: penumbra width matches light source size
    (analytic for area lights).
- Score 10: GI feels like real life. Light has a *history*.
- Score 5: GI present but flat — shadows wrong direction,
  bouncing light missing.
- Score 0: GI is noise or solid color blob.

### D3. Signal / noise / denoise (weight 1.5)
**"Is the image clean enough to look at without squinting?"**
- Sub-metrics:
  - `Noise_sigma` (8x8 patch stddev / mean): < 0.02 = 10 pts,
    0.02–0.05 = 7, 0.05–0.10 = 4, > 0.10 = 0.
  - `Firefly_count`: pixels > 5σ from local mean. > 100 = 0 pts
    on this sub-metric (the v236 firefly signature).
  - `Temporal_stability_sigma`: frame-to-frame pixel delta on a
    static camera. < 0.5% = 10, 0.5–2% = 6, > 2% = 0 (flicker).
  - `Denoise_artifacts`: no overshoot halos around high-contrast
    edges (bilateral / SVGF / ReBLUR check).
- Score 10: clean as a Cycles 4096-sample offline render.
- Score 5: visible noise but recognizable.
- Score 0: noise floor dominates (ReSTIR init, 1 SPP).

### D4. Composition & framing (weight 0.5 — lowest)
**"Does the camera frame the subject?"**
- Sub-metrics:
  - `Subject_in_third`: bounding box center is in 1/3 band (not
    dead-center, not edge-cropped).
  - `Horizon_line`: not at the horizon y = H/2 (rule-of-thirds).
  - `Foreground_midground_background`: scene depth bucketed;
    each bucket has ≥ 1 non-degenerate region.
- Score 10: cinematic framing.
- Score 5: acceptable default.
- Score 0: subject cut off / horizon through middle of face.

### D5. Material fidelity (weight 1.0)
**"Does each surface look like itself?"**
- Sub-metrics:
  - `Texture_resolution_match`: detail in albedo matches camera
    distance (no mip blur when close, no aliasing when far).
  - `Roughness_curve`: smooth surface shows envmap clearly,
    rough surface blurs it, mirror shows sharp reflection.
  - `Normal_map_response`: bumpy surface shows parallax-correct
    lighting (or fallback: tangent-space normal in screen space).
  - `Subsurface_proxy`: organic materials (skin, wax, leaves) show
    translucency hint (not yet shipped, so 0 for now — N/A).
- Score 10: chrome looks like chrome, wood like wood.
- Score 5: materials are visually distinguishable but "plasticky".
- Score 0: everything looks like the same default shader.

### D6. Temporal & interactive coherence (weight 1.0)
**"Does the scene hold up when you move?"**
- Sub-metrics:
  - `TAA_reprojection`: first frame after camera move has ≤ 5%
    ghosted pixels; subsequent frames have ≤ 0.5%.
  - `Motion_vector_consistency`: moving object has valid motion
    vectors (no smear trails).
  - `GI_temporal_accumulation`: ReSTIR / path-trace history blends
    correctly (no flash frames on cut).
  - `Dynamic_light_response`: light moving visibly affects shadow
    within 1 frame.
- Score 10: indistinguishable from a live movie.
- Score 5: minor ghosting or lag.
- Score 0: smear / strobe / judder.

---

## 3. Total score formula

```
total = (
    1.5 * D1_pbr
  + 2.0 * D2_light_transport
  + 1.5 * D3_signal_noise
  + 0.5 * D4_composition
  + 1.0 * D5_material
  + 1.0 * D6_temporal
) / (1.5 + 2.0 + 1.5 + 0.5 + 1.0 + 1.0)
# weight sum = 7.5, so the result is naturally 0–10; multiply by 10 to get 0–100.
```

**Why these weights?**
- D2 (light transport) is highest because it is the *defining
  capability* of a path tracer — if GI is wrong, nothing else saves
  the image.
- D1 (PBR) and D3 (signal) tied at 1.5 because both gate "is this
  even renderable" — wrong physics or unviewable noise = 0.
- D4 (composition) is lowest because a test frame with bad framing
  is still scorable on the technical dimensions; this isn't a film
  school.

---

## 4. Reference scenes

The harness evaluates on **3 canonical reference scenes** — the
baseline that competing agents improve from.

| Scene          | Path                              | What it stresses            |
|----------------|-----------------------------------|------------------------------|
| Cornell Box    | `TestPathTraceGI_Data/CornellBox_Lights.json` | D2 light transport, D3 noise at low SPP |
| San Miguel     | `Samples/Scenes/SanMiguel.gltf` (TODO: stage) | D5 materials, D4 composition |
| Sponza         | `Samples/Scenes/Sponza.gltf` (TODO: stage) | D1 PBR at scale, D6 temporal |
| Bistro         | `Samples/Scenes/Bistro.gltf` (TODO: stage) | Outdoor + skybounce (v236 fix) |

The Cornell Box is the *gate* — its score must be ≥ 70/100 for
the overall pass. Other scenes are bonus.

---

## 5. How to measure (compute contract)

The scorer agent reads:
- `Binary/Debug/dumps/<run_id>/*.exr` or `.png`
- Reference render: `docs/reference_renders/<scene>_reference.exr`
  (pre-baked ground truth, generated by offline Cycles, committed
  once).
- Camera state: `Binary/Debug/dumps/<run_id>/camera.json`.

Output: `docs/SCORES/<run_id>.md` with six dimension scores + total,
plus a 2-line reason for each dimension's score.

The scorer is **independent of the renderer agent** — different
profile, fresh context, no incentive to over-score.

---

## 6. Anti-gaming rules

To prevent agents from "optimizing the metric":

1. **Reference render is committed once and frozen.** No agent may
   edit `docs/reference_renders/`. Hashes are checked by the
   dispatcher at the top of each round.
2. **Score moves by 0.5 increments only.** Rounded scores below 0.5
   are zero. This prevents over-fitting.
3. **All 3 reference scenes are scored.** Optimizing only Cornell
   Box while breaking Bistro yields a net score *drop* (Bistro is
   weighted equally).
4. **D3 noise penalty is non-linear.** A score that achieves D2 = 10
   by jacking SPP to 4096 with no denoiser gets D3 = 0, so the
   total is gated. Real fix: better denoiser, not raw SPP.
5. **Submitter cannot be the scorer.** Different profiles, different
   prompts, different memory.

---

## 7. First-night baseline

What we expect before the cron runs overnight (best-effort guess
based on the v236 sky-bounce fix landing and the v248 ReSTIR
rewrite in progress):

| Scene       | Total today (best guess) | Total overnight goal |
|-------------|--------------------------|-----------------------|
| Cornell Box | ~55 / 100                | ≥ 70 / 100            |
| San Miguel  | not staged yet           | stage + ≥ 60 / 100    |
| Bistro      | not staged yet           | stage + ≥ 50 / 100    |

The score going UP overnight is the metric we report to the user
in the morning.

---

## 8. What the user sees

Every morning, the cron's final summary (delivered to chat) shows:
- Yesterday's best score per scene.
- The diff vs the day before (delta + per-dimension breakdown).
- The winning agent's strategy (what they changed).
- A copy of the winning frame inline (PNG attached).

This is the *visible* output of the overnight run — the user opens
the morning chat and sees a frame that's better than yesterday's.
The iteration loop is:
`render → score → identify weakest dimension → improve → repeat`.

---

## 9. Out of scope for taste score

- **Performance / framerate.** That's a separate harness (the
  `render-quality-loop` skill). Taste is purely image quality.
- **Subjective UX.** No human-in-the-loop scoring. The user can
  *override* the score by replying to the morning digest
  ("yesterday's frame looked great, today's is muddy — D2 fix
  reverted?"). The override is logged but doesn't change the
  numeric score.
- **Video / animation.** Taste is per-frame for now. Temporal
  stability (D6) is the only motion metric.

---

## 10. Self-review (critic loop on this doc)

- Round 1 (this doc): produced.
- Round 2 (to run before cron starts): a fresh-context critic
  subagent should:
  1. Are the six dimensions non-overlapping?
  2. Do the weights match the project's actual bottlenecks (ReSTIR
     GI, sky-bounce, denoise) from the v236/v248 diagnostic trail?
  3. Are reference scenes reasonable (not trivially gamed, not
     impossibly hard)?
  4. Is the 0.5-increment granularity appropriate for overnight
     iteration (not too coarse, not too fine)?
- Hard cap: 1 critic round. Apply fixes inline, ship.

---