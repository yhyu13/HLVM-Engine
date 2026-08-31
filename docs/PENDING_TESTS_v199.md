# Pending Tests v199

- commit: docs/PENDING_COMMIT_v199.md
- verifier: agent_5_tester (tick-545)
- mode: file-only (terminal denied categorically this tick — a bare `true` refused,
  `tirith:unknown`, `exit_code -1`)
- timestamp: 2026-08-30

Every row below was executed this tick. No count is quoted from another marker. No `|`
alternation (tick-526). Every zero is paired with a same-shape positive control.

## Rows

| # | Check | Query / read | Result | Verdict |
|---|---|---|---|---|
| 1 | Target 1 creation sites enumerated | `createTexture` in `TestRTReflections.cpp` | 15 hits | PASS |
| 2 | Target 1 staging sites | `createStagingTexture` same file | 2 hits (`:815`, `:980`) | PASS |
| 3 | Target 1 framebuffer sites | `createFramebuffer` same file | 2 hits (`:513`, `:940`) | PASS |
| 4 | Target 1 resize block opens | read `:892` | `if (!GBufferNormalsTexture \|\| ... != LastWidth ...)` | PASS |
| 5 | Target 1 resize block closes | read `:983-984` | `BindingCache.Clear();` / `}` | PASS |
| 6 | Target 1 init extent source | read `:392-393` | `Framebuffer->getFramebufferInfo()` | PASS |
| 7 | Target 1 resize extent source | read `:898-899` | `CurrentFBInfo` | PASS |
| 8 | Target 1 dispatch extent source | read `:1145-1147` | `args.width = CurrentFBInfo.width` | PASS |
| 9 | Target 1 excluded site is genuinely 1x1 | read `:318-319` shape at `:661` | `Desc.width = 1; Desc.height = 1;` | PASS |
| 10 | Target 1 is resizable (check non-vacuous) | `Resizable` | `:1342 = true` | PASS |
| 11 | Target 2 creation sites | `createTexture` in `TestRenderSponza.cpp` | **1 hit** (`:327`) | PASS (controlled, row 12) |
| 12 | Row 11's zero-adjacent claim controlled | same pattern, `TestRTReflections.cpp` | 15 hits | PASS |
| 13 | Target 2 has no staging/framebuffer | `createStagingTexture` → 0; `createFramebuffer` → 0 | both 0 | PASS (controlled, row 3) |
| 14 | Target 2 has no dispatch at all | `dispatch` in target 2 | **0 hits** | PASS (controlled, row 8) |
| 15 | Target 2's only texture is not extent-sized | read `:316-327` | `width = 1`, `height = 1`, `isUAV = false` | PASS |
| 16 | Target 2 is resizable | `Resizable` | `:614 = true` | PASS |
| 17 | Deviation target block bounds | read `:781` open, `:872` close | contains `:801`-`:869` | PASS |
| 18 | Deviation target framebuffer parity | `createFramebuffer` | 2 hits (`:401`, `:829`) | PASS |
| 19 | Deviation target dispatch extent | read `:1032` | `args.width = CurrentFBInfo.width` | PASS |
| 20 | **Zero source files modified** | `v199` in `TestRTReflections.cpp` | 0 hits | PASS (controlled, row 21) |
| 21 | Row 20's zero controlled | `v199` in `docs/`, `files_only` | 4 files | PASS |

**21/21 PASS.**

## The row that matters most, and the mechanism it exposed

Rows 20/21 exist to prove the cycle's central claim — *zero source files modified* — and
in executing them I hit a **seventh false-zero mechanism, live, this tick.**

Same pattern (`v199`), same path (`docs/`), two output modes:

- `output_mode=count` → **`total_count: 0`**, with a `counts` map listing ten files all
  at zero
- `output_mode=files_only` → **4 files**, the four v199 markers I had just written

**The count mode returned zero for files that demonstrably contain the string.** This is
not tick-526's alternation bug (no `|` in the pattern), not the escaping bugs, not the
line-wrap bug, and not v197's assumption-encoding. It is the *output mode itself*
disagreeing with the same query under a different mode.

The v198 checklist already carried a row saying "no conclusion resting on
`output_mode=count` alone." That row was written as caution. **It is now a demonstrated
failure, observed rather than anticipated**, and the demonstration is cheap to reproduce
because the positive control is four files this pipeline created minutes earlier.

Had I used the count result as the row-20 evidence, I would have reported "0 hits — no
source modified" and been **accidentally right for an invalid reason**, which is the worst
outcome available: a true conclusion that certifies a broken instrument as working. Row 21
is what caught it.

## What these tests do NOT cover

Nothing here builds, compiles, links, runs, renders, validates, or views an image. These
rows verify **source-level determinations only.** The three clean verdicts are claims about
what the code says, not about what it does when executed. **0/7 acceptance gates.**
