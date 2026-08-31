# Pending Tests v206

- commit: docs/PENDING_COMMIT_v206.md
- tester: agent_5_tester (tick-552)
- timestamp: 2026-08-30
- test files produced: **none** (`produces_test_files: no`)

## Mode

File-only structural verification. Terminal is categorically denied this tick —
three probe shapes (compound, bare `pwd`, no-op `true`) all refused with
`pending_approval / tirith:unknown / exit_code -1`. A refused no-op builtin rules
out command content, arguments, path and working directory, so no build, run,
shader compile, validator or image inspection was possible. **Nothing below is
runtime evidence.**

Every row carries a same-shape positive control on a file known to contain the
token, per audit row 18, and I read the `error` field on every query.

## Verification table

| # | Claim | Method | Control | Result |
|---|---|---|---|---|
| 1 | Header edit is comment-only | `nvrhi::TextureHandle` on the header → 8, partitioned: 5 `FDesc` members `:94-98`, 2 overload params `:116`, 2 dummies `:142-143` | partition read in place | **PASS** |
| 2 | All 5 `FDesc` members present, original order, same types | direct read `:92-98` | — | **PASS** |
| 3 | `OutputWidth`/`OutputHeight` survive with defaults | direct read `:99-100` | — | **PASS** |
| 4 | Primary shader indexes guides raw | `GB(` → 0 on `TestReSTIR_GI_Temporal_Data/ReBLUR_cs.hlsl` | 22 in the same directory, same query | **PASS** |
| 5 | Guide reads use the bare dispatch coord | `:157` `gDepth.Load(int3(dispatchThreadID.xy,0))`, `:178` `gNormalRoughness.Load(…)` | — | **PASS** |
| 6 | Neighbour taps clamp to `OutputSize` | `:112` `clamp(samplePixel, 0, int2(gConstants.OutputSize)-1)` | — | **PASS** |
| 7 | No scale field in `FReBLURConstants` | read `:31-52` in full, 20 fields, none a scale; size-pinned by `static_assert` `:53` | `GuideScale` → 1 hit in the sibling header's struct | **PASS** |
| 8 | 6/6 primary operands at W x H | each traced to its creation site (table in the commit marker) | `const uint32_t W = WIDTH, H = HEIGHT` `:1616` | **PASS** |
| 9 | Consumer set closed at 2 | `ReBLURPass.Dispatch` → 2 tree-wide | — | **PASS** |
| 10 | Second consumer satisfies the invariant | 5/6 operands track `GBufferWidth/Height`; recreated in the resize block | creations + resize pairs read at `:566`/`:1187`, `:612`/`:1199`, `:876`/`:1233`, `:894`/`:1237` | **PASS** |
| 11 | Control's history is the card-L exception | `ReBLURHistoryTexture` → 13, creations `:932`/`:934` only, none in the resize block | the four recreated pairs in row 10 are the positive | **PASS** |
| 12 | Neither `ReBLUR_cs.hlsl` copy edited | 2 copies by query; both still raw-index | `dispatchThreadID.xy, 0` → 4 hits on the control copy | **PASS** |
| 13 | Cross-reference to the sibling is accurate | `GuideScale` → 4 hits: 1 in the new comment, 3 in `FBilateralDenoisePass.h` (`:27`, `:34`, `:36`) | — | **PASS** |
| 14 | `TestCornellBoxGI.cpp` unmodified | no write or patch issued against it this cycle | — | **PASS** |

**14/14 PASS.**

## Row 13 is the row I would flag if it failed

The header's new comment makes an assertion **about another file**: that
`FBilateralDenoisePass` documents the opposite contract. A cross-reference is
only as good as its referent, and a stale one is worse than none — it would
teach the wrong rule with the authority of a citation. Verified directly:
`FBilateralDenoisePass.h:27` "Dispatch derives a single GuideScale (guide width /
OutputWidth)", `:34` "dispatches half-res over full-res guides, which is what
GuideScale [exists for]", `:36` "required; sources GuideScale" — v205's fix,
present and saying what the new comment says it says.

## Row 12 note — the dual-copy check found a real asymmetry, not just a null

The control's copy raw-indexes too (`:155`, `:162`, `:172`, `:189`), so **both**
copies share the invariant. Had only one, the header comment would be false for
one consumer. This is the check v182 exists to force, and here it passes on
content rather than on byte-equality.

## Instrument note

No escaping failures this tick. `GB(` was issued **unescaped** — the v205 audit
recorded that escaping it (`GB\(`) yields `{"total_count": 0, "error": "grep:
Unmatched ( or \\("}`, a zero from a query that never ran. Unescaped, it returns
22 on the directory and 0 on the file under test, which is the controlled form.

## NOT ESTABLISHED

That anything compiles, links, runs, renders or validates. Gates 1-7 of the job
instruction are untouched by this cycle; see the audit for the table.
