# Pending Commit v219

- plan: docs/PENDING_PLAN_v219.md
- plan_review: docs/PENDING_PLAN_REVIEW_v219.md (KEEP)
- files: **none — zero source files modified** (diagnostic cycle; the defect is not in this repo)
- source: `/home/hangyu5/Documents/Gitrepo-My/hermes-agent` (the running agent's own source)
- target: (uncommitted working tree — this pipeline does not commit)
- task: Trace the `terminal` refusal to its return site in agent source
- verify: `cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine && ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`
- skip_impl_review: no
- produces_test_files: no
- notes: Two independent net-new findings. Neither was reachable from `config.yaml`, which is the only
  artifact ticks 563-566 examined.

## Plan Deviations

None. Both plan-gate additions were followed: the branch was identified by result-dict field set
(§Finding 1 step 5), and §Finding 1 states explicitly which link is inferred rather than read.

## FINDING 1 — the blocker is a two-condition conjunction, and BOTH prior remedies address neither

Traced end-to-end in `hermes-agent`. A `terminal` call reaches the refusal only via this path:

| # | Site | Content | Consequence |
|---|---|---|---|
| 1 | `tools/approval.py:2692-2698` | `is_cli = _is_interactive_cli()`; `is_gateway = _is_gateway_approval_context()`; branch requires all three false | the cron fast-path is entered ONLY here |
| 2 | `:2700` | `if env_var_enabled("HERMES_CRON_SESSION")` | **the gate. If unset, the whole cron branch is skipped** |
| 3 | `:2701` | `if _get_cron_approval_mode() == "deny"` | our config yields `"approve"` (`:1969` maps `allow`→`approve`), so this is False |
| 4 | `:2762` | `return {"approved": True, "message": None}` | **the fall-through. Two fields.** |
| 5 | `:2999-3009` | `{approved, pattern_key, status, approval_pending, command, description}` + `allow_permanent` via `terminal_tool.py:2352-2358` | **seven fields — this is what we received** |

**We received the seven-field dict, so control reached `:2999`, not `:2762`.** `:2999` is inside the
`if is_gateway or is_ask:` block at `:2887`, which is only reachable when the `:2698` branch was NOT
taken. Field-set identification per the plan gate: `status` + `approval_pending` + `command` +
`description` + `pattern_key` co-occur at exactly two sites (`:3000-3006`, `:3223-3229`); the second
is the plugin-escalation path with `pattern_key` from a plugin rule, and ours is `tirith:*`
(`:2825`), which is constructed only in the flow containing `:3000`. **One site.**

**[CORRECTED at the impl-review gate — the first draft named one cause; the source supports two.]**
`:2698`'s condition has THREE operands (`not is_cli and not is_gateway and not is_ask`) and `:2887`'s
is `is_gateway or is_ask`, so reaching `:2999` means `is_gateway or is_ask` was true. `:241-242` makes
`HERMES_CRON_SESSION` mutually exclusive with `is_gateway` only — **`is_ask` is independent of it.**
`is_cli` is excluded separately: an interactive-CLI session returns a choice from `:3025` and never
constructs the pending dict. So exactly two candidates survive, and **this session cannot separate
them**, because doing so requires reading the process environment via the terminal under investigation:

> **(A)** `HERMES_CRON_SESSION` is unset in this process → `_get_session_platform()` (`:245`) is
> non-empty → the session is classified **gateway** → routed to a `/approve` prompt no one answers.
>
> **(B)** `HERMES_CRON_SESSION` IS set, but `HERMES_EXEC_ASK` is ALSO set → `:2698` is skipped on the
> `is_ask` operand → same `:2999` return.

Both reduce to the same statement — **the scheduler process's environment does not match what `:2698`
expects of a cron session** — and both are diagnosed and fixed by the same operator action below,
which is written to be correct under either.

`cron/scheduler.py:2812` sets that var — `os.environ["HERMES_CRON_SESSION"] = "1"` — with the comment
*"process-wide and persists for the lifetime of the scheduler process"*. **That line is on the
in-repo job-execution path.** Its own comment states the invariant that is violated here.

**What this means for the two prior remedies, stated plainly:**
- **v215's remedy (install `tirith` / disable it) would not have worked.** `tirith` only supplies the
  `description` and `pattern_key` *fields* (`:2822-2828`); it does not choose the branch. With the
  branch unchanged, a `tirith`-clean command still routes to `:2999` on the `is_dangerous` leg.
- **v216 correctly retired v215 but concluded "nothing to change."** Also wrong: there is something
  to change, it is just not in `config.yaml`. `approvals.cron_mode: allow` (`:475`) is **already
  correct and already inert**, because `:2700` gates it behind a var that is unset.
- **`security.tirith_fail_open: true` (`:487`) is likewise inert here** — it governs only the
  spawn-failure and unknown-exit paths inside `tirith_security.py` (`:793`, `:822`), which are not on
  this path at all.

## FINDING 2 — `tirith` IS installed. The 567-tick premise that it is missing is FALSE.

`search_files target=files path=/home/hangyu5/.hermes/bin pattern="*"` → **3 hits, one of which is
`/home/hangyu5/.hermes/bin/tirith`.** `tirith_security.py:13-17` documents exactly this: *"if tirith
is not found on PATH or at the configured path, it is automatically downloaded from GitHub releases
to `$HERMES_HOME/bin/tirith`"*. The auto-installer ran, at some point, successfully.

Ticks 563-566 recorded `tirith` as absent from `/usr/bin`, `/usr/local/bin`, `/opt`, `~/.local` and
`~/.hermes`. The first four are true and irrelevant — the installer does not use them. **The fifth was
a false zero**, see Finding 3. And the scan returning `block`/`warn` on the degenerate command `true`
**is not explicable by an absent binary under this configuration**: an absent binary takes the
`OSError` path at `:783` and, with `fail_open: true` (`config.yaml:487`), returns `allow` at `:794`
with no findings. The one remaining scanner-less route to a `warn` is the synthesis at `:2789-2805`,
excluded on two independent grounds — it fires only when `tirith_fail_open` is **false**, and it
stamps `rule_id: "tirith-import-error"`, whereas the `pattern_key` we received was `tirith:unknown`
(i.e. `rule_id` absent from a real findings list, per `:2824`).

## FINDING 3 — THIRD false-zero class in `search_files`: dotfile directories are skipped

Controlled this tick, same tool, same token:

| Query | Result |
|---|---|
| `path=/home/hangyu5/.hermes pattern="tirith_enabled"` | **0 hits** |
| `path=/home/hangyu5/.hermes/config.yaml pattern="tirith_enabled"` | **1 hit, `:484`** |
| `path=/home/hangyu5/.hermes pattern="approvals"` | **0 hits** |
| `read_file ~/.hermes/config.yaml` offset 460 | `:472 approvals:` — present |
| `path=/home/hangyu5/.hermes/cron pattern="enabled_toolsets"` | **0 hits** |
| `path=/home/hangyu5/.hermes/cron/jobs.json pattern="enabled_toolsets"` | **4 hits** |

**Directory-scoped content search under a dotfile directory returns 0 unconditionally; file-scoped
search on a file inside it works.** After tick-526's alternation bug and v217's project-root scoping,
this is the third distinct false-zero class.

**Consequence — v216's central row is vacuous.** v216 checked for a per-job `cron_mode` override with
`search_files pattern="cron_mode" path=~/.hermes/cron` → 0 hits, and read that as "no override
exists." That query was guaranteed 0. Re-derived file-scoped this tick:
`path=~/.hermes/cron/jobs.json pattern="cron_mode"` → **0 hits** (with `enabled_toolsets` → 4 as the
same-file positive control). **v216's conclusion survives on sound evidence** — but it was not sound
when made.

## Corroboration: this session IS the cron job, read first-hand

`~/.hermes/cron/jobs.json:95-139`, job `c6abd4d5fc39` "HLVM ReSTIR six-role (5m) v3":
`enabled: true`, `state: scheduled`, `repeat.completed: 3547`, `last_run_at 2026-08-21T16:36:41`,
`next_run_at 16:47:32`, `enabled_toolsets: ["terminal","file"]`, `workdir` = this project. Its stored
`prompt` is byte-identical to this session's instruction. Two other jobs (`4d9ef7842c63`,
`f76d8941aaad`) are `enabled: false / paused` since 2026-07-29 — matching
`DIAGNOSTIC_2026-07-30.md:154-156`, which says those two were paused. **The diagnostic's own closing
lines are corroborated; it is only its §"root cause" that is refuted.**

The toolset grant is real and irrelevant: `enabled_toolsets` controls whether the tool is *offered*,
and it is — the call reaches the approval layer, which is downstream.

## What this cycle did NOT establish

That anything compiles, links, runs, renders or validates. Nothing was built, run, or viewed. The
v183-v219 chain remains unbuilt. **Finding 1's final link — that the var is unset in *this* process —
is an inference from the returned field set, not a direct read of the process environment**, because
reading it requires the terminal this finding is about. It is a strong inference (the field set
identifies one return site, and that site is unreachable when the var is set) but it is an inference,
and the operator action below is written so that it is correct either way.
