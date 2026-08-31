# Pending Test Audit v218

- tests: docs/PENDING_TESTS_v218.md
- commit: docs/PENDING_COMMIT_v218.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-566)
- timestamp: 2026-08-21

## Broken-pattern audit

- [x] No `|` alternation in any pattern (tick-526)
- [x] No `file_glob` in any load-bearing query (v217)
- [x] Every load-bearing zero paired with a same-shape, same-scope positive control (v217)
- [x] No project-root-scoped query — the false-partial-result class (v217)
- [x] No count inherited across markers without re-derivation (v211)
- [x] No conclusion resting on `output_mode=count` alone (v198) — row 18's control is the only `count`
      query this cycle, and it supports a zero rather than carrying a conclusion
- [x] No absence asserted where a scope must be read (v198) — row 16 is reported as a classification of
      four read hits, not as a count
- [x] No runtime result fabricated — nothing built, run, or viewed by any role this cycle

## Rows I re-ran rather than read

**Row 20 (carries the cycle).** `path=FGIPass.cpp pattern="waitForIdle"` → **3 hits: `:177` comment,
`:197`, `:441`.** `PENDING_COMMIT_v214.md:10` predicts "exactly 1 hit at line 415". Reproduces:
v214's verify command returns a false failure against a correct tree.

**Row 5 — the tester asserted it "by enclosing scope" without exhibiting the bounds, so I derived
them.** `void FGIPass::DispatchRays` begins at **`:533`** and its dispatch call is at `:733`;
`void FGIPass::Shutdown` begins at **`:205`**. Therefore `:196`/`:197` sit in `Initialize` (before
`:205`) and `:440`/`:441` sit in `Shutdown` (after `:205`, and 93 lines before `DispatchRays` opens).
**Neither pair is inside the per-frame path.** v214's functional intent holds. This is the row that
would have hidden a regression if taken on faith, and it was the weakest-evidenced row in the set.

**Row 11's control.** `GuideScale_Unused` → 1 against `GuideScale` → 16 in the same scope; the scope
completed.

## Per-row verdict

**20 PASS / 1 MISMATCH — all 21 KEEP.** The mismatch is row 20, and it is a *finding*, not a broken
test: the row is correctly constructed, correctly scoped, and its disagreement is with a closed
cycle's marker rather than with the tree. Rows 3, 5, 16 and 20 carry the cycle.

- **Rows 7-10** are the rows that justify the cycle existing. They are the two classes this lineage has
  proven are silent — a cbuffer whose tail desyncs compiles clean and renders wrong, and a shader copy
  that diverges is invisible until someone diffs three files. v200 checked them for v183-v199; nobody
  had checked them for the 18 cycles since, which include the only new cbuffer field in the lineage.
- **Row 10 is the one a naive audit gets wrong.** Checking slot *position* passes; checking field
  *name* would flag `GuideScale_Unused` as a divergence and send a cycle to "fix" a deliberate,
  documented design. The plan gate's correction is what prevented that.
- **Row 16** is reported as "4 hits, 0 calls". A tick that recorded only the count would read it as
  "the symbol is used" and never open card S.

## What this cycle established, and what it did not

**Established (file-only, controlled):**
1. The v200-v217 delta is **compile-coherent in both silent classes** — cbuffer layout agrees four-way
   on both ReSTIR structs, and the new `GuideScale` slot agrees three-way by position across all three
   `BilateralDenoise_cs.hlsl` copies.
2. v209's deletion is complete (0, controlled at 5) and v214's move is coherent — the stall was
   relocated to `Initialize`, not deleted, and is provably outside `DispatchRays` by derived bounds.
3. **Card S**: the shared shader's copy-selection comment names a mechanism with zero call sites, and
   that mechanism resolves only `BlitVS`/`BlitPS`. Wrong in two independent ways. Not a defect; a trap
   positioned exactly where someone decides which of three copies to edit.
4. **Card T**: v214's own `verify:` command returns a **false failure** on a correct tree. Per v192, a
   false failure outranks a false pass — it sends an operator to re-patch code that is already right.

**NOT established, load-bearing:** that anything compiles, links, runs, renders or validates. The
v183-v218 chain remains unbuilt and unexecuted. This audit lowers the probability that the operator's
first build fails silently; it is not a substitute for the build.

## Acceptance gates vs the job instruction: 0 of 7 (unchanged)

| # | Gate | Status | Basis |
|---|---|---|---|
| 1 | Debug target builds | UNKNOWN | `terminal` refused; even `true` → `pending_approval` |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` | BLOCKED | same |
| 3 | No Vulkan VUID/ERROR | UNKNOWN | newest log 2026-08-14, predates 35 unbuilt cycles |
| 4 | No command-list errors | UNKNOWN | same |
| 5 | `validate_restir_gi.py` newest group | BLOCKED | same |
| 6 | Vision: recognizable Sponza | BLOCKED (structural) | no image tool in this runspace |
| 7 | Mode 20 non-zero `GBufferMaterial` | UNKNOWN | needs one approved run |

## What this auditor did NOT do

Did not build, run, compile, validate, or view any image. Did not commit, push, or modify any engine
source or governance file. Did not fabricate any runtime result.
