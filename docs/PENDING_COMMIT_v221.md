# Pending Commit v221

- plan: docs/PENDING_PLAN_v221.md
- files: docs/PENDING_PLAN_v221.md, docs/PENDING_PLAN_REVIEW_v221.md, docs/PENDING_COMMIT_v221.md,
  docs/PENDING_IMPL_REVIEW_v221.md, docs/PENDING_TESTS_v221.md, docs/PENDING_TEST_AUDIT_v221.md,
  docs/PIPELINE_HEALTH_2026-08-21.md (append)
- source: no bundle — direct reads
- target: no branch — nothing committed
- task: Re-derive the approval-gate finding first-hand; probe untried execution modes; correct v220.
- verify: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` (still refused this tick)
- skip_impl_review: no
- produces_test_files: no
- notes: **Zero engine source modified. Nothing built, run, or viewed.**

## FINDING 1 — "terminal is categorically blocked" was under-tested for 568 ticks, and now IS tested

Every prior denial used the **default foreground** shape. `terminal` exposes two other execution
paths that were never once exercised. Run this tick, same command (`date`), three shapes:

| Shape | Result |
|---|---|
| foreground (default) | `pending_approval` / `tirith:unknown` / `exit_code -1` |
| `background=true` | **identical** |
| `pty=true` | **identical** |

The lineage's "categorical" claim is a claim about all shapes; it had one shape's evidence. It now
has three. **The claim is upheld — but it was luck, not rigour, and the marker should say so.**
The gate sits in `check_all_command_guards`, upstream of any execution-mode branch, so all shapes
converge on `:2698` — consistent with what we observe.

## FINDING 2 — v220's rejection of the `command_allowlist` remedy is WRONG, and the ground is removable

v220 rejected the allowlist on `_has_allowlist_shell_operator` (`:1660`, regex contains `&&`,
`:1678-1679` returns False on match), calling the acceptance command "categorically ineligible."
That is true of the command **as the lineage has always written it** —
`cd /home/hangyu5/... && ./Build.sh ...` — and the `&&` is an artifact of that habit, not a property
of the build. **`jobs.json:138` already sets `"workdir": "/home/hangyu5/Documents/Gitrepo-My/HLVM-Engine"`
for this job**, so the `cd` is redundant. Ran the bare form this tick:

```
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test
```

No shell operator: no `\n`, `&&`, `||`, `;`, `&`, `|`, `<`, `>`, backtick, or `$(`. **It passes the
`:1660` guard.** So the allowlist remedy is *eligible*, and v220's stated reason for excluding it
does not hold. (It was still refused this tick — because the entry is not in the config yet;
eligibility is not the same as membership.)

Wiring verified end-to-end rather than assumed: `load_permanent_allowlist` (`:1702`) reads
`config["command_allowlist"]` (`:1711`) → `load_permanent(patterns)` (`:1713`) → `_permanent_approved`,
which `_command_matches_permanent_allowlist` reads under lock at `:1681-1682`, comparing by exact
string (`:1690`) **or** `fnmatch` when the pattern contains `*?[` (`:1692`). And `:2689-2690` sits
**above** the defective cron branch at `:2698`, so it returns approved without ever reaching it.

**This yields a second viable remedy, narrower than v220's `approvals.mode: off`.** `mode: off`
disables approval for *every* command in *every* session; an allowlist entry authorises exactly one
command. On a host whose config the agent is forbidden to edit, the narrower ask is the one more
likely to be granted.

## FINDING 3 — the defect has an author-written confession, which no tick has quoted

`_is_gateway_approval_context`'s docstring (`:234-239`) states:

> "Cron jobs are NEVER gateway-approval contexts even when they originate from a gateway platform
> ... letting cron fall through to the gateway branch would **submit a pending approval with no
> listener and block the job indefinitely**."

**That is our symptom, named exactly, by the author, as the thing the design exists to prevent.**
The body honours it: `:241-242` returns False when `HERMES_CRON_SESSION` is set — first check in
the function, before the gateway checks. So `is_gateway` is correctly False for us.

**The bug is that `is_ask` has no such guard.** All three operands of `:2698` were re-derived:

| Operand | Callee | Cron guard? | Value here |
|---|---|---|---|
| `is_cli` | `_is_interactive_cli` `:84-93` | no — but env is absent anyway | False |
| `is_gateway` | `_is_gateway_approval_context` `:227-245` | **YES, `:241-242`, explicitly for this** | False |
| `is_ask` | `env_var_enabled("HERMES_EXEC_ASK")` `:2694` | **NO — bare env read** | **True** |

`gateway/run.py:1791` sets `HERMES_EXEC_ASK=1` at module scope, col 0, unconditionally. So one
operand of a three-way conjunction was hardened against cron and the one beside it was not — and
because the conjunction is `not A and not B and not C`, the single unhardened operand is sufficient
to defeat the other two. **v220 identified the operand; it did not notice the neighbouring guard
that proves the omission was an oversight rather than a design.**

The `:2887` consequence is the mechanism: `if is_gateway or is_ask:` enters the notify-and-wait
path. `notify_cb` is looked up per session (`:2890`) — for a cron session there is none. Precisely
the "pending approval with no listener" the docstring warned of.

## Plan Deviations

None. Both plan items ran; the plan gate's sibling-operand requirement produced FINDING 3, which
is the cycle's strongest result and which the plan had not anticipated.
