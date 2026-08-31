# Pending Test Audit v197

- tests: docs/PENDING_TESTS_v197.md
- commit: docs/PENDING_COMMIT_v197.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-543)
- timestamp: 2026-08-30

## Broken-pattern audit

- [x] No from-x-import-y propagation bugs — N/A (C++)
- [x] No test-bug-in-itself — I re-executed rows 1, 5, 10, 12 myself
- [x] No source-incomplete-relative-to-test — every row names path and query
- [x] No missing isolation fixture — verifier is read-only
- [x] No AsyncMock/sync mismatch — N/A
- [x] No `|` alternation (tick-526)
- [x] No conclusion resting on `output_mode=count` alone
- [x] No count quoted from another marker — re-derived
- [x] Every zero controlled by a same-shape positive
- [x] No query pasted from a line that wraps (v196)
- [x] No query encoding an unverified declaration form (v197, new)

## Independent re-derivation

**Row 1, with context — the row the whole cycle rests on.** Re-ran
`RenderGBuffer()` with surrounding lines: `:804` `RenderGBuffer();` and `:2179`
`void RenderGBuffer()`. Both sites carry the new shape; the call and the
definition agree on arity. **This is the row that decides whether the file
compiles**, and it is sound: a `2` where the pre-patch value was `0`, in a file
where the identifier demonstrably matches.

**Row 5, read rather than counted.** `:2445-2446` reads
`HLVM_LOG(..., "RenderGBuffer frame {}: drew {} meshes, viewport {}x{}"),
FrameCount, MeshCount, WIDTH, HEIGHT);` — **four format placeholders, four
arguments.** I checked the arity of the *format string* and not merely the
presence of the token, because an argument-count mismatch in a `fmt`-style macro
is exactly the sort of thing a substitution row would wave through. It balances.

**Row 10, the no-regression row.** `nvrhi::Viewport Vp` → 1 hit, `:2353`,
`(0.f, float(WIDTH), 0.f, float(HEIGHT), 0.f, 1.f)` — **byte-unchanged from
v195.** The cycle edited the log that *describes* the viewport without touching
the viewport itself, which is the correct direction: the log was wrong, the
viewport was right.

**Row 12/13/14, the control set.** `v197` → 3 in the target, **0** in
`TestPathTraceGI.cpp`, **0** in `GIPathTracing.hlsl`, and 4 marker files in
`docs/`. The zeros are controlled twice over. v196's protection of the known-good
control survives this cycle intact, and neither shader copy was touched, so the
v182 dual-copy hazard is not engaged.

## No defect found in the test marker

This is the first cycle since v191 where I found nothing wrong with the tester's
rows. v196's row 11 cited a query that could not return what it claimed; v195's
had stale line numbers. Here every row I re-ran returned what it said, and the
one row built on a corrected query (row 7) **documents its own correction in the
marker rather than quietly presenting the fixed version.** That is the behaviour
the previous two audits asked for, appearing without being asked again.

## The finding this cycle adds, and why it outranks the four before it

The impler hit `constexpr uint32_t WIDTH` → **0** and did not treat it as a
finding. The declaration is `static const uint32_t WIDTH = 800;`.

Four false-zero mechanisms were already on record — alternation (tick-526),
escaped metacharacters (v192), unescaped metacharacters (v196), line wrap (v196).
**All four are properties of `search_files`.** This fifth one is not: the tool
behaved correctly and answered the question asked. The question was wrong.

> **A query that encodes an assumption about the code returns zero when the
> assumption is false, not when the code is absent.** Escaping rules and
> line-wrap rules cannot catch this class, because nothing about the query is
> malformed. The only defence is to widen until the query returns something,
> then narrow.

This is the card-E/G/H over-claiming failure mode expressed as a query rather
than as prose, which makes it the sixth appearance of the same underlying fault
and the first time it has shown up in the *verification* layer rather than in a
card's description. **Where the earlier instances produced a wrong plan that a
gate could catch, this one produces a wrong fact that every downstream gate would
inherit** — the impler would have reported "WIDTH may not be in scope," the
reviewer would have re-derived from the same wrong premise, and the cycle would
have carded a non-existent problem. It was caught only because the impler widened
before believing the zero.

## Per-row verdict

**14/14 KEEP.** No row is padding; no row is unsound. Rows 8 and 9 are the two I
would have added had the tester omitted them — they are the only rows that check
for damage *outside* the intended edit, and row 9 is the one that answers card I's
standing question for the primary target.

