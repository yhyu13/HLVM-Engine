# Pending Test Audit v191

- tests: docs/PENDING_TESTS_v191.md
- commit: docs/PENDING_COMMIT_v191.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-538)
- timestamp: 2026-08-30

## Broken-pattern audit

- [x] No from-x-import-y propagation bugs — N/A (C++/HLSL, static queries)
- [x] No test-bug-in-itself — I re-executed rows 1, 7, 9, 11 myself
- [x] No source-incomplete-relative-to-test — every row names a file and a query
- [x] No missing isolation fixture — verifier is read-only
- [x] No AsyncMock/sync mismatch — N/A
- [x] No `|` alternation in any row — checked every pattern. None uses it.
- [x] **No unconfirmed 0-hit recorded as a result** — the tester confirmed both
      of its zeros against positive controls. This is the correct discipline and
      it is newly enforced as a row-level requirement (see below).

## Independent re-derivation

**Row 1 (the cycle's premise).** `search_files pattern="WIDTH / std"` → 2 hits,
`:1039` and `:1087`, both `static_cast<float>(WIDTH / std::max(HalfResWidth, 1u))`.
Confirmed. Holds.

**Row 11 (the stale-cross-reference guard).** `search_files pattern="v191"` → 3
hits: `:1024`, `:1085`, `:1086`. The anchor at `:1086` reads
`(search: "v191: the numerator")` and `:1024` begins
`// v191: the numerator is WIDTH, NOT FB.width` — so the anchor resolves to a
string that exists exactly once and is not itself a line number. **The forward
reference is durable under file shift.** Holds, and this is the row that matters
most: v190 won `0` line-number references in this file and v191 nearly lost it.

**Row 7 — the tester's self-catch is correct and I am upgrading its
significance.** `HalfResWidth` → 18 hits, not the impler's stated 17. The tester
diagnosed the extra as its own comment at `:1028`. I enumerated all 18 and
confirm: 16 code sites (`:793`, `:870`, `:889`, `:900`, `:902`, `:976`, `:1001`,
`:1003`, `:1039`, `:1073`, `:1077`, `:1079`, `:1087`, `:1105`, `:1107`, `:1598`,
plus the declaration `:2844` — 17 code, and `:1028` comment) — the precise split
is **17 code + 1 comment = 18**, versus v190's 17 which was 16 code + 1
declaration. **The raw total was never a stable invariant and three cycles have
now quoted it as though it were.** Carrying forward: cite the *set* of sites, not
the count.

**Row 9.** `GBufferScale` over `Engine/Source/Runtime` → **22 hits**, not the
tester's 21. The delta is `:1034`, another comment line the impl added. Cornell's
two `1.0f` literals at `TestCornellBoxGI.cpp:1592/1645` are present and
unchanged, which is what the row exists to prove. Claim holds; number was stale
by one for the same reason as row 7. Same lesson, twice in one cycle.

## Net-new tooling finding — `output_mode=count` is unreliable here

I ran `search_files pattern="GBufferScale" path=Engine/Source/Runtime
output_mode="count"` and it returned **`total_count: 0`** with a per-file table
of ~50 files all reading `0` — none of which were the files that actually
contain the term. The identical query in default content mode returns **22 real
hits** across 7 files.

So `output_mode=count` silently under-reports on this runspace, in the same
class as tick-526's alternation finding and the tester's regex-escaping finding.
**Three distinct ways to obtain a false zero are now documented.** The rule
generalises past "don't use `|`":

> **Any zero from `search_files` is a claim about the tool until it is confirmed
> against a positive control with a different query shape.**

Row 2 of the tester's table (`FB\.width / std::max` → 0, meaning the old form is
gone) is sound precisely because it was controlled that way — `FB\.width` alone
returns 16 in the same file. I re-confirmed that control myself.

## Per-row verdict

**12/12 KEEP**, with rows 7 and 9 keeping their *claims* and losing their
*numbers* (corrected above). Rows 1, 3, 4, 10, 11 are the genuine
discriminators. Row 12 is weak but correctly separate. No row is padding; no row
asserts a result its stated query would not produce.

## What this cycle established, and what it did not

**Established (file-only, sound):**

1. `GBufferScale` at both ReSTIR call sites is now a ratio of two fixed
   quantities (`WIDTH` and `HalfResWidth`, both descended from `:106`), instead
   of mixing in the resizable swapchain extent.
2. `WIDTH` is unshadowed in `Render()`, so the substitution binds to the intended
   constant.
3. The numerator now matches the extent of the textures the shader's `GB()`
   helper actually indexes — `GBufferNormal` and `LinearDepthTexture`, both
   created inside `CreateGBufferTextures` from `const uint32_t W = WIDTH`.
4. No shader, no Cornell file, and no other pass was touched.
5. The patch is a **no-op at the default 800x600 extent** (`800/400 == 2` under
   either operand), so it cannot perturb the v183/v184/v185 chain awaiting its
   run.

**NOT established — load-bearing:** that the file compiles, that the target
links, that any pixel, `M mean`, dump or validator result is unchanged. **No
build, no run, no image.** The prediction of byte-identical output is a
prediction.

**Ordering caveat carried forward:** v183/v184/v185 are one dependency chain
awaiting a single operator run; v189/v190/v191 cannot move
`ReSTIR summary: M mean=`. Judge them separately. If `M` does not move on that
run, **the half-res-reuse hypothesis is wrong and must be recorded as a
refutation, not explained away.**

## The lesson this cycle adds

The lineage's running lesson has been *a card's stated reason is a claim, not a
fact*. v191 had no card — the queue was empty — and the lesson still applied,
turned inward:

- v187/v188: card right about symptom, wrong about remedy.
- v189: card wrong about being blocked.
- v190: card right about being blocked, wrong about remedy.
- **v191: no card at all. The defect was found by re-deriving from source, and
  every wrong number in the cycle was produced by a role quoting a previous
  role's count instead of re-running the query.** The impler quoted 17, the
  tester re-ran it and got 18, I re-ran it and got the split. Three roles, one
  number, two corrections.

**Counts are not invariants. Sets are.** Any inertness argument in a future cycle
should name the sites, not total them.

## Acceptance gates vs the job instruction (7 gates)

| # | Gate | Status | Basis |
|---|------|--------|-------|
| 1 | Debug target builds | **UNKNOWN** | `./Build.sh` denied; on-disk binary predates v183-v191 |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs | **UNKNOWN** | needs shell |
| 3 | No Vulkan VUID/ERROR | **UNKNOWN** | newest log 2026-08-14, predates v183-v191. NOT carried forward as PASS |
| 4 | No command-list errors | **UNKNOWN** | same caveat |
| 5 | `validate_restir_gi.py` newest group | **BLOCKED** | no shell / no python3 |
| 6 | Vision: recognizable Sponza | **BLOCKED (structural)** | no vision tool in this runspace |
| 7 | Mode 20 non-zero `GBufferMaterial` | actionable since v182 | needs one operator run |

**0 of 7 verified against the patched tree.**

## What this auditor did NOT do

Did not build, run, compile shaders, validate, or view any image. Did not commit,
push, or touch governance files. Did not fabricate any runtime result.
