# Pending Plan v225 (iteration 2 — after plan-review FIX)

- task: Card U — reconcile the two disagreeing queue states in `docs/PENDING_PICK.md`. **Card U's premise is false; the reason it is false is the finding.**
- source: no bundle — direct edit of one marker file
- skip_plan_review: no
- diff_estimate: +0 / -0 engine source; marker text only

## Iteration note

Iteration 1 diagnosed this as "bracket-class regex with a space, under directory scope." **The plan gate falsified that** with a bracket-free control and found the real variable. This iteration adopts the gate's diagnosis. The original theory is retained below as a recorded dead end, per `software-development-practices §Path-Tracing Methodology` rule 7 ("write down every dead end").

## The finding: `output_mode=count` silently omits large files from its enumeration

Card U (PICK:247) asserts the lineage's standing queue query returns 5 hits. It returns **4** at file scope — and **0** at directory scope, which is the scope ticks have actually been using.

**Controlled matrix, every row run this tick, one variable changed at a time:**

| # | pattern | brackets? | path | output_mode | PENDING_PICK.md |
|---|---|---|---|---|---|
| 1 | `^- \[ \]` | yes | FILE | count | **4** |
| 2 | `^- \[ \]` | yes | DIR | count | **0** |
| 3 | `known-good control` | **no** | FILE | count | **14** |
| 4 | `known-good control` | **no** | DIR | count | **0** |
| 5 | `known-good control` | no | DIR | **files_only** | **PRESENT** (1 of 55) |
| 6 | `^- \[ \] \*\*NEW card` | yes | DIR | count | **0** |
| 7 | `^- \[ \] \*\*NEW card` | yes | DIR | **files_only** | **PRESENT** (1 of 1) |
| 8 | `tenth instance, in the known-good control` | no | DIR | count | **0** |
| 9 | `tenth instance, in the known-good control` | no | DIR | **files_only** | **PRESENT** (1 of 1) |

- **Rows 3/4 falsify the bracket theory**: a plain-text pattern with no regex metacharacters reproduces the split exactly.
- **Rows 4/5, 6/7 and 8/9 isolate the variable**: same pattern, same directory, two output modes, two answers — and `files_only` is correct all three times.
- In `count` mode the per-file enumeration is capped and **PENDING_PICK.md is absent from the returned map entirely** — not reported as zero, simply not enumerated. Raising `limit` to 400/500 did not surface it. The file is 366 KB / 265 lines, the largest marker in the tree, and it is the sole target of every queue-state query.

Positive controls establishing the tool works and the file is readable: row 1 (4), row 3 (14), `^- \[x\]` at file scope (**106**), `NEW card` at file scope (**22**).

## Why this outranks card U's own proposal

Card U proposes folding L/M/N into a consolidated card. That is bookkeeping on a queue whose **read mechanism is unreliable**. The lineage's protocol for "is the queue empty" is a directory-scoped `count` query, and that shape omits the queue file. Every tick that recorded *"PENDING_PICK queue has 0 actionable `- [ ]` items at queue level"* and fired Rule 10 on that basis was reading a false zero. **There are 4 actionable items and there have been throughout.**

**This is not a new rule — it is v198's existing rule, unenforced on the one query that decides whether the pipeline runs.** `PENDING_TEST_AUDIT_v224.md:16` already reads *"No conclusion resting on `output_mode=count` alone."* Every cycle attested to it, and every cycle then read the queue with `count`. The protocol was present; the queue read was exempted from it by habit.

## Corrected protocol

1. Queue-state reads use `files_only`, or `content`/`count` scoped to the **file**, never `count` at directory scope.
2. Any load-bearing zero from directory-scoped `count` is **invalid** until re-run in a second mode.
3. Every load-bearing zero is paired with a same-shape positive control (v217, restated).

## Bound on the finding — re-derived file-scoped, per the gate

The lineage's VUID evidence is **not** invalidated, but the plan gate correctly noted iteration 1 established this with a query of the very shape in question. Re-derived:

- `VUID` at `Binary/Debug` (DIR, count) → 23 hits, and the five hit-bearing logs **do appear** in the returned enumeration (`_1.log` 10, `_2.log` 8, `TestPathTraceGI_1.log` 5). The citation is sound *because the relevant files were enumerated*, not because the shape is reliable.
- The load-bearing half re-run **file-scoped**: `VUID` at `TestReSTIR_GI_Temporal.log` → **0**. This is the claim gate 3 rests on, and it is now established by a query shape that cannot silently omit its target.

The finding is bounded to: **directory-scoped `count` under-enumerates, and large files are the ones dropped.** It is not "search_files is broken."

## Recorded dead end (iteration 1's falsified theory)

Bracket-class-with-space under directory scope. Falsified by rows 3/4. Related but genuinely separate: `- [ ]` **unescaped** at any scope hard-errors with `grep: invalid option -- ' '`. That is a real second defect, worth recording, but it is loud — it cannot produce a silent false zero, and it is not what happened here.

## approach

Marker-only. Tick card U closed, append a closure entry to `docs/PENDING_PICK.md` recording the matrix, the corrected protocol, the VUID bound, and the dead end. **L/M/N stay unticked** — their build precondition is genuinely unmet and this finding does not change that.

**No engine source. No governance file. No commit, no push.** The v183–v224 chain stays byte-unchanged.

## test_strategy

Role #5 re-runs the matrix independently rather than reading it, and must independently re-derive the file-scoped `VUID` zero. Every load-bearing zero paired with a same-shape positive control.

## risks

- **Over-claiming scope.** Must be stated as directory-scoped `count` under-enumeration, bounded by the VUID rows. Role #6 checks the bound is stated.
- **Patch-tool fuzzy match** (v203, v224). Anchor on the smallest unique substring; read the returned diff.
- **Ticking L/M/N.** Precondition unmet; ticking them would be `§Anti-patterns §6` drift.
