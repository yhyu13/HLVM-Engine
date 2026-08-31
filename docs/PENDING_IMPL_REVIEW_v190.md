# Pending Impl Review v190

- plan: docs/PENDING_PLAN_v190.md
- commit: docs/PENDING_COMMIT_v190.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-537)
- timestamp: 2026-08-30

## plan_fidelity_check

The plan recommended a comment-only correction and explicitly forbade applying
card D's remedy. The impl does exactly that. One deviation was declared (hunk 2,
cutting 7 now-duplicated lines from the v189 comment) and it is justified: after
hunk 1 states grid-independence and the scope limits, the v189 text asserted both
again twelve lines later. Two copies of a fact is two places for a future
correction to miss. Still comment-only, so the plan-criticer's binding constraint
is not engaged.

## Inertness — re-verified, not inherited

The load-bearing claim is that no functional line moved. I re-ran the impler's
row-1 query myself: `search_files pattern="Bd\."` → **9 hits** at `:871-874` and
`:882-886`, carrying `InputTexture`/`DepthTexture`/`NormalTexture`/
`OutputTexture` and `OutputWidth = HalfResWidth`/`OutputHeight = HalfResHeight`/
`0.05f`/`0.5f`/`4.0f`. Identical to the field list recorded in
`PENDING_COMMIT_v189.md`, which is the pre-edit ground truth for this block.

Two independent corroborations that the block's control flow is intact:

- `search_files pattern="BilateralDenoisePass.Dispatch"` → 1 hit at `:887`, with
  `:886` `Bd.SpatialSigma = 4.0f;` before it and `:888` `}` after — the guard
  `if (!bBypass)` at `:868` still encloses the whole block.
- `search_files pattern="HalfResWidth"` → 17 hits. `:882` is the bilateral site;
  the other sixteen (GI trace `:793`, generation `:901`, temporal `:988`,
  spatial `:1069`, resolve `:1099`, declaration `:2838`, assignment `:1592`, and
  the constants) are untouched. Nothing outside the comment shifted semantically.

## Verification of the claim the comment now makes

A comment that asserts a mechanism is only as good as the mechanism. I checked
the nvrhi source directly rather than accepting rows 7-8:

- `vulkan-compute.cpp:112-152` — `CommandList::setComputeState` ends with
  `commitBarriers();` at `:145`, after `insertComputeResourceBarriers` (`:122`)
  and `bindBindingSets` (`:139`).
- `vulkan-compute.cpp:166-173` — `CommandList::dispatch` in full is `assert`,
  `updateComputeVolatileBuffers()`, `cmdBuf.dispatch()`. **No barrier call.**
- `FReSTIRPass.cpp:400` — `DispatchGeneration` issues `setComputeState`;
  temporal's binding sets are created later at `:481`/`:489`.

So the comment's central assertion — the flush belongs to `setComputeState`, not
to the dispatch, and generation supplies one before temporal binds — is
accurate. The old comment it replaced was not.

## The comment does not overstate (check 2 from the impler)

It says deletion is "gated on absence-evidence (that no VUID-00344 appears in a
real run), which cannot be established from source." That is the correct
epistemic status and it does not read as licence to delete. Good.

## Stale-cross-reference check (check 3)

The impler's row-5 query is the right shape — `// .*:[0-9][0-9]+` scans comment
text file-wide, not just the edited hunks. I re-ran it: **0 hits.** This is
stronger than v189 achieved, where the verification pass had to catch stale
`:1148`/`:1570` references *after* they were written. Notably it also means the
planner's off-by-twelve `:1111` (caught at the plan gate) did not reach source.

## Security scan

- [x] No hardcoded secrets — comment text only
- [x] No shell injection — no process execution added
- [x] No eval/exec — N/A (C++)
- [x] No SQL injection — N/A

## Self-review checklist

- [x] Validation: no input handling changed
- [x] Error handling: no control flow changed; `if (!bBypass)` guard intact
- [x] Tests: none required — zero functional lines; `produces_test_files: no`

## The one thing I want on record against this cycle

A comment-only cycle is close to the anti-pattern this pipeline is warned about
(`§Anti-patterns §5`: don't run six roles for a trivial patch). I considered FIX
on those grounds and rejected it, because the *deliverable of this cycle is not
the comment* — it is the refutation of a queued instruction that would otherwise
have been executed next tick. Card D would have deleted a dispatch and inserted
a line that nvrhi's source shows to be a no-op in that position. The comment is
just where that finding gets recorded so it survives.

That said: **if the next cycle also produces no functional change, the pipeline
should stop and say so**, rather than continue generating documentation. The
remaining work on this repair is genuinely build-gated.

## Feedback for impler (FIX only)

None — KEEP.
