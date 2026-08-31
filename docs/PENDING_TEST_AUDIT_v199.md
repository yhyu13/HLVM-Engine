# Pending Test Audit v199

- tests: docs/PENDING_TESTS_v199.md
- commit: docs/PENDING_COMMIT_v199.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-545)
- timestamp: 2026-08-30

## Broken-pattern audit

- [x] No from-x-import-y propagation bugs — N/A (C++)
- [x] No test-bug-in-itself — I re-executed rows 1, 11, 12, 20, 21 myself
- [x] No source-incomplete-relative-to-test — every row names path and query
- [x] No missing isolation fixture — verifier is read-only
- [x] No AsyncMock/sync mismatch — N/A
- [x] No `|` alternation (tick-526)
- [x] No conclusion resting on `output_mode=count` alone (v198) — **fired this cycle**
- [x] No count quoted from another marker — re-derived
- [x] Every zero controlled by a same-shape positive
- [x] No query pasted from a line that wraps (v196)
- [x] No query encoding an unverified declaration form (v197)
- [x] No absence asserted by a query (v198)
- [x] **No query whose `path` is a directory used for a load-bearing negative (v199, new)**

## Independent re-derivation

Rows 1, 11, 12 re-run: `createTexture` → 15 in `TestRTReflections.cpp`, 1 in
`TestRenderSponza.cpp`. Both confirmed. The 15-vs-1 contrast is the cycle's whole
determination in one comparison and it reproduces.

## The tester's false-zero finding: CONFIRMED, and I isolated the mechanism

The tester reported that `v199` over `docs/` returns 0 under `output_mode=count` but 4
files under `files_only`. I reproduced it with a *different* pattern (`Pending Tests
v199`) to rule out a pattern-specific artefact — same split. Then I bisected the variable:

| `path` argument | mode | result |
|---|---|---|
| `docs/` (directory) | count | **0**, with a `counts` map of ~unrelated files all at 0 |
| `docs/PENDING_TESTS_v199.md` (file) | count | **1** |

**The pattern is fine and the mode is fine. The failing variable is `path` pointing at a
large directory.** In that case the enumeration is capped, the `counts` map is populated
with whichever files the cap admitted, files outside it are simply absent, and
`total_count` sums only the admitted set — reporting **0 for a directory that provably
contains matches.** Raising `limit` to 2000 produced a 197 KB `counts` map still summing
to 0, so the cap is not the `limit` parameter.

This is why it is genuinely distinct from the six on record and why the checklist needed a
new row rather than a footnote on the old one: the previous `output_mode=count` caution was
about *interpreting* counts, whereas this is the count being **wrong**. The safe form is
what the tester used — `files_only` for existence, or `path` at the specific file.

**Severity note for the lineage.** Tick-526 found that vacuous alternation searches had
been recorded as evidence of no-Vulkan-errors across dozens of ticks. This has the same
shape and the same blast radius: any prior tick that ran a count-mode query over `docs/`
or a source directory and recorded "0 hits" as a negative finding was reading an artefact.
I am not re-auditing those ticks here — out of scope — but flagging that the exposure
exists and is not hypothetical.

## Per-row verdict

**21/21 KEEP.** Row 21 is the one that carries the cycle: without it row 20's zero would
have been accepted, and the cycle would have certified "no source modified" from an
instrument that was returning zero for the wrong reason. That it happened to be the
*correct* answer is precisely what makes it dangerous, and the tester said so.

## On ALL_KEEP for a third consecutive zero-change cycle

I considered whether three zero-change cycles (v196, v198, v199) indicate a stalled
pipeline rather than three sound determinations, since that is what the pattern would look
like from outside.

They are distinguishable and the markers distinguish them: v196 determined no defect
existed in a control; v198 found a real defect and deliberately did not patch it, on a
stated precondition; v199 closed a card's deferred half and returned clean on three files.
Each produced a determination that changes what the next cycle should do. **But I record
the pattern explicitly**, because the honest reading of three consecutive no-diff cycles is
that **the source-level work is approaching exhaustion while the actual blocker is
untouched.**

## What this cycle established, and what it did not

**Established (file-only, sound):**

1. `TestRTReflections.cpp` is clean — ten extent-sized resources, ten recreations, one
   extent source throughout. It is a **positive control** demonstrating the v198
   set-difference procedure has specificity, not just sensitivity.
2. `TestRenderSponza.cpp` cannot host this defect class — 1 texture (1x1), no UAV, no
   dispatch. Card L mis-targeted it.
3. `TestRTShadowsGBuffer.cpp` (impler deviation) is clean on the same ten-for-ten basis.
4. Card L's second half is closed. Its first half remains correctly deferred.
5. Zero source files modified; both controls and both shader copies byte-unchanged.
6. A seventh false-zero mechanism, isolated to `path`-at-a-directory in count mode.

**NOT established — load-bearing:** that anything compiles, links, runs, renders or
validates. **No build, no run, no image. 0/7 acceptance gates.**

## Acceptance gates vs the job instruction (7 gates)

| # | Gate | Status | Basis |
|---|------|--------|-------|
| 1 | Debug target builds | **UNKNOWN** | `./Build.sh` denied (`tirith:unknown`) |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs | **UNKNOWN** | needs shell |
| 3 | No Vulkan VUID/ERROR | **UNKNOWN** | newest log 2026-08-14, predates v183 |
| 4 | No command-list errors | **UNKNOWN** | same caveat |
| 5 | `validate_restir_gi.py` newest group | **BLOCKED** | no shell, no python3 |
| 6 | Vision: recognizable Sponza | **BLOCKED (structural)** | no image tool in this runspace |
| 7 | Mode 20 non-zero `GBufferMaterial` | actionable since v182 | needs one operator run |

**0 of 7 verified against the patched tree.** Gates 3/4 deliberately NOT carried forward
as PASS from the 2026-08-14 log — that log describes a pre-v183 tree.

## What this auditor did NOT do

Did not build, run, compile shaders, validate, or view any image. Did not commit, push, or
touch governance files. Did not fabricate any runtime result.
