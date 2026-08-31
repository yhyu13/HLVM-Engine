# Pending Impl Review v197

- plan: docs/PENDING_PLAN_v197.md (v197.2)
- commit: docs/PENDING_COMMIT_v197.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-543)
- timestamp: 2026-08-30

## plan_fidelity_check

The impl matches v197.2 exactly: three functional lines at the three sites the
revised plan named, no more. `PENDING_COMMIT_v197.md` declares **no deviations**,
and I confirm that independently — the diff touches one file, and every hunk is
one of the three table rows plus its comment. The plan gate's FIX items were
absorbed into the plan *before* implementation rather than applied as an impler
deviation, which is the correct route (HARD INVARIANT #4: a plan FIX loops to the
planner, it does not get patched around downstream).

## Independent verification of the arity change

The one thing that could make this cycle a compile failure is a call site the
impler did not see. Re-derived, not accepted:

| Query | Result | Reading |
|---|---|---|
| `RenderGBuffer()` | **2** — `:804`, `:2179` | call and definition, both new shape |
| `RenderGBuffer(FB.width, FB.height)` | **0** | old call shape gone |
| `RenderGBuffer(` (tree-wide) | **2** | complete set; no third site exists |
| `MeshCount, WIDTH, HEIGHT` | **1** — `:2446` | log substitution landed |

The `0` on the third row of the *old* shape is controlled by the `2` on the first
row of the new shape — same token, same tool, same file, non-zero. This is the
polarity discipline v197.2 required and it is satisfied.

**The method is not virtual, not overridden, has no forward declaration, and its
address is never taken** — it is a plain method on a single struct with one
caller. The arity change therefore has no hidden edge.

## The scope check I re-ran, because it is the only way this compiles or doesn't

`WIDTH`/`HEIGHT` must be visible inside `RenderGBuffer` for `:2446`. Confirmed at
`:106-107`: `static const uint32_t WIDTH = 800;` / `HEIGHT = 600;` at **file
scope**, so they are visible in every member of the struct below them, `:2446`
included. Type is `uint32_t`, identical to `LastWidth`/`LastHeight` (`:3014-3015`),
so the `HLVM_LOG` argument types are unchanged and the `{}x{}` substitution cannot
change formatting behaviour.

**`LastWidth`/`LastHeight` remain used** at `:754` (comparison) and `:756-757`
(assignment), so removing their only *other* use does not orphan them into an
unused-variable warning. I checked this because it is the kind of second-order
effect a substitution cycle would not have had to think about — and this is the
first non-substitution cycle in the lineage.

## The `FB.width` set after the patch — the row that proves nothing else moved

14 hits, of which **exactly 3 are live code**: `:754` resize detect, `:756`
`LastWidth` assign, `:1335` blit destination. All three are the deliberate
exclusions documented across v191-v196. The other 11 are comment prose, two of
them newly written by this cycle. **Card I's standing question — is the primary
target's `FB.width` enumeration clean? — is now answerable: yes, and this cycle
is what made it so.** v196 could only report "not clean, but the one live site is
inert"; that site is now gone.

## Security scan

- [x] No hardcoded secrets
- [x] No shell injection — no shell surface in this diff
- [x] No eval/exec
- [x] No SQL
- [x] No behavioural change to rendering — the arity change removes discarded
      arguments, and the log change alters a log string's arguments. **Neither can
      move a rendered pixel.** The viewport at `:2353` is byte-unchanged.

## Self-review checklist

- [x] Validation: candidate set closed at 2 and re-derived by me, not inherited
- [x] Error handling: n/a — no new failure path introduced
- [x] Tests: file-only, per host constraints; see `PENDING_TESTS_v197.md`

## What I am flagging without downgrading the verdict

**The impler's own false-zero (`constexpr uint32_t WIDTH` → 0) is the most
instructive thing in this cycle, and its marker undersells it slightly.** The
commit calls it a fifth mechanism and notes it is "not a tool quirk." I would put
it more sharply: the previous four mechanisms (alternation, escaped
metacharacters, unescaped metacharacters, line wrap) are all properties of
`search_files`. **This one is a property of the author.** The query asserted a
declaration form, the code used a different one, and the zero came back looking
exactly like a fact about the code. That is the card-E/G/H over-claiming failure
mode wearing a query's clothes — and it is more dangerous than the tool quirks,
because no rule about escaping or line wrap would have caught it. The only defence
is the one the impler used: pair the zero with a broader positive
(`uint32_t WIDTH` → 1) before believing it.

**Promoted rule:** *a query that encodes an assumption about the code returns
zero when the assumption is wrong, not when the code is absent. Before reading a
zero as a finding, widen the query until it returns something, then narrow.*

## Feedback for impler

None. KEEP.
