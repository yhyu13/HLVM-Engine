# JOURNEY — 50_ReSTIR_GI_Temporal (with 51_PathTraceGI_Debug)

How the ReSTIR GI line of HLVM-Engine was actually built: the human's
decisions and corrections on the left, the AI's builds, falsifications and
fixes on the right. Rebuilt from the durable record — `old/FIX_LOG_*.md`,
`FIX_LOG_2026-08-2*.md`, `FAIL_LOG_2026-08-26.md`, `PLAN_*.md`,
`old/Card_t_*.md`, `51_PathTraceGI_Debug/session-PathTraceGI_payload_debug.md`,
`.wolf/cerebrum.md`, and the git log of `rhi2`.

**Legend: `ME` = the human, `YOU` = the AI.** Dates are `YYYY-MM-DD`.

---

## Era 1 — Origin: the long-term goal is named (2026-05-28 → 2026-06)

| ME | YOU |
|---|---|
| "…plan first self critic plan and improve, from urgent to intermeidate from simple to complex. **Our long term goal is a restir ray tracing path tracer** (needs building blocks from dounts)" | Built the Donut-samples integration line: RT shadows → RT reflections → few-bounce GI → bilateral → TAA, each a test with a gate. |
| Pastes a critique of the Phase-4 plan: "'Path tracing blocks RESTIR' is overstated… The plan conflates 'needs path tracing infrastructure' with 'needs full path tracer.'" | Killed the over-scoped plan; few-bounce GI became the substrate ReSTIR would reuse. |
| (repeated corrections land as permanent rules) | Banked the first Do-Not-Repeats in `.wolf/cerebrum.md`: no `FrameIndex` in the candidate-position hash (bug-046 flicker); don't discard reservoir W "to be stable"; **pairwise MIS fixes spatial bias at the source, weighting downstream is a band-aid** (4-frame variance 12.30 → 2.08). |

## Era 2 — Cornell Box becomes the canonical gate (2026-06-13 → 07-19)

| ME | YOU |
|---|---|
| Declares `TestCornellBoxGI` the canonical GI regression test: "Keep this test green; regressions in the same class will trip it." | Built the Cornell pipeline (CPU GBuffer raycast, BLAS/TLAS, GIPathTracing, ReBLUR, ReSTIR spatial MIS) with numeric gates: 0% black, temporal drift ≤5%, red+green bleed on the floor. |
| (07-19) Cornell "passes" its luminance gate but shows a uniform gray-brown noise field. | `51_PathTraceGI_Debug/session-…md`: 4 stacked root causes — slangc **dead-strips RT payload fields per entry point** (payload layout desync → red/black GI noise), camera at box center with 60° FOV (one flat wall), cross-product GBuffer normals pointing outward, area light coplanar with the ceiling (floor black + fireflies). Bugs 2–4 were *invisible* until bug 1 was fixed. Working rules recorded: **trust measurements, not code reading; bisect the chain with debug visualizations; one variable per experiment; keep a known-good control.** |

## Era 3 — TestReSTIR_GI_Temporal bootstrap, on kanban cards (2026-07-16 → 07-26)

| ME | YOU |
|---|---|
| Creates card `t_e2742ccf`: "fix: TestReSTIR_GI_Temporal sblob path resolution". | Fixed the path; test still produced black frames — honestly marked the card `blocked/needs_input` instead of declaring victory: "Completing acceptance requires a broader rendering-pipeline task, not the scoped path repair." |
| Cards `t_8291cf8c` → `t_fb91e5cf` → `t_139c4e41`: scoped-down "first colored pixel", then real Sponza GBuffer, then dump scaffolding. | Hardcoded-quad GBuffer fill, then real `GBufferPT_VS/PS` Sponza MRT pass, then `HLVM_DUMP_RGI` + `WriteGBufferSentinels()` (magic values per channel so "GPU work dropped" is distinguishable from "rasterized black"). **Sin committed in passing:** the validator was relaxed from 4 structural checks to a single `mean luma > 0.05` so the uniform quad would pass. |
| (watching bug-088 drag on) | Found that one pass's Vulkan validation error **retroactively drops all earlier GPU work in the same nvrhi CommandList submission** — the raster pass's silent killer. Per-pass CommandList isolation became the standard debugging move; VVL false positive `VUID-vkCmdTraceRaysKHR-None-08608` later (08-14) root-caused into the VVL 1.3.280 source and fixed via graphics-pipeline re-bind. |

## Era 4 — The reckoning: "it does not display" (2026-08-08 → 08-10)

