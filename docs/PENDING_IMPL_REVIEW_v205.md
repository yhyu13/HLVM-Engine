# Pending Impl Review v205

- plan: docs/PENDING_PLAN_v205.md
- commit: docs/PENDING_COMMIT_v205.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-551)
- timestamp: 2026-08-30

## plan_fidelity_check

The impl matches the plan, including the plan's instruction to test the card's
framing rather than implement it. No deviations declared and none found: the
plan authorised documenting the invariant, and the impler documented it — but
only after fixing the operand the documentation depends on, which is what the
plan gate required. Both files listed in `files:` were changed; no others.

## Independent re-derivation (not accepted from the marker)

- `Desc.NormalTexture` → **1** hit, `:199`, the binding-set item. The optional
  guide is still bound, correctly; it just no longer sources the scale.
- `Desc.DepthTexture` → **3** hits, `:183`/`:185`/`:198`.
- `ConstantsData\[5\]` → 1 hit; `ConstantsData` → 9 hits, slots 0,1,2,3,4,5 in
  a `float[64]` with a preceding `memset`. **Slot index unchanged from v204**,
  so the cbuffer layout is identical and both HLSL copies remain correct
  untouched — I verified the shader files were not modified rather than
  assuming it from the `files:` list.

## FALSE ZERO CAUGHT AND CORRECTED MID-REVIEW — worth recording

My first query for the constant slot was `pattern="ConstantsData[5]"` → **0
hits**, which reads as "the patch did not land". `[5]` is a regex character
class matching the single character `5`, so the pattern means
`ConstantsData5`. The escaped form `ConstantsData\[5\]` returns the real hit.

This is a new member of the false-instrument family this lineage tracks
(tick-526 alternation, v192's `WIDTH \+ 7`, v202's operand-vs-operation): **an
unescaped array subscript is a character class, and array subscripts are how
this codebase writes cbuffer slots** — the single most common shape of
load-bearing query in these audits. It is a false FAILURE, the dangerous
polarity: taken at face value it would have sent this cycle chasing a patch
that was already correctly applied.

Standing rule adopted: **escape `[` and `]` in any query naming an array slot,
and control every zero with the unsubscripted positive** (`ConstantsData` → 9).

## Security scan

- [x] No hardcoded secrets
- [x] No shell injection
- [x] No eval/exec
- [x] No SQL injection

## Self-review checklist

- [x] Validation: the null-guard is preserved, moved onto the mandatory guide;
      `GuideW && outputW` still prevents a zero-divide; shader-side
      `max(int(GuideScale), 1)` still floors the map at identity. Three
      independent degeneracy guards survive the change.
- [x] Error handling: unchanged. No new failure path.
- [x] Tests: none required — no test files produced. `produces_test_files: no`
      is accurate, so HARD INVARIANT #2 is not engaged; I ran anyway.

## Assessment of the choice not to parameterise

Endorsed. Card O presented two options and the impler took the cheaper one, so
this needs scrutiny rather than acceptance. The per-guide-scale option would
add a cbuffer field and a second remap helper to serve a consumer that does not
exist, in **both** HLSL copies — dragging the known-good control's shader into
the change while the v183-v205 chain is unbuilt. v196 established that the
control's value is at its maximum immediately before the chain's first build.
The cheap option is also the safe one here, and the expensive option's benefit
is unverifiable today.

## Card opened by this review

**Card P** — recorded in `docs/PENDING_PICK.md`. `FReBLURPass::Dispatch`'s
convenience overload manufactures dummy guides at `W, H`, i.e. the *dispatch*
extent. That is correct for that class today, but it is the same
guide-extent-versus-dispatch-extent relation this cycle just pinned in the
sibling class, and no cycle has checked whether ReBLUR's full `FDesc` path can
receive guides at an extent other than `OutputWidth`. Not bundled: different
class, and bundling would make this cycle's two-line claim unverifiable.

## Feedback for impler (FIX only)

None. KEEP.
