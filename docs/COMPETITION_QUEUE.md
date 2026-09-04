# HLVM-Engine Taste-Score Competition — Active Queue

**Initial priority order** (last updated: 2026-09-02, first dispatch)
**Re-ranked after cycle 0 round 0** (2026-09-03): see "Status" below.
**Re-ranked after cycle 1 round 0** (2026-12-17, manual tick): see below.

| Rank | Profile           | Specialty       | Target dim | Weight | Priority score |
|------|-------------------|-----------------|------------|--------|----------------|
| 1    | `conserver-noise` | Signal / noise  | D3         | 1.5    | +15 (cycle 1: delta_total = +1.75 > 0 → +10; delta_target_dim (D3) = +1.5 > 0 → +5; no other dim regressed → 0) |
| 2    | `conserver-pbr`   | PBR correctness | D1         | 1.5    |   0 (parent override for cycle 2 — explicit test directive; D1 at 4.0 next-lowest non-tied dim after D5/D6 floor) |
| 3    | `conserver-gi`    | Light transport | D2         | 2.0    |   0 (cycle 0: D2 lift direction confirmed, held per bootstrap rule) |
| 4    | `conserver-mat`   | Material fidelity| D5        | 1.0    |   0 (D5 tied-lowest at 3.0; deferred to cycle 3) |

**Cycle 1 re-rank note:** `conserver-noise` promoted to top per HARNESS §7
(delta_total > 0 + delta_target_dim > 0). For cycle 2 the parent
session explicitly directed `conserver-pbr` to test the D1 axis (per
manual end-to-end test instruction). Natural next-weakest-dim logic
would have been `conserver-mat` (D5 = 3.0); parent's explicit override
takes precedence in this manual tick. Cycle 3 reverts to algorithmic
ranking.

**Bootstrap rule (cycle 0):** first scored cycle does NOT re-rank the
queue. From cycle 1 onward, HARNESS §7 ranking kicks in.

Re-rank after each scored cycle per `docs/COMPETITION_HARNESS.md §7`.

## Cycle 0 outcome

- Total: **45/100** (synthetic-build baseline; real engine render pending).
- D2 lift direction confirmed (un-clamp sky-bounce ≈ v236 fix hypothesis).
- Weakest dims: D5 = 3.0, D6 = 3.0 (tied) — but these are unmeasurable on
  synthetic frame, so the real-render evidence will re-rank.

## Per-cycle budget

- Wall-clock: 30 min total per cycle
- Diff: ≤ 200 lines
- SPP cap: 32 (lower is better; agent scoring well at 8 SPP wins D3)
- Build target: `TestPathTraceGI`

## Single-profile fallback

If host has only one worker profile (likely the case on this host),
all `conserver-*` profiles collapse to "same head with different
prompt text." This is acceptable but reduces "specialist" to
prompt-routing. The `scorer` MUST still be a separate prompt (or
profile) with `enabled_toolsets: ["file"]` only — no source-code
write access.

In this cron (single-profile mode), the scorer role is run inline
per `docs/agents/dispatcher_competition.md §"Single-profile fallback"`:
"the dispatcher runs the scorer role inline with `enabled_toolsets`
enforced via prompt — the prompt text explicitly forbids `patch` /
`write_file` to source code paths." This is what cycle 0 round 0
used.

## Cycle 1 outcome

- Total: **47/100** (+1.75 vs cycle 0; D3 lift from 5.0 → 6.5).
- D3 first measurable noise score: σ ≈ 0.04, ~35 fireflies (below 100-floor).
- Weakest dims: D5 = 3.0, D6 = 3.0 (tied) — both still unmeasurable on synthetic.
- Queue re-rank per HARNESS §7: conserver-noise promoted to rank 1.

## Status

- [x] Cycle 0 dispatched
- [x] First scored cycle (45/100 baseline, synthetic frame)
- [x] Cycle 1 dispatched (47/100, +1.75 Δ, D3 measurable)
- [x] First WIN marker (cycle 1 delta +1.75 > 0.5 → WIN logged)
- [ ] Cycle 2 dispatched (parent override: conserver-pbr → D1)

Last update: 2026-12-17 — cycle 1 round 0 scored (47/100), queue re-ranked.
