# Pending Tests v194

- commit: docs/PENDING_COMMIT_v194.md
- tester: agent_5_tester (tick-540)
- timestamp: 2026-08-30
- mode: **file-only**. `terminal` denied categorically this tick (two probes,
  including a bare `/bin/true`), so no build, no run, no shader compile, no
  validator, no image. Every row below is a source-level check.

## Discipline applied

- No `|` alternation in any pattern (tick-526: alternation silently returns 0).
- `path` always at a file or directory that exists; no `file_glob`.
- No `output_mode=count` relied on for a load-bearing conclusion.
- Plain substrings only; no escaped regex metacharacters (v191/v192 rule).
- **Every zero controlled** by a same-token or same-file positive.
- Every row re-executed by me; nothing quoted from the commit marker.

## Rows

| # | Check | Query | Expected | Actual | Verdict |
|---|---|---|---|---|---|
| 1 | `OutputSize` substituted | `ReBLURConstants.OutputSize` in the test `.cpp` | 2 hits, `WIDTH`/`HEIGHT` | 2 hits, `static_cast<float>(WIDTH)` / `(HEIGHT)` | PASS |
| 2 | `RcpOutputSize` substituted | `ReBLURConstants.RcpOutputSize` | 2 hits, `WIDTH`/`HEIGHT` | 2 hits, `1.0f / static_cast<float>(WIDTH)` / `(HEIGHT)` | PASS |
| 3 | Desc extents substituted, texture not | `ReBLURDesc.Output` | 3 hits; 2 changed, `OutputTexture` unchanged | exactly that | PASS |
| 4 | **Complete candidate set** | `FB.width` | all remaining hits justified | 12 hits, every one classified (below) | PASS |
| 5 | Blit correctly untouched | `FB.width, FB.height` | blit still present | 3 hits: `UpdateViewConstants`, `RenderGBuffer`, and the `BlitTexture` call | PASS |
| 6 | Shadowing cleared, both operands | `const uint32_t W = WIDTH, H = HEIGHT;` | none in scope at the patch site | 3 hits at 1607/1733/1826, all *after* the 1182-1233 block | PASS |
| 7 | `WIDTH`/`HEIGHT` are the file-scope constants | `static const uint32_t` | `WIDTH = 800`, `HEIGHT = 600` | both present, plus `DEFAULT_ACCUM_TARGET_FRAMES` | PASS |
| 8 | Desc field types | `OutputWidth` in `FReBLURPass.h` | `uint32_t` | `uint32_t OutputWidth = 0;` / `OutputHeight = 0;` | PASS |
| 9 | Fallback stays disengaged | `outputW` in `FReBLURPass.cpp` | guard is `if (!outputW && ...)` | 5 hits; guard confirmed, and `WIDTH` is non-zero | PASS |
| 10 | Fallback could not have fixed the constants | `Constants.OutputSize` in `FReBLURPass.cpp` | no write on the fallback path | 4 hits: 2 verbatim marshalling, 2 in the *other* overload. None in the fallback | PASS |
| 11 | ReSTIR shader copy untouched | `v194` in `TestReSTIR_GI_Temporal_Data/ReBLUR_cs.hlsl` | 0 | 0 | PASS (controlled, row 14) |
| 12 | `FReBLURPass.cpp` untouched | `v194` in that file | 0 | 0 | PASS (controlled, row 14) |
| 13 | Sibling shader copy exists and is untouched | `ReBLUR*.hlsl` by filename | 2 files | `TestReSTIR_GI_Temporal_Data/`, `TestCornellBoxGI_Data/` | PASS |
| 14 | **Zero-control** | `v194` in the test `.cpp` | non-zero | 3 hits (1182, 1232, 1233) | PASS |
| 15 | Default-extent no-op | `WindowProps.Extent` | `{ WIDTH, HEIGHT }` | `WindowProps.Extent = { WIDTH, HEIGHT };` | PASS |
| 16 | Kernel has no extent guard | `return;` in the ReSTIR `ReBLUR_cs.hlsl` | all returns are content tests | 3 hits: sky branch, zero-radiance branch, end of `main`. None compares against an extent | PASS |
| 17 | All UAV stores raw-indexed | `gOutput` in that shader | declaration + stores by `dispatchThreadID.xy` | 4 hits: `register(u0)` + 3 stores, all `gOutput[dispatchThreadID.xy]` | PASS |
| 18 | Scale-corruption path is real | `pixelUv` in that shader | exactly 2, definition + `IsHistoryValid` | exactly 2 | PASS |
| 19 | ReBLUR branch is on by default | `bBypass =` | set only from getenv | `bBypass = (std::getenv("HLVM_RGI_BYPASS") != nullptr);` | PASS |
| 20 | Output feeds the acceptance path | `DenoisedTexture` | assigned to `AccumInput` | `AccumInput = DenoisedTexture;` at the end of the branch | PASS |

**20/20 PASS.**

### Row 4 — the 12 `FB.width` hits, classified

| Lines | What | Correct operand? |
|---|---|---|
| 754, 756 | resize detection (`FB.width != LastWidth`, `LastWidth = FB.width`) | yes — the swapchain is the subject |
| 764 | `UpdateViewConstants(FB.width, FB.height)` | **arguable — carded as H** |
| 773 | `RenderGBuffer(FB.width, FB.height)` | inert — callee ignores both params |
| 1317 | `BlitTexture(..., FB.width, FB.height, ...)` | yes — target is the swapchain |
| 863, 1024, 1029, 1085, 1113, 1182, 1267 | comments (v189/v191/v193/v194 notes) | prose |

Row 4 is the row that matters most, and it is the strongest form available to a
grep-only tester: it does not merely show the tool can match — it **enumerates
the entire candidate set and assigns every member**. That answers "was a seventh
site missed?" with No, rather than with silence.

## Rows 16-18 are not padding, and I want to say why

Rows 1-9 verify the patch. Rows 16-18 verify the *reason for the patch* — they
are the ones that would fail if the plan's model of the defect were wrong. If
the kernel did have an extent guard, mode 1 (out-of-bounds store) would be
false. If `pixelUv` had other consumers, mode 3 (silent pass-through) would be
incompletely characterised. Both were checked against the shader rather than
taken from the plan. A test suite that only confirms the diff is present cannot
detect a correct patch applied for a wrong reason.

Rows 19-20 do the same for the severity claim, which the plan raised above what
card G asserted. A severity re-rating is a claim, and claims get rows.

## What these rows do NOT establish

That the file compiles. That the target links. That any pixel, `M mean`, dump,
validator result or image matches what this cycle predicts. **No build, no run,
no image.** Rows 16-20 establish that the *argument* for the patch is sound
against the source; they cannot establish the patch's runtime effect, and this
cycle's defect is in any case not exercised by the standard recipe, which runs
at the default extent.
