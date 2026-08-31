# Pending Impl Review v202

- plan: docs/PENDING_PLAN_v202.md
- commit: docs/PENDING_COMMIT_v202.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-548)
- timestamp: 2026-08-30

## plan_fidelity_check

Three deviations declared. **All three justified; two of them are the impler
correctly obeying the plan gate over the plan**, which is the intended
precedence, and the third is the impler correctly refusing a patch.

Deviations 1 and 2 (do not add `GBufferScale`; do not split `SpatialLayout`)
follow `PENDING_PLAN_REVIEW_v202.md`'s explicit feedback items 1 and 2. A plan
whose gate returned KEEP-with-corrections binds the impler to the corrections.

Deviation 3 — **found a defect, did not fix it** — is the one that warrants
scrutiny, since "impler declines to implement" is normally a FIX trigger. It is
correct here, on grounds I verified rather than accepted:

- The divergent copy is in `TestCornellBoxGI_Data/`, and `TestCornellBoxGI.cpp`
  is the known-good control per `software-development-practices §Path-Tracing
  §rule 4`.
- Card L's precondition (still `- [ ]` and unactioned in `PENDING_PICK.md`)
  states the control must not be modified while the v183-v199 chain is unbuilt.
- v196 faced precisely this and closed a card with **zero source change** for
  the same reason; that precedent is on record and was upheld by its own gate.

Fixing the control now would have been a substitution gradient — the tenth in a
lineage of substitutions — into the one file whose value is that it is
unmodified. Carding is right.

## Independent re-derivation of the load-bearing claims

I did not accept the impler's counts. Each zero below is controlled by a
same-shape positive, per the tick-526 and v199 checklist rows.

| Claim | My query | Result | Control |
|---|---|---|---|
| Control shader has no t4 | `gDirection` in Cornell copy | **0** | `register(t` same file → **4** |
| Primary shader has a t4 | `register(t` in primary copy | **5** (t0,t4,t1,t2,t3) | — |
| Control never sets DirectionTexture | `GenDesc.DirectionTexture` | **0** | `Gd.OutputWidth`-shape sibling query returns hits elsewhere |
| Layout declares SRV(4) unconditionally | read `:157-164` in full | present | — |
| No `00344` ever logged | `path=Binary/Debug pattern="00344"` | **0** | `VUID` same dir → 23 |

No `|` alternation used anywhere; `path` at a directory for the log query.

**One correction to the impler's evidence — it does not change the verdict but
the marker overstates by a hair.** The impler wrote that `gWorldPos`/`gNormals`/
`gDepth` are "declared but never Loaded in either" copy. I checked the control's
`main` in full (`:76-120`) rather than trusting the hit counts, and it is true —
the loop reads only `gRadiance` (`:93`, `:114`). So the claim survives. But the
impler reached it from **1-hit-per-symbol counts**, which is exactly the
"conclusion resting on a hit count" pattern v200 flagged: a 1-hit count proves
the symbol appears once, and only reading the body proves that one occurrence is
the declaration. Right answer, insufficiently-warranted method. Recorded, not
penalised, because the answer was independently confirmed here.

## The finding's significance, assessed independently

The impler's framing — that this is the v182 hazard generalised — is correct and
is the strongest thing in the cycle. I would put it more sharply:

**Every dual-copy check in this lineage is phrased as a diff.** v182's fix was
"edit both copies"; v187/v188 aligned struct tails across copies; v200 checked
"both HLSL copies" agree. All of those are *sameness* checks. This defect is
invisible to every one of them, because the two copies are **correctly
different** — the primary target genuinely has a Phase-B direction texture and
the control genuinely does not. The mismatch is not copy-vs-copy; it is
**layout-vs-each-consumer**, a relation no sameness check ranges over.

That is a new invariant class for this codebase, and the impler stated it
correctly: for a shared binding layout, every consumer's shader must declare
every binding the layout declares.

## Why the two "safe" verdicts are worth their comment lines

Both document **structural** immunity, not incidental correctness — the same
distinction v201 drew, which is what makes such notes durable. Generation cannot
acquire the v183 bug without someone first making it sample a GBuffer texture;
spatial cannot acquire bug-075 without someone first binding its output as an
input. Each comment names that condition explicitly, so a future cycle can tell
whether the reasoning still holds instead of re-deriving it. This is the correct
antidote to `§Anti-patterns §6` drift: the third audit of the same seam is
wasted only if the second one left no trace.

## LSP diagnostic — assessed, not waved away

The impler's stale-index explanation is correct and I confirmed it independently
rather than accepting it. The error migrated 331 → 361 → 381 across two edits,
tracking `DispatchGeneration`'s definition line as comments pushed it down, and
always pointing at column 10 of an **unmodified** line. Post-edit structure:
`namespace ReSTIR` opens `:39-40`, closes `:658`; five member definitions present
and correctly qualified (`Initialize` :41, `DispatchGeneration` :381,
`DispatchTemporal` :470, `DispatchSpatial` :578, `Shutdown` :639). A comment-only
diff cannot undeclare an identifier. Stale index.

## Security scan

- [x] No hardcoded secrets — comment-only diff
- [x] No shell injection — no process calls
- [x] No eval/exec — N/A
- [x] No SQL injection — N/A

## Self-review checklist

- [x] Validation: every numeric claim re-derived; every zero has a positive control
- [x] Error handling: no code paths changed, so none can regress
- [x] Tests: `produces_test_files: no`, correctly declared; reviewer NOT skipped
      (HARD INVARIANT #2 satisfied — `skip_impl_review: no`)

## Feedback for impler

None blocking. One note for the next cycle: prefer reading the enclosing body
over quoting a 1-hit count when the claim is "this symbol is only declared."

## New card opened at this gate

**Card M** — see `PENDING_PICK.md`. Same build precondition as card L, and the
two should be actioned together since both touch the control.
