# Pending Commit v189

- plan: docs/PENDING_PLAN_v189.md
- files: Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp
- source: no bundle — direct edit
- target: (no branch — working tree only; job instruction forbids commit/push)
- task: size the bilateral denoise dispatch to its half-res input (card B)
- verify: `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild`
- skip_impl_review: no
- produces_test_files: no
- notes: see "What the reviewer must check" below

## Change applied

One hunk, `TestReSTIR_GI_Temporal.cpp:852-853` (pre-edit numbering):

```
-            Bd.OutputWidth     = FB.width;
-            Bd.OutputHeight    = FB.height;
+            Bd.OutputWidth     = HalfResWidth;
+            Bd.OutputHeight    = HalfResHeight;
```

+2 / -2 functional, plus 20 comment lines. Post-edit the assignments are at
`:872-873`; file grew 2974 → 2994 lines.

## Verified after applying (queries actually run, not asserted)

Per the v188 lesson — *an impler asserting a query result must have run that
exact query* — every claim below is a query I executed this turn, quoted with
its real result.

| # | Check | Query / read | Result |
|---|---|---|---|
| 1 | Edit landed, no stray sites | `search_files pattern="Bd\.Output"` | **3 hits**: `:851` OutputTexture, `:872` `= HalfResWidth`, `:873` `= HalfResHeight`. No `FB.width` remains on any `Bd.` field |
| 2 | Members exist | `search_files pattern="HalfResWidth"` | 16 hits incl. declaration `:2808` `uint32_t HalfResWidth = 0` |
| 3 | Initialised before use | read `:1562` `HalfResWidth = HalfW`; `:1560` `HalfW = W / 2` | assigned in resource setup |
| 4 | **In-function order proof** | `search_files pattern="HalfResWidth"` → `:793` | `:793 Desc.OutputWidth = HalfResWidth` is in the **same function**, 79 lines before `:872`. If it were 0 at `:872` it would be 0 at `:793` and the GI trace grid would be empty |
| 5 | Input extent | read `:1564-1566` | `OutputTexture` created at `HalfW x HalfH` — matches the new dispatch extent exactly |
| 6 | Output is dead — ReBLUR | read `:1148`, `:1149-1150` | `ReBLURDesc.OutputTexture = DenoisedTexture` at `FB.width/height`; `FReBLURPass.cpp:248-249` derives its grid from those → full overwrite |
| 7 | Output is dead — accumulate | `search_files pattern="AccumInput"` → 8 hits | only two assignments: `:1111` (`FullResGIRaw`/`FullResSpatial`) and `:1168` (`DenoisedTexture`, **inside** `if (bReBLURInitialized && !bBypass)` at `:1112`, after the `:1148` overwrite). No path carries bilateral output to `:1191` |
| 8 | Output is dead — validator | `search_files pattern="denoised"` on `validate_restir_gi.py` | **0 hits**. `:190` `required = ["display","spatial","gi_raw","gbuffer_material"]`; all 4 checks run on `display` (`:207-214`) |
| 9 | Output is dead — recipe | `search_files pattern="denoised"` on `v176-recipe.sh` | **0 hits**. Gate 7 reads the `gi_raw` dump (`:294`, `:299`) |
| 10 | Barrier flush preserved | read `FBilateralDenoisePass.cpp:167-186` | binding set `:167-176`, `setComputeState` `:185`, `dispatch` `:186`; `dispatchX/Y` computed `:179-180`, consumed only at `:186`. Grid-independent |
| 11 | No shader touched | this cycle's only edited file is the `.cpp` | v182 dead-copy trap cannot fire |
| 12 | Cornell untouched | read `TestCornellBoxGI.cpp:1478-1488`; `:867-868`, `:885-886` | passes `CurrentFBInfo.width/height`; `HDRTexture` and `DenoisedHDRTexture` both `GBufferWidth x GBufferHeight`. Already consistent; not edited |

Queries 8 and 9 were run as single-term searches per the tick-526 alternation
rule (no `|`).

## Plan Deviations

**None on the code.** The two assignments are exactly what the plan specified.

**One deviation on scope, additive:** the plan budgeted "+8 / -2 lines (1 file,
plus comment)". I wrote 20 comment lines rather than a short note. Reason: this
patch leaves the pass *incoherent by design* (guides full-res, output full-res,
input now half-res), and that is only defensible because the output is dead. A
one-line comment would leave the next reader to rediscover queries 6-9, or worse,
to read the patch as having fixed the pass. The comment states the limitation and
the "do not cite this patch as evidence the pass is correct" caveat in the
source, where it will be read. No functional difference.

## What the reviewer must check

1. That I did not overstate deadness. The load-bearing claim is query 7 — that
   `:1168` is unreachable with pre-ReBLUR content. Re-read `:1111-1168` and
   confirm the `bBypass` and `!bReBLURInitialized` paths independently. If any
   path carries bilateral output to `:1191`, this patch changes a validated
   pixel and the verdict must be FIX.
2. That the barrier-flush argument (query 10) is about the *right* dispatch. The
   comment at `:838-844` describes flushing transitions before the **ReSTIR**
   binding sets are created — confirm the ordering claim still holds with the
   grid changed.
3. That `:872-873` are inside the `if (!bBypass)` block (`:845`), so the bypass
   path is unaffected.

## Not claimed

Not compiled. Not run. No image inspected. `terminal` is denied in this runspace
(two probes this tick: `echo probe-ok; date -u; pwd` and a bare `/usr/bin/true`
with `workdir=/tmp` — both `status: pending_approval, pattern_key:
tirith:unknown, exit_code: -1, smart_denied: false`). No commit, no push, no
governance file touched.
