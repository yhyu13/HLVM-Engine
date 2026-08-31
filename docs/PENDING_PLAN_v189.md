# Pending Plan v189

- task: Card B — bilateral denoise dispatch is sized from the FULL-res
  framebuffer while its input texture is HALF-res (fourth instance of the
  Phase-D omission class, first outside the ReSTIR passes)
- source: no bundle — direct edit
- diff_estimate: +8 / -2 lines (1 file, plus comment)
- skip_plan_review: no
- produces_test_files: no

## The card was deferred three times. Both stated reasons are refuted from source.

Card B was opened by tick-533 and deferred by ticks 533/534/535 for two stated
reasons. This plan's substance is that **both are checkable without a build, and
both are false.** That is what unblocks a card that has been called
"hard-blocked" for three cycles.

### Deferral reason 1 — "the correct value is genuinely ambiguous because the
### two choices populate different regions of a dump that `validate_restir_gi.py`
### and gate 6 both read." **REFUTED.**

`DenoisedTexture` is written by the bilateral pass at
`TestReSTIR_GI_Temporal.cpp:851`. Traced every subsequent read of that handle
(`search_files` → 13 hits, all inspected):

| Line | Use | Reaches a validated artifact? |
|---|---|---|
| 1117/1148 | ReBLUR `OutputTexture` — **full overwrite**, dispatched at `FB.width/FB.height` (`:1149-1150`, grid at `FReBLURPass.cpp:248-249`) | no — bilateral content is gone |
| 1162-1166 | copy to `ReBLURHistoryTexture[0]` — *after* the ReBLUR overwrite | no |
| 1168 | `AccumInput = DenoisedTexture` — **inside `if (bReBLURInitialized && !bBypass)`**, i.e. only ever the post-ReBLUR content | no |
| 2519 | `DumpRGBA32FTexture(DenoisedTexture, TXT("denoised"))` | **no — see below** |
| 2651 | dump-name table entry `{"denoised", DenoisedTexture}` | no |

The `denoised` dump is the only artifact carrying bilateral output, and **no
acceptance gate reads it**:

- `validate_restir_gi.py:190` — `required = ["display", "spatial", "gi_raw",
  "gbuffer_material"]`. `search_files pattern="denoised"` over the validator →
  **0 hits**. All four checks run on `display` only (`:207-214`).
- `v176-recipe.sh` — `search_files pattern="denoised"` → **0 hits**. Gate 7 reads
  the `gi_raw` dump (`:294`, `:299`), not `denoised`.
- Gate 6 (vision) reads `display`.

When ReBLUR is off (`HLVM_RGI_REBLUR=0`, `:551`), `AccumInput` stays
`FullResSpatial` (`:1111`) — the bilateral result still never reaches `display`.

**So the dispatch extent cannot move any validated pixel, on any code path.**
The choice is not ambiguous; it is unobservable at the gates, which is the
opposite of the blocking condition the card asserted.

### Deferral reason 2 — "the dispatch is retained for a barrier-flushing side
### effect, so changing its grid could perturb the layout transitions it exists
### to flush." **REFUTED.**

The flush is a property of **binding-set creation + `setComputeState`**, not of
the grid. `FBilateralDenoisePass::Dispatch` builds the binding set at
`:167-176` and calls `setComputeState` at `:185` — both **before** `:186`
`CmdList->dispatch(dispatchX, dispatchY, 1)`, and both independent of
`dispatchX/Y`. The comment at `TestReSTIR_GI_Temporal.cpp:838-842` says exactly
this: "its execution forces nvrhi to emit the pending layout transitions ...
BEFORE the ReSTIR binding sets are created."

Changing `OutputWidth/Height` changes only `:179-180` (`dispatchX/Y`) and
`:158-159` (`TexelSize`). The textures bound, their required states, and the
barrier list are byte-identical. **The side effect is preserved exactly.**

## The actual defect

