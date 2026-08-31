# Pending Plan Review v210

- plan: docs/PENDING_PLAN_v210.md
- verdict: KEEP
- reviewer: agent_2_plan_criticer (tick-556)
- timestamp: 2026-08-30

## Design soundness

The plan identifies a real and previously unrecorded gap: three sibling
classes, three distinct guide-extent contracts, two documented. It correctly
identifies that the two documented ones are the *sibling-consistent* pair
(v205 said "guides free", v206 said "guides must match" specifically because
v205's header misleads about v206's class) — and that the third class, whose
contract is different from **both**, is the one with no header comment at all.
A reader arriving from either existing header carries a wrong invariant into
`FReSTIRPass`. That is the same failure mode v206 was written to prevent,
one class over.

## The plan's question — RULED, and its claim is TRUE and UNDERSTATED

The plan asked whether the remedy should be a comment or whether
`FReSTIRPass` should derive `GBufferScale` itself, and explicitly asked me to
test rather than accept its reasoning. Tested; it holds, and it is stronger
than the plan argued.

The plan said the class is "handed six-plus textures spanning both extents,
so a callee-derived ratio would have to pick a guide to trust." I checked the
actual read sites rather than the struct:

`GB(` in `ReSTIR_Temporal_cs.hlsl` → **5 hits** — the definition (`:78`) and
**four** call sites: `gDepth` `:135`, `gPrevDepth` `:178`, `gPrevNormals`
`:179`, `gNormals` `:180`. **One scale, four guides.** So the callee-derived
alternative would have to choose one of four to source the ratio from, and
that is precisely the shape v205 removed from `FBilateralDenoisePass` — where
the scale was derived from the **optional** guide and any consumer declining
it silently fell to `GuideScale = 1.0f`, restoring v204's defect through the
branch that looks like it handles the null case.

Worse here than there: `FTemporalDesc`'s four guides have **no** null-guard
anywhere in `DispatchTemporal`, so a `getDesc()` on an unset one is a null
dereference rather than a silent wrong weight. **Comment-only is correct, and
the functional alternative is actively unsafe.** The plan reached the right
answer for a slightly weaker reason than the one available.

## Plan completeness

Complete, with three additions I verified myself:

1. **`getDesc` → 3 hits in `FReSTIRPass.cpp`**, all on `OutReservoir0` /
   `OutRadiance` (the dispatch grid) — confirmed, none on a guide. The plan
   cited this; I re-derived it rather than accept it. It is the load-bearing
   fact: the class *structurally cannot* derive the ratio.
2. **`GuideScale` → 0 hits in `FReBLURPass.cpp`**, controlled by the 1 hit in
   `FReBLURPass.h` (v206's comment) and 3 in `FBilateralDenoisePass.h`. So the
   raw-index contract of the middle sibling is confirmed by absence-with-a-
   control, not by assumption.
3. **Both consumers already set the field correctly** — primary
   `WIDTH / max(HalfResWidth,1u)` (`:1061`, `:1109`), control `1.0f` (`:1592`,
   `:1645`, correct because that target is not half-res). **So this cycle has
   no defect to fix today**, and the marker must say so without inflation. It
   documents a contract that is currently satisfied by both consumers and is
   enforced and stated nowhere.

## Risk 2 is the one that matters — endorsed as written

v203 produced exactly this failure one cycle after v202 declared it the only
realistic one for a comment-only diff: an `old_string` anchored on a comment
above a braced initialiser matched into the list and deleted three live
binding items. `FReSTIRPass.h`'s `FTemporalDesc` / `FSpatialDesc` are brace-
initialised member lists. **Anchor on a declaration statement, and read the
returned diff before claiming the line count.**

## Feedback for planner

None — proceed to impl.
