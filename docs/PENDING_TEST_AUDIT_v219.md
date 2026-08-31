# Pending Test Audit v219

- tests: docs/PENDING_TESTS_v219.md
- commit: docs/PENDING_COMMIT_v219.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-567)
- timestamp: 2026-08-21

## Broken-pattern audit

- [x] No `|` alternation in any pattern (tick-526)
- [x] No `file_glob` in any load-bearing query (v217)
- [x] Every load-bearing zero paired with a same-shape, same-scope positive control (v217)
- [x] **No dotfile-directory-scoped query treated as evidence (v219 — new this cycle)**
- [x] No count inherited across markers without re-derivation (v211)
- [x] No conclusion resting on `output_mode=count` alone (v198)
- [x] No runtime result fabricated — nothing built, run, or viewed by any role this cycle

## Rows I re-ran rather than read

**Row 14 (carries the cycle).** The tester's `HERMES_EXEC_ASK` → 0 in `scheduler.py` is the row that
inverts the commit's candidate ordering, so it is the one that must not be taken on faith. Re-ran the
control in the opposite direction: `path=cron/scheduler.py pattern="HERMES_CRON_SESSION"` → **3 hits**,
not the 1 the tester reported — `:2704` and `:2710` are `HERMES_CRON_SESSION_DB_TIMEOUT`, a different
variable that shares the prefix. **The tester's count was wrong; its conclusion was not.** The
assignment at `:2812` is the only write, and the two extras are `os.getenv` reads of a distinct name.
Corrected here rather than in the tester marker, per the standing rule that a closed marker is not
rewritten. *(This is v217's "counts are not invariants, sets are" recurring: a prefix-sharing token
inflated a control.)*

**Rows 16-17 re-read at source.** `gateway/run.py:1791` is at module scope — confirmed by reading
`:1785-1797`: the neighbouring statements `os.environ["HERMES_QUIET"] = "1"` (`:1788`) and the
`TERMINAL_CWD` block (`:1793+`) are at the same indentation, zero. **Not inside any function or
conditional.** So importing `gateway.run` sets `HERMES_EXEC_ASK=1` unconditionally, process-wide.

**NEW row 18 — the link the tester left implicit, and the one the whole finding rests on.** Rows
14-17 only matter if the cron scheduler can share a process with the gateway. The tester asserted
"if" and did not check. Checked: `gateway/run.py:4211-4229` `_active_cron_job_count()`, whose docstring
states *"Cron jobs run through a standalone `AIAgent` on the scheduler's own thread pool
(`cron/scheduler.py::run_job`)"* and which calls `from cron.scheduler import get_running_job_ids` to
count jobs *in flight in this process*. `:5850-5854` folds that count into the gateway's own shutdown
drain, and `:8301-8307` imports `mark_running_jobs_interrupted` to mark them. **A process that counts,
drains and interrupts in-flight cron jobs is the process they run in.** Co-residency is not
speculative; it is the documented architecture, in three independent places in the gateway's own file.

**Therefore candidate (B) is not merely live, it is the default configuration**: whenever cron runs
under the gateway, `gateway/run.py:1791` has already set `HERMES_EXEC_ASK=1` at import, and
`approval.py:2698`'s `not is_ask` operand is false regardless of `scheduler.py:2812`. The cron branch
at `:2700` — and with it `approvals.cron_mode: allow` — is **structurally unreachable in this
deployment.** That is why 567 ticks of config-level remedies could not work: the key is correct, and
it is read on a line that control never reaches.

## Per-row verdict

**17 PASS / 1 corrected-and-PASS / 1 added — all 18 KEEP.** Rows 8, 9, 14, 16 and 18 carry the cycle.

- **Row 8 is the one that retires a 567-tick premise.** `tirith` exists at `~/.hermes/bin/tirith`.
  Every marker asserting "the binary does not exist" is false, and the remedy chain built on it
  (v215's *install tirith*) was aimed at a non-problem.
- **Row 9/10 is the one that explains how the premise survived so long.** Directory-scoped search
  under a dotfile directory returns 0 unconditionally. `~/.hermes` is where the binary is. The search
  that would have found it was the search that cannot.
- **Row 18 is the one that makes the diagnosis actionable** rather than a pair of guesses.

## What this cycle established, and what it did not

**Established (file-only, controlled):**
1. The refusal is produced at `approval.py:2999`, identified by a result-dict field set unique to one
   construction site — not inferred from config semantics.
2. `approvals.cron_mode: allow` and `security.tirith_fail_open: true` are **both already correct and
   both inert**, because the branches that read them are not on this control path.
3. `tirith` **is installed**; the 567-tick "missing binary" premise is false, and the false zero that
   sustained it is a newly-identified third `search_files` failure class.
4. The most probable mechanism is `HERMES_EXEC_ASK=1`, set at module scope by `gateway/run.py:1791`
   in a process that — per the gateway's own cron-drain code — also runs the scheduler.

**NOT established, load-bearing:** that anything compiles, links, runs, renders or validates; and the
*final* link of item 4, which requires reading the live process environment. The operator action below
is written to be correct whether the cause is (A) or (B), and its first step **prints both variables**,
so it diagnoses and fixes in one pass.

## Acceptance gates vs the job instruction: 0 of 7 (unchanged)

| # | Gate | Status | Basis |
|---|---|---|---|
| 1 | Debug target builds | UNKNOWN | `terminal` refused; even `true` → `pending_approval` |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` | BLOCKED | same |
| 3 | No Vulkan VUID/ERROR | UNKNOWN | newest log 2026-08-14, predates 36 unbuilt cycles |
| 4 | No command-list errors | UNKNOWN | same |
| 5 | `validate_restir_gi.py` newest group | BLOCKED | same |
| 6 | Vision: recognizable Sponza | BLOCKED (structural) | no image tool in this runspace |
| 7 | Mode 20 non-zero `GBufferMaterial` | UNKNOWN | needs one approved run |

**The change this cycle makes is to gates 1-5 and 7's *cause*, not their status.** They were recorded
as blocked by a missing scanner; they are blocked by an environment-variable interaction, and that has
a fix the operator can apply in one line.

## What this auditor did NOT do

Did not build, run, compile, validate, or view any image. Did not commit, push, or modify any engine
source or governance file. **Did not modify the `hermes-agent` source** — a cron job must not patch
the agent executing it; the finding is reported for the operator to act on. Did not fabricate any
runtime result.
