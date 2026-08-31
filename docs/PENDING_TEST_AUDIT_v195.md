# Pending Test Audit v195

- tests: docs/PENDING_TESTS_v195.md
- commit: docs/PENDING_COMMIT_v195.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-541)
- timestamp: 2026-08-30

## Broken-pattern audit

- [x] No from-x-import-y propagation bugs — N/A (C++/HLSL)
- [x] No test-bug-in-itself — I re-executed rows 1, 3, 5, 6, 20, 24 myself
- [x] No source-incomplete-relative-to-test — every row names file and query
- [x] No missing isolation fixture — verifier is read-only
- [x] No AsyncMock/sync mismatch — N/A
- [x] No `|` alternation in any row (tick-526 rule honoured throughout)
- [x] No `output_mode=count` relied on for a conclusion
- [x] No escaped regex metacharacters — plain substrings
- [x] No count quoted from another marker — re-derived
- [x] Every zero controlled — three zeros (rows 3, 5, 24), each paired with a
      same-shape positive (rows 1, 6, 23)

## Independent re-derivation

**Row 5/6, the zero-control pair — the row I most wanted to break.** A zero from
`float(LastWidth)` is only meaningful if the identical query shape can return
non-zero. Row 6's `float(WIDTH)` → 1 hit at `:2356` is the same shape at the
same line. Sound.

**Row 24 — the strongest row this cycle.** `v195` in the shader data directory →
0, controlled by row 23's `v195` → 2 in the `.cpp`. This is what proves the
v182 dual-copy hazard is not engaged: not an assertion that "no shader was
edited," but a positive-and-negative pair with the same token. Rows 17/18 back
it independently by showing both `GIPathTracing.hlsl` copies still carry the
identical 3 `gbScale` hits.

## DEFECT FOUND IN THE TEST MARKER — stale line numbers, substance intact

Row 20 cites the denominator chain as `HalfW = W / 2` at **`:1649`** and
`Desc.OutputWidth = HalfResWidth` at **`:793`**. I re-queried both. `:1649` is
now `DpDesc.initialState = ...DepthWrite` — a depth-texture line. The real
locations are **`:1672`** and **`:816`**.

The cause is benign and predictable: the tester read those lines *before* the
+39 comment lines landed and quoted the pre-patch numbering. **The chain itself
re-verifies exactly** — `Desc.OutputWidth = HalfResWidth` (`:816`) ←
`HalfResWidth = HalfW` (`:1674`) ← `const uint32_t HalfW = W / 2` (`:1672`) ←
`const uint32_t W = WIDTH` (`:1617`) ← `WIDTH = 800` (`:106`). Denominator fixed
at 400. Row 20's *conclusion* stands; only its citations were stale.

I am **not** downgrading the row, because the row's claim is the chain and the
chain is true. But this is the third consecutive cycle in which a marker quoted
line numbers that had already moved (v192 caught its own, v194 caught two, v195
needed the verifier). **The lineage keeps re-learning that `:NNNN` references
inside markers rot within the same tick that writes them.** The v192 audit
already concluded "counts are not invariants, sets are"; the sharper form is:
**line numbers are not invariants either — cite symbols, and re-query any
`:NNNN` immediately before the marker is finalised.** v192's impler got this
right by writing comments with symbol names only; the *markers* have not yet
adopted the same discipline.

## Per-row verdict

**24/24 KEEP.** Rows 1-16 verify the diff and its exclusions. Rows 17-18 and 24
are the dual-copy invariant. Rows 19-22 verify the plan's *model of the defect*
against the shader and the presentation stage rather than against its own prose
— the discipline the v194 audit asked for, applied here without prompting. No
row is padding.

## What this cycle established, and what it did not

**Established (file-only, sound):**

1. Both patch sites are substituted; neither old form survives; both zeros are
   controlled.
2. `LastWidth`/`LastHeight` retain exactly their resize-detection, logging and
   declaration roles — the viewport use is the only one removed.
3. The `gbScale` defect model is real and independently re-derived:
   swapchain-derived numerator over a `WIDTH`-derived, fixed denominator.
4. The camera-aspect question is answered **in source** — the blit presents with
   an unconditional stretch (fixed fullscreen NDC quad, no letterbox term), so
   the render target's aspect is the correct upstream aspect.
5. Neither shader copy was touched; the v182 hazard is not engaged.
6. No-op at 800x600, so the unbuilt v183-v195 chain is unperturbed.

**NOT established — load-bearing:** that the file compiles, that the target
links, that any pixel, dump, `M mean` or validator output is what this cycle
predicts. **No build, no run, no image.**

## The lesson this cycle adds

**A card can be wrong about the callee's behaviour, not merely about its own
judgement — and that is a more dangerous error than the last three.**

The running lesson about cards gains an eighth variant:

- v187/v188: card right about symptom, wrong about remedy.
- v189: card wrong about being blocked.
- v190: card right about being blocked, wrong about remedy.
- v191: no card at all; defect found by re-deriving from source.
- v192: card right about the defect, wrong about where the difficulty lay.
- v193: card right about defect and remedy; its investigative instruction was
  actively misleading.
- v194: card right that something choice-shaped existed; the choice was illusory.
- **v195: card stated a false fact about the callee.** Card H asserted
  `UpdateViewConstants` "uses its two parameters for exactly one thing: the
  camera aspect ratio." It uses them for three things, and the one the card
  named is the *least* consequential. Cards E, G and H all deferred on
  design-choice grounds; E's and G's dissolved on reading the callee, and H's
  dissolved *and* took a factual claim down with it.

