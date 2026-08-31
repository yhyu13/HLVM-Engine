# Pending Test Audit v200

- tests: docs/PENDING_TESTS_v200.md
- commit: docs/PENDING_COMMIT_v200.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-546)
- timestamp: 2026-08-30

## Broken-pattern audit

- [x] No from-x-import-y propagation bugs — N/A (C++)
- [x] No test-bug-in-itself — I re-ran rows 1, 7, 11, 12, 15/16 myself
- [x] No source-incomplete-relative-to-test — every row names path and query
- [x] No missing isolation fixture — verifier is read-only
- [x] No AsyncMock/sync mismatch — N/A
- [x] No `|` alternation (tick-526)
- [x] No conclusion resting on `output_mode=count` alone (v198)
- [x] No `path`-at-a-directory used for a load-bearing negative (v199)
- [x] No count quoted from another marker — re-derived; **this fired, see below**
- [x] Every zero controlled by a same-shape positive
- [x] No query pasted from a line that wraps (v196)
- [x] No absence asserted by a query (v198)
- [x] **No conclusion resting on a hit-count where the hits are comments (v200, new)**

## Independent re-derivation

Row 12 re-run under a *tighter* query than the tester used
(`GBufferScale     =`, with the file's actual alignment whitespace) → 2 hits,
`:1061` and `:1109`, both `static_cast<float>(WIDTH / std::max(HalfResWidth, 1u))`.
Read `:1104-1111` in full to confirm the spatial assignment is inside the live
constants block and not in a disabled branch. It is.

Rows 15/16 re-run: 12 hits in each `GIPathTracing.hlsl` copy at identical line
numbers. The dual-copy sync reproduces.

## The count discrepancy: the tester is right, the commit is wrong, and it is benign

Three independent recounts (reviewer, tester, me) give **46 of 64**; the impl
marker says 45. The disagreement is one scalar in the `:432-442` run.

I record it as ALL_KEEP rather than SOME_RELAX because the tester **did not
inherit** the number — it recounted, disagreed, and said so, which is the
behaviour the row exists to produce. A pipeline that catches its own arithmetic
one role downstream is working.

## New checklist row, and why it is not a footnote on an existing one

Row 7 is the first row in this lineage whose **hit count is the wrong instrument
entirely**. The lineage has now recorded seven false-zero mechanisms — all about
a query returning 0 when matches exist, or a count being wrong. This is
different in kind: `Pad\[` returns 3 hits, the count is *correct*, and the
correct count still supports the opposite conclusion, because every hit is a
comment. Reading "3 hits for `Pad[`" as "an array is present in the cbuffer tail"
would have reversed the v184 verdict.

The safe form is what the tester used: for any query whose subject is a
**declaration**, read the hits; a count only answers existence, never kind.

## Per-row verdict

**18/18 KEEP.** Rows 7 and 11 carry the cycle — 7 because a count-only reading
inverts it, 11 because it is the one row where the marker chain disagreed with
itself and the disagreement was caught in-flight.

## What this cycle established, and what it did not

**Established (file-only, sound):**

1. The v183-v199 chain is coherent in the two defect classes this lineage has
   demonstrated: arity (`RenderGBuffer` = 2 hits, both zero-arg) and cbuffer
   layout (four-way agreement on both structs, absolute float offsets 43/44/45
   confirmed by five independent derivations).
2. No cbuffer overflow: 46 of 64 floats, 18 headroom.
3. `GBufferScale` is explicitly assigned at all four call sites — the v184
   silent-zero revert is not present.
4. Both `GIPathTracing.hlsl` copies are in sync (v182 dual-copy hazard clear).
5. Zero source files modified this cycle.

**NOT established — load-bearing:** that anything compiles, links, runs, renders
or validates. This is a **static** audit. It lowers the expected cost of the
operator's first build; it does not substitute for it, and no row above should
ever be cited as evidence that a gate passed.

## Acceptance gates vs the job instruction (7 gates)

| # | Gate | Status | Basis |
|---|------|--------|-------|
| 1 | Debug target builds | **UNKNOWN** | `./Build.sh` denied (`tirith:unknown`); static audit found no blocker |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs | **UNKNOWN** | needs shell |
| 3 | No Vulkan VUID/ERROR | **UNKNOWN** | newest log 2026-08-14, predates v183 |
| 4 | No command-list errors | **UNKNOWN** | same caveat |
| 5 | `validate_restir_gi.py` newest group | **BLOCKED** | no shell, no python3 |
| 6 | Vision: recognizable Sponza | **BLOCKED (structural)** | no image tool in this runspace |
| 7 | Mode 20 non-zero `GBufferMaterial` | actionable since v182 | needs one operator run |

**0 of 7 verified against the patched tree.** Gates 3/4 deliberately NOT carried
forward as PASS from the 2026-08-14 log — that log describes a pre-v183 tree.

## What this auditor did NOT do

Did not build, run, compile shaders, validate, or view any image. Did not commit,
push, or touch governance files. Did not fabricate any runtime result.
