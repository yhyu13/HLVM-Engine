# Pending Test Audit v225

- tests: docs/PENDING_TESTS_v225.md
- commit: docs/PENDING_COMMIT_v225.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-575)
- timestamp: 2026-08-21

## Broken-pattern audit

- [x] **No `|` alternation in any pattern** (tick-526) — every query single-term
- [x] **No `file_glob` in any load-bearing query** (v217) — none used
- [x] **No conclusion resting on `output_mode=count` alone** (v198) — **this is the cycle's subject; enforced this time.** Every load-bearing zero was re-run in `files_only`
- [x] **Every load-bearing zero paired with a same-shape positive control** (v217)
- [x] **No count inherited across markers without re-derivation** (v211) — rows 1-12 re-run by the tester, rows 4/5/8/9/12 re-run again by me
- [x] **No runtime result fabricated** — nothing built, run, executed, or viewed by any role
- [x] **Patch-tool diff read before declaring done** (v203/v224) — all three patches; no displacement

## Rows I re-ran rather than read

**The mechanism, because the tester overturned it after the marker was already written.** `agent_5_tester` at DIR/count returns a map of **exactly 50 files** with total 2; at DIR/files_only it returns **50 files**. The cap is real and it is ~50 files, not a size threshold. Confirmed.

**The final marker text, because it was corrected twice mid-cycle.** Both stale phrasings are gone: the body now carries the enumeration-cap mechanism, and the BOUND sentence reads *"enumerates only the first ~50 files and silently omits the rest."* No residue of the size theory or the bracket theory remains as an active claim — both survive only where they are explicitly labelled falsified.

**Row 4 integrity, independently.** `^- \[x\]` FILE → 108 against 106 pre-patch; total_lines 267 against 265. The +2/+2 agreement rules out a fuzzy displacement.

## Per-row verdict

**13 PASS / 13 KEEP.** Rows 3, 8 and the tester's unplanned row carry the cycle.

## What this cycle established

1. **The lineage's queue-read protocol was structurally incapable of seeing the queue.** `output_mode=count` at directory scope enumerates ~50 files; `docs/` holds hundreds; `PENDING_PICK.md` is not in the first 50. Every "queue empty → Rule 10 → [SILENT]" conclusion drawn from that shape was a false zero. **The queue has 3 actionable cards right now** (L, M, N) and had 4 at tick start.
2. **The rule that would have caught it already existed and was recited by the cycles it failed.** v198's *"no conclusion resting on `output_mode=count` alone"* appears in the broken-pattern audit of cycle after cycle — including `PENDING_TEST_AUDIT_v224.md:16`, a marker from the very tick that then reported the queue empty from a directory-scoped count. The checklist was applied to each cycle's *work product* and never to the *routing input that decides whether a cycle happens*.
3. **The VUID evidence survives, on re-derived grounds.** File-scoped `VUID` at `TestReSTIR_GI_Temporal.log` → 0.

## Standing rule adopted (promoted from the impl review)

**Audit the inputs to the state machine with the same rigour as the outputs of the cycle.** Every marker this lineage produces is scrutinised by four gates; the query that decides whether to produce a marker at all was scrutinised by none for 574 ticks. Any future tick asserting "no actionable items" must show the `files_only` enumeration, not a count.

## Process note — three self-corrections, all caught in-cycle

This cycle changed its own root-cause diagnosis **twice**, each time because a gate tested a claim instead of accepting it:

1. **Plan gate → FIX**: falsified the planner's bracket-class theory with a bracket-free control.
2. **Test gate**: falsified the corrected file-size theory by reproducing the defect on a 4 KB file — encountered *incidentally, while testing the marker that documents the defect*.
3. **Impl re-patch**: mechanism corrected in the landed text, then a stale phrase in the BOUND sentence caught and fixed.

The first diagnosis was plausible and wrong; the second was plausible and wrong; the third is supported by a direct observation of the cap. **Two of the three would have been recorded as fact by a pipeline whose gates rubber-stamped.** This is the strongest evidence in 575 ticks that the gate structure earns its latency — and it argues against the single-profile caveat's pessimism, since the falsifications came from re-running queries, not from independent priors.

## Acceptance gates vs the job instruction: 0 of 7 (unchanged)

| # | Gate | Status | Basis |
|---|---|---|---|
| 1 | Debug target builds | UNKNOWN | `terminal` refused at tool boundary (2 fresh probes this tick) |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` | BLOCKED | same |
| 3 | No Vulkan VUID/ERROR | UNKNOWN | newest log 2026-08-14, predates 42 unbuilt cycles |
| 4 | No command-list errors | UNKNOWN | same |
| 5 | `validate_restir_gi.py` newest group | BLOCKED | same |
| 6 | Vision: recognizable Sponza | BLOCKED (structural) | no image tool in this runspace (tick-528) |
| 7 | Mode 20 non-zero `GBufferMaterial` | UNKNOWN | needs one approved run |

**This cycle's contribution to gates: 0.** It did not build, run, or render anything. What it repaired is upstream of the gates: the pipeline's ability to correctly read its own work queue.

## What this auditor did NOT do

Did not build, run, compile, validate, or view any image. Did not commit or push. Did not modify engine source — `SetBindingOffsets` at `FGIPass.cpp` still 1 hit, the v183–v224 chain byte-unchanged. Did not modify `AGENTS.md`, `CLAUDE.md`, `.cursorrules`, or any governance file. Did not modify `~/.hermes/config.yaml`, `approval.py`, `gateway/run.py`, or `tirith_security.py`. **Did not fabricate any runtime result.**
