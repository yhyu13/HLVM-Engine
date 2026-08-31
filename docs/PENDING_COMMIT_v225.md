# Pending Commit v225

- plan: docs/PENDING_PLAN_v225.md (iteration 2, KEEP)
- files: docs/PENDING_PICK.md
- source: no bundle — direct edit of one marker file
- target: no branch; nothing committed, nothing pushed
- task: Close card U. Its premise was false; the mechanism that makes it false — `output_mode=count` at directory scope silently omitting `PENDING_PICK.md` from its enumeration — invalidates the query shape the lineage uses to decide whether the pipeline runs.
- verify: `search_files pattern="^- \[ \] \*\*NEW card" path=docs/PENDING_PICK.md output_mode=count` → **3** (cards L, M, N; card U no longer matches). Same query with `path=docs` (directory) → **0**, which is the defect this cycle documents. `search_files pattern="^- \[ \] \*\*NEW card" path=docs output_mode=files_only` → **1 hit, PENDING_PICK.md** — the correct answer, from the corrected protocol.
- skip_impl_review: no
- produces_test_files: no
- notes: per v203's standing rule as generalised by v224, the `old_string` was anchored on the smallest unique substring (`- [ ] NEW card U (opened by tick-574 at the v224 plan gate, this turn)`) and the returned diff was read before declaring done. **The diff shows exactly one line replaced by three (closure entry + blank + preserved original with `[x]` and strikethrough); no other line in the 265-line file moved.** The v224 near-miss did not recur.

## What the patch did NOT do

- Did NOT modify any engine source. The v183–v224 chain is byte-unchanged, so this cycle cannot perturb the chain the operator's first build must exonerate.
- Did NOT tick cards L, M, N. Their build precondition is genuinely unmet and this finding does not change it.
- Did NOT modify `AGENTS.md`, `CLAUDE.md`, `.cursorrules`, or any governance file.
- Did NOT modify `~/.hermes/config.yaml`, `approval.py`, `gateway/run.py`, or `tirith_security.py` — a cron job must not patch the agent running it.
- Did NOT commit or push.
- Did NOT build, run, compile, validate, or view any image.

## Plan Deviations

None. The plan was followed as written after its iteration-2 revision.

One item carried in from the plan review rather than the plan: the reviewer required that the marker **not** assert a historical count ("there have been 4 actionable items throughout"), because this runspace has the current state and not the file's history. The marker states the defensible form instead — *the queue is non-empty now, and the query shape that reported it empty was incapable of reporting otherwise*. That is what was written.

## Acceptance gates moved: 0 of 7

This cycle changed one marker file. It did not build, run, or render anything. **What it changed is the reliability of the pipeline's own routing input** — which is upstream of every future cycle's decision to run, but is not itself an acceptance gate.
