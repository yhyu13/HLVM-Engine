# Pending Tests v192

- commit: docs/PENDING_COMMIT_v192.md
- tester: agent_5_tester (tick-538)
- timestamp: 2026-08-30
- mode: **static verification only.** `terminal` denied; nothing compiled, run or
  measured. Every row is a source query, reproducible by re-running the pattern.
- discipline: no `|` alternation; `path` at a real file or directory; **every
  zero confirmed against a positive control** (three false-zero mechanisms are
  now documented on this runspace); **no count quoted from another marker.**

## Rows

| # | Claim under test | Query | Result | Verdict |
|---|---|---|---|---|
| 1 | Resolve constants use the fixed extent | `RcpFullW` | 2 hits: field `:1101`, assignment `:1126` `= 1.0f / static_cast<float>(WIDTH)` | PASS |
| 2 | Resolve constants use fixed height | `RcpFullH` | 2 hits: field `:1101`, assignment `:1127` `= 1.0f / static_cast<float>(HEIGHT)` | PASS |
| 3 | Resolve dispatch uses the fixed extent | `dispatch((WIDTH` | 1 hit `:1156` `dispatch((WIDTH + 7) / 8, (HEIGHT + 7) / 8, 1)` | PASS |
| 4 | No swapchain extent left in the resolve block | `FB\.width` | 15 hits; none between `:1104` and `:1160` | PASS |
| 5 | Shader byte-unchanged | `int2 fp = hp` | 1 hit `Resolve_cs.hlsl:60` `int2 fp = hp * 2 + 1;` | PASS |
| 6 | Shader not in the commit | `files:` in `PENDING_COMMIT_v192.md` | one entry, `TestReSTIR_GI_Temporal.cpp` | PASS |
| 7 | Half-res operands untouched | `RcpHalfW` | still `1.0f / static_cast<float>(HalfResWidth)` | PASS |
| 8 | Both resolve outputs are fixed-size | `DispatchResolve` | 3 hits: lambda `:1132`, calls `:1159` `(OutputTexture, FullResGIRaw)`, `:1160` `(SpatialRadiance, FullResSpatial)` | PASS |
| 9 | `HEIGHT` unshadowed in `Render()` | `HEIGHT` | 8 hits; sole declaration `:107`; every local `H = HEIGHT` in another function | PASS |
| 10 | No new stale cross-reference | `// .*:[0-9][0-9][0-9]` | 5 hits, all into *other* files, unchanged by this cycle | PASS |
| 11 | v191's fix undisturbed | `WIDTH / std` | 2 hits `:1039`, `:1087` | PASS |
| 12 | The card's open question is answered in source | `deliberately NOT` | 1 hit `:1121`, the shader-hardcode note | PASS |
| 13 | The severity claim is real | `FullResOutput` | 3 hits in `Resolve_cs.hlsl`: declaration `:10`, writes `:34` and `:73`, both indexed by raw `tid.xy`, no extent guard in the kernel | PASS |

## False zeros encountered and controlled — TWO this cycle

Recording both, because the mechanism is now reproducible rather than anecdotal.

1. **Row 3.** `WIDTH \+ 7` → **0 hits**. `dispatch((WIDTH` → 1 hit. The escaped
   `\+` form fails where the plain substring succeeds.
2. **Row 5.** `hp \* 2 \+ 1` → **0 hits**. `int2 fp = hp` → 1 hit.

Both would have read as "the patch is not present" / "the shader was changed" —
i.e. **false failures**, the opposite polarity from tick-526's false passes. Both
were caught by re-querying in a different shape before recording.

Positive control for row 4's absence-claim: `FB\.width` alone returns 15 hits in
this file, so the pattern demonstrably matches text here; the absence within
`:1104-1160` is a source fact.

**Running total: three distinct false-zero mechanisms** — `|` alternation
(tick-526), `output_mode=count` (v191 audit), over-escaped regex metacharacters
(v191 tester, and twice more here).

## What these rows do NOT establish

That the file compiles or links. That any pixel, `M mean`, dump or validator
result is what it was. **No build, no run, no image.** The byte-identical-output
prediction is a prediction.
