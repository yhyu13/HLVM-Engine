# Pending Tests v196

- commit: docs/PENDING_COMMIT_v196.md
- tester: agent_5_tester (tick-542)
- timestamp: 2026-08-30
- test files produced: **none** (`produces_test_files: no` — file-only verification of a zero-diff determination)

## Method

File-only row table. Every row executed by me this cycle, not quoted from
another marker. Discipline carried from the audits that established it:

- **No `|` alternation** in any pattern (tick-526: alternation silently returns 0).
- **`path` at a directory**, never at a file, when a zero must be meaningful.
- **Every zero paired to a same-shape positive**, or it is not evidence.
- **Cite symbols, not `:NNNN`** (v195 finding 6 — line numbers rot within the
  tick that writes them). Line numbers appear below only where the row *is* a
  location claim, and were re-queried at write time.

## Rows

| # | Claim | Query | Result | Verdict |
|---|---|---|---|---|
| 1 | Control not perturbed | `v196` in `TestPathTraceGI.cpp` | 0 | PASS (controlled by row 2) |
| 2 | Zero-control for row 1 | `v196` in `docs` | >0 (this cycle's markers) | PASS — same token returns non-zero elsewhere |
| 3 | Window non-resizable | `WindowProps.Resizable = false` | 1 hit `:1500` | PASS |
| 4 | Sibling contrast | `WindowProps.Resizable` in `TestReSTIR_GI_Temporal.cpp` | 1 hit, value `true` | PASS — contrast real |
| 5 | Denominator swapchain-derived | `Desc.OutputWidth = CurrentFBInfo.width` | 1 hit `:438` | PASS |
| 6 | Denominator NOT fixed-derived | `Desc.OutputWidth = WIDTH` | 0 | PASS — controlled by row 5, same shape same line |
| 7 | Numerator swapchain-derived | `UpdateViewConstants(CurrentFBInfo.width, ...)` | 1 hit `:422` | PASS |
| 8 | Ratio operands identical | rows 5+7 both `CurrentFBInfo` | — | PASS — `gbScale ≡ 1` |
| 9 | Candidate set closed | `CurrentFBInfo` in `TestPathTraceGI.cpp` | 12 hits, all classified in commit table | PASS |
| 10 | Fixed constants exist | `WIDTH =` | `static const uint32_t WIDTH = 800` `:54` | PASS |
| 11 | Resources fixed-size | `CreateTexture2D(NvrhiDevice, WIDTH, HEIGHT` | multiple (GBuffer + Output + Accum + Display) | PASS |
| 12 | Callee params unused | `void RenderGBuffer` | 1 hit, `(uint32_t /*W*/, uint32_t /*H*/)` | PASS — card K premise confirmed |
| 13 | Call site still passes swapchain | `RenderGBuffer(FB.width, FB.height)` | 1 hit `:793` | PASS — card K confirmed live |
| 14 | Call site not already fixed | `RenderGBuffer(WIDTH, HEIGHT)` | 0 | PASS — controlled by row 13 |
| 15 | CPU-ref camera inconsistency | `GetCameraProj(Rig, WIDTH, HEIGHT)` | 1 hit `:1058` | PASS — reviewer's finding confirmed |
| 16 | Aspect helper single-source | `float(W) / float(H)` | 1 hit `:189` | PASS |

**16/16 PASS.**

## DEFECT FOUND IN MY OWN METHOD — an uncontrolled zero, caught before it was recorded as evidence

Row 12 was first queried as the literal signature
`RenderGBuffer(uint32_t /*W*/, uint32_t /*H*/)`. **It returned 0.** Taken at face
value that reads as "the signature is not what the impler claimed" — a false
FAILURE, which the v192 audit identified as worse than a false pass, because a
false pass wastes a cycle while a false failure sends the next cycle to fix
something that is not broken.

The cause is regex metacharacters: `/*W*/` contains `*` quantifiers, and the
pattern is a regex, not a substring. Re-queried as the plain prefix `void
RenderGBuffer` → **1 hit, showing the full signature verbatim**, confirming the
claim the first query appeared to refute.

This is a **third** distinct false-zero mechanism in the lineage's tooling
record, and the tick-526 rule does not cover it:

- tick-526: `|` alternation → silent 0 (false pass).
- v192: `WIDTH \+ 7` and `hp \* 2 \+ 1` escaped forms → 0 (false failure).
- **v196 (new): unescaped `*` inside a code-literal pattern → 0 (false failure).**

The general rule these three converge on, stated at full strength: **`search_files`
patterns are regexes, so any query built by pasting a line of source is a
latent false zero.** Paste-a-line is the most natural way to build a
verification row and it is unsafe by default. **Query on the longest
metacharacter-free prefix instead, and read the returned line to confirm the
rest.** That is what row 12 now does.

## What these rows establish, and what they do not

**Establish (file-only, sound):** the control is byte-unchanged; both `gbScale`
operands in `TestPathTraceGI` are the same swapchain quantity so the ratio is
identically 1; the window is non-resizable while the sibling's is resizable; the
`CurrentFBInfo` candidate set is closed at 12; card K's premise (unused
parameters, live call site) is confirmed on both halves with a controlled zero.

**Do NOT establish — load-bearing:** that anything compiles, links, runs,
renders, or validates. **No build, no run, no image.** `terminal` was probed
twice this tick at two shapes and refused both times.
