# Pending Test Audit v198

- tests: docs/PENDING_TESTS_v198.md
- commit: docs/PENDING_COMMIT_v198.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-544)
- timestamp: 2026-08-30

## Broken-pattern audit

- [x] No from-x-import-y propagation bugs — N/A (C++)
- [x] No test-bug-in-itself — I re-executed rows 3, 4, 12, 17 myself
- [x] No source-incomplete-relative-to-test — every row names path and query
- [x] No missing isolation fixture — verifier is read-only
- [x] No AsyncMock/sync mismatch — N/A
- [x] No `|` alternation (tick-526)
- [x] No conclusion resting on `output_mode=count` alone
- [x] No count quoted from another marker — re-derived
- [x] Every zero controlled by a same-shape positive
- [x] No query pasted from a line that wraps (v196)
- [x] No query encoding an unverified declaration form (v197)
- [x] **No absence asserted by a query (v198, new)** — see below

## Independent re-derivation

**Row 12, the row the cycle rests on, and the only one that could silently invert the
verdict.** The tester was right that this row is a read and not a query, and right to say so.
I checked the one thing that would make it wrong: **the block bounds**. Read `:1253-1256` —
`}` / blank / `BindingCache.Clear();` / `}`. The resize block closes at `:1256`. Read
`:950-955` — the ReSTIR texture block opens at `:955`, after a `HLVM_LOG` at `:954`, under a
banner comment at `:951-953`.

So the ReSTIR creation block at `:955-1010` lies **300 lines before** the resize block opens
at `:1160`, not merely outside it. The partition is not marginal and does not depend on a
brace-counting judgement call. Confirmed.

**Row 4** re-run: `GenDesc.OutputWidth` → 1 hit, `:1531`, `= CurrentFBInfo.width`.
**Row 3 and 17**, the two controls: `v198` → 0 in `TestPathTraceGI.cpp` and 0 in
`TestReSTIR_GI_Temporal_Data`, against 4 hits in `docs/`. Both zeros controlled.

## The audit item I am adding, and why it belongs in the checklist

The tester articulated something no prior cycle had, and it generalises past this cycle:

> A `search_files` result can establish that something is **present**. It can never
> establish that something is **absent from a region**, because the region is not part of
> the query.

Five false-zero mechanisms are on record — alternation, escaped metacharacters, unescaped
metacharacters, line wrap, and v197's assumption-encoding. All five are about a query
returning zero when it should have returned something. **This is the inverse and it is
worse**: a query that returns hits, all of them real, from which an absence is then inferred
by a human reading line numbers. Nothing is malformed. Nothing returns zero. The inference
happens entirely outside the tool, and the tool's authority gets attached to it anyway.

The defence the tester used is the right one and is now a checklist row: **for a claim about
an absence, do not use a query for the load-bearing step.** Enumerate everything, read the
bounds, partition by hand, and say in the marker that this is what was done so the next
reader knows which step to re-check.

## Per-row verdict

**17/17 KEEP.** No row is padding. Row 16 is the one I would have added had it been omitted —
it is what separates "this cycle changed nothing" from "this cycle changed nothing and
nothing else drifted," and without it a zero-change cycle is indistinguishable from a cycle
that failed to run.

## On the verdict for a cycle that produced no patch

I considered whether ALL_KEEP is the honest verdict for a cycle whose output is a card and a
determination. It is, and the reason matters: **the deliverable of this cycle was a
determination, and the determination is sound and load-bearing.** The rows verify what the
cycle actually claimed. A cycle is not weak because it produced no diff; it would be weak if
its claims were unsupported, and these are supported at a higher standard than most cycles
that did produce diffs — because the central claim is an absence, and absences had to be
established by reading rather than by grepping.

**The one thing I want on record against this cycle**: the finding is in the known-good
control, and the control is now known to be defective under resize. That does not retroactively
invalidate the control's use in the lineage — every prior use was at the fixed startup extent
where the defect is dormant — but **the phrase "known-good control" now carries a
qualification**, and any future cycle that leans on `TestCornellBoxGI` as an exoneration must
state that it is relying on it at 800x600 only.

## What this cycle established, and what it did not

**Established (file-only, sound):**

1. A tenth instance of the extent class exists in `TestCornellBoxGI.cpp`, and it is the
   first that is a **lifetime** mismatch rather than an extent-source mismatch: 14 textures
   created once at startup extent, dispatched over at swapchain extent, `Resizable = true`.
2. Both compute guards are tautological (clipped against the dispatch's own extent), so the
   failure is an unguarded OOB UAV store — second instance of v193's sub-pattern.
3. **No query shape can find this class member.** The defect exists as an absence. This
   retires card I's framing and replaces it with a per-pass procedure.
4. No source file was modified; both controls and both shader copies are byte-unchanged.
5. The primary target's `FB.width` enumeration remains clean after this cycle.

**NOT established — load-bearing:** that anything compiles, links, runs, renders or
validates. **No build, no run, no image. 0/7 acceptance gates.**

## Acceptance gates vs the job instruction (7 gates)

| # | Gate | Status | Basis |
|---|------|--------|-------|
| 1 | Debug target builds | **UNKNOWN** | `./Build.sh` denied (`tirith:unknown`) |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs | **UNKNOWN** | needs shell |
| 3 | No Vulkan VUID/ERROR | **UNKNOWN** | newest log 2026-08-14, predates v183-v198 |
| 4 | No command-list errors | **UNKNOWN** | same caveat |
| 5 | `validate_restir_gi.py` newest group | **BLOCKED** | no shell, no python3 |
| 6 | Vision: recognizable Sponza | **BLOCKED (structural)** | no image tool in this runspace |
| 7 | Mode 20 non-zero `GBufferMaterial` | actionable since v182 | needs one operator run |

**0 of 7 verified against the patched tree.** Gates 3/4 deliberately NOT carried forward as
PASS from the 2026-08-14 log — that log describes a pre-v183 tree.

## What this auditor did NOT do

Did not build, run, compile shaders, validate, or view any image. Did not commit, push, or
touch governance files. Did not fabricate any runtime result.
