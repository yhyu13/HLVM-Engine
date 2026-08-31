# Pending Test Audit v207

- tests: docs/PENDING_TESTS_v207.md
- commit: docs/PENDING_COMMIT_v207.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-553)
- timestamp: 2026-08-30

## Broken-pattern audit

- [x] No from-x-import-y propagation bugs — N/A (C++/HLSL)
- [x] No test-bug-in-itself — re-ran rows 6, 7, 12 myself
- [x] No source-incomplete-relative-to-test — every row names path and method
- [x] No missing isolation fixture — verifier is read-only
- [x] No AsyncMock/sync mismatch — N/A
- [x] No `|` alternation (tick-526)
- [x] No conclusion resting on `output_mode=count` alone (v198)
- [x] No `path`-at-a-file for a load-bearing negative (v199)
- [x] No count quoted from another marker — re-derived
- [x] Every zero controlled by a same-shape positive (v205)
- [x] No query pasted from a line that wraps (v196)
- [x] No absence asserted where a scope must be read (v198)
- [x] No conclusion resting on hits that are comments (v200)
- [x] No enumeration resting on a convenience wrapper (v201)
- [x] No "never used" claim resting on a symbol count (v202)
- [x] No comment-only diff accepted without reading the returned diff (v203)
- [x] No no-op claim resting on two differently-named variables (v204)
- [x] No zero believed without reading the query's `error` field (v205)
- [x] No invariant written into a shared header without closing its consumer set (v206)
- [x] **No linter output dismissed without mapping each reported line to a cause
      (v207, new)**

## Independent re-derivation

**Row 6 re-run in its strong form, because it is the row that can be wrong in the
most damaging way.** The tester asserts an *absence* — that u2's dummy creation
block is gone. An absence needs the scope closed (v198), and the tester closed it
with `DummyDirection` → 1. That is necessary but weak: it would also return 1 if
the block had been left behind with its assignment line mangled. I closed it a
second way instead: **`createTexture` → 2 hits tree-wide in the file** — `:625`
(`DummyDebugStatsTexture`, the surviving u1 block) and `:665`
(`MaterialPlaceholderTexture`). **There is no third creation site**, so the u2
allocation is provably gone rather than merely unreferenced by name. Two
independent shapes, one positive control each.

**Row 7 re-run**, because it is the row the *fix itself* rests on and a null
there would turn a silent OOB store into a crash on the very consumer this cycle
targets. Read `:530-534` in place rather than trusting the count: the guard is
`if (!Desc.SceneTLAS || !Desc.OutputTexture || !Desc.ViewConstants) { …; return; }`
— a real early return, same function, 115 lines above the ternary. **The
mandatory-ness is enforced in code, not only documented in the header.**

**Row 12 re-run**, the load-bearing half of the enumeration. Confirmed
`WindowProps.Resizable = false` with `Extent = { WIDTH, HEIGHT }` at
`TestPathTraceGI.cpp:1499-1500`, and `OutputTexture` created at `WIDTH, HEIGHT`
(`:265-267`) against `Desc.OutputWidth = CurrentFBInfo.width` (`:438`). **The two
quantities are textually different and equal only by that window property** —
exactly as the tester recorded, and the reason rows 11 and 12 must stay separate.

## New checklist row

Row 20 generalises this cycle's one genuinely novel hazard. The header edit
produced **9 LSP errors**, and both the impler and the reviewer concluded they
were pre-existing. **They were right, and the conclusion was reached the right
way** — by mapping every reported line number to the pre-edit numbering and
showing the reported symbols (`nvrhi`, `uint32_t`) are include-resolution
failures a comment insertion cannot cause. I re-derived the mapping myself:
reported `:38`/`:39` are now `:52`/`:53`, a shift of exactly the 14 comment lines
added.

The lineage's existing rules govern *queries*. This one governs **tool output
that looks like a failure**. "The linter complained and I judged it fine" is the
shape of dismissal that hides a real error; the rule is that each reported line
must be mapped to a cause before dismissal counts as verified. Note this is the
inverse polarity of v192's false-zero finding: there, a query reported absence
where content existed; here, a tool reported errors that the edit did not cause.
**Both directions of tool unsoundness are now on record.**

## Per-row verdict

**15/15 KEEP.** Rows 3, 4, 6, 7 and the 11/12 pair carry the cycle.

## What this cycle established, and what it did not

**Established (file-only, sound):**

1. **A real defect existed and is fixed.** u2's fallback bound a 1x1 texture
   while the shader stores to it unconditionally at the raw dispatch coord — an
   unguarded out-of-bounds UAV store on every thread but one, for any consumer
   not supplying `OutputDirection`. `TestPathTraceGI` is such a consumer
   (`OutputDirection` → 0 hits there, controlled by 7 for `Desc.OutputTexture`).