`:848` `Bd.InputTexture = OutputTexture`, created at `HalfW x HalfH` = 400x300
(`:1560-1566`, `W = 800`). `:852-853` `Bd.OutputWidth = FB.width` = 800.

`FBilateralDenoisePass::Dispatch` derives **both** the grid (`:179-180`,
800/8 = 100 groups) and `TexelSize` (`:158-159`, 1/800) from that width. The
shader recovers `outputSize` by inverting `TexelSize`
(`BilateralDenoise_cs.hlsl:60`) and uses it for its early-out (`:62`) and its
5x5 neighbour bounds test (`:87`).

So ~3/4 of launched threads pass an early-out computed against 800x600 and then
`Load` a 400x300 input at coordinates up to 800x600. HLSL out-of-bounds `Load`
returns 0, so three quarters of the work is a bilateral blur of zeros, at 4x the
thread cost, feeding a texture nothing reads.

## Approach

One edit in `TestReSTIR_GI_Temporal.cpp`, matching the input texture:

```
Bd.OutputWidth  = HalfResWidth;    // was FB.width
Bd.OutputHeight = HalfResHeight;   // was FB.height
```

`HalfResWidth`/`HalfResHeight` are members (`:1562-1563`) already used in this
same function 19 lines later at `:871-872`, so no new plumbing.

## What this fix does and does not claim

**Does:** every `t_Input.Load` becomes in-bounds; the launched grid stops being
4x oversized; the pass stops matching the Phase-D omission signature.

**Does NOT:** make the pass *coherent*. `DepthTexture`/`NormalTexture`
(`:849-850`) are full-res MRTs and `DenoisedTexture` (`:1570-1572`) is full-res,
so after this fix the guides are still sampled in their top-left quadrant and
the output is still written only in its top-left quadrant. **That is acceptable
only because the output is provably dead** (table above) — and it must be stated,
not glossed, or a later reader will cite this patch as evidence the pass is
correct.

The honest full fix is to delete the dispatch and replace it with an explicit
`CommandList->commitBarriers()` — the idiom this very file already uses for this
very purpose at `:1157`. That is deliberately **not** done here: it is a
different change (removing a pass vs. resizing one), it is the one edit that
could actually perturb the VUID-00344 flush the comment describes, and it wants
its own cycle with a run. Opened as a follow-up card instead.

## test_strategy

File-only static verification (no shell in this runspace):
1. The two edited lines read `HalfResWidth`/`HalfResHeight`.
2. `HalfResWidth` is assigned before use in program order (`:1562` vs `:852`).
3. The input texture's creation extent matches the new dispatch extent.
4. Re-derive deadness: every `DenoisedTexture` read site, and 0 hits for
   `denoised` in both the validator and the recipe.
5. Grid-independence of the barrier flush: binding-set creation and
   `setComputeState` both precede `dispatch` and neither reads `dispatchX/Y`.
6. Cornell (the other `FBilateralDenoisePass` caller,
   `TestCornellBoxGI.cpp:1478-1488`) is untouched and already consistent —
   `HDRTexture` and `DenoisedHDRTexture` are both `GBufferWidth x GBufferHeight`
   (`:867-868`, `:885-886`), and it passes `CurrentFBInfo.width/height`. Confirm
   this cycle does not touch that file.

## risks

- **The `denoised` dump changes content.** Today: top-left quadrant filtered,
  remainder a blur of out-of-bounds zeros. After: top-left quadrant filtered,
  remainder untouched. Neither is validated by any gate, but the PNG will differ
  and that must not be reported as a regression *or* as an improvement.
- No shader is touched, so the v182 "patched a copy nothing compiles" trap does
  not apply.
- v183/v184/v185 remain one dependency chain awaiting a single operator run;
  this patch is in a different pass and must not be bundled into that judgement.
- Single-profile host: all six roles are the same model (`§Anti-patterns §7`).
