# Pending Commit v210

- plan: docs/PENDING_PLAN_v210.md
- files: Engine/Source/Runtime/Public/Renderer/PostProcess/FReSTIRPass.h
- source: no bundle
- target: no branch — working tree only (no commit per job instruction)
- task: Card S — document `FReSTIRPass`'s guide-extent contract, the third of
  three distinct sibling contracts and the only undocumented one.
- verify: ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test
- skip_impl_review: no
- produces_test_files: no
- notes: +22 comment, **0 functional**, 1 file. No shader, no cbuffer, no
  signature, no member touched.

## Change

One block at `FTemporalDesc` stating the three sibling contracts side by side
and why this class cannot derive the ratio; one two-line pointer at
`FSpatialDesc`. Both anchored on **declaration statements**, not on comments
adjacent to a braced initialiser — the v203 hazard the plan review flagged.

## Verification performed (file-only)

1. **Read the returned diff** rather than asserting the line count. Purely
   additive: two `@@` hunks, every `-` line count zero. All 14 `FTemporalDesc`
   members and all 8 `FSpatialDesc` members present and in original order.
   This is the check v203's near-miss made mandatory.
2. **Line count corroborates the diff independently**: 169 → 191 = +22,
   exactly the comment lines added, so nothing was displaced.
3. **`nvrhi::TextureHandle` → 26** in the file, unchanged in kind; structure
   intact — class closes `:190`, namespace `:191`, 191 lines total.
4. **Every factual claim in the comment was verified before writing it**, not
   after:
   - "calls getDesc() only on OutReservoir0 ... never on a guide" —
     `getDesc` → 3 hits in `FReSTIRPass.cpp`, all `OutReservoir0`/`OutRadiance`.
   - "one scale serves all four guides" — `GB(` → 5 hits in
     `ReSTIR_Temporal_cs.hlsl`: definition `:78` + `gDepth` `:135`,
     `gPrevDepth` `:178`, `gPrevNormals` `:179`, `gNormals` `:180`.
   - "max(int(s),1) turns 0 into the identity map" — `:80`,
     `int s = max(int(gConstants.GBufferScale), 1);` read in place.
   - "FReBLURPass ... has no scale field at all" — `GuideScale` → **0** in
     `FReBLURPass.cpp`, controlled by 1 in `FReBLURPass.h` and 3 in
     `FBilateralDenoisePass.h` (same query, same directory).
   - "FBilateralDenoisePass: the CALLEE derives the ratio" —
     `.cpp:185-187` read in place.

   This is the v190 check: a comment that states a false mechanism as its own
   justification is a defect, not documentation.

## Plan Deviations

**None.** The plan proposed comment-only, the plan gate ruled on it explicitly
and upheld it with a stronger argument than the plan's own, and that is what
was implemented.

## Severity — stated without inflation

**This cycle moves no pixel and clears no acceptance gate.** Both consumers
already set `GBufferScale` correctly (primary `WIDTH / max(HalfResWidth,1u)`
at `:1061`/`:1109`; control `1.0f` at `:1592`/`:1645`, correct because that
target is not half-res). There is **no defect today**. What the cycle removes
is the condition under which a future third consumer — or a future edit to
either existing one — reintroduces v183 silently, and the specific trap that a
reader arriving from `FBilateralDenoisePass.h` or `FReBLURPass.h` carries the
wrong invariant in.

## What was NOT done

Not built, not compiled, not run, not validated, not viewed. No commit, no
push, no governance file touched.
