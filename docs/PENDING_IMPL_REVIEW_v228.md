# Pending Impl Review v228

- plan: docs/PENDING_PLAN_v228.md
- commit: docs/PENDING_COMMIT_v228.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-581)
- timestamp: 2026-08-21

## plan_fidelity_check

The impl did what the plan specified and nothing more: it decided the branch question by control flow, re-probed `terminal` in the bare form, and recorded envelope fields. The commit declares "no deviations," and I confirm that: no engine source, no governance file, no commit, no push. The `## Plan Deviations` section is present and correctly empty.

The plan's stated self-granting risk was respected — the remedy touches `~/.hermes/config.yaml`, and the impler wrote it as **operator advice** rather than applying it. That restraint is correct and is the single most important thing this cycle got right. An agent editing the allowlist that gates its own shell access is a privilege escalation regardless of how well-motivated the edit is.

## Independent re-derivation (I did not accept the impl's line numbers)

| Claim | Re-derived | Result |
|---|---|---|
| `:2694 is_ask = env_var_enabled("HERMES_EXEC_ASK")` | own query | 2 hits, `:2694` + `:3118` — confirms the guard shape is duplicated elsewhere, so the read is of the right function |
| `:2762` unconditional return ends the `:2698` block | `read_file 2725-2794` | confirmed: `:2762 return {"approved": True, "message": None}` at block indent, followed by `:2764` "Phase 1" comment at function indent |
| Deny returns lack `status`/`approval_pending` | `read_file 2685-2760` | confirmed at `:2705-2714`, `:2724-2733`, `:2750-2760` — all two-key dicts |
| Observed envelope only at `:2999-3012` | `read_file 2985-3012` | confirmed field-for-field incl. `allow_permanent`/`smart_denied` polarity |
| Allowlist entry is a description | `pattern="script execution via"` | `:637` regex/description tuple — **exact string match** to the config entry |
| `cron_mode: allow` at config `:475` | file-scope query | 1 hit, confirmed |
| Dot-dir false zero | 6-row table incl. non-hidden positive control | reproduced |

## The argument is valid, and I want to state precisely why, because the lineage has mis-stated this class before

This is a **modus tollens on the emitted envelope**, not an inference from configuration. Structure:

1. Every path entering `:2698` returns at `:2705`, `:2724`, `:2750`, or `:2762`.
2. All four of those returns lack `status: pending_approval`.
3. We received `status: pending_approval`.
4. ∴ We did not enter `:2698`.
5. `cron_mode` and `tirith_fail_open` are read only *inside* `:2698`.
6. ∴ Neither was consulted on our path.

Step 2 is the load-bearing one and it is a direct read, not a summary. The conclusion needs **no** assumption about `HERMES_CRON_SESSION`, which is exactly the improvement over v227 — v227's finding was hostage to an env-var propagation question it could not answer, and this one dissolves that question rather than answering it.

## What this does to the lineage's standing advice

Ticks 563/564/569/580 each prescribed an operator action aimed at the cron branch. All four are now retired **with cause**, not merely superseded. Notably tick-564 "retired" tick-563's remedy and substituted its own — also inside the dead branch. The lineage has been refining advice about unreachable code for ~18 ticks. The reason it persisted is instructive: each tick verified its predecessor's *facts* (which were mostly right) and never questioned whether the branch containing them executes.

**Standing rule for this pipeline:** before prescribing a config change, prove the code that reads the config value is on the observed execution path. A config key being set correctly is not evidence that it is consulted.

## Security scan

- [x] No hardcoded secrets — markers contain no credentials
- [x] No shell injection — no code written
- [x] No eval/exec — none
- [x] No SQL — none
- [x] **No self-granting privilege change** — `~/.hermes/config.yaml` read only, byte-unchanged. Verified `command_allowlist` still holds exactly one entry.

## Self-review checklist

- [x] Validation: every zero paired with a same-scope positive control
- [x] Error handling: n/a (no code)
- [x] Tests: role #5 rows below, all file-only and re-runnable

## Card opened by this gate

**NEW card O — the `command_allowlist` entry is corrupt, and the persistence path that wrote it is a latent bug worth reporting upstream.** The entry `script execution via -e/-c flag` is the *description* half of the `(regex, description)` tuple at `approval.py:637`. A user who answers "always" should have the **command** (or a glob of it) persisted; instead the dangerous-pattern description was stored. Note `_command_matches_permanent_allowlist`'s own docstring (`:1671-1673`) acknowledges both conventions coexist — "permanent approvals historically store dangerous-pattern keys such as `recursive delete`" — but the *matcher compares against command text only*, so any historically-stored description is permanently dead weight. Either the writer should store command text, or the matcher should also consult `pattern_key`. **Not actionable from this runspace** (upstream repo + would alter this agent's own permissions). Report only.
