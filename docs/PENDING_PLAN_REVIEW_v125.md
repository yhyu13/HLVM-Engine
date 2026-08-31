# Pending Plan Review v125

- plan: docs/PENDING_PLAN_v125.md
- verdict: FIX
- reviewer: file-only plan-criticer (no terminal access, same runspace as planner per EC-039)
- timestamp: 2026-07-30 (tick 106)

## Design soundness
The plan correctly identifies the remaining search space (handle identity, slangc binding reflection, sampling correctness) and orders the experiments cheapest-first. Each experiment is single-variable as required by gpu-rendering-bisect-debug methodology. The discriminator outputs (handle address match/mismatch, magenta-pixel fraction, binding location in SPIR-V reflection) are all directly measurable and falsifiable.

## Plan completeness
Two gaps:

1. **The plan's expected outcome branch C2** describes a real bug (GBuffer textures reflected at `set=1` due to a `, space1` leak), but the fix path in the plan text is vague ("fix is to remove `, space1` from the GBuffer texture declarations OR add a separate binding layout for them"). The plan should commit to ONE fix path so the impler doesn't have to re-design under uncertainty. Recommendation: prefer "remove `, space1` if present on the GBuffer textures" — but the current shader (line 96-98) has NO `, space1` on the GBuffer textures. So C2 is only reachable if slangc itself moves them to space1 due to the `, space1` on `Output` (line 88). That would be a slangc peculiarity. The plan should also test this hypothesis with `spirv-cross --hlsl-iomap` or similar to confirm.

2. **The plan does not address the v93 stale-sblob hypothesis** that PIPELINE_HEALTH_2026-07-30_tick105.md flagged as "single most likely explanation". The sblob mtime vs source-file mtime diff is the single 5-second check that would resolve that question before any of the three experiments. Recommend: prepend a precondition step "verify sblob is current; if stale, rebuild and re-run case 20/21/22 BEFORE experiments A/B/C". This may short-circuit the entire bisect if the sblob was never rebuilt after v101.

## Acknowledged state-machine caveat
Per six-role-pipeline HARD INVARIANT #4, "Plan-criticer FIX always loops to planner. No 'proceed with caveat'." This review is FIX. The state machine should route to planner (Rule 3) to incorporate the two gaps above before impler dispatches.

The two gaps above are non-blocking for a parent runspace — a parent session can simply prepend the sblob-mtime check before running experiments A/B/C. But the file-only cron runspace cannot do that. So this tick ends with the plan FIXED, the planner dispatched again, and the impler not yet able to run.

## Other observations (non-blocking)
- Experiment A is the cheapest and most likely to yield a signal (handle addresses are logged on every frame already; the only cost is one log line per site). Recommend running it FIRST even though the plan orders A/B/C correctly by category — re-order to: A, sblob-mtime precondition, B, C.
- The plan correctly notes that validate_restir_gi.py has a documented false-positive bias on uniform-white dumps (per DIAGNOSTIC_2026-07-29.md). Validator is not the verdict gate.
- The plan correctly identifies that no test files are produced, so `skip_impl_review: yes` is acceptable.

## Feedback for planner (FIX)
1. Add sblob-mtime precondition as step 0. Rebuild + re-run case 20/21/22 BEFORE any of A/B/C. If modes 20/21/22 return non-zero after rebuild, the bisect is closed and the plan becomes retrospective. The sblob mtime diff is 5 seconds of work and may resolve the entire investigation.
2. For experiment C outcome C2, commit to ONE fix path. Prefer the slangc-leak hypothesis test: temporarily move `Output : register(u0, space1)` back to `register(u0)` (drop `, space1`), rebuild, see if SPIR-V places GBufferWorldPos at set=0 binding=1. If yes, the bug is the slangc leak and the fix is to keep Output at the default space0 (revert v101 for the Output UAV but keep the second binding layout). If no, the leak is in the GBuffer texture declarations (add explicit `, space0`).

## Self-review checklist
- [x] Plan is single-variable per experiment (one source change + one dump analysis per cycle).
- [x] Each experiment has a falsifiable predicted outcome.
- [x] Plan does not require test files (so `skip_impl_review: yes` is honest).
- [x] Plan does not commit, push, or modify governance files.
- [x] Plan acknowledges the file-only runspace limitation and routes to parent for terminal-bound steps.