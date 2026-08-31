# Pending Tests v208

- commit: docs/PENDING_COMMIT_v208.md
- tester: agent_5_tester (tick-554)
- mode: file-only (terminal denied categorically — bare `true` refused this tick)
- test files produced: **none** (`produces_test_files: no`)

Every row re-derived by me. Every negative paired with a same-shape positive
control. No count quoted from another marker.

| # | Claim | Method | Result |
|---|---|---|---|
| 1 | v204 writer targets slot 5 | `ConstantsData\[5\]` in `FBilateralDenoisePass.cpp` | **1** ✅ |
| 2 | Slot 6 not also written (no overlap) | `ConstantsData\[6\]` same file | **0**, controlled by row 1 ✅ |
| 3 | Buffer holds slot 5 | read `:122` `byteSize=256`, `:156` `float[64]` | 256 B ≥ 24 B ✅ |
| 4 | HLSL primary declares slot 5 | read `BilateralDenoise_cs.hlsl:17-23` | `float2`+3 floats ⇒ offset 5 ✅ |
| 5 | HLSL control same offset | read `TestCornellBoxGI_Data/…:17-28` | `GuideScale_Unused` at offset 5 ✅ |
| 6 | Control does not consume it | `GB(` in control | **0**, controlled by **10** in primary ✅ |
| 7 | v203 `SpatialLayout` intact | read `:325-333` in place | 7 items, correct order ✅ |
| 8 | v203 `TemporalLayoutSRV` intact | read `:236-248` in place | cb + t0..t9 = 11 ✅ |
| 9 | Row 7/8 scope closed | `BindingLayoutItem::` tree-wide in file | **29** positive control ✅ |
| 10 | v207 u2 ternary present | read `FGIPass.cpp:645-650` in place | ternary + `setTextureState` ✅ |
| 11 | v207 fallback consumer omits u2 | `OutputDirection` in `TestPathTraceGI.cpp` | **0** ✅ |
| 12 | Row 11 controlled | `OutputTexture` same file | **8** ✅ |
| 13 | BRE: escaped alternation works | `Texture_SRV\|Texture_UAV\|ConstantBuffer` | **76** ✅ |
| 14 | BRE: bare `\|` is literal | same pattern unescaped, `FReBLURPass.h` | **0** vs **4** escaped ✅ |
| 15 | BRE: parens literal unescaped | `sizeof(FReBLURConstants)` | **1** ✅ |
| 16 | BRE: `\(` is a metachar | `sizeof\(FReBLURConstants\)` | **0** ✅ |
| 17 | BRE proven by error string | `static_assert\(sizeof` | `grep: Unmatched ( or \(` ✅ |

**17/17 PASS.**

## Correction to a number in v207's marker — recorded, not glossed

v207's commit states `Desc.OutputTexture` → **7 hits** in `TestPathTraceGI.cpp` as
the positive control for its `OutputDirection` → 0. **I could not reproduce 7.**
`Desc.OutputTexture` → **1**; the bare token `OutputTexture` → **8**.

Cause, determined rather than assumed: `output_mode=count` counts **matching
lines**, and the two tokens are different sets — `Desc.OutputTexture` appears once
(`:434`), while `OutputTexture` appears on 8 lines (creation `:265`, teardown
`:369`, the assignment `:434`, transitions `:454`, binding `:478`, dump `:1239`,
member decl `:1460`). v207 appears to have quoted the bare-token figure against
the qualified-token label, and the file has since had no relevant edit.

**This does not weaken v207's conclusion** — its load-bearing claim is that the
control never supplies u2, and `OutputDirection` → **0** with a same-file positive
of **8** establishes exactly that. But per the lineage's own rule that counts are
re-derived and never inherited, the mismatch is recorded rather than smoothed
over. **The label was wrong; the finding was right.**

## Reviewer's requested phrasing sharpening — applied

The soundness rule is stated over the **divergent** set, not "metacharacters"
generally:

> A recorded zero is sound iff its pattern contains no unescaped `|`, `+`, `?`,
> `(`, `)`, `{`, `}`. Bracket expressions (`[...]`), `^`, `$`, `.` and `*` behave
> identically in BRE and ERE, so queries such as `^- \[ \]` are sound as written.

## What was NOT tested

Nothing was compiled, built, linked, run, rendered, validated or viewed. No image
was inspected — this runspace has no image tool. No acceptance gate is exercised
by any row above.
