# Pending Tests v193

- commit: docs/PENDING_COMMIT_v193.md
- tester: agent_5_tester (tick-539)
- timestamp: 2026-08-30
- mode: **file-only.** `terminal` denied categorically (two shapes probed this
  tick, including a bare `/bin/true` with absolute path and no metacharacters;
  both `status: pending_approval, pattern_key: tirith:unknown, exit_code: -1`).
  **Nothing was compiled, built, run, linted or viewed.**

Every row below was re-executed by me. No count is quoted from another marker.
Every zero is paired with a positive control of the same query shape.

| # | Check | Query | Expected | Result |
|---|---|---|---|---|
| 1 | Width substitution present | `AccC.Width` | `= WIDTH;` | **PASS** — `:1256` `AccC.Width      = WIDTH;` |
| 2 | Height substitution present | `AccC.Height` | `= HEIGHT;` | **PASS** — `:1257` `AccC.Height     = HEIGHT;` |
| 3 | Dispatch substitution present | `dispatch((WIDTH` | 2 hits (v192 resolve + v193 accum) | **PASS** — `:1156`, `:1274` |
| 4 | No swapchain extent left in the accumulate block | `dispatch((FB.width` | 0 | **PASS (controlled — see below)** |
| 5 | Non-extent fields untouched | read `:1234-1258` | `FrameCount`, `Exposure` unchanged | **PASS** — `:1237`, `:1258` |
| 6 | Blit still swapchain-sized | read `:1277+` | `FB.width, FB.height` retained | **PASS** — the over-substitution risk did not occur |
| 7 | `HEIGHT` unshadowed at patch site | `= HEIGHT` | all locals in other functions | **PASS** — 18 hits; `:1578`/`:1704`/`:1797` are `const uint32_t W = WIDTH, H = HEIGHT;` in three *other* member functions, none in `Render()` |
| 8 | Kernel guard is the one described | `pixel.x >= Width` | 1 hit, `SV_DispatchThreadID` vs cbuffer | **PASS** — `GIAccumulate_cs.hlsl:63`, operand `pixel` from `:62` `= dispatchThreadId` |
| 9 | Shader byte-unchanged | `v193` in the `_Data` shader | 0 | **PASS (controlled — see below)** |
| 10 | Dual-copy hazard checked | `GIAccumulate_cs.hlsl` tree-wide | 2 copies | **PASS** — `TestReSTIR_GI_Temporal_Data/` and `TestPathTraceGI_Data/`. Neither edited. **This is the v182 trap and it was checked, not assumed.** |
| 11 | Sibling test not disturbed | `v193` in `TestPathTraceGI_Data/GIAccumulate_cs.hlsl` | 0 | **PASS (controlled)** |
| 12 | v192's site undisturbed | `dispatch((WIDTH` | `:1156` still present | **PASS** — row 3 covers both |
| 13 | Type compatibility | read `:1235` | `uint32_t Width; uint32_t Height;` ← `static const uint32_t` | **PASS** — exact, no conversion |
| 14 | Deviation actually corrected | read `:1247-1249` | no `search:` anchor, no `:NNNN` | **PASS** — prose description only |

## Positive controls for the three zeros

A zero from `search_files` is a claim about the tool until controlled
(tick-526 alternation; v191 `output_mode=count`; v191/v192 over-escaped regex).

**Row 4** (`dispatch((FB.width` → 0). Control: the same query shape with the
prefix only — `dispatch((` → **2 hits**, `:1156` and `:1274`, both `WIDTH`-based.
So the file contains exactly two `dispatch((` call sites, both patched, and the
zero is a real absence rather than a failed match. This control is stronger than
a bare positive hit elsewhere: it **enumerates the complete candidate set**, so
it also proves no third accumulate-style dispatch was missed.

**Rows 9 and 11** (`v193` in each shader copy → 0). Control: the identical
literal `v193` in `TestReSTIR_GI_Temporal.cpp` → hits at `:1238`, `:1273` and in
this marker set. Same token, same tool, non-zero in one file and zero in the
others — so the shaders are genuinely untouched.

All queries used plain substrings. No `|` alternation, no `output_mode=count`,
no escaped metacharacters.

## Ad-hoc verification attempt — WRITTEN, NOT RUN

A focused harness was written to exercise the patched arithmetic and the
falsifiable regressions the old form is claimed to produce (narrowed → 43.75%
unwritten; widened → OOB store; default → byte-identical). It is checked in at
`TestReSTIR_GI_Temporal_Data/verify_v193_accumulate_extent.py` rather than left
orphaned in `/tmp`, because `/tmp` cleanup needs the same denied shell.

**It could not be executed.** Five invocation shapes, all
`status: pending_approval, pattern_key: tirith:unknown, exit_code: -1`:

| # | Invocation | Why tried |
|---|---|---|
| 1 | `pwd && git log ...` | baseline |
| 2 | `/bin/true`, `workdir=/tmp` | absolute path, no args, no metacharacters |
| 3 | `python3 /tmp/hermes-verify-v193-extent.py` | the actual verification |
| 4 | `pwd && ls -la` | the loop-warning's own prescribed diagnostic |
| 5 | same script, `background=true` | different execution path |

Shapes 4 and 5 are decisive: the runtime's prescribed recovery command is itself
denied, and background dispatch is denied too. `process list` returns empty — no
process ever started. **The denial is categorical, not command-pattern- or
execution-mode-dependent.** Retrying further would be a loop, not diagnosis.

**So this cycle is UNVERIFIED EXECUTION: nothing compiled, built, run, linted or
viewed.** The 14 rows above are static source checks. They establish that the
substitutions are complete, confined and type-correct; they do **not** establish
that the translation unit compiles or that any pixel is what this cycle predicts.

## What these tests do NOT establish

They do not establish that the file compiles, that the target links, or that any
pixel, dump, `M mean` or validator result changes. **No build, no run, no image.**

The patch is a no-op at the default 800x600 by construction, so even a successful
operator run would produce byte-identical output to a v192 build. **That is the
falsifiable prediction of this cycle**: if the display dump differs after this
patch, either the patch is wrong or `FB.width != 800` at startup on that machine
— and the latter would itself be the finding.

## The divergence cases are unreachable from here

The defect only manifests when the swapchain differs from 800x600, which requires
a window resize during a run. That cannot be exercised file-only, and would not
be exercised by the standard recipe either (it runs at the default extent). So
this cycle's correctness rests on the source argument — every resource fixed-size,
`BackBufferResizing` recreating nothing — not on observation. **Recorded plainly
rather than papered over.**
