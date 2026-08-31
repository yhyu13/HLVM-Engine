# Pending Test Audit v193

- tests: docs/PENDING_TESTS_v193.md
- commit: docs/PENDING_COMMIT_v193.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-539)
- timestamp: 2026-08-30

## Broken-pattern audit

- [x] No from-x-import-y propagation bugs — N/A (C++/HLSL)
- [x] No test-bug-in-itself — I re-executed rows 3, 4, 6, 9 myself
- [x] No source-incomplete-relative-to-test — every row names file and query
- [x] No missing isolation fixture — verifier is read-only
- [x] No AsyncMock/sync mismatch — N/A
- [x] No `|` alternation in any row
- [x] No `output_mode=count` relied on
- [x] No escaped regex metacharacters — all plain substrings
- [x] No count quoted from another marker — the tester re-ran everything
- [x] Every zero controlled — three zeros this cycle, all three controlled

## Independent re-derivation

**Row 3 / row 12.** `dispatch((` → 2 hits, `:1156` and `:1274`, both
`(WIDTH + 7) / 8, (HEIGHT + 7) / 8`. Both the v192 resolve grid and the v193
accumulate grid are present and fixed-extent. Confirmed.

**Row 4 (the cycle's central negative).** The tester's control is the strongest
form used in this lineage so far and I want to record why. Rather than proving
the tool can match *something*, it **enumerated the complete candidate set**:
`dispatch((` returns exactly two call sites in the file, and both are patched.
That answers a question a bare positive control cannot — *was a third accumulate-
style dispatch missed?* No. Re-ran it myself: 2 hits, same two lines.

**Row 6.** Read `:1277-1288` directly. The blit passes `FB.width, FB.height`
into `BlitTexture` against `Framebuffer`. Correct and correctly left alone — this
is the one site in the block where the swapchain extent is the right operand.
The over-substitution risk the plan flagged did not materialise.

**Row 9 / row 14.** `v193` in `TestReSTIR_GI_Temporal.cpp` → exactly 2 hits,
`:1238` and `:1273`, both comments. Confirms simultaneously that (a) the token is
matchable by this tool in this tree, so the shader zeros are real absences, and
(b) the impler's corrected comment carries no `search:` anchor and no `:NNNN`
reference — the deviation was actually fixed, not just declared fixed.

## Row 10 is the row I would have expected to be missing

`GIAccumulate_cs.hlsl` exists in **two** places —
`TestReSTIR_GI_Temporal_Data/` and `TestPathTraceGI_Data/`. That is exactly the
v182 dual-copy configuration, where a fix applied to one copy while the build
compiles the other produced a cycle's worth of wasted work. This cycle edits
neither, so the hazard is inert — **but the tester checked rather than assuming,
and controlled the sibling's zero separately (row 11).** Checking a hazard that
turns out not to apply is the correct spend; it is how you learn the hazard does
not apply.

## Per-row verdict

**14/14 KEEP.** Rows 1-4, 7, 8 and 10 are genuine discriminators. Rows 5, 6, 12
are collateral-damage checks, correctly kept separate. Rows 9/11/13 are cheap
invariants. Row 14 verifies a declared deviation was actually corrected in source
rather than only in prose — the same species as v192's row 12, and worth keeping
for the same reason: **markers are read by this pipeline, source is read by
whoever comes next.** No row is padding.

## What this cycle established, and what it did not

**Established (file-only, sound):**

1. The accumulate pass is fixed-extent end to end: constants, dispatch grid, SRV
   input and both UAVs all descend from `WIDTH`/`HEIGHT`.
2. The kernel's guard was a **tautology** — keyed to the same two values the
   dispatch grid was computed from, so it clipped the dispatch against itself.
   Fixing the constants fixes the guard; the shader needed no edit.
3. `HEIGHT` is unshadowed at the patch site, checked for both operands.
4. The blit is untouched; v192's site is untouched; both shader copies untouched.
5. No-op at 800x600, so the v183/v184/v185 chain is unperturbed.

**NOT established — load-bearing:** that the file compiles, that the target
links, that any pixel, `M mean`, dump or validator output is what this cycle
predicts. **No build, no run, no image.**

**Reachability caveat, stated plainly:** the defect manifests only when the
swapchain diverges from 800x600, which requires a resize mid-run. The standard
recipe runs at the default extent and would not exercise it. This cycle's
correctness therefore rests on the source argument, not on observation, and the
operator run will not confirm it — it can only fail to contradict it.

## The lesson this cycle adds

**A bounds guard keyed to the wrong extent is more dangerous than no bounds
guard.** v192 recorded "no extent guard anywhere in the shader" as the most
severe form of this class. That ordering is now wrong. v192's resolve kernel was
*visibly* unguarded — an auditor sweeping for the pattern would stop there. This
kernel's guard reads as protection and survives that sweep, while providing
none: it compares `SV_DispatchThreadID` against the same constants the grid was
sized from.

Sixth variant of the running lesson about cards:

- v187/v188: card right about symptom, wrong about remedy.
- v189: card wrong about being blocked.
- v190: card right about being blocked, wrong about remedy.
- v191: no card at all; defect found by re-deriving from source.
- v192: card right about the defect, wrong about where the difficulty lay.
- **v193: card right about the defect and right about the remedy — but it
  prescribed "check the accumulate shader for an extent guard first, as v192
  found the resolve kernel had none." Following that instruction literally
  yields the wrong answer: the guard is present, and its presence is the
  defect's camouflage.**

And a second, about the anchor convention: v190 banned `:NNNN` references
because they rot under later edits; v191 prescribed grep anchors instead. The
impler here wrote an anchor into a file this cycle deliberately does not edit —
**dangling on the first read.** A remedy applied without checking its
precondition. Caught by the impler itself.

## Acceptance gates vs the job instruction (7 gates)

| # | Gate | Status | Basis |
|---|------|--------|-------|
| 1 | Debug target builds | **UNKNOWN** | `./Build.sh` denied |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs | **UNKNOWN** | needs shell |
| 3 | No Vulkan VUID/ERROR | **UNKNOWN** | newest log 2026-08-14, predates v183-v193 |
| 4 | No command-list errors | **UNKNOWN** | same caveat |
| 5 | `validate_restir_gi.py` newest group | **BLOCKED** | no shell / no python3 |
| 6 | Vision: recognizable Sponza | **BLOCKED (structural)** | no vision tool in this runspace |
| 7 | Mode 20 non-zero `GBufferMaterial` | actionable since v182 | needs one operator run |

**0 of 7 verified against the patched tree.** Gates 3/4 deliberately NOT carried
forward as PASS from the 2026-08-14 log: that log says nothing about a tree
patched on 2026-08-30.

## What this auditor did NOT do

Did not build, run, compile shaders, validate, or view any image. Did not commit,
push, or touch governance files. Did not fabricate any runtime result.
