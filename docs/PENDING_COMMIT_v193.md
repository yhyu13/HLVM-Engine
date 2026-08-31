# Pending Commit v193

- plan: docs/PENDING_PLAN_v193.md
- files: Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp
- source: no bundle
- target: (no branch — no commit performed, per job instruction)
- task: Card F — fix the accumulate pass's swapchain-derived extents; seventh
  instance of the Phase-D/extent class and the first on the acceptance path.
- verify: bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/v176-recipe.sh
- skip_impl_review: no
- produces_test_files: no
- notes: shader byte-unchanged; no-op at the default 800x600.

## The change

Three functional substitutions, all inside the accumulate block:

| Site | Was | Now |
|---|---|---|
| `AccC.Width` | `FB.width` | `WIDTH` |
| `AccC.Height` | `FB.height` | `HEIGHT` |
| dispatch grid | `(FB.width + 7) / 8, (FB.height + 7) / 8` | `(WIDTH + 7) / 8, (HEIGHT + 7) / 8` |

**+3 / -3 functional, +19 comment.** `GIAccumulate_cs.hlsl` untouched.

## Verification performed (file-only — nothing was compiled or run)

| Check | Query / basis | Result |
|---|---|---|
| All three substitutions present | `AccC.Width` → `AccC.Width      = WIDTH;`; `AccC.Height` → `= HEIGHT;`; `dispatch((WIDTH` → 2 hits, the v192 resolve grid and this one | PASS |
| No fourth substitution | `FB.width` → 43 hits before, and the accumulate ones are gone; the blit still reads `FB.width, FB.height` | PASS |
| Blit unchanged | blit call still passes `FB.width, FB.height` into the swapchain framebuffer | PASS |
| `WIDTH`/`HEIGHT` not shadowed at the patch site | sole declarations are the file-scope `static const uint32_t` pair; every `const uint32_t W = WIDTH, H = HEIGHT;` is inside a *different* member function (three of them), none in `Render()` | PASS |
| Types | `AccC.Width`/`Height` are `uint32_t` in the local `FAccumC`; `WIDTH`/`HEIGHT` are `static const uint32_t`. Exact match, no conversion. | PASS |
| Dispatch signature | `dispatch(uint32_t, uint32_t, uint32_t)`; `(WIDTH + 7) / 8` is `uint32_t` | PASS |
| Shader byte-unchanged | not edited this cycle | PASS |
| No-op at default | `WindowProps.Extent = { WIDTH, HEIGHT }`, so `FB.width == 800` at startup; all three expressions evaluate identically | PASS |

**Every zero in this table was avoided by using plain substrings** rather than
escaped regex, per the v191/v192 audit rule. No `|` alternation was used and no
`output_mode=count` query was relied on.

## Plan Deviations

**None functional.** One self-correction during implementation, recorded because
the pipeline's value is in catching exactly this class:

My first draft of the comment contained `(search: "v193 note" in
GIAccumulate_cs.hlsl)` — a grep anchor pointing into the shader. But this cycle
deliberately leaves the shader byte-unchanged, so **the anchor would have been
dangling on the very first read.** Worse, it was the v191-prescribed remedy
(symbol/grep anchors instead of `:NNNN` line numbers) applied without checking
that the target exists. Replaced with a prose description of the early-out.

The lesson generalises: v190 banned line numbers because they rot under later
edits; a grep anchor into a file the cycle is not editing rots *immediately*.
**An anchor is only as good as the write that creates its target.**

## What this commit does NOT establish

Nothing was compiled, built, run, linted, validated or viewed — `terminal` is
denied categorically on this runspace (re-probed twice this tick, including a
bare `/bin/true` with an absolute path and no metacharacters). The checks above
establish that the substitutions are type-correct, complete, and confined; they
do **not** establish that the translation unit compiles.
