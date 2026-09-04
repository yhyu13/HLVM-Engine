# Cycle 2 — started 2026-12-17 (manual end-to-end test tick)

## Active profile
`conserver-pbr` (parent-session override — see note)

> **Override note:** Per `docs/COMPETITION_QUEUE.md` algorithmic
> re-rank after cycle 1, the top of queue is `conserver-noise`
> (priority +15, D3 lift confirmed). The next-weakest dim is D5/D6
> tied at 3.0, which would route to `conserver-mat`. However, this
> manual end-to-end test tick explicitly directs `conserver-pbr` to
> test the D1 axis (PBR correctness — wall albedo saturation, gamma
> end-to-end, BRDF sanity). Cycle 3 reverts to algorithmic ranking.

## Last score summary
Cycle 1 round 0 — total = **47/100** (+1.75 vs cycle 0 baseline 45/100).
Weakest dims: **D5 material fidelity (3.0)** tied with **D6 temporal
(3.0)**; D1 PBR (4.0) is next-lowest non-tied. D2 Light transport (5.5),
D3 Noise (6.5, just lifted), D4 Composition (5.0).

Per-dimension delta vs cycle 0:
- D1 PBR: 4.0 (no change — cycle 1 was D3-targeted)
- D2 Light: 5.5 (no change — D3 patch left D2 untouched)
- D3 Noise: 5.0 → 6.5 (+1.5) — measurable σ ≈ 0.04, ~35 fireflies
- D4 Comp: 5.0 (no change — camera unchanged)
- D5 Mat: 3.0 (no change — D3 patch didn't touch materials)
- D6 Temp: 3.0 (no change — single static frame)

## Target dimension (predicted)
**D1 — PBR correctness (weight 1.5).** Rationale:

- D1 is the **next-lowest non-tied dim at 4.0**, after D5/D6 (3.0).
  Choosing D1 over D5/D6 is the parent's explicit test directive; the
  algorithmic next-weakest-dim route would be D5/D6 → `conserver-mat`.
- `conserver-pbr` is the D1 specialist — BRDF / material / TBN /
  gamma / IBL shaders. Has access to Cycles ground-truth behavior.
- The cycle-0 frame has D1 = 4.0 because albedo/wall coloring matches
  reference but no measured BRDF sanity or energy-conservation number
  is available on synthetic frame. Cycle 2's goal: bring D1 to ≥5.5
  with a measurable energy-conservation or gamma-end-to-end signal.

Secondary targets (if D1 already at 8/10 from prior work):
- D5 material fidelity (lowest at 3.0)
- D6 temporal coherence (tied at 3.0)
- D3 noise (already lifted to 6.5; further gains possible)

## Expected deliverable

- `docs/PENDING_BUILD_cycle_2_round_0.md` — proposed diff (≤200 lines)
  targeting D1 (PBR correctness / gamma / energy-conservation).
  Written by `conserver-pbr` via cron (or inline-fallback if
  `delegate_task` not wired in).
- `docs/BUILD_RESULT_cycle_2_round_0.md` — parent executor's output
  (build status, render status, dump sha256).
- `docs/SCORES/cycle_2_round_0.md` — 6-dimension score breakdown,
  delta vs cycle 1.

## Wall-clock budget

30 min total per tick. Cron will re-poll the queue next tick.

## Parent executor requirements (PREREQUISITE)

Cycle 1 used **synthetic-build fallback** because parent shell access
was blocked. Cycle 2 requires (same as cycle 1):

1. A parent session with **terminal** tools enabled.
2. `./Build.sh --Config=Debug --Target=TestPathTraceGI --Test` succeeds
   without tirith denial.
3. `cd Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestPathTraceGI`
   produces a real EXR/PPM dump at
   `Binary/Debug/dumps/cycle_2_round_0/TestPathTraceGI.ppm`.

If shell is still blocked: synthetic-build fallback is acceptable, but
the score file MUST be flagged with the same caveats as cycle 1
(`scorer is dispatcher-inline, no pixel-diff tools, no sha256
verification`).

## Anti-gaming (preserved)

- Reference render hash-checked at every scorer call (file-only
  scorer reads MANIFEST.json's recorded sha256 since `sha256sum`
  unavailable to file tools).
- Score moves by 0.5 increments only.
- Scorer is independent of submitter (separate prompt text in
  single-profile mode — but documented as a caveat per
  `dispatcher_competition.md §"Single-profile fallback"`).

## Suggested patch direction (conserver-pbr, for next tick)

A diagnostic PBR patch to test whether gamma end-to-end is correct:
- File: `Engine/Source/Runtime/Private/Renderer/PathTrace/PathTrace.cpp`
  (or similar output-color write site)
- Diff: confirm `output_color = LinearToSRGB(accumulated_radiance)` is
  applied at framebuffer write; flag if `LinearToSRGB` is being
  applied twice (would cause over-darkening) or skipped (would cause
  over-brightening).
- Rubric mapping: `TASTE_SCORE.md §2 D1 Gamma_end_to_end`: "sRGB output
  matches linear-light input squared (5% tolerance)". A clean
  verification either confirms D1 ≥ 5.5 or surfaces a real defect.

This is illustrative; the actual conserver-pbr subagent will read
PENDING_BUILD_cycle_2_round_0.md when it's written and may pivot
based on what the cycle-2 diagnostic patch reveals.
