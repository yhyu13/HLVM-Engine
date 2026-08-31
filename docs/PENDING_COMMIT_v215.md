# Pending Commit v215

- plan: docs/PENDING_PLAN_v215.md
- files: docs/PENDING_PLAN_v215.md, docs/PENDING_PLAN_REVIEW_v215.md, docs/PENDING_COMMIT_v215.md,
  docs/PENDING_TESTS_v215.md, docs/PENDING_TEST_AUDIT_v215.md, docs/PIPELINE_HEALTH_2026-08-21.md
- source: no bundle — direct investigation
- target: no branch (no commit made)
- task: Correct three load-bearing negative claims that 562 consecutive ticks asserted from a wrong-scope query
- verify: `cat ~/.hermes/cron/jobs.json | python3 -c "import json,sys; [print(j['id'], j['name'], j['state'], j['enabled']) for j in json.load(sys.stdin)['jobs']]"`
- skip_impl_review: no
- produces_test_files: no
- notes: ZERO source files touched. All findings are about the pipeline's own operating environment.

## Finding 1 — `jobs.json` EXISTS, and the cron IS live (refutes 562 ticks)

The lineage's standing claim, repeated verbatim across hundreds of markers:

> "0 `jobs.json` / 0 `crontab` / 0 `*.cron` files → pipeline DORMANT per
> `six-role-pipeline §\"I built the skill but I never actually created the cron\"`"

**That claim is false, and it was produced by searching the project root for a file that lives in the
Hermes home.** Searched at the correct scope, `/home/hangyu5/.hermes/cron/jobs.json` is present (12,935
bytes, 188 lines) and contains **four** registered jobs, two of them `enabled: true, state: scheduled`:

| id | name | state | toolsets | completed |
|---|---|---|---|---|
| `c6abd4d5fc39` | HLVM ReSTIR six-role (5m) v3 | **scheduled / enabled** | `terminal`, `file` | **3543** |
| `fdc2760d58cb` | HLVM ReSTIR kanban-card watcher (3m) | **scheduled / enabled** | `terminal`, `file` | **4702** |
| `4d9ef7842c63` | HLVM ReSTIR six-role autonomous pipeline (5m) | paused 2026-07-29 | `terminal`, `file` | 455 |
| `f76d8941aaad` | HLVM ReSTIR goal-loop watchdog (10m) | paused 2026-07-29 | `terminal`, `file` | 287 |

Job `c6abd4d5fc39`'s stored `prompt` is **byte-identical to this session's own instruction**, including the
`DIAGNOSTIC_2026-07-30.md` reference and all seven acceptance gates. **This session IS that cron tick.**

Corroborated independently: `~/.hermes/cron/ticker_heartbeat` is live, and `~/.hermes/cron/output/`
contains per-job transcripts (`c6abd4d5fc39/2026-08-21_15-14-27.md`, 2056 lines).

**Consequence.** Every marker asserting the pipeline is dormant, and every recommendation to "register a
cronjob", was wrong. The cron has been firing on schedule ~3,543 times. What is dormant is not the
scheduler — it is *progress*, because each tick re-derives the same wrong-scope conclusion and exits.

## Finding 2 — the jobs declare `enabled_toolsets: ["terminal","file"]`

All four jobs grant terminal. The lineage's premise that this is a "file-only host" is contradicted by the
job definition this very session runs under. The blocker is therefore NOT the toolset.

## Finding 3 — `terminal` is NOT "blocked"; it is an UNANSWERED APPROVAL (plan-criticer's split)

Observed response to `true` — the minimal possible command:

```
status: pending_approval,  exit_code: -1,
pattern_key: tirith:unknown,  smart_denied: false,  allow_permanent: true
```

`smart_denied: false` and `allow_permanent: true` are not the signature of a denial. `~/.hermes/config.yaml`
resolves it:

- `:473  approvals.mode: manual`   ← every command needs a human to approve
- `:474  approvals.timeout: 60`    ← unanswered prompts expire after 60s
- `:484  security.tirith_enabled: true`
- `:485  security.tirith_path: tirith`
- `:487  security.tirith_fail_open: true`

And **the `tirith` binary does not exist on this system**: 0 hits in `/usr/bin`, `/usr/local/bin`,
`/opt`, `~/.local`, `~/.hermes`. That zero is controlled — the same query shape returns `/usr/bin/git`.

So the scanner named in config is missing; it fails "open" into a `tirith:unknown` verdict, which under
`approvals.mode: manual` escalates to a human approval prompt — **in a cron session where no human is
present.** The prompt expires at 60s and the tool reports `exit_code: -1`.

**This is a liveness failure, not a permission failure.** 562 markers recorded it as
"tirith EC-039 categorical denial" and prescribed "widen the runspace permissions" — a remedy that would
not have worked, because the toolset was already granted (Finding 2).

## Plan Deviations

One, and it is worth recording because it fired *inside* this cycle. My first query against `jobs.json`
used `pattern="six-role|HLVM|name"` and returned **0 hits** — I was one step from recording "jobs.json
contains no jobs". tick-526's rule (never use `|` in a `search_files` pattern) caught it; re-running
without alternation returned the full file. The trap that helped produce the 562-tick error is still live
and still catches a careful reader. Both the plan and the audit call it out.
