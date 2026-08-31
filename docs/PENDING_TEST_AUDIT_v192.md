# Pending Test Audit v192

- tests: docs/PENDING_TESTS_v192.md
- commit: docs/PENDING_COMMIT_v192.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-538)
- timestamp: 2026-08-30

## Broken-pattern audit

- [x] No from-x-import-y propagation bugs — N/A (C++/HLSL)
- [x] No test-bug-in-itself — I re-executed rows 2, 5, 12, 13 myself
- [x] No source-incomplete-relative-to-test — every row names file and query
- [x] No missing isolation fixture — verifier is read-only
- [x] No AsyncMock/sync mismatch — N/A
- [x] No `|` alternation in any row
- [x] No count quoted from another marker — the tester re-ran everything
- [x] Every zero confirmed against a positive control — **two zeros this cycle,
      both controlled and both recorded rather than hidden**

## Independent re-derivation

**Row 2.** `RcpFullH` → 2 hits: field `:1101`, assignment `:1127`
`= 1.0f / static_cast<float>(HEIGHT)`. Confirmed.

**Row 5 (shader inertness — the cycle's central negative).** `int2 fp = hp` → 1
hit at `Resolve_cs.hlsl:60`, `int2 fp = hp * 2 + 1;`. Unchanged. Confirmed.

**Row 12.** `deliberately NOT` → 1 hit `:1121`, the shader-hardcode note. So
card E's open question ("is `hp * 2 + 1` a design decision that needs
parameterising?") is now answered **in source**, not merely in a marker. That
matters: markers are read by this pipeline, source is read by whoever comes next.

**Row 13 (the severity claim).** `FullResOutput` → 3 hits in the shader:
declaration `:10`, writes `:34` and `:73`, both `FullResOutput[tid.xy]`. Reading
the kernel end to end, the sole early-out is `:32` on `centerDepth <= 0.0` — a
data condition. **No extent guard exists.** So the widened-swapchain case really
was an unguarded out-of-bounds UAV store, which is undefined behaviour rather
than the benign zero-return of an out-of-range `Load`. The claim is sound and
this is the most severe instance of the class found so far.

## The two false zeros are the finding of this cycle's test phase

The tester hit `WIDTH \+ 7` → 0 and `hp \* 2 \+ 1` → 0, and in both cases the
plain-substring form returned the real hit. **Taken at face value these were
false *failures*** — "the patch is not present," "the shader was modified" — the
opposite polarity from tick-526's false passes, and considerably more dangerous
in a pipeline whose only evidence is grep output: a false pass wastes a cycle, a
false failure would have triggered a FIX loop against a correct patch.

Running tally of distinct false-zero mechanisms on this runspace:

1. `|` alternation (tick-526)
2. `output_mode="count"` (v191 audit)
3. **over-escaped regex metacharacters** (`\+`, `\*`) — v191 tester, twice more
   here. **Now confirmed reproducible, not anecdotal.**

The standing rule holds and is strengthened: *any zero from `search_files` is a
claim about the tool until confirmed against a positive control with a different
query shape.* Both zeros here were controlled before recording. **Prefer plain
substrings over escaped regex whenever the target permits it.**

## Per-row verdict

**13/13 KEEP.** Rows 1-4, 5, 8 and 13 are genuine discriminators. Rows 6, 9, 10,
11 are collateral-damage checks, correctly kept separate. Row 12 is unusual and
worth keeping: it tests that a *conclusion* was written where the next reader
will find it. No row is padding.

## What this cycle established, and what it did not

**Established (file-only, sound):**

1. The resolve pass is now fixed-extent end to end: constants, dispatch grid,
   input, guides and both outputs all descend from `WIDTH`/`HEIGHT`.
2. `Resolve_cs.hlsl` is byte-unchanged, and the reason is recorded in source:
   `hp * 2 + 1` encodes the fixed half-to-full footprint, not a swapchain ratio.
   Card E's open design question is **closed, with an argument.**
3. The pre-patch defect was an unguarded out-of-bounds UAV store on widening and
   a smear-plus-stale-columns on narrowing — verified against the kernel, which
   has no extent guard at all.
4. `HEIGHT` is unshadowed at the patch site (v191 had only cleared `WIDTH`).
5. v191's two sites are undisturbed.
6. No-op at 800x600, so the v183/v184/v185 chain is unperturbed.

**NOT established — load-bearing:** that the file compiles, that the target
links, that any pixel, `M mean`, dump or validator output is unchanged. **No
build, no run, no image.**

**Ordering caveat:** v183/v184/v185 are one chain awaiting a single run.
v189/v190/v191/v192 touch passes downstream of reservoir accumulation and
**cannot** move `ReSTIR summary: M mean=`. Judge separately. If `M` does not move
on that run, the half-res-reuse hypothesis is wrong and must be recorded as a
refutation, not explained away.

## The lesson this cycle adds

**A card's framing of the open question can be wrong even when the card is
right.** Card E identified a real defect and then said the hard part was deciding
what to do about the shader's hardcoded `2`. Reading the shader showed there was
no decision to make — the hardcode is correct and invariant, and the entire fix
is three C++ lines. The cycle's real work was **proving a negative** and writing
it where it will not be re-litigated.

This is a fifth variant of the running lesson:

- v187/v188: card right about symptom, wrong about remedy.
- v189: card wrong about being blocked.
- v190: card right about being blocked, wrong about remedy.
- v191: no card at all; defect found by re-deriving from source.
- **v192: card right about the defect, wrong about where the difficulty lay.**

## Acceptance gates vs the job instruction (7 gates)

| # | Gate | Status | Basis |
|---|------|--------|-------|
| 1 | Debug target builds | **UNKNOWN** | `./Build.sh` denied |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs | **UNKNOWN** | needs shell |
| 3 | No Vulkan VUID/ERROR | **UNKNOWN** | newest log 2026-08-14, predates v183-v192 |
| 4 | No command-list errors | **UNKNOWN** | same caveat |
| 5 | `validate_restir_gi.py` newest group | **BLOCKED** | no shell / no python3 |
| 6 | Vision: recognizable Sponza | **BLOCKED (structural)** | no vision tool in this runspace |
| 7 | Mode 20 non-zero `GBufferMaterial` | actionable since v182 | needs one operator run |

**0 of 7 verified against the patched tree.**

## What this auditor did NOT do

Did not build, run, compile shaders, validate, or view any image. Did not commit,
push, or touch governance files. Did not fabricate any runtime result.
