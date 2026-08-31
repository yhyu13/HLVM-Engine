# Pending Commit v216

- plan: docs/PENDING_PLAN_v216.md
- files: docs/PENDING_PLAN_v216.md, docs/PENDING_PLAN_REVIEW_v216.md, docs/PENDING_COMMIT_v216.md,
  docs/PENDING_TESTS_v216.md, docs/PENDING_IMPL_REVIEW_v216.md, docs/PENDING_TEST_AUDIT_v216.md,
  docs/PIPELINE_HEALTH_2026-08-21_six-role-tick-564.md
- source: no bundle — direct probes
- target: no branch — nothing committed, nothing pushed
- task: Test v215's prescribed operator remedy against the remainder of the config block it was derived from
- verify: `cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine && ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test`
- skip_impl_review: no
- produces_test_files: no
- notes: **ZERO engine source modified.** No commit, no push, no governance file touched.

## Finding — v215's remedy is prescribed against two flags that are ALREADY SET THE WAY IT ASKS

Every claim below re-derived this tick by independent query, not cited from v215.

**Re-derived and CONFIRMED from v215:**

| Claim | Query this tick | Result |
|---|---|---|
| `terminal` is blocked | `terminal command="true"` | `status: pending_approval`, `exit_code: -1`, `pattern_key: tirith:unknown`, `smart_denied: false`, `allow_permanent: true` |
| Block is command-independent | probed `true` — the most trivial command expressible | blocked identically ⇒ not command-shape dependent |
| `tirith` binary absent | `/usr/bin`, `/usr/local/bin`, `~/.local/bin`, `~/.local/lib` | 0 hits in all four |
| ...controlled | same query shape, `pattern="git"`, `path=/usr/bin` | 1 hit `/usr/bin/git` ⇒ the zeros are real, not vacuous |
| `jobs.json` exists, 4 jobs | `search_files path=~/.hermes/cron/jobs.json pattern='"name"'` | 4 hits (lines 5, 51, 96, 143) |
| 2 enabled / 2 paused | `pattern='"state"'` | `:31-32` paused, `:76-77` paused, `:122-123` **scheduled**, `:168-169` **scheduled** |
| All 4 grant terminal | `pattern="enabled_toolsets"` | 4 blocks, each `["terminal","file"]` (`:43`, `:88`, `:134`, `:180`) |
| This session IS job `c6abd4d5fc39` | `pattern="six-role pipeline for the HLVM-Engine"` | `:97` prompt matches this instruction incl. the `DIAGNOSTIC_2026-07-30.md` clause and the 7 gates |

So the "file-only host / dormant pipeline" premise remains refuted, independently of v215.

**NET-NEW — the part v215 did not read.** v215 quoted `approvals.mode: manual` (`:473`) and
`tirith_enabled: true` (`:484`) and stopped. The same two blocks contain two more terms, both of which
bear directly on its causal story:

- **`:475  cron_mode: allow`** — sits *inside* the `approvals:` block, three lines below the `mode: manual`
  that v215 blamed. A distinct approval mode for cron sessions, set to `allow`. This session is a cron
  session (established above by prompt identity).
- **`:487  tirith_fail_open: true`** — sits *inside* the `security:` block, two lines below the
  `tirith_path: tirith` that v215 blamed. Fail-open is the standard name for *"if the scanner is
  unavailable, permit rather than block"* — precisely the missing-binary case.

**Consequence: v215's remedy is not actionable as written.** It prescribed three options —
(a) install `tirith`, (b) set `tirith_enabled: false`, (c) reconcile `cron_mode`. But (c) is **already
`allow`**, and (b)'s intent is **already expressed** by `tirith_fail_open: true`. An operator following
v215 would open the config, find the flags already set the way the remedy asks, and have nothing to change.
The 563rd tick's operator action would have failed at step one.

**What this distinguishes:** the blocker is *not* "the config is set wrong". Either (i) these two flags are
not honored by the runtime for this code path — a runtime defect, not a config error — or (ii) they are
honored but scoped narrowly (e.g. `fail_open` covering scanner *timeouts* via `tirith_timeout: 5` but not
scanner *absence*; `cron_mode` covering scheduling but not tool approval). Both are consistent with the
observed `pending_approval` + `smart_denied: false`.

**What this does NOT establish — stated per the plan-criticer's explicit requirement.** I read config
*files*. I did not observe the runtime read them, and I cannot: `search_files` for `tirith_fail_open` across
`~/.hermes` returns **1 hit — the config line itself** — i.e. no interpreting code is present in any path
readable from this runspace. **"The flag is set" is NOT upgraded to "the flag works."** I cannot distinguish
(i) from (ii) from here, and I am not asserting a runtime bug. What is established is narrower and sufficient:
**the remedy v215 handed the operator is a no-op, so the operator action must change.**

## Plan Deviations

None. Scope held to config re-derivation; the plan-criticer's completeness requirement is discharged in
"What this does NOT establish" above.
