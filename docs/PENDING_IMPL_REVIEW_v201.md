# Pending Impl Review v201

- plan: docs/PENDING_PLAN_v201.md
- commit: docs/PENDING_COMMIT_v201.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-547)
- timestamp: 2026-08-30

## plan_fidelity_check

The impl matches the plan and honours the plan gate's specific instruction — it
reports **structurally immune** rather than **clean**, with the distinction
argued rather than asserted. One deviation is declared (§Plan Deviations) and it
is not a departure at all: it is compliance with the review gate's directive.
Justified.

## The impler's enumeration was INCOMPLETE, and I closed it

This is the review's substantive catch. The impler established immunity from
16 `CreateTexture2D` sites, all resolving to `WIDTH`/`HEIGHT`. But
`CreateTexture2D` is a file-local helper (`:210`), **not** the only way this file
creates a texture. The raw API is also called directly: `createTexture` → 8 hits,
of which **two lie outside the impler's set**:

- `:570` `ReBLURHistoryTexture[i]` — inside a loop whose `Desc.width = WIDTH;`
  / `Desc.height = HEIGHT;` (`:563-564`). Fixed. **Consistent.**
- `:315` `PlaceholderTexture` — `Desc.width = 1; Desc.height = 1;`
  (`:307-308`). A 1x1 placeholder, correctly excluded from any extent set —
  the same shape and the same exclusion v199 made for `TestRTReflections.cpp:661`.

So the conclusion survives, but it survives a **stricter** enumeration than the
one that produced it. Had either site been swapchain-sized, the impler's method
would have missed it entirely, because it enumerated a *helper* rather than the
*operation*.

**Standing rule, promoted from this cycle:** enumerate the operation, not the
convenience wrapper. A helper-based enumeration is only complete if the helper
is the sole route to the operation, and that premise must itself be checked —
here it was false, and the marker's completeness claim rested on it silently.
This is the same failure shape as v198's (a query that cannot express what it
claims to cover), one abstraction level up.

## Corroboration the impler did not offer

`WindowProps.Resizable = true` (`:3035`) — so the immunity is *load-bearing*,
not moot. The control's card-J escape hatch (`Resizable = false`, extent pinned
at creation) does **not** apply here: this window really can resize, and the
file really is unaffected, because nothing it owns is extent-sized. That is a
stronger result than "the question never arises."

## Security scan

- [x] No hardcoded secrets — zero source files modified
- [x] No shell injection — no code changed
- [x] No eval/exec — n/a
- [x] No SQL injection — n/a

## Self-review checklist

- [x] Validation: every zero in the marker carries a same-shape positive; the
      `FB.width` classification reads all 13 hits rather than counting them
      (v200 rule applied correctly)
- [x] Error handling: the new `search_files` escaping failure is recorded as a
      loud failure, correctly distinguished from the eight prior silent ones
- [x] Tests: file-only enumeration; no test files produced, so
      `skip_impl_review: no` was still correct to set (HARD INVARIANT #2 is
      about not skipping, and it was not skipped)

## Feedback for impler (FIX only)

n/a — KEEP. The enumeration gap is closed in this marker rather than returned,
because the correction strengthens the impler's conclusion instead of reversing
it, and a FIX loop would produce the same verdict one round later.