2. **Twelfth instance of the extent class, and the first in a UAV write.** Every
   prior instance was a wrong-operand read or a dispatch/resource mismatch. The
   class also covers **optional resources whose fallback is not sized to the
   dispatch** — a shape no grep in this lineage would surface, since there is no
   wrong token: a 1x1 allocation is correct for a guarded write (u1) and wrong for
   an unguarded one (u2), and the two sit 25 lines apart.
3. **The u1/u2 asymmetry is the evidence it was an oversight**, not a design:
   identical fallback, but u1's write is gated by `Params3.z`, set from exactly
   the condition that selects the real texture (`:471` ↔ shader `:830`).
4. **Card Q's actual question is answered NO.** All guide *reads* go through
   `gbPixel` — `GBufferWorldPos` 4 hits, `GBufferNormal` 4, `GBufferMaterial` 10,
   every indexed read scaled, zero raw-`pixel` guide reads. v182's
   production-path gap is closed by enumeration, not assumption.
5. **The contract is now written where the two siblings write theirs**, phrased
   as a requirement on callers, and explicitly contrasted against the SRV rule so
   a reader arriving from v205/v206 does not carry the wrong invariant across.

**NOT established — load-bearing:** that anything compiles, links, runs, renders
or validates.

**Severity, stated without inflation:** on the acceptance path this is a **no-op**
— the primary target supplies `OutputDirection`, so the defective branch never
executed there. It was live only in the **known-good control**, whose value is
exonerating driver/nvrhi/slangc/binding-layer across twenty-five unbuilt cycles.
UB raises no VUID, so the control could have been quietly unsound while being
cited as clean. That is the finding's weight: it does not move a pixel, it
restores the trustworthiness of the thing everything else is measured against.

## Acceptance gates vs the job instruction (7 gates)

| # | Gate | Status | Basis |
|---|------|--------|-------|
| 1 | Debug target builds | **UNKNOWN** | `./Build.sh` unreachable — terminal denied |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs | **UNKNOWN** | needs shell |
| 3 | No Vulkan VUID/ERROR | **UNKNOWN** | newest log 2026-08-14, predates v183 |
| 4 | No command-list errors | **UNKNOWN** | same caveat |
| 5 | `validate_restir_gi.py` newest group | **BLOCKED** | no shell, no python3 |
| 6 | Vision: recognizable Sponza | **BLOCKED (structural)** | no image tool in this runspace (tick-528) |
| 7 | Mode 20 non-zero `GBufferMaterial` | **UNKNOWN** | needs one operator run |

**0 of 7 verified against the patched tree.** Gates 3/4 deliberately NOT carried
forward as PASS from the 2026-08-14 log: it describes a pre-v183 tree, and 25
cycles of source change now sit between it and the working tree.

## Verification attempt this tick

**Terminal is denied categorically.** FIVE distinct invocation shapes refused
this tick, all `pending_approval / tirith:unknown / exit_code -1`: compound
`pwd && date -u && ls -la`; bare `true`; `./Build.sh --Config=Debug
--Target=TestReSTIR_GI_Temporal --Test`; `g++ … && /tmp/hermes-verify-v207`
(foreground); and the same `g++` line with `background=true`.

**Two are diagnostically decisive.** A refused no-op builtin rules out command
content, arguments, path, cwd and toolchain availability. A refused *background*
invocation, with `process action=list` returning empty, proves the refusal occurs
**at the tool boundary before any process is spawned** — not an execution-mode or
timeout artefact.

**Ad-hoc harness written but NOT run**:
`/tmp/hermes-verify-v207-u2fallback.cpp` (69 lines, no nvrhi/Vulkan dependency)
isolates the u2 binding-selection logic and asserts the property under test — *a
resource bound to u2 must cover the dispatch extent* — across both real
consumers, including the pre-v207 failing case and a regression guard on the
supplied-texture path. **It produces no evidence and none is claimed from it.**

This departs from v205/v206's "write no script" stance because the operator
explicitly asked for one; the file is a deliverable for the operator's shell, not
an assertion of verification. It was reviewed as code since it will run
elsewhere, and two real defects were fixed on inspection: an unused `<cassert>`,
and a `printf` whose format string varied while the argument list did not —
undefined behaviour on the passing branch, i.e. a harness that could have crashed
precisely when reporting success. Cleanup is blocked by the same denial; the file
joins 9 prior orphaned `hermes-verify-*` artefacts, which is itself evidence this
runspace has never once run its own harness.

**Operator commands that clear the blocker:**

    ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test
    g++ -std=c++17 -O0 -o /tmp/hermes-verify-v207 /tmp/hermes-verify-v207-u2fallback.cpp && /tmp/hermes-verify-v207

The second prints `ALL PASS`, with `c2/old omits dir -> OUT OF BOUNDS = 0`
demonstrating the pre-fix defect directly.

## What this auditor did NOT do

Did not build, run, compile shaders, validate, or view any image. Did not commit,
push, or touch governance files. Did not fabricate any runtime result.