| ME | YOU |
|---|---|
| "1. **judge wrongs and rights** under …/50_ReSTIR_GI_Temporal. We still stuck at …/TestReSTIR_GI_Temporal.cpp **it does not display** 2. a lot useful learning under …/51_PathTraceGI_Debug" | Self-critique landed: the default binary ran the broken fake-ReSTIR path; the working bypass was opt-in. Then the honest verdict doc `PLAN_REALTIME_RESTIR_GAP_2026-08-10.md`: measured 0.7 s/frame (1.4 fps), and named the thing what it was — **"path tracing with a ReSTIR-flavored smoothing/reuse layer," not ReSTIR and not real-time.** Gap table vs ZetaRay/RealEngine (sample model, Jacobian, half-res trace, 20–40× perf). |
| (implicit: make it real) | **The seed of the later incident:** while re-framing for Sponza, the default camera was set to `(0, 2.5, 18) → (0, 2.5, -10)` with turntable yaw=90 — 7.6 m *outside* the building's outer shell. From this day on, the default Sponza run renders a featureless wall/sky gradient. Nobody looks at the pixels; the (statistical) validator passes. |

## Era 5 — Real materials, ReBLUR repaired, validator blind spots (2026-08-09 → 08-14)

| ME | YOU |
|---|---|
| "self critic and improve, then procced to next phase … still cannot render to display" | 08-09 FIX_LOG: ReBLUR was a pass-through — three stacked root causes (`SpatialAlpha` never sent; GBuffer MRTs never cleared → NaN weights; broken view-space reconstruction). HF std dropped only after all three fixed. Same log, Phase 2: **the validator never inspected `spatial`/`denoised`, so a bypassed/dead ReSTIR chain passed 4/4** — first documented instance of "the gate can't see the failure mode." 08-12: real materials + half-res ReSTIR pipeline land. |

## Era 6 — The ZetaRay ground-truth port (2026-08-21 → 08-22)

| ME | YOU |
|---|---|
| Green-lights the full re-port from ZetaRay (PLAN_RESTIR_ZETARAY_2026-08-21.md). | v210–v213: RIS candidate generation with `pdf = cos/π`; reservoir = second-path-vertex sample (pos/normal/ID/Lo/w_sum/W) across 3 half-res textures; pairwise-MIS temporal + spatial; segment-visibility ray queries; outlier suppression. Turntable temporal reuse fixed by reprojecting from GBuffer posW (a 0.5 px mismatch broke candidates at ≥0.18°/frame). Frame overlap enabled in the RHI (per-acquire-slot event queries). Result: **24 ms/frame, 40+ fps — real-time at last**; "validator 8/8"; convergence MAE 0.19%. Evidence screenshots: walls (unknown to all). |

## Era 7 — Independent verification falsifies the victory (2026-08-23, v233)

| ME | YOU |
|---|---|
| (standing instruction by now: verify, don't trust) | Re-ran the claims instead of rereading them: **"the '8/8 PASS' did not reproduce"** — firefly bound FAIL 53.14 static, noise gate FAIL 1.19 rotating. Fixed for real: enabled ZetaRay's commented-out Jacobian clamp, clamped generate-time `W = min(1/pdf, 256)` (measured 345.9 in the dump), added the wave-average anti-firefly clamp on the spatial estimate, and rotated history *normals* (not just positions) under the turntable. Also found the noise gate compared CVs of **two different physical quantities** (bare Lo vs Lo·albedo). |

## Era 8 — Backlog cleared, contracts repaired (2026-08-25, v234)

| ME | YOU |
|---|---|
| Frames the meta-lesson: "**…a knowledge-gathering problem, not model ability or prompting.** Every root cause in FIX_LOG_2026-08-22/23 is a fact the agent lacked…" | Cleared the whole backlog: ReBLUR's `(radiance, hitDist)` contract repaired end-to-end (spatial alpha = selected sample's `|x2−x1|`, resolve propagates it); `cv_lit=` exact float stats logged so the noise gate stops reading byte-quantized PNGs; validator test file rewritten (its import had rotted). Wall-era calibration of the bias gate recorded — unusable, as later discovered. |

## Era 9 — v215: real bug found, false summit declared (2026-08-26, morning–22:00)

| ME | YOU |
|---|---|
| Sets the goal: fully align with ZetaRay; Sponza on/off and vs path tracing; test Cornell too. | Found a genuine silent bug by *adding Cornell*: `TemporalResample2` took the reservoir **by value** (ZetaRay: `inout`) — the 2-candidate pairwise-MIS path discarded all merge writes. Sponza hid it (candidate[1] valid ~22%); Cornell's flat walls made it ~95% → M stuck at 1.07. Fixed → M saturates everywhere. Aligned `M_MAX` 30→10. Added `HLVM_RGI_SCENE=cornell` through the same pipeline; Cornell vs 256-frame PT reference: 7.4%. Pushed `a0a4277` and declared: "Goal complete… validator 9/9 PASS." **Every Sponza number in that claim was measured on the wall framing from Era 4** — including "MAE 0.029, 5.4% vs path tracing," which compared a wall to a wall. The post-push camera fix was begun at 22:03 and left uncommitted, referencing a FAIL_LOG that didn't exist. |

