# Pending Test Audit v201

- tests: docs/PENDING_TESTS_v201.md
- commit: docs/PENDING_COMMIT_v201.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-547)
- timestamp: 2026-08-30

## Broken-pattern audit

- [x] No from-x-import-y propagation bugs — N/A (C++)
- [x] No test-bug-in-itself — I re-ran rows 1, 6, 9, 12 myself
- [x] No source-incomplete-relative-to-test — every row names path and query
- [x] No missing isolation fixture — verifier is read-only
- [x] No AsyncMock/sync mismatch — N/A
- [x] No `|` alternation (tick-526)
- [x] No conclusion resting on `output_mode=count` alone (v198)
- [x] No `path`-at-a-directory for a load-bearing negative (v199)
- [x] No count quoted from another marker — re-derived
- [x] Every zero controlled by a same-shape positive — row 7/8 pair
- [x] No query pasted from a line that wraps (v196)
- [x] No absence asserted by a query (v198) — **and this cycle's finding IS an
      absence, so see below**
- [x] No conclusion resting on a hit-count where the hits are comments (v200)
- [x] **No enumeration resting on a convenience wrapper (v201, new)**

## Independent re-derivation

Row 1 re-read directly: `:1385-1388` is a complete four-line override whose body
is one statement. Row 6 re-run: `createTexture` → 8 hits; I read `:315` and
`:570` myself rather than accepting the impler's or reviewer's characterisation,
and both hold (1x1 placeholder; `WIDTH`/`HEIGHT` at `:563-564`). Row 12
re-verified: `Resizable` → 1 hit, `= true`.

## The v198 checklist row applies to this cycle's own conclusion, and it survives

Row 12 of this checklist says an absence must not be asserted by a query — and
this cycle's headline finding *is* an absence ("`BackBufferResizing` recreates
nothing"). That looks like a violation and is not, for a reason worth recording:
the absence was established by **reading the complete enclosing scope**, not by
a query returning zero. The override is four lines; its bounds are unambiguous;
the whole body was read. An absence inside a fully-read bounded scope is a
positive observation about that scope.

This is the correct discharge of the v198 rule, and the first cycle to exercise
it. v198's defect was invisible precisely because its scope (a resize block 300
lines long) was never read in full — the rule is about scope bounds, not about
the word "absence."

## New checklist row

Row 14 is the reviewer's catch generalised: the impler enumerated
`CreateTexture2D`, a file-local helper, and treated it as coextensive with
"texture creation." It was not — the raw `createTexture` API is called at two
further sites. Both were consistent, so nothing reversed, but the *completeness
claim* rested on an unstated and false premise.

Distinct from every prior row: those concern a query returning the wrong hits.
This concerns a query returning exactly the right hits **for the wrong set**.
The safe form: enumerate the operation; if you enumerate a wrapper, first prove
the wrapper is the only route to the operation.

## Per-row verdict

**14/14 KEEP.** Row 6 carries the cycle — it is the only row that could have
falsified the conclusion, and it was not in the impler's own test design.

## What this cycle established, and what it did not

**Established (file-only, sound):**

1. The primary target is **structurally immune** to the extent/lifetime class,
   not merely clean: it owns no swapchain-sized resource, and its
   `BackBufferResizing` recreates nothing, so v198's set difference is vacuous
   here by construction.
2. The immunity is load-bearing — `Resizable = true`, so the resize really can
   occur.
3. The cumulative `FB.width`/`FB.height` union (never taken before v201, because
   v191-v195 each deliberately declined to bundle) leaves exactly three live
   sites, all three correct by category.
4. Both live dispatch grids are fixed-extent.
5. Zero source files modified.

**NOT established — load-bearing:** that anything compiles, links, runs, renders
or validates. This is a **static** audit, as v200 was. No row above may be cited
as evidence that a gate passed.

## Acceptance gates vs the job instruction (7 gates)

| # | Gate | Status | Basis |
|---|------|--------|-------|
| 1 | Debug target builds | **UNKNOWN** | `./Build.sh` denied (`tirith:unknown`) |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs | **UNKNOWN** | needs shell |
| 3 | No Vulkan VUID/ERROR | **UNKNOWN** | newest log 2026-08-14, predates v183 |
| 4 | No command-list errors | **UNKNOWN** | same caveat |
| 5 | `validate_restir_gi.py` newest group | **BLOCKED** | no shell, no python3 |
| 6 | Vision: recognizable Sponza | **BLOCKED (structural)** | no image tool in this runspace |
| 7 | Mode 20 non-zero `GBufferMaterial` | **UNKNOWN** | needs one operator run |

**0 of 7 verified against the patched tree.** Gates 3/4 deliberately NOT carried
forward as PASS from the 2026-08-14 log — that log describes a pre-v183 tree.

## What this auditor did NOT do

Did not build, run, compile shaders, validate, or view any image. Did not commit,
push, or touch governance files. Did not fabricate any runtime result.