The general form, stated at full strength: **a card's description of code is
evidence about the card's author, not about the code.** Re-read the callee
before acting on any card, including the parts that read as settled fact rather
than as judgement.

Second, narrower: **the ninth instance of the extent class was invisible to the
query shape that found the first eight.** Every prior instance was a `FB.width`
in a C++ statement. This one crossed a constant buffer into HLSL and became a
*division* — `RenderTargetSize / DispatchRaysDimensions()`. No `FB.width` sweep
would surface it. Card I proposes sweeping this class by re-running that
enumeration; **this cycle is direct evidence that the enumeration is not
sufficient**, and card I should be re-scoped accordingly.

Third: **card I's proposed remedy is unsafe as written.** An
`HLVM_ENSURE(FB.width == WIDTH)` at the top of `Render()` would abort on an
ordinary user resize — `WindowProps.Resizable = true` — which the
`BindingCache.Clear()` path at `:754-758` exists to handle gracefully. The
invariant that actually holds is narrower: *the fixed-extent passes must be
driven from the fixed extent*, which is a statement about each pass's operands,
not about the swapchain.

## Acceptance gates vs the job instruction (7 gates)

| # | Gate | Status | Basis |
|---|------|--------|-------|
| 1 | Debug target builds | **UNKNOWN** | `./Build.sh` denied (`tirith:unknown`) |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs | **UNKNOWN** | needs shell |
| 3 | No Vulkan VUID/ERROR | **UNKNOWN** | newest log 2026-08-14, predates v183-v195 |
| 4 | No command-list errors | **UNKNOWN** | same caveat |
| 5 | `validate_restir_gi.py` newest group | **BLOCKED** | no shell, no python3 |
| 6 | Vision: recognizable Sponza | **BLOCKED (structural)** | no image tool in this runspace |
| 7 | Mode 20 non-zero `GBufferMaterial` | actionable since v182 | needs one operator run |

**0 of 7 verified against the patched tree.** Gates 3/4 deliberately NOT carried
forward as PASS from the 2026-08-14 log: that log describes a tree from before
v183-v195.

## Ad-hoc verification attempt (post-audit, same tick)

A focused verification script was written to
`/tmp/hermes-verify-v195-extent.py`. **It could not be executed** — `python3`
was refused (`pending_approval / tirith:unknown / exit_code -1`), the **fourth**
distinct invocation shape denied this tick after a compound `git status`, a bare
`/bin/true` and `./Build.sh`. The denial is categorical. **The file could not be
cleaned up for the same reason and remains at that path** — flagging it rather
than claiming a cleanup that did not happen.

Both parts were then **hand-executed**, which is what the v194 audit's rule
demands ("evaluate every consumer at concrete numbers, by hand if necessary").

**Part A — static, comment-stripped.** The script strips `//` comments before
matching, which the marker row-tables did *not* do — they grepped raw text, so a
substitution row could in principle have been satisfied by the +39 comment lines
this very patch added. Re-verified against source: `UpdateViewConstants(WIDTH,
HEIGHT)` present; `UpdateViewConstants(FB.width` → **0 raw hits**, therefore 0
stripped; `UpdateViewConstants(` → exactly 2 (call `:787` + definition `:2427`);
`float(LastWidth)` → 0; viewport ctor at `:2356`. Exclusions intact:
`FB.width != LastWidth` (`:754`), `LastWidth = FB.width` (`:756`), blit
`FB.width, FB.height, BlitParams` (`:1327`). Fixed chain re-confirmed at
post-patch line numbers: `WIDTH = 800` (`:106`), `HEIGHT = 600` (`:107`),
`const uint32_t W = WIDTH, H = HEIGHT;` (`:1617`), `const uint32_t HalfW = W / 2;`
(`:1672`), `HalfResWidth = HalfW;` (`:1674`), `Desc.OutputWidth = HalfResWidth`
(`:816`). Shader: the division survives at `GIPathTracing.hlsl:498` and `v195`
→ **0 hits** in the data directory, so neither shader copy was touched.

**Part B — the arithmetic, evaluated at concrete numbers.** Modelling
`gbPixel.x_max = int((DISPATCH_W - 1 + 0.5) * numerator / DISPATCH_W)` with
`DISPATCH_W = 400` (fixed off `WIDTH`) against the fixed 800-wide GBuffer:

| window | legacy numerator = `FB.width` | patched numerator = `WIDTH` |
|---|---|---|
| 600 | 599 — **under-reads**, samples only the left 600 columns | 799 |
| 800 | 799 | 799 — **identical, patch is a no-op** |
| 1024 | 1022 — **over-reads** past 800 | 799 |
| 1200 | 1198 — over-reads | 799 |
| 1920 | 1917 — over-reads | 799 |

The patched column is constant at 799 for every window: always `< 800`
(in-bounds) and always `>= 798` (full coverage). **The over-read rows are the
sky bug**: an out-of-range `Load` returns 0, `length(worldPos) < 0.001` at
`:518` is satisfied, and the pixel is shaded as sky rather than left visibly
blank. **32/32 checks pass by hand.**

**This is AD-HOC verification, not suite green.** It confirms the substitution
and the arithmetic it fixes. It does **not** confirm the file compiles, the
target links, or that any rendered pixel changes — `./Build.sh` remains denied,
and no image tool exists in this runspace. Gates 1-7 are unchanged at 0/7.

## What this auditor did NOT do

Did not build, run, compile shaders, validate, or view any image. Did not commit,
push, or touch governance files. Did not fabricate any runtime result.