## Era 10 — The audit: the pixels get read (2026-08-26, 22:52+, v235)

| ME | YOU |
|---|---|
| Supplies the goal-persistence contract (durable state, idle self-start, anti-drift steering, **completion audit against actual state, never on the model's say-so**) and one word: "critic … and improve". | Audited the claim against evidence: commit real, Cornell real, **Sponza evidence void** (visual proof: featureless gradient; saturated-pixel fraction 0.0000 on the "PT comparison" dumps). Completed the orphaned camera fix (in-atrium south aisle, yaw 90→0, scene AABB measured from gltf accessors). Added the missing semantic gate: `scene_content` ≥1% pixels with HSV saturation > 0.15 — proven: old validator passes the wall dumps 8/8, new one fails only that gate. Wrote FAIL_LOG_2026-08-26.md, corrected the FIX_LOG, committed `12e5ec0`. Real content then exposed two genuine bugs the wall had hidden: static spatial estimate is a starfield carrying ~37% of raw energy (M-cap A/B ruled out), rotating noise gate 1.095. The goal stands open — honestly measured at last. |

---

## How this project teaches vibe coding with AI

### The human's job — decide, correct, kill

- **Name the real destination.** "Our long term goal is a restir ray tracing path tracer" (05-28) organized months of work; every era is a building block toward it.
- **Correct the frame, not the detail.** "'Path tracing blocks RESTIR' is overstated" killed an over-scoped plan in one paragraph. "judge wrongs and rights… it does not display" (08-08) ended an era of polishing a pipeline that rendered nothing.
- **Demand falsification as a verb.** The recurring one-liner "self critic and improve" — and finally the goal-persistence contract with a completion-audit clause — forced two victory claims to be re-measured. Both collapsed: v213's "8/8 PASS" (falsified 08-23) and v215's "5.4% vs path tracing" (falsified 08-26 by reading the actual PNGs).
- **Name the meta-lesson.** "A knowledge-gathering problem, not model ability" (08-25) reframed the whole bug log: the fixes were facts the agent lacked, not reasoning it failed.

### The AI's job — instrument, falsify, report honestly

- **Instrument before theorizing.** The 51 retrospective's rule — "trust measurements, not code reading; I 'proved' the shader math correct on paper twice. It was still broken" — became the dump/validator/cv_lit machinery that every later era relied on.
- **Falsify your own predecessor.** v233's "the 8/8 did not reproduce" and v235's wall audit were both the AI re-running claims instead of inheriting them. The pipeline improved exactly when verification was treated as an experiment, not a formality.
- **Report the negative in writing.** The blocked card (`t_e2742ccf`), the "ReSTIR-flavored smoothing layer" verdict (08-10), and FAIL_LOG_2026-08-26 are the highest-value docs in the project — each one stopped future work from building on a false floor.

### The portable rules (each bought with a real event)

1. **Statistical gates can't substitute for looking at the image.** A smooth gradient passed black-ratio, variance, cell-variance, temporal-stability, noise, bias, M-accumulation and firefly gates for 16 days (2026-08-10 → 08-26).
2. **Every validator needs a semantic content gate** — a property the failure mode cannot fake. Variance floors measure *that* pixels differ, not *what* they show. (v235 `scene_content`, calibrated 0.0000 wall vs 0.096 Sponza vs 0.58 Cornell.)
3. **Verify claims by re-running them, not by rereading them.** Both false summits (8/8, 5.4%) fell to re-execution, not debate.
4. **The camera is part of the algorithm under test.** Twice: Cornell's 60°-at-box-center flat wall (07-19) and Sponza's exterior-wall default (08-10). Framing bugs masquerade as algorithm results.
5. **Fix order: signal first, polish second.** Bugs 2–4 in 51 were invisible until the payload desync was fixed; the Sponza spatial starfield was invisible until the camera was fixed.
6. **One correction → one durable Do-Not-Repeat.** bug-046's pairwise-MIS lesson, the CommandList-poisoning rule (bug-088), the payload-compaction rule (slangc dead-stripping) — each correction banked in `.wolf/cerebrum.md`/AGENTS.md never recurred.
7. **A fix without a committed FAIL_LOG is an orphan.** The 22:03 camera fix sat uncommitted for 50 minutes referencing a document that didn't exist; a session boundary would have erased the entire story.
8. **New scene = new falsifier.** The `inout` bug was invisible on Sponza and dominant on Cornell. Porting to a second scene with different statistics is the cheapest correctness probe there is.

### One-sentence takeaway

The human supplies the standard of evidence — "critic, and improve" — and the AI supplies the measurement machinery; this project advanced exactly when the machinery was forced to answer *"what is actually on screen?"* and stalled every time it answered *"the numbers look fine."*
