# Pending Test Audit v196

- tests: docs/PENDING_TESTS_v196.md
- commit: docs/PENDING_COMMIT_v196.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-542)
- timestamp: 2026-08-30

## Broken-pattern audit

- [x] No from-x-import-y propagation bugs — N/A (C++)
- [x] No test-bug-in-itself — I re-executed rows 1, 2, 11, 12, 13 myself
- [x] No source-incomplete-relative-to-test — every row names file and query
- [x] No missing isolation fixture — verifier is read-only
- [x] No AsyncMock/sync mismatch — N/A
- [x] No `|` alternation in any row (tick-526 rule honoured)
- [x] No `output_mode=count` relied on for a conclusion
- [x] No count quoted from another marker — re-derived
- [x] Every zero controlled — rows 1, 6, 14 each paired with a same-shape positive

## Independent re-derivation

**Row 1/2, the zero-control pair that matters most this cycle.** The entire
cycle's claim is "the known-good control is byte-unchanged," which rests on a
zero. A zero is not evidence unless the same token can return non-zero. Re-ran
both: `v196` in `TestPathTraceGI.cpp` → 0; `v196` in `docs` → **5 files**, this
cycle's own markers. Same token, same tool, non-zero elsewhere. **Sound** — and
this is the strongest row in the cycle, because it is the one that proves the
instrument was not touched.

**Row 12/13/14, card K.** Re-ran all three. `void RenderGBuffer` → 1 hit showing
`(uint32_t /*W*/, uint32_t /*H*/)`; `RenderGBuffer(FB.width, FB.height)` → 1 hit;
`RenderGBuffer(WIDTH, HEIGHT)` → 0, controlled by the preceding positive. Card K
is confirmed on both halves: parameters unused, call site live. Sound.

## DEFECT FOUND IN THE TEST MARKER — row 11's query cannot return what it claims

Row 11 states the query `CreateTexture2D(NvrhiDevice, WIDTH, HEIGHT` and reports
"multiple." **That query returns 0**, and the tester did not run it as written:
every one of these calls is **line-wrapped**, with `CreateTexture2D(` ending one
line and `NvrhiDevice, WIDTH, HEIGHT, ...` beginning the next. `search_files` is
line-oriented, so no single-line pattern can span the break.

**The row's conclusion is nonetheless correct**, which is why this is a KEEP and
not a RELAX. Re-derived with a query that respects the wrap: `NvrhiDevice, WIDTH,
HEIGHT` → **6 hits**, and the surrounding context names all six resources —
`PathTraceOutput`, `PathTraceAccum`, `PathTraceDisplay`,
`PathTraceGBufferWorldPos`, `PathTraceGBufferNormal`, `PathTraceGBufferMaterial`.
Every texture in the target is fixed-size. Row 11's claim stands on sound
evidence for the first time.

**This is the same class of fault the tester itself caught in row 12, one row
later, and did not generalise far enough.** The tester's own new rule —
"queries built by pasting a line of source are latent false zeros" — was stated
for *metacharacters*. Row 11 shows the rule is broader: **a pasted query fails
if the source line wraps, too, with no metacharacter involved.** The unifying
statement, which I am promoting over the tester's narrower form:

> **`search_files` matches within a single line of a regex. A query copied from
> source can fail for two independent reasons — metacharacters, and line wrap —
> and both fail silently as a zero. Query the longest metacharacter-free
> fragment that cannot straddle a newline, then read the returned context to
> confirm the rest.**

Four distinct false-zero mechanisms are now on record (tick-526 alternation;
v192 escaped metacharacters; v196 unescaped metacharacters; v196 line wrap).
Three of the four produce false *failures*.

## Per-row verdict

**16/16 KEEP.** Row 11's citation was unsound and its conclusion true; corrected
above rather than downgraded, on the same principle the v195 audit applied to
its own stale-line-number row: *the row's claim is the chain, and the chain is
true.* No row is padding.

## What this cycle established, and what it did not

**Established (file-only, sound):**

1. `TestPathTraceGI.cpp` is byte-unchanged; the known-good control is intact.
2. `gbScale` in that target is identically 1 — both operands are the same
   `CurrentFBInfo` quantity. Card J's first branch. **Not a tenth instance.**
3. The window is non-resizable (`false`) where the sibling's is `true`, so every
   swapchain-derived quantity there is a compile-time-equal alias for the fixed one.
4. All six textures in the target are created from `WIDTH`/`HEIGHT`.
5. Card K is real and inert: unused parameters, live call site passing swapchain
   extents into them.
6. The `CurrentFBInfo` candidate set is closed at 12, each site classified.

**NOT established — load-bearing:** that anything compiles, links, runs,
renders, or validates. **No build, no run, no image.** Unchanged at 0/7 gates.

## The lesson this cycle adds

**A cycle that correctly changes nothing is a result, and the pipeline had to
resist a pull to make it a patch.**

The running lesson about cards gains a ninth variant:

- v187/v188: card right about symptom, wrong about remedy.
- v189: card wrong about being blocked.
- v190: card right about being blocked, wrong about remedy.
- v191: no card at all; defect found by re-deriving from source.
- v192: card right about the defect, wrong about where the difficulty lay.
- v193: card right about defect and remedy; investigative instruction misleading.
- v194: card right that something choice-shaped existed; the choice was illusory.
- v195: card stated a false fact about the callee.
- **v196: the card was right to refuse to guess, and the answer was "no defect."**
  Card J is the first card in the lineage that declined to assert *anything*
  about the callee — it named both branches and said "I did not determine
  which." **Every card that asserted more than it had read was wrong (E, G, H);
  the one card that asserted less than it could have was right.** That is the
  cleanest evidence available that the failure mode is not carelessness but
  over-claiming, and card J's phrasing is the template.

Second: **nine consecutive substitution cycles create a gradient toward a tenth.**
The query shape matched, the remedy was three characters, and the file was the
one target whose value is that it is unmodified. The plan declined, the plan
gate tested the declining rather than rubber-stamping it, and the impler held.
Had it gone the other way, a typo would have surfaced at the first build of the
entire unbuilt v183-v196 chain, indistinguishable from a real defect in the nine
cycles this control exists to exonerate — the instrument correlated with the
measurement.

## Acceptance gates vs the job instruction (7 gates)

| # | Gate | Status | Basis |
|---|------|--------|-------|
| 1 | Debug target builds | **UNKNOWN** | `./Build.sh` denied (`tirith:unknown`) |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs | **UNKNOWN** | needs shell |
| 3 | No Vulkan VUID/ERROR | **UNKNOWN** | newest log 2026-08-14, predates v183-v196 |
| 4 | No command-list errors | **UNKNOWN** | same caveat |
| 5 | `validate_restir_gi.py` newest group | **BLOCKED** | no shell, no python3 |
| 6 | Vision: recognizable Sponza | **BLOCKED (structural)** | no image tool in this runspace |
| 7 | Mode 20 non-zero `GBufferMaterial` | actionable since v182 | needs one operator run |

**0 of 7 verified against the patched tree.** Gates 3/4 deliberately NOT carried
forward as PASS from the 2026-08-14 log: that log describes a tree from before
v183-v196.

## What this auditor did NOT do

Did not build, run, compile shaders, validate, or view any image. Did not commit,
push, or touch governance files. Did not fabricate any runtime result.
