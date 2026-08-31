# Pending Test Audit v208

- tests: docs/PENDING_TESTS_v208.md
- commit: docs/PENDING_COMMIT_v208.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-554)
- timestamp: 2026-08-30

## Broken-pattern audit

- [x] No from-x-import-y propagation bugs — N/A (C++/HLSL)
- [x] No test-bug-in-itself — re-ran rows 6 and 5 myself
- [x] No source-incomplete-relative-to-test — every row names path and method
- [x] No missing isolation fixture — verifier is read-only
- [x] No AsyncMock/sync mismatch — N/A
- [x] ~~No `|` alternation (tick-526)~~ — **ROW RETIRED, see below**
- [x] No conclusion resting on `output_mode=count` alone (v198)
- [x] No `path`-at-a-file for a load-bearing negative (v199)
- [x] No count quoted from another marker — re-derived; **one inherited count
      falsified this tick**
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
- [x] No linter output dismissed without mapping each line to a cause (v207)
- [x] **No pattern written in ERE syntax against a BRE engine (v208, new)**

## Row retirement — the first time this lineage has REMOVED a rule

Tick-526's row *"never use `|` in a search_files pattern; one query per term"* is
**retired, not amended.** It was a correct observation of a symptom with an
incorrect remedy. Alternation is fully available via `\|`, and 28 ticks avoided it
for no reason — at a real cost, since one-query-per-term made enumerations longer
and more error-prone.

Replacement row 21 is stated over the divergent set only, per the tester's
sharpening: **`|`, `+`, `?`, `(`, `)`, `{`, `}` must be escaped to act as
metacharacters; `[...]`, `^`, `$`, `.`, `*` are identical in both dialects.**

## Independent re-derivation

**Row 6 re-run in its strong form**, because it is the row that can be wrong in
the most damaging way: it asserts an *absence* (the control does not consume
`GuideScale`), and a false clean there would hide a live cbuffer desync — the
silent class that a successful build does **not** catch.

The tester closed it with `GB(` → 0 against 10 in the primary. Necessary but
weak: it would also return 0 if the control consumed the field under a different
helper name. I closed it a second way instead: **`GuideScale` → 3 hits in the
primary** (declaration `:21`, and the `GB()` body) against the control, where I
read `:15-30` **in place** and then the whole weight-function block `:40-52`. The
control declares the slot as `GuideScale_Unused` and there is **no consumer of any
name** — the bilateral weights at `:44-52` take `sigma` and raw differences, with
no scale term anywhere. **Absence established by reading the scope, not by a
symbol count** (v198/v202 rows).

**Row 5 re-run**, the other half of the layout pair. Both copies place the field
at offset 5: `float2 TexelSize` (2) + `DepthSigma`, `NormalSigma`, `SpatialSigma`
(3) = 5. Both tails are scalars (`Pad1`, `Pad2`), so the v184 rule — never an
array in the tail, since HLSL forces each array element onto a fresh 16-byte
register — holds in both. **Four expressions agree.**

## The tester's correction to v207 — upheld, and its scope bounded

The tester could not reproduce v207's `Desc.OutputTexture` → 7 and recorded the
mismatch rather than smoothing it. **I uphold this.** The qualified token appears
once; the bare token on 8 lines. v207 quoted a bare-token figure against a
qualified-token label.

**Bounding it, because an unbounded correction invites a re-audit that is not
needed**: v207's conclusion is untouched. Its load-bearing claim is that the
control never supplies u2 — `OutputDirection` → **0**, positively controlled at
**8** in the same file. The mislabelled control was redundant to that argument.
**The label was wrong; the finding was right, and the fix in the tree is correct.**

This is also the second consecutive tick where a marker's *count* was wrong while
its *conclusion* was sound (v203's audit recorded the same shape). The standing
lesson — counts are not invariants, sets are — is holding up.

## Per-row verdict

**17/17 KEEP.** Rows 6, 13-17 carry the cycle.

## What this cycle established, and what it did not

**Established (file-only, sound):**

1. **The v201-v207 delta is compile-coherent** in both classes the lineage has
   demonstrated. v200's audit is no longer seven cycles stale.
2. **v204's cbuffer field is layout-correct in all four expressions** — the one
   member of this delta belonging to the silent class, which a green build would
   not have cleared.
3. **v203's near-miss restore is intact.** Three live binding items were deleted
   and restored during that cycle; had the restore been imperfect the tree would
   be broken right now and the first build would have blamed any of 26 cycles.
4. **The false-zero mechanism is diagnosed: POSIX BRE, not ERE.** One cause
   explains all four catalogued instances (tick-526 alternation, v192 escaped
   `+`/`(`, and both polarities of false result). Proven three independent ways,
   including a BRE-specific error string and a combined `\(…\|…\)` query.
5. **Soundness of every recorded zero in the lineage is now decidable** — the
   overwhelming majority are plain substrings and are unaffected. This retires
   the standing worry that "dozens of prior ticks recorded vacuous searches."

**NOT established — load-bearing:** that anything compiles, links, runs, renders
or validates.

**Severity, stated without inflation:** this cycle **moves no pixel and clears no
acceptance gate.** Its audit half returned a negative result — nothing broken —
which is worth having precisely because the operator's build is the constraint
gating cards L/M/N. Its diagnostic half improves the *instrument*, not the
subject: it corrects a wrong rule, retires four symptom rows, and makes the
lineage's own evidence base decidable rather than doubtful.

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

**0 of 7 verified against the patched tree.** Gates 3/4 deliberately NOT carried
forward as PASS from the 2026-08-14 log: it describes a pre-v183 tree with 26
cycles of source change since.

## Verification attempt this tick

**Terminal is denied categorically.** Two shapes refused: compound
`pwd && date -u && ls -la Build.sh`, and a bare `true`. Both
`pending_approval / tirith:unknown / exit_code -1`. A refused no-op builtin rules
out command content, arguments, path, cwd and toolchain availability as causes —
the refusal is at the tool boundary.

No ad-hoc harness was written this tick. v207's went unrun and joined 9 prior
orphaned `/tmp/hermes-verify-*` artefacts that cannot be cleaned up; another
would produce no evidence and one more orphan.

**Operator command that clears the blocker:**

    ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test

## What this auditor did NOT do

Did not build, run, compile shaders, validate, or view any image. Did not commit,
push, or touch governance files. Did not fabricate any runtime result.
