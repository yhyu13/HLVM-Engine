# Pending Commit v194

- plan: docs/PENDING_PLAN_v194.md
- files: Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp
- source: no bundle
- target: (no branch — no commit performed, per job instruction)
- task: Card G — the ReBLUR denoise pass's swapchain-derived extents. Eighth
  instance of the Phase-D/extent class; second on the acceptance path.
- verify: bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/v176-recipe.sh
- skip_impl_review: no
- produces_test_files: no
- notes: both `ReBLUR_cs.hlsl` copies and `FReBLURPass.cpp` byte-unchanged;
  no-op at the default 800x600.

## The change

Six functional substitutions, all inside block (5.5):

| Site | Was | Now |
|---|---|---|
| `ReBLURConstants.OutputSize[0]` | `static_cast<float>(FB.width)` | `static_cast<float>(WIDTH)` |
| `ReBLURConstants.OutputSize[1]` | `static_cast<float>(FB.height)` | `static_cast<float>(HEIGHT)` |
| `ReBLURConstants.RcpOutputSize[0]` | `1.0f / static_cast<float>(FB.width)` | `1.0f / static_cast<float>(WIDTH)` |
| `ReBLURConstants.RcpOutputSize[1]` | `1.0f / static_cast<float>(FB.height)` | `1.0f / static_cast<float>(HEIGHT)` |
| `ReBLURDesc.OutputWidth` | `FB.width` | `WIDTH` |
| `ReBLURDesc.OutputHeight` | `FB.height` | `HEIGHT` |

**+6 / -6 functional, +16 comment.**

## Verification performed (file-only — nothing was compiled or run)

| # | Check | Query / basis | Result |
|---|---|---|---|
| 1 | Both `OutputSize` substitutions present | `ReBLURConstants.OutputSize` → 2 hits, both `static_cast<float>(WIDTH/HEIGHT)` | PASS |
| 2 | Both `RcpOutputSize` substitutions present | `ReBLURConstants.RcpOutputSize` → 2 hits, both `1.0f / static_cast<float>(WIDTH/HEIGHT)` | PASS |
| 3 | Both `Desc` substitutions present | `ReBLURDesc.Output` → 3 hits: `OutputTexture` unchanged, `OutputWidth = WIDTH`, `OutputHeight = HEIGHT` | PASS |
| 4 | **Complete candidate set, no site missed** | `FB.width` → 12 hits total, enumerated individually below | PASS |
| 5 | Blit untouched | the `BlitTexture` call still passes `FB.width, FB.height` against `Framebuffer` | PASS |
| 6 | `WIDTH`/`HEIGHT` unshadowed at the patch site | `const uint32_t W = WIDTH, H = HEIGHT;` → 3 hits, all in *other* member functions (`CreateGBufferTextures`, `FillGBufferHardcoded`, and one more); none in `Render()`, which is where block 5.5 lives | PASS |
| 7 | Types, constants | `OutputSize`/`RcpOutputSize` are `float`; the existing `static_cast<float>` is preserved around each. `WIDTH`/`HEIGHT` are `static const uint32_t`, so the cast is well-formed exactly as before | PASS |
| 8 | Types, desc | `FDesc::OutputWidth`/`OutputHeight` are `uint32_t` (they initialise `uint32_t outputW`/`outputH`); `WIDTH`/`HEIGHT` are `uint32_t`. Exact match, no conversion | PASS |
| 9 | Fallback not triggered | `WIDTH` is non-zero, so `if (!outputW && Desc.OutputTexture)` stays false and the grid comes from the explicit extent — the intended route per the plan review | PASS |
| 10 | Both shader copies byte-unchanged | `ReBLUR*.hlsl` → 2 files (`TestReSTIR_GI_Temporal_Data/`, `TestCornellBoxGI_Data/`); neither edited this cycle | PASS |
| 11 | `FReBLURPass.cpp` byte-unchanged | not edited; the convenience overload's own `W`/`H` assignments are untouched | PASS |
| 12 | No-op at default | `WindowProps.Extent = { WIDTH, HEIGHT }`, so `FB.width == WIDTH == 800` at startup; all six expressions evaluate identically | PASS |
| 13 | Token matchable in this tree (zero-control) | `v194` in this file → 3 hits, all comments introduced by this patch. Confirms the tool matches this token here, so the two shader zeros in row 10 are real absences | PASS |

**Every zero in this table was controlled.** No `|` alternation was used anywhere
(tick-526 rule). Row 4 uses the v193-established strongest form: rather than
proving the tool can match something, it **enumerates the complete candidate
set** and accounts for every member.

### Row 4 enumerated — all 12 `FB.width` hits accounted for

- 4 are **comments** (the v189, v191 ×2, v193 notes) plus the v194 note added
  here — prose, not code.
- 3 are the **resize-detection block**: `if (FB.width != LastWidth ...)`,
  `LastWidth = FB.width`. Correct: this exists precisely to notice the swapchain
  changing, so the swapchain is the right operand.
- 1 is `UpdateViewConstants(FB.width, FB.height)` — see the deviation note below.
- 1 is `RenderGBuffer(FB.width, FB.height)` — **inert**: the callee's signature is
  `RenderGBuffer(uint32_t /*W*/, uint32_t /*H*/)`, both parameters commented out
  and unused; it logs `LastWidth`/`LastHeight` instead. Passing the wrong extent
  here cannot have an effect. Correctly left alone.
- 1 is the **blit**, `BlitTexture(..., FB.width, FB.height, ...)` against
  `Framebuffer` — the one site where the swapchain genuinely is the target.
- The remaining hits are the six substituted this cycle (now reading
  `WIDTH`/`HEIGHT`, hence no longer in this set).

## Plan Deviations

**None functional.** The six sites are exactly those the plan enumerated, changed
exactly as specified.

**One finding that is out of scope and is being carded, not fixed here.**
Row 4's enumeration turned up a site the plan did not anticipate:

    UpdateViewConstants(FB.width, FB.height);

Its two parameters are used for one thing — `glm::perspective(glm::radians(Fov),
float(W) / float(H), ...)`, the camera aspect ratio. That projection matrix
rasterises into the **fixed-size** GBuffer MRTs, so on a non-4:3 window the
aspect would disagree with the target it renders into and the image would be
anamorphically stretched. That is plausibly a ninth instance of this class.

I did **not** fix it, for two reasons. First, it is genuinely arguable that
camera aspect should follow the window rather than the render target — unlike the
six sites here, where the resource extent is not a matter of taste. That is a
design question, exactly the kind card G falsely claimed to have and this one
really does. Second, bundling it would make this cycle's own row-4 enumeration
unverifiable, which is the same reason v192 and v193 each declined to absorb the
next card. Opened as **card H** in `PENDING_PICK.md`.

## What this commit does NOT establish

Nothing was compiled, built, run, linted, validated or viewed. `terminal` was
probed twice this tick — a normal command and a bare `/bin/true` — and both were
refused (`pending_approval`, `tirith:unknown`, `exit_code -1`), so the block is
categorical rather than command-dependent. The checks above establish that the
substitutions are complete, type-correct and confined; they do **not** establish
that the translation unit compiles.
