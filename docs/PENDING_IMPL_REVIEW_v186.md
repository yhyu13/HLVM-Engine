# Pending Impl Review v186

- plan: docs/PENDING_PLAN_v186.md
- commit: docs/PENDING_COMMIT_v186.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-533)
- timestamp: 2026-08-30

## plan_fidelity_check

The impl matches the plan's intent and exceeds its file list by one. The
deviation is declared in `## Plan Deviations` and I re-derived it
independently rather than accepting it: `search_files pattern="float2 Pad;"`
over the Runtime tree now returns 3 hits, and **none of them is a
`ReSTIR_Generate_cs.hlsl`** — confirming both generation copies were migrated.
`pattern="Pad0"` shows the Cornell generation copy at `:16`. Reading the
Temporal_Data copy directly shows `:27-28 float Pad0; float Pad1;`. Header at
`:28` reads `TFP32 Pad0; TFP32 Pad1;`. All three sides agree in kind.

**The deviation is justified and was the right call.** Had the impler patched
only the file the card named, `TestCornellBoxGI_Data` would have been left
declaring `float2 Pad` against a scalar C++ struct — the mismatch relocated,
not removed, under a marker claiming it fixed. Since `TestCornellBoxGI` is the
lineage's known-good control, degrading it to fix a cosmetic issue in the
target would have been a bad trade.

## Inertness — re-derived, not accepted

- `FReSTIRPass.cpp:354-363`: nine `offset++` writes ending at `DebugVis`.
  Floats 9/10 never written; `:352` memsets the buffer.
- `search_files pattern="gConstants.Pad"` over Runtime → 0 hits.
- Full read of both generation shaders: `Pad` appears only in the struct.

Write-never, read-never. The patch cannot move a pixel. Correct.

## Correction to the source card, which the plan caught and I confirm

The card asserted the two sides "would desync under the same HLSL
array-packing rule that v184 fixed." That is wrong as stated: `float2` is a
vector, not an array, so HLSL packs it at floats 9/10 and the C++ `Pad[2]`
also occupied 9/10. They agreed. The real justification is the narrower one
the plan substituted — two declarations disagreeing in kind is a trap for the
next appended field. Verdict unaffected; worth recording so the fix is not
later cited as evidence for a desync that never existed.

## NET-NEW DEFECT FOUND DURING THIS REVIEW — and it is live

While sweeping for other copies of the same pattern I checked the **spatial**
struct, which the card did not mention. The three sides do not agree, and this
one is not inert:

| side | fields after `DebugVis` |
|---|---|
| `FReSTIRPass.h:70-72` | `DebugVis; GBufferScale; Pad;` |
| `TestReSTIR_GI_Temporal_Data/ReSTIR_Spatial_cs.hlsl:24-26` | `DebugVis; GBufferScale; Pad;` ✅ |
| `TestCornellBoxGI_Data/ReSTIR_Spatial_cs.hlsl:18-19` | `DebugVis; float2 Pad;` ❌ **no `GBufferScale`** |

`FReSTIRPass::DispatchSpatial` (`:544-547`) writes `DebugVis` at float 8 and
`GBufferScale` at float **9** unconditionally, for every caller. The Cornell
spatial shader has no `GBufferScale` field, so float 9 lands in its `Pad.x` —
which it never reads (`search_files pattern="GBufferScale"` on that file → 0
hits). So today the effect is a silently-swallowed constant rather than
visible corruption.

**But it is a genuine C++/HLSL layout desync in the known-good control**, of
exactly the class v184 was created to fix, and it is one appended field away
from producing wrong values in the test the whole lineage uses to exonerate
the driver and framework. That makes it materially more dangerous than the
card this cycle closed.

**Not fixed here** — correctly. It is a different struct in a different file,
it needs the Cornell control rebuilt to verify, and bundling it would break
the "one variable per experiment" discipline this repair depends on. Queued to
PICK as its own card, with the evidence above.

## Security scan

- [x] No hardcoded secrets — three struct field renames
- [x] No shell injection — no shell available
- [x] No eval/exec — N/A
- [x] No SQL injection — N/A

## Self-review checklist

- [x] Validation: all three sides read directly post-patch, agree in kind
- [x] Error handling: unchanged; no control flow touched
- [x] Tests: none produced (`produces_test_files: no`, correctly declared, so
      HARD INVARIANT #2 is satisfied — the reviewer ran anyway)

## Assessment of the deferred bilateral finding

I verified the numbers independently: `:106 WIDTH = 800`; `:852
Bd.OutputWidth = FB.width`; `:848 Bd.InputTexture = OutputTexture`; `:1564-66`
`OutputTexture` created at `HalfW/HalfH`. The mismatch is real.

I also endorse **not** patching it. Two independent reasons, both checked:
the target resolution is ambiguous (input half-res, output texture full-res,
guides full-res), and `:837-844` documents the pass as retained for its
*barrier-flushing side effect* with its output unconsumed — later overwritten
by ReBLUR at `:1148`. Changing a dispatch whose purpose is barrier ordering,
without being able to run the validation layer, could reintroduce the
VUID-00344 class this comment says it suppresses. That is a fix that must be
made with a build in hand.

## Feedback for impler

None blocking. Two items carried to PICK rather than this cycle: the Cornell
spatial `GBufferScale` desync (found in this review) and the bilateral
half/full dispatch mismatch (found by the impler).

## Caveat

Single-profile host: reviewer and impler are the same model. I re-derived
every claim from source rather than reading the commit marker's assertions,
which is the most this shape offers, but it is a self-check, not fresh eyes
(`six-role-pipeline §Anti-patterns §7`). **No build, no compile, no run.**