## What this cycle established, and what it did not

**Established (file-only, sound):**

1. `RenderGBuffer` takes no parameters and its single caller passes none; the
   caller set is closed at 2 tree-wide, so the arity change is complete.
2. The log at `:2446` now reports the same extent the viewport at `:2353` uses;
   format placeholders and arguments balance at 4.
3. `WIDTH`/`HEIGHT` are file-scope `static const uint32_t`, in scope at the log
   site and identically typed to what they replace.
4. `LastWidth`/`LastHeight` retain live uses and do not become unused.
5. **The primary target's live `FB.width` set is now exactly its three intentional
   sites** — resize detect, `LastWidth` assign, blit destination. Card I's
   question is answered for this file: the enumeration is clean.
6. Neither the known-good control nor either shader copy was touched.

**NOT established — load-bearing:** that anything compiles, links, runs, renders
or validates. **No build, no run, no image. 0/7 acceptance gates.**

## The lesson this cycle adds

The running lesson gains a tenth variant, and it is the first about a *fix* rather
than a card:

- v196: the card was right to refuse to guess; the answer was "no defect."
- **v197: the card was right about the defect and right that the honest remedy
  was a signature change rather than a substitution — and the plan gate found a
  worse defect three lines away that the card had not seen.**

Card K described an inert trap: unused parameters at a live call site. True, and
worth fixing. But `:2446` was a **log line reporting the wrong extent**, left
behind by the same v195 fix, in a lineage where log lines *are* the evidence.
Card K looked at the signature and stopped. **The cheapest place to find the
second defect was inside the function the first one pointed at** — which is an
argument for gates that re-read the neighbourhood rather than only the cited
lines, and the reason this cycle's plan review returned FIX rather than KEEP.

Second: **nine substitution cycles, then v196 correctly changed nothing, then
v197 correctly changed a signature.** The pipeline has now produced all three
outcome shapes. The gradient toward "make it a substitution" that v196 had to
resist appeared again here — substituting `WIDTH`/`HEIGHT` into the unused
parameters would have satisfied a `FB.width` sweep while preserving the exact
misleading appearance the card was about — and the plan refused it in advance,
naming the reason. That is the discipline generalising rather than being
re-derived each cycle.

## Post-audit amendment — comment cleanup (same 3 functional lines)

After the audit closed, a self-review pass against the KISS/DRY bar found the
diff carried **29 comment lines for 3 code lines**, restating the v195/extent
rationale at all three sites. Prose duplication is still duplication: three
copies drift, and a reader who fixes one leaves two stale. Reduced to **14**,
with the rationale stated once at the definition and two-line pointers at the
call site and the log.

Re-ran the load-bearing rows against the tightened file — **all still pass**:
`RenderGBuffer()` → 2 (`:795`, `:2173`); `MeshCount, WIDTH, HEIGHT` → 1
(`:2435`); `v197` → 3; viewport `:2364` byte-unchanged. Line numbers shifted by
the comment removal, which is why they were re-derived rather than carried
forward — the v195 audit's lesson that **counts are not invariants, sets are**.

**No functional line changed in this amendment.** Verdict stands at ALL_KEEP.

## Acceptance gates vs the job instruction (7 gates)

| # | Gate | Status | Basis |
|---|------|--------|-------|
| 1 | Debug target builds | **UNKNOWN** | `./Build.sh` denied (`tirith:unknown`) |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs | **UNKNOWN** | needs shell |
| 3 | No Vulkan VUID/ERROR | **UNKNOWN** | newest log 2026-08-14, predates v183-v197 |
| 4 | No command-list errors | **UNKNOWN** | same caveat |
| 5 | `validate_restir_gi.py` newest group | **BLOCKED** | no shell, no python3 |
| 6 | Vision: recognizable Sponza | **BLOCKED (structural)** | no image tool in this runspace |
| 7 | Mode 20 non-zero `GBufferMaterial` | actionable since v182 | needs one operator run |

**0 of 7 verified against the patched tree.** Gates 3/4 deliberately NOT carried
forward as PASS from the 2026-08-14 log: that log describes a pre-v183 tree.

## What this auditor did NOT do

Did not build, run, compile shaders, validate, or view any image. Did not commit,
push, or touch governance files. Did not fabricate any runtime result.
