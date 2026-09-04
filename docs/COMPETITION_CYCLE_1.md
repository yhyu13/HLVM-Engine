# Cycle 1 — started 2026-09-03T00:00:00+08:00

## Active profile
`conserver-noise` (parent-session override — see note)

> **Override note:** Per `docs/COMPETITION_QUEUE.md`, queue top is
> `conserver-gi` (bootstrap rule: first cycle doesn't re-rank). However,
> the parent session explicitly selected `conserver-noise` for cycle 1
> to test the most likely dimension-to-diverge in a real engine render
> (synthetic cycle 0 can't validate noise sub-metrics). Once a real
> engine render lands, the queue re-rank per HARNESS §7 kicks in.

## Last score summary
Cycle 0 round 0 — total = **45/100** (baseline). Weakest dim = **D5
material fidelity (3.0)** tied with **D6 temporal (3.0)**. D1/D2/D3/D4
all between 4.0 and 5.5. Caveat: synthetic-build frame; real engine
render pending parent executor with shell access.

## Target dimension (predicted)
**D3 — Signal / noise / denoise (weight 1.5).** Rationale:

- The cycle-0 score has D3 at 5.0/10 (default mid) because the
  synthetic frame is noise-free — but that score is meaningless: we
  don't know whether a real engine render at 8 SPP would produce
  acceptable noise or firefly hell.
- A real render requires the parent executor to actually run
  `./TestPathTraceGI` with `HLVM_RGI_ACCUM=8`. Until that happens,
  D3 is the most likely dimension to either jump (if denoiser is
  working) or collapse (if ReSTIR init is noisy).
- `conserver-noise` is the specialist — ReSTIR reservoir sampling,
  temporal accumulator, denoise configuration. If the engine has a
  noise floor at 8 SPP, this profile can attack it directly.

Secondary targets (if D3 already at 8/10 from prior work):
- D5 material fidelity (lowest at 3.0)
- D6 temporal coherence (tied at 3.0)

## Expected deliverable

- `docs/PENDING_BUILD_cycle_1_round_0.md` — proposed diff (≤200 lines)
  targeting D3 (noise/denoise). Written by `conserver-noise` via cron.
- `docs/BUILD_RESULT_cycle_1_round_0.md` — parent executor's output
  (build status, render status, dump sha256).
- `docs/SCORES/cycle_1_round_0.md` — 6-dimension score breakdown,
  delta vs cycle 0.

## Wall-clock budget

30 min total per tick. Cron will re-poll the queue next tick.

## Parent executor requirements (PREREQUISITE)

⚠️ Cycle 0 used **synthetic-build fallback** because parent shell
access was blocked. Cycle 1 requires:

1. A parent session with **terminal** tools enabled.
2. `./Build.sh --Config=Debug --Target=TestPathTraceGI --Test`
   succeeds without tirith denial.
3. `cd Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8
   ./TestPathTraceGI` produces a real EXR/PPM dump at
   `Binary/Debug/dumps/cycle_1_round_0/TestPathTraceGI.ppm`.

If shell is still blocked: synthetic-build fallback is acceptable, but
the score file MUST be flagged with the same caveats as cycle 0.

## Anti-gaming (preserved)

- Reference render hash-checked at every scorer call (file-only
  scorer reads MANIFEST.json's recorded sha256 since `sha256sum`
  unavailable to file tools).
- Score moves by 0.5 increments only.
- Scorer is independent of submitter (separate prompt text in
  single-profile mode).
