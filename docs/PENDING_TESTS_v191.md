# Pending Tests v191

- commit: docs/PENDING_COMMIT_v191.md
- tester: agent_5_tester (tick-538)
- timestamp: 2026-08-30
- mode: **static verification only.** `terminal` is denied in this runspace, so
  nothing was compiled, run or measured. Every row below is a source query whose
  result is reproducible by re-running the stated pattern.
- tooling rule honored: **no `|` alternation in any pattern** (tick-526 proved
  alternation silently returns 0 hits here, making such rows vacuous). One term
  per query; `path` always at a directory or a file that exists.

## Rows

| # | Claim under test | Query | Result | Verdict |
|---|---|---|---|---|
| 1 | Both sites use the fixed operand | `WIDTH / std` | 2 hits: `:1039` `TC.GBufferScale`, `:1087` `SC.GBufferScale`, both `static_cast<float>(WIDTH / std::max(HalfResWidth, 1u))` | PASS |
| 2 | The old form is gone tree-wide | `FB\.width / std::max` over `Engine/Source/Runtime` | **0 hits** | PASS |
| 3 | `FB.width` survives only where it should | `FB\.width` in the file | 16 hits: `:754/756` resize check, `:764` view constants, `:773` raster, `:863/1024/1029/1085` comment text, `:1109/1138` resolve, `:1164/1166/1185` ReBLUR, `:1220/1237` accumulate, `:1251` blit. **None in the temporal or spatial constants blocks.** | PASS |
| 4 | `WIDTH` is not shadowed at either site | `WIDTH` in the file | 40 hits; sole declaration `:106` `static const uint32_t WIDTH = 800`; locals are `const uint32_t W = WIDTH` at `:1541`, `:1667`, `:1760` — all in other functions. `Render()` binds neither. | PASS |
| 5 | Numerator matches the textures `GB()` indexes | `const uint32_t W = WIDTH` | `:1541` opens `CreateGBufferTextures`, which creates `GBufferNormal` and `LinearDepthTexture` from that `W` | PASS |
| 6 | Divide-by-zero guard intact | `std::max(HalfResWidth, 1u)` (via row 1) | present at both `:1039` and `:1087` | PASS |
| 7 | Denominator untouched | `HalfResWidth` | **18 hits** — see the discrepancy note below | PASS (with correction) |
| 8 | Shader consumer unchanged | `GBufferScale` in `TestReSTIR_GI_Temporal_Data` | 5 hits: `ReSTIR_Spatial_cs.hlsl:25/54`, `ReSTIR_Temporal_cs.hlsl:38/42/80` — identical to pre-patch | PASS |
| 9 | Cornell untouched | `GBufferScale` over `Engine/Source/Runtime` | 21 hits; `TestCornellBoxGI.cpp:1592` and `:1645` still assign `1.0f` literals | PASS |
| 10 | No new stale cross-reference | `// .*:[0-9][0-9][0-9]` in the file | 5 hits (`:894`, `:990`, `:993`, `:994`, `:2305`), all referencing *other* files | PASS |
| 11 | The v191 comments carry no line numbers | `v191` in the file | 3 hits at `:1024`, `:1029`, `:1085`; the grep anchor at `:1086` reads `(search: "v191: the numerator")` and `:1024` is that anchor's target | PASS |
| 12 | No shader file edited this cycle | `ShaderMake.cfg` contents | 12 entries, all unchanged; no `.hlsl` appears in `PENDING_COMMIT_v191.md` `files:` | PASS |

## Discrepancy found — row 7, and it is the tester's own catch

`PENDING_COMMIT_v191.md` row 5 states `HalfResWidth` → **17 hits, unchanged
count**. The actual count after the patch is **18**
(`search_files output_mode=count` and the enumerated form agree).

Diagnosed rather than waved off: the extra hit is `:1028`, which is **comment
text the impler itself added** ("HalfResWidth is W/2 off that same constant").
The sixteen non-`GBufferScale` *code* sites are unchanged, which is what the row
was actually trying to establish. So the underlying claim holds and the stated
number does not.

This matters beyond bookkeeping: v190's review gate quoted "17 hits" as evidence
of inertness, and a later cycle re-running that query would now get 18 and could
read it as drift. **The correct invariant to carry forward is "16 code sites plus
N comment mentions", not a raw total.** Recorded so the number is not re-quoted
as-is.

## A row that would have been vacuous, and why it isn't recorded as a pass

My first attempt at row 1 used the fully-escaped form
`static_cast<float>\(WIDTH / std::max\(HalfResWidth, 1u\)\)` → **0 hits**. Taken
at face value that reads as "the patch is not present" — a false failure. The
plain-substring form `WIDTH / std` returns the 2 real hits. The 0 was a regex
artifact, not a source fact.

This is the tick-526 lesson in its second form: alternation is not the only way
to get a vacuous result on this runspace. **Any 0-hit result must be confirmed
against a positive control before being recorded either as a pass or a failure.**
Row 2's 0-hit (the old form being gone) was confirmed the same way — `FB\.width`
alone returns 16, so the query shape does match text in this file, and the 0 for
`FB\.width / std::max` is a genuine absence.

## What these rows do NOT establish

That the file compiles. That the target links. That any pixel, `M mean`, dump or
validator result is what it was. **No build, no run, no image.** The patch's own
falsifiable prediction — byte-identical output at the default 800x600 extent — is
untested and stated as a prediction, not a result.
