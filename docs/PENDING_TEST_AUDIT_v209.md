# Pending Test Audit v209

- tests: docs/PENDING_TESTS_v209.md
- commit: docs/PENDING_COMMIT_v209.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-555)
- timestamp: 2026-08-30

## Broken-pattern audit

- [x] No from-x-import-y propagation bugs — N/A (C++)
- [x] No test-bug-in-itself — re-ran rows 1 and 6 myself
- [x] No source-incomplete-relative-to-test — every row names path and method
- [x] No missing isolation fixture — verifier is read-only
- [x] No AsyncMock/sync mismatch — N/A
- [x] No pattern in ERE syntax against a BRE engine (v208) — **fired once this
      cycle, in the reviewer, and was caught rather than acted on**
- [x] No conclusion resting on `output_mode=count` alone (v198)
- [x] No `path`-at-a-file for a load-bearing negative (v199)
- [x] No count quoted from another marker — all re-derived
- [x] Every zero controlled by a same-shape positive (v205)
- [x] No absence asserted where a scope must be read (v198)
- [x] No conclusion resting on hits that are comments (v200)
- [x] No "never used" claim resting on a symbol count (v202)
- [x] No comment-only diff accepted without reading the returned diff (v203)
- [x] No zero believed without reading the query's `error` field (v205)
- [x] No linter output dismissed without mapping each line to a cause (v207)
- [x] **No conclusion drawn from a query that reported `search_timeout` (v209, new)**

## New row 18, and why it needed adding this cycle

Every catalogued false-instrument mechanism so far has been about **pattern
syntax** or **query scope**. This cycle produced one that is neither: three
`search_files` calls over `Engine/Source` returned

    {"total_count": 0, "truncated": true, "limit_reason": "search_timeout"}

for terms that demonstrably exist two directories down. **The pattern was
correct, the scope was correct, and the answer was still zero** — the engine
gave up. This is more dangerous than the BRE/ERE trap it joins, because the
syntax rules v208 established are all about *writing the query right*, and
writing this one right does not help. The only defence is reading
`limit_reason` on every zero. The planner did; the row makes it obligatory.

## Independent re-derivation

**Row 1 re-run in a strictly stronger form.** The tester closed it with the
exact token `DummyDirectionTexture` → 0, controlled at 5 and 6. Necessary but
weak in one specific way: it would also return 0 if a reference survived under
a *truncated or differently-spelled* form — a partial rename, or a reference to
`DummyDirection` in a comment that a future reader would take as live.

I closed it a second way: **prefix query `Dummy` on the GI implementation
directory → 13 hits, and every one of the 13 belongs to
`DummyDebugStatsTexture` or its local `DummyDesc` builder** (`:191`, `:613`,
`:615-624`, `:626`), including the `debugName` string literal
`"FGIPass.DummyDebugStats"`. `DummyDirection` as a prefix → **0**, and that
zero is controlled by the 13. So the symbol is gone under partial-match too,
and the surviving 13 are a single coherent live member — not a residue.

**Row 6 re-run**, the row the tester added and correctly identified as the
one that matters. Read `FGIPass.cpp:640-649` in place rather than counting:
the v207 ternary is present and unmodified —
`DirectionUAV = Desc.OutputDirection ? Desc.OutputDirection :
Desc.OutputTexture`, followed by the state transition and
`SetTextureUAV(2, DirectionUAV)`. **The deletion did not re-arm the
out-of-bounds UAV store v207 removed**, which is the only way this "dead code
cleanup" could have caused real harm.

## The reviewer's caught near-miss — upheld as a catch, not a defect

The reviewer ran two queries against the shader that returned 0 for a symbol
that exists (`DebugBounceStats` is the C++-side name; the shader declares
`DebugStatsTexture`), **did not conclude from either**, and located the
declaration by a third route before endorsing the comment. That is the correct
handling and it is worth recording as a positive: the failure this lineage
keeps producing is not *getting a bad zero*, it is *believing one*.

## Per-row verdict

**10/10 KEEP.** Rows 1, 6 and 9/10 carry the cycle.

## The tester's correction to the commit marker — upheld, and bounded

The tester flagged that the commit's change table cites pre-patch line numbers
that now point at different statements. Correct, and the table does say
"Line (pre)", so it is not false — it is misreadable. **Bounding it: no
conclusion in the commit depends on those numbers**, since the verification
rests on symbol counts and in-place reads. Label imprecise, finding sound,
patch correct. Third consecutive cycle where a marker's *numbering* was
imperfect while its *conclusion* held — the standing lesson (counts are not
invariants, sets are) continues to hold.

## What this cycle established, and what it did not

**Established (file-only, sound):**

1. `FGIPass::DummyDirectionTexture` is removed, and its absence is closed
   **structurally** (private member, no `friend`, so only `FGIPass.cpp` can
   name it) as well as by controlled enumeration and prefix query.
2. The live sibling `DummyDebugStatsTexture` is intact at all 5 sites, and the
   asymmetry that makes it correctly live is now **documented in the header**,
   with both halves of that documentation verified against the shader.
3. v207's u2 ternary is intact — the cleanup did not undo the fix that made
   the cleanup possible.
4. A new false-instrument mechanism (`search_timeout` zeros) is diagnosed and
   ruled on.

**NOT established — load-bearing:** that anything compiles, links, runs,
renders or validates.

**Severity, stated without inflation:** this cycle **moves no pixel and clears
no acceptance gate.** It removes dead private state and, more usefully, closes
the path by which a future cycle would have restored it on a symmetry argument.

## Acceptance gates vs the job instruction (7 gates)

| # | Gate | Status | Basis |
|---|------|--------|-------|
| 1 | Debug target builds | **UNKNOWN** | `./Build.sh` unreachable — terminal denied |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs | **UNKNOWN** | needs shell |
| 3 | No Vulkan VUID/ERROR | **UNKNOWN** | newest log 2026-08-14, predates v183 |
| 4 | No command-list errors | **UNKNOWN** | same caveat |
| 5 | `validate_restir_gi.py` newest group | **BLOCKED** | no shell, no python3 |
| 6 | Vision: recognizable Sponza | **BLOCKED (structural)** | no image tool in this runspace |
| 7 | Mode 20 non-zero `GBufferMaterial` | **UNKNOWN** | needs one operator run |

**0 of 7 verified against the patched tree.** Gates 3/4 deliberately NOT
carried forward as PASS from the 2026-08-14 log: it describes a pre-v183 tree
with 27 cycles of source change since.

## Verification attempt this tick

**Terminal denied categorically.** Two shapes refused this tick — a compound
`ls`/`git log` and a bare `true` — both `pending_approval / tirith:unknown /
exit_code -1`. A refused no-op builtin rules out command content, arguments,
path and toolchain as causes; the refusal is at the tool boundary.

No ad-hoc harness was written. Prior cycles' harnesses went unrun and left
orphaned `/tmp/hermes-verify-*` artefacts that cannot be cleaned up; another
would add an orphan and produce no evidence.

**Operator command that clears the blocker:**

    ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test

## What this auditor did NOT do

Did not build, run, compile shaders, validate, or view any image. Did not
commit, push, or touch governance files. Did not fabricate any runtime result.
