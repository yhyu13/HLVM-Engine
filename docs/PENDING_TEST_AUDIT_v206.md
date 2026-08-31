# Pending Test Audit v206

- tests: docs/PENDING_TESTS_v206.md
- commit: docs/PENDING_COMMIT_v206.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-552)
- timestamp: 2026-08-30

## Broken-pattern audit

- [x] No from-x-import-y propagation bugs — N/A (C++/HLSL)
- [x] No test-bug-in-itself — re-ran rows 4, 7, 13 myself
- [x] No source-incomplete-relative-to-test — every row names path and method
- [x] No missing isolation fixture — verifier is read-only
- [x] No AsyncMock/sync mismatch — N/A
- [x] No `|` alternation (tick-526)
- [x] No conclusion resting on `output_mode=count` alone (v198)
- [x] No `path`-at-a-file for a load-bearing negative (v199)
- [x] No count quoted from another marker — re-derived
- [x] Every zero controlled by a same-shape positive (v205 row 18)
- [x] No query pasted from a line that wraps (v196)
- [x] No absence asserted where a scope must be read (v198)
- [x] No conclusion resting on hits that are comments (v200)
- [x] No enumeration resting on a convenience wrapper (v201)
- [x] No "never used" claim resting on a symbol count (v202)
- [x] No comment-only diff accepted without reading the returned diff (v203)
- [x] No no-op claim resting on two differently-named variables (v204)
- [x] No zero believed without reading the query's `error` field (v205)
- [x] **No invariant written into a shared header without closing its consumer
      set by query (v206, new)**

## Independent re-derivation

**Row 13 re-run, because it is the row that can be wrong in the most damaging
way.** The header's new comment asserts something about a *different file*. I
read `FBilateralDenoisePass.h:24-41` in place rather than trusting the 4-hit
`GuideScale` count, and the referent says what the citation claims:
`:33-35` "They need NOT match OutputWidth/Height: the Phase-D consumer dispatches
half-res over full-res guides, which is what GuideScale exists for." The new
ReBLUR comment states the exact inverse for its own class. **The cross-reference
is accurate in substance, not just in existence** — a count alone could not have
established that, since a `GuideScale` hit would have appeared even if v205 had
documented the opposite.

**Row 7 re-run in its strong form.** The tester's claim is an absence — no scale
field in `FReBLURConstants`. An absence asserted from a struct read needs the
scope closed (v198), and it is: the struct is `:31-52`, bounded by a
`static_assert(sizeof(FReBLURConstants) == 340)` at `:53` that makes the member
set load-bearing at compile time. Independently, `GuideScale` → **0** on
`TestReSTIR_GI_Temporal_Data/ReBLUR_cs.hlsl`, controlled by the 4 hits the same
token returns in the `PostProcess` header directory. **Both sides of the cbuffer
agree there is no scale**, which is stronger than either alone: a C++-only check
would miss a shader-side constant, and a shader-only check would miss a field the
marshaller writes.

**Row 4 re-run** with its control: `GB(` → 0 on the file under test, 22 on the
enclosing directory, same query shape, same tick, no `error` field on either.

## New checklist row

Row 19 generalises the review gate's finding. The impler enumerated six operands
at **one** call site and wrote an invariant into a **shared** class's header,
where its domain is every consumer. `ReBLURPass.Dispatch` → 2 tree-wide; the
second consumer had to be closed before the edit was verifiable.

The lineage's existing scope rules concern queries and files. This one concerns
**a comment's audience**, and it is the third instance of the same error one
level apart each time: v202 caught v200/v201 scoped to a single `.cpp`; v204
caught v203 scoped to a single class's layouts; v206 catches an invariant scoped
to a single consumer. The pattern is stable enough to state as a rule: **when a
cycle writes a contract into shared code, close the set of things bound by that
contract before the write counts as verified.**

## Per-row verdict

**14/14 KEEP**, plus the review gate's second-consumer row, which I accept as
part of the cycle's evidence. Rows 4, 7, 10 and 13 carry it.

## What this cycle established, and what it did not

**Established (file-only, sound):**

1. **Card P is answered NO — `FReBLURPass` is not a twelfth instance.** Clean by
   enumeration of all six operands from their creation sites, not by pipeline
   ordering. The acceptance path is unaffected by this cycle.
2. **The real finding is a contract divergence between two sibling classes**:
   `FBilateralDenoisePass` guides need not match `OutputWidth`;
   `FReBLURPass` guides must. Same directory, same-looking `FDesc`, opposite
   rules, and until this cycle only one was written down — so the documented one
   actively misled about the undocumented one.
3. **The invariant holds across both consumers by two different mechanisms** —
   the primary because every operand is pinned to a compile-time constant, the
   control because every operand tracks the swapchain together. That is what
   makes it a contract rather than a coincidence of one caller.
4. **The control's `ReBLURHistoryTexture` is the single exception, and it is
   card L.** The header comment now states in source the precise property card
   L's remaining half must restore — the card gained a written specification it
   did not previously have.
5. Zero functional lines changed; both HLSL copies byte-unchanged; the
   known-good control untouched for the 24th unbuilt cycle.

**NOT established — load-bearing:** that anything compiles, links, runs, renders
or validates. This cycle changed 0 functional lines and added 21 comment lines.

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
forward as PASS from the 2026-08-14 log: that log describes a pre-v183 tree, and
24 cycles of source change now sit between it and the working tree.

## Verification attempt this tick

**None scripted.** v205 wrote an ad-hoc script to `/tmp` and could not run it,
leaving an uncleanable temp file. With the block established as categorical by
three probe shapes this tick — including a refused no-op builtin — writing a
fourth unrunnable script would produce no evidence and one more orphaned file.
Verification was done with file tools throughout, which is real evidence rather
than an unexecuted assertion.

## Post-audit revision (same tick, prompted by the KISS/DRY directive)

The comment was tightened from **+21 to +14 lines** after the audit closed. The
first draft was verbose next to the sibling it cross-references (v205 said the
same kind of thing in 10 lines), and length is not evidence. All three claims
survive intact — raw indexing, the inverse-of-sibling contrast, the size-pin
argument — stated once each instead of twice.

**Re-verified after the revision** rather than assumed (v203's near-miss was a
"comment-only" edit that deleted three live items): `nvrhi::TextureHandle` → **8**
hits, same partition as before the revision (5 `FDesc` members `:88-92`, 2
overload params `:110`, 2 dummies `:136-137`); `struct FDesc` `:86` and its
closing brace `:95` intact; both scalars with defaults at `:93-94`. **Member set,
order, types and layout byte-identical to pre-v206.**

**Style check, done rather than assumed**: the em-dash is house style here, not
an import — `—` returns 19 hits across `Public/Renderer`, including `:15` of this
same file, predating this cycle.

## What this auditor did NOT do

Did not build, run, compile shaders, validate, or view any image. Did not commit,
push, or touch governance files. Did not fabricate any runtime result.
