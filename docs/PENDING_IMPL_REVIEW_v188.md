# Pending Impl Review v188

- plan: docs/PENDING_PLAN_v188.md
- commit: docs/PENDING_COMMIT_v188.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-535)
- timestamp: 2026-08-30

## plan_fidelity_check

The impl matches the plan on every acceptance criterion, and the one declared
deviation (line count 4x the estimate, all comment) is justified — the excess is
the 16-byte-straddle rationale the plan-criticer explicitly asked to be carried
into the comment. Declaring it rather than absorbing it silently is the correct
handling under `six-role-pipeline §Impler deviation policy`.

The second deviation note (buffer-size risk discharged) is also correct and I
re-confirmed it independently: `FReSTIRPass.cpp:317 BufferDesc.byteSize = 256`,
marshaller `float ConstantsData[64]` at `:424`, struct 45 floats = 180 bytes.

## Defect caught at this gate

**The impler's marker claimed `search_files pattern="float Pad\["` over
`Engine/Source/Runtime` → 0 hits. I ran it: 6 hits.** The claim was false as
written.

This matters more than a typo, because "0 hits" was offered as evidence that the
bug class is eradicated tree-wide, and a future cycle could have cited it that
way. Triaged all six:

| Hit | Kind | Verdict |
|---|---|---|
| `TestCornellBoxGI_Data/ReSTIR_Temporal_cs.hlsl:15` | v188 comment | not a declaration |
| `TestReSTIR_GI_Temporal_Data/ReSTIR_Temporal_cs.hlsl:37` | v184 comment | not a declaration |
| `FReSTIRPass.cpp:451` | v184 comment | not a declaration |
| `FGBufferFillPass.h:21` `Pad[12]` | real | **not this bug class** |
| `FToneMappingPass.h:30` `Pad[56]` | real | **not this bug class** |
| `FContactShadowsPass.h:18` `Pad[2]` | real | **not this bug class** |

The three real ones are safe for two independent reasons, both checked:

1. **They are trailing.** An array's own displacement only harms fields *after*
   it. `FGBufferFillPass.h:21`, `FToneMappingPass.h:30` and
   `FContactShadowsPass.h:18-19` are the last members of their structs. Nothing
   sits behind them to be pushed.
2. **They are memcpy'd, not marshalled field-by-field.** `FContactShadowsPass.cpp:152`
   `memcpy(ConstantsData, &Constants, sizeof(FContactShadowConstants))`, guarded by
   `static_assert(sizeof(...) == 256)` at `:21`. The v184 bug requires a
   *hand-written flat-offset* writer whose offsets can drift from HLSL's
   register packing — `FReSTIRPass.cpp` is the only such writer in this family.

So the corrected claim, now in the marker, is the narrow true one: 0 `Pad`
arrays remain in any `ReSTIR_*` shader or in `FReSTIRPass.h`. **Marker corrected
in place at this gate rather than passed downstream.**

General lesson, and it is the second of its kind in two cycles (v187 caught a
stale line-number cross-reference): **an impler asserting a query result in a
marker must have run that exact query.** A near-miss pattern that returns a
different count is worse than no evidence, because it reads as verified.

## Independent re-derivation of the substantive claims

- **Read-inertness, post-edit.** `gConstants\.` over the edited Cornell shader →
  7 hits (`:63/:89/:94/:98/:126/:156/:167`), none naming `SceneYaw`,
  `PrevSceneYaw`, `NearPlane`, `FarPlane` or `GBufferScale`. The patch cannot
  move a Cornell pixel via any consumed constant. Ran this myself on the
  post-edit file, not the pre-edit one.
- **Assignment coverage.** Queried each of the five separately (per the
  known `search_files` alternation defect recorded at tick-526 — alternation
  silently returns 0): `SceneYaw` `:1579`, `PrevSceneYaw` `:1580`, `NearPlane`
  `:1585`, `FarPlane` `:1586`, `GBufferScale` `:1592`. 5/5 assigned, all before
  `DispatchTemporal`. **Note: the combined alternation query returned 0 hits,
  which would have read as "none assigned" — the tick-526 rule earned its keep
  here.**
- **Compiled path.** `TestCornellBoxGI_Data/ShaderMake.cfg:6
  ReSTIR_Temporal_cs.hlsl -T cs`. Live, not dead. (v182 trap.)
- **Scope fence.** Only the two named files changed.

## Security scan

- [x] No hardcoded secrets
- [x] No shell injection (no shell at all)
- [x] No eval/exec
- [x] No SQL

## Self-review checklist

- [x] Validation: constants derived from the call site (`:1276` projection,
      `:1585-1586` dispatch extents), not copied from the sibling test
- [x] Error handling: N/A — declarations and assignments only
- [x] Tests: none produced; `produces_test_files: no`, so HARD INVARIANT #2 is
      satisfied and `skip_impl_review: no` was honoured anyway

## Feedback for impler (FIX only)

None outstanding — the one defect was corrected in the marker at this gate.

## What this reviewer did NOT do

Did not build, compile, run, or view an image. slangc acceptance of the widened
struct remains the load-bearing unknown.
