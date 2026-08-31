# Pending Test Audit v212

- tests: docs/PENDING_TESTS_v212.md
- commit: docs/PENDING_COMMIT_v212.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-558)
- timestamp: 2026-08-30

## Broken-pattern audit

- [x] No from-x-import-y propagation bugs — N/A (HLSL/C++)
- [x] No test-bug-in-itself — re-ran rows 6, 7, 9 and 12 myself
- [x] No source-incomplete-relative-to-test — every row names path and method
- [x] No missing isolation fixture — verifier is read-only
- [x] No AsyncMock/sync mismatch — N/A
- [x] No ERE pattern against a BRE engine (v208)
- [x] No conclusion resting on `output_mode=count` alone (v198)
- [x] No `path`-at-a-file for a load-bearing negative (v199)
- [x] No count quoted from another marker — all re-derived
- [x] Every zero controlled by a same-shape positive (v205)
- [x] No absence asserted where a scope must be read (v198)
- [x] No conclusion resting on hits that are comments (v200) — **see caveat**
- [x] No "never used" claim resting on a symbol count (v202)
- [x] No comment-only diff accepted without reading the returned diff (v203)
- [x] No zero believed without reading the query's `error` field (v205)
- [x] No linter output dismissed without mapping each line to a cause (v207)
- [x] No conclusion drawn from a query reporting `search_timeout` (v209)
- [x] No enumeration accepted from a truncated file list (v210)
- [x] No cardinality claim inherited across cycles without re-derivation (v211)
- [x] **No domain treated as swept without partitioning the enumeration (v212, new — row 21)**

## Row 21 as adopted — narrowed per the impl review, and the narrowing matters

The planner proposed: *re-derive the domain's membership, not only each
member's count.* The reviewer objected that this is unbounded — a domain can
always be widened one level (file → directory → class → repository), and a rule
with no stopping condition licenses infinite regress, which in a thinning seam
is indistinguishable from drift. The reviewer forwarded a narrowed form rather
than silently applying it (correct, per v195).

**I adopt the reviewer's form**, with the stopping condition made explicit:

> *A domain is swept only when it was derived from an enumeration that was
> actually run **and partitioned**, with both the query and the partition
> stated in the marker. You stop when the enumeration is exhausted — not when
> the groups you already knew about are checked.*

The stopping condition is what makes it a rule rather than an anxiety. This
cycle's own sweep satisfies it: 205 files enumerated → vendor trees excluded →
partitioned by basename → 9 first-party multi-copy groups → each determined.

**Why the row was needed:** v211 named 4 groups; the v212 draft named 7 while
citing v211's row 20 as the reason it was being careful; the true count is 9.
Both cycles re-derived the *count of every group they had already named*, which
is what row 20 literally asks for, and neither re-derived *which groups exist*.
The enumeration was run and quoted in both cases. It was never partitioned.

Row 20 is about a number. Row 21 is about a set. v191's audit put it as
"counts are not invariants, sets are" — two cycles later that lesson had to be
learned again one level up.

## Independent re-derivation

**Row 7 (the partition) closed a THIRD way, from a direction neither the plan
nor the tester used.** Both used `MRT4 : SV_TARGET4` (4) and
`Tangent : TEXCOORD3` (5 → 2 PS + 3 VS). Those are structural. I ran two more:

- `MRT3 : SV_TARGET3` → **7 hits**, and the *comment text* splits the families
  cleanly: `Emissive RGBA` at `:38` in the four RT copies, `Emissive RGB` at
  `:36` in the two PBR copies. Different text, different line number, exactly
  4/2. The seventh hit is `GBufferPT_PS.hlsl:64` (`LinearDepth (R32F)`) — the
  singleton, correctly outside both families.
- `cbuffer MaterialConstants : register(b1)` → **exactly the 2 PBR copies**
  (`TestSponzaDeferred_Data`, `TestGPUInstancing_Data`) and no RT copy.

So the partition now holds under **four** independent discriminators — MRT
count, vertex-input tangent, comment text, and cbuffer presence — three of them
disjoint in shape. That is materially stronger than the two the tester ran.

