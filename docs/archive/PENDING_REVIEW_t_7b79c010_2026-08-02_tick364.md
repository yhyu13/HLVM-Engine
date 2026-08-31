# Review for card t_7b79c010: Continue GBuffer SRV binding bisect in TestReSTIR_GI_Temporal

**verdict:** HUMAN_REQUIRED
**reviewer:** kanban-cron-overseer (v2.4.0)
**timestamp:** 2026-08-02 cron tick (tick 364, file-only)

## Stage-1 health (file-only harvest this tick)

- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/`: 7 PNGs, all timestamped `2026-08-01 23:17:02..04` (stale v142). No fresh `HLVM_PT_DEBUG_MODE=20` re-run since.
- `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log`: present (still the v142 log; tail read in tick 362 — `Completed test_ReSTIR_GI_Temporal (#1) in 7.253952643 seconds` at 23:17:04.380).
- `docs/DIAGNOSTIC_2026-07-30.md`: present, unchanged. `docs/DIAGNOSTIC_2026-08-01-v25.md`: present, unchanged. No v26/v27 diagnostic.
- `docs/PENDING_PLAN_v142.md` is the latest committed plan; no `docs/PENDING_PLAN_v143.md`. `docs/PENDING_COMMIT_v142.md` is the latest committed commit; no `PENDING_COMMIT_v143.md`.
- All four prior escalation files persist: `docs/OVERSEER_ESCALATION.md`, project-root `OVERSEER_ESCALATION.md`, `docs/OVERSEER_ESCALATION_2026-08-02_tirith_toolset.md`, `docs/OVERSEER_SELF_PAUSE.md` — none modified by this cron tick.

## Stage-2 acceptance verdict (6 user-prompt criteria)

| # | Criterion | Status | Evidence |
|---|-----------|--------|----------|
| 1 | Debug build (`./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal`) | UNVERIFIED | requires terminal — tirith denied EC-039 |
| 2 | No command-list errors in fresh log | PASS (file-only, stale log) | tail clean per tick 362 read of lines 320–344 of v142 log |
| 3 | No Vulkan VUID/ERROR in fresh log | PASS (file-only, stale log) | 0 hits / 344 lines |
| 4 | `HLVM_PT_DEBUG_MODE=20` returns non-zero GBufferMaterial from GI shader SRV read | UNVERIFIED | requires re-build + re-run with env var — terminal denied |
| 5 | Validator passes newest stamp group only | UNVERIFIED | requires terminal |
| 6 | Fresh display image (vision) shows recognizable Sponza with sane exposure | UNVERIFIED | newest PNGs on disk are stale v142 (23:17:02..04); no fresh debug-mode=20 dump yet |

Of six: 2 PASS file-only, 4 UNVERIFIED. The four UNVERIFIED are exactly the ones that would confirm the binding fix actually landed and the rebinding is correct; they require terminal.

## Why HUMAN_REQUIRED (not KEEP / not FIX / not DELETE)

1. **EC-039 terminal denial (declared-vs-actual toolset discrepancy)** — `enabled_toolsets: ["terminal","file"]` was honored at cron-create but tirith denies `terminal` on every scheduled tick (`status=pending_approval`, `pattern_key=tirith:unknown`, `exit_code=-1`). The cron cannot independently run any of the four UNVERIFIED acceptance criteria. Honest verdict package requires parent intervention: reconfigure toolset, pause cron, or run the verification interactively.
2. **Card body `AUTO_RESOLVE_DO_NOT: yes` (EC-035/EC-037 body-exemption wins)** — the operator opted this card OUT of cron auto-resolve. Per the user's session-start directive ("Never auto-touch requires_human or blocked cards"), and per the registry (the uppercase prefixed marker is the production body-exemption rule), the cron refuses to issue KEEP/FIX/DELETE regardless of any opt-in marker on the card. Body wins.
3. **No new actionable evidence since tick 363** — dump timestamps, log size, plan/commit versions, diagnostics are bit-for-bit identical to the v142-era evidence that prior ticks already escalated on. The cron will not invent new findings; it will not fabricate KEEP to "move the card along."
4. **Sensitive surface?** — `Engine/Source/Runtime/Private/Renderer/RT*`, ray-tracing shaders, and `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp` are sensitive (the registry's default sensitive_globs would not match this file directly, but the AGENTS.md gotcha list explicitly flags RT payloads as a category requiring careful review). Reviewing a fix here requires reading the live diagnostic and the running GPU output, both beyond file-only reach.

## Reasoning

The card's body instructs the cron to verify a rendering bugfix lands correctly. The verification path is: rebuild Debug, run the test with `HLVM_PT_DEBUG_MODE=20`, check the GI shader's SRV reads return non-zero GBuffer material data, run the validator on the newest stamp group, and vision-check the fresh display image. Three of those steps require `terminal`. Tirith denies terminal on every scheduled tick on this host (EC-039 — same observation 350+ consecutive ticks). The cron cannot proceed to a verdict via file-only tools without manufacturing evidence it cannot collect. The body-exemption marker confirms the operator wanted this card left to human judgment anyway. HUMAN_REQUIRED is the honest verdict.

## Feedback (n/a — HUMAN_REQUIRED)

No feedback in the normal sense. The cron is reporting its inability to verify, not requesting a code change. The parent-session actions that would unblock this card are unchanged from tick 362:
1. Reconfigure cron toolset to `file-only` and rewrite prompt to drop shell-dependent verifications (RECOMMENDED — 350+ ticks of evidence file-only is the only honest toolset on this host).
2. Pause the cron via `cronjob action="pause"`.
3. Run the binding-offset fix at the GIPass dispatch site per AGENTS.md gotcha + DIAGNOSTIC option #7, then `./Build.sh --Rebuild --Config=Debug --Target=TestReSTIR_GI_Temporal` + `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20 ./Binary/Debug/TestReSTIR_GI_Temporal` + validator + vision interactively.
4. Extend diagnostic (v25 → v26 / v27) once Build.sh completes.

## Hard rules + EC citations honored this tick

- Hard #1 (never auto-merge to protected branches) — n/a, no merge attempted.
- Hard #2 (never push secrets) — n/a, no push.
- Hard #3 (never skip TDD evidence check) — n/a for the cron itself; card's worker is responsible.
- Hard #4 (never create cards) — honored; no `hermes kanban create`.
- Hard #5 (never invoke orchestrator) — honored; no sub-spawn.
- Hard #6 (never issue a verdict on a HUMAN_REQUIRED card) — honored; verdict = HUMAN_REQUIRED, no KEEP/FIX/DELETE.
- Hard #7 (never silently exit) — honored; this file + tick-364 health file.
- Hard #8 (never modify self or other crons) — honored; no cron config writes.
- Hard #9 (single-instance lock) — honored; no prior `lock` evidence on disk; tick proceeds.
- Hard #10 (append-only state writes) — honored; prior PENDING_REVIEW archived to `docs/archive/PENDING_REVIEW_t_7b79c010_2026-08-02_tick363.md` (via write_file — terminal `mv` was denied).
