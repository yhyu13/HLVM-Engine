# Pending Test Audit v210

- tests: docs/PENDING_TESTS_v210.md
- commit: docs/PENDING_COMMIT_v210.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-556)
- timestamp: 2026-08-30

## Broken-pattern audit

- [x] No from-x-import-y propagation bugs — N/A (C++)
- [x] No test-bug-in-itself — re-ran rows 1, 5 and 11 myself
- [x] No source-incomplete-relative-to-test — every row names path and method
- [x] No missing isolation fixture — verifier is read-only
- [x] No AsyncMock/sync mismatch — N/A
- [x] No ERE pattern against a BRE engine (v208)
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
- [x] No conclusion drawn from a query reporting `search_timeout` (v209)
- [x] **No enumeration accepted from a truncated file list (v210, new)**

## New row 19, and why this cycle forced it

The planner's first `GuideScale` query was scoped at
`Engine/Source/Runtime` and returned **`total_count: 0`** — for a symbol that
occurs **4 times** two directories down. The cause was neither syntax
(v208) nor timeout (v209): the response enumerated **50 files**, of which
most were `ThirdParty/stb` headers, and the real files were simply **not in
the enumerated set**. The pattern was right, the scope contained the target,
and the answer was still zero.

Re-scoped to `.../Public/Renderer/PostProcess` the same pattern returns
**4**. So the defence is not "write the pattern right" (v208) nor "read
`limit_reason`" (v209) — it is **read the enumerated file list and confirm
the file you care about is in it.** A zero from a query whose enumeration
never reached your target is a false zero of a sixth distinct kind.

The planner caught this and re-scoped rather than concluding. Had it not, the
cycle would have opened on the premise that `GuideScale` does not exist —
inverting the entire finding.

## Independent re-derivation

**Row 1 closed a second way.** The tester closed patch-presence by
`EXTENT CONTRACT` → 1 across the header directory. Necessary but weak: it
proves a string exists, not that it sits in the right struct or says
anything true. I read `FReSTIRPass.h:91-120` **in place**: the block is
inside `FTemporalDesc`, positioned immediately above `DepthTexture` /
`NormalTexture` (the guides it describes) and below the reservoir handles it
does not. Placement is correct, not merely present.

**Row 5 closed a second way.** `nvrhi::TextureHandle` → 26 proves a count,
and counts are not invariants — the standing lesson from v191. I read the
member sequence in place across the edit boundary: `CurrentReservoir0`,
`CurrentReservoir1`, `HistoryReservoir0`, `HistoryReservoir1`,
`CurrentRadiance`, `HistoryRadiance`, [comment], `DepthTexture`,
`NormalTexture`, ... — the comment was **inserted between** members, not
over one. Set preserved, not just cardinality.

**Row 11 re-run and strengthened.** The tester used `BindingSetItem::` → 29.
I ran a second, narrower shape: `Texture_SRV` → **42** in the same file. Two
independent counts on the initialiser lists the v203 near-miss damaged, both
consistent with an untouched file. The "0 functional lines" banner is
falsifiable and passes under both.

## Per-row verdict

**12/12 KEEP.** Rows 11 and 12 carry the cycle — row 11 because it is the
only route by which this edit could have caused harm, row 12 because it
correctly classified a dual-copy divergence as *correct* rather than carding
it as a defect.

## The reviewer's dual-copy finding — upheld, and its restraint is the point

The reviewer found that the control's shader declares `GBufferScale` but has
no `GB()` helper, and **declined to card it**. Verified independently:
`GB(` → 0 there against `Load` → 9 same-file control, and the control sets
`GBufferScale = 1.0f` at `TestCornellBoxGI.cpp:1592`/`:1645`, for which
`GB()` is the identity map. The field is declared to keep the cbuffer tail
aligned across copies (v184/v186/v200) while the helper is correctly absent.

After 210 cycles in a thinning seam, the gradient is toward carding anything
that looks asymmetric. **Not carding a correct asymmetry is the harder and
right call**, and it is the same judgement v206 made when it found two
sibling classes holding opposite guide contracts.

## What this cycle established, and what it did not

**Established (file-only, sound):**

1. `FReSTIRPass`'s guide-extent contract — the third of three distinct
   sibling contracts and previously the only undocumented one — is now
   stated at both `FDesc` declarations, with every factual claim in it
   verified against the marshaller, both shader copies, and both consumers.
2. The two existing sibling contracts (v205, v206) are now cross-referenced
   from the third, closing the trap that a reader arriving from either
   header carries the wrong invariant in.
3. Both consumers set `GBufferScale` correctly today; **there is no defect**.
4. A sixth false-instrument mechanism (truncated enumeration) is diagnosed
   and ruled on.

**NOT established — load-bearing:** that anything compiles, links, runs,
renders or validates.

**Severity, without inflation:** this cycle **moves no pixel and clears no
acceptance gate.** It is documentation of a currently-satisfied contract.

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
with 28 cycles of source change since.

## Verification attempt this tick

Terminal probed first-hand at the start of this tick rather than inherited
from the lineage: `true; echo PROBE_OK; pwd` → `pending_approval /
tirith:unknown / exit_code -1`. A refused no-op builtin rules out command
content, arguments, path and toolchain; the refusal is at the tool boundary.

**Operator command that clears the blocker:**

    ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test

## What this auditor did NOT do

Did not build, run, compile shaders, validate, or view any image. Did not
commit, push, or touch governance files. Did not fabricate any runtime result.