**Caveat honestly flagged against v200's row:** the `MRT3` discriminator
partitions on *comment text*, and v200's rule warns against conclusions resting
on comment hits. I am not resting the partition on it — it is the fourth of
four, and the other three are structural declarations. Noted rather than
hidden, because a reader scanning the row list would otherwise see a v200
violation.

**Row 12 (zero source modified) — accepted, and I re-tested it the same way.**
`GuideScale` in both v211-patched copies → 3 hits each, at slot 5, the
degradation comment, and `max(int(GuideScale), 1)`. Undisturbed. The tester's
controlled positive (five successful marker writes this tick, each returning a
diff) is the right control for a negative and I accept it.

**Row 9 re-read in place**, not by query: `GBufferPT_PS.hlsl` → 1 file;
`:63` declares `MRT2 : SV_TARGET2 // Material (RGBA32F)`, `:75` selects a real
texture sample by `MaterialFlags` bit 0, `:77` stores `float4(albedo, Roughness)`.

## Per-row verdict

**12/12 KEEP.** Rows 7, 9 and 12 carry the cycle:

- **Row 7** because it is the only route by which this cycle could have caused
  harm — a wrong reading there produces four wrong shader patches against an
  unbuilt chain, surfacing at the operator's first build and being
  misattributed to v183-v212.
- **Row 9** because it is the only row that bears on the job instruction's
  actual card.
- **Row 12** because a determination cycle's entire claim is a negative.

## The restraint is the finding

The lineage has now closed 212 cycles; 10 of the last 12 produced a patch. The
gradient toward producing a thirteenth was unusually strong here, because six
copies of one filename *reads* as five stale siblings and the remedy would have
been mechanical. It was wrong. Reading all six in full before forming a
hypothesis is what caught it, and that is the third time in this lineage
(v196, v206, now v212) that the correct output was no patch and it took active
resistance to produce.

## What this cycle established, and what it did not

**Established (file-only, sound):**

1. v182's dual-copy domain is **closed at nine groups**, all determined — the
   invariant discovered 30 cycles ago and cited eleven times as a reason to
   avoid touching shaders has finally been swept end-to-end.
2. The domain's two largest groups are **two coherent variant families**, not
   staleness — preventing four wrong patches.
3. The job instruction's card is refuted at the **producer** end
   (`GBufferPT_PS.hlsl`, singleton, MRT2 written from a real texture sample),
   completing the four-link chain producer → transport (t-527) → consumer
   (t-528) → probe (v182's half-res bug, which explains the original black
   mode-20 reading that started the card).
4. A twenty-first audit row, narrowed at the impl gate and given a stopping
   condition here.

**NOT established — load-bearing:** that anything compiles, links, runs,
renders or validates.

**Severity, without inflation: latent. This cycle moves no pixel and clears no
acceptance gate.**

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

**0 of 7 verified against the current tree.** Gates 3/4 are deliberately NOT
carried forward as PASS from the 2026-08-14 log: it describes a pre-v183 tree
with 30 cycles of source change since.

## Verification attempt this tick

`terminal command="pwd && ls -la | head -20 && date"` — a no-op builtin plus
two read-only commands — refused with
`pending_approval / tirith:unknown / exit_code -1 / allow_permanent: true`.
The refusal is at the tool boundary, not command-dependent; this is consistent
with ≥558 refusals across the lineage in every invocation shape tried
(foreground, background, absolute path, no-argument builtin, alternate workdir).

**Two structural blockers, orthogonal:** (a) terminal denied at the tool
boundary; (b) no vision/image tool exists in this runspace at all, so gate 6 is
unreachable even if shell were granted.

## Operator command that clears the blocker

    ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test

This single command unblocks gates 1-5 and 7, the three deferred cards
(L, M, N), and re-runs `Common_ShaderMake` — without which v211's fix stays
inert because the stale blob is still on disk.

## What this auditor did NOT do

Did not build, run, compile shaders, validate, or view any image. Did not
commit, push, or touch governance files. Did not fabricate any runtime result.
