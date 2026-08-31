# Pending Test Audit v204

- tests: docs/PENDING_TESTS_v204.md
- commit: docs/PENDING_COMMIT_v204.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-550)
- timestamp: 2026-08-30

## Broken-pattern audit

- [x] No from-x-import-y propagation bugs — N/A (C++/HLSL)
- [x] No test-bug-in-itself — re-ran rows 7, 12, 13, 14 myself
- [x] No source-incomplete-relative-to-test — every row names path and method
- [x] No missing isolation fixture — verifier is read-only
- [x] No AsyncMock/sync mismatch — N/A
- [x] No `|` alternation (tick-526)
- [x] No conclusion resting on `output_mode=count` alone (v198)
- [x] No `path`-at-a-file for a load-bearing negative (v199)
- [x] No count quoted from another marker — re-derived
- [x] Every zero controlled by a same-shape positive — rows 12/13, 5/7
- [x] No query pasted from a line that wraps (v196)
- [x] No absence asserted where a scope must be read (v198)
- [x] No conclusion resting on hits that are comments (v200) — **row 13 is
      exactly this pattern and the tester caught it**
- [x] No enumeration resting on a convenience wrapper (v201)
- [x] No "never used" claim resting on a symbol count (v202)
- [x] No comment-only diff accepted without reading the returned diff (v203)
- [x] **No no-op claim resting on two differently-named variables without
      closing their assignment sets (v204, new)**

## Independent re-derivation

**Row 14 re-run, because it is the row protecting the known-good control.**
The claim is that the shared-class patch is a byte-exact no-op for
`TestCornellBoxGI`. That rests on `GBufferWidth` (guide creation) equalling
`CurrentFBInfo.width` (dispatch). Two different names. I closed the assignment
set myself rather than accepting the tester's: `GBufferWidth =` returns exactly
2 assignments, `:521` from `Framebuffer->getFramebufferInfo().width` and `:1166`
from `CurrentFBInfo.width`. Both the framebuffer width, at init and at resize
respectively. The ratio is 1 by derivation at every extent, so `GB()` is the
identity map and the control's generated SPIR-V is additionally unchanged
because no `GB()` was added to its copy at all.

**Row 13 re-run** and I confirm the tester's correction. `GB(` over the control
directory returns 1 hit, in `ReSTIR_Spatial_cs.hlsl:31`, inside a pre-existing
comment. The per-directory form of this query is unsound for this claim; the
per-file form is sound and returns 0 for `BilateralDenoise_cs.hlsl`.

**Row 7 re-run.** `t_Input.Load` → 2 hits, both raw. This is the row that
distinguishes a correct fix from an over-applied one.

## New checklist row

Row 17 generalises what row 14 demonstrated. v191's defect was precisely two
quantities that agreed at startup and were nonetheless independent; the lineage
has repeatedly been saved by closing assignment sets rather than trusting
apparent equality. This cycle inverts it: a claim that a patch is a **no-op**
depends on two differently-named quantities being equal, and that direction is
just as dangerous — a false no-op claim perturbs the known-good control, which
is the artifact fifteen unbuilt cycles depend on for their exoneration.

The row is mechanical: **before asserting that a change is a no-op for some
consumer, close the assignment set of every variable the claim depends on. Two
names that agree today are not one quantity.**

## Per-row verdict

**17/17 KEEP.** Rows 7, 13 and 14 carry the cycle.

## What this cycle established, and what it did not

**Established (file-only, sound):**

1. The shared-pass-class domain of v202's invariant, which v203 left unswept,
   is now closed: `FReBLURPass` clean on both invariants, `FBilateralDenoisePass`
   clean on v202's and **defective on v183's**.
2. **Eleventh instance of the Phase-D extent class** — the bilateral denoise
   pass indexed full-res GBuffer guides with half-res dispatch coordinates, so
   all 25 taps of its 5x5 kernel weighted radiance against unrelated geometry.
3. It is the **first half-fix** in the class rather than an omission: v189
   fixed three of this call site's four operands and left the two guides, and
   the comment it left behind camouflaged the remainder.
4. The fix degrades to a byte-exact no-op for the known-good control **by
   derivation**, not by inspection.
5. `t_Input` correctly left in dispatch space — the over-application that would
   have regressed v189's fix.

**NOT established — load-bearing:** that anything compiles, links, runs,
renders or validates.

## Acceptance gates vs the job instruction (7 gates)

| # | Gate | Status | Basis |
|---|------|--------|-------|
| 1 | Debug target builds | **UNKNOWN** | `./Build.sh` denied (`tirith:unknown`) |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs | **UNKNOWN** | needs shell |
| 3 | No Vulkan VUID/ERROR | **UNKNOWN** | newest log 2026-08-14, predates v183 |
| 4 | No command-list errors | **UNKNOWN** | same caveat |
| 5 | `validate_restir_gi.py` newest group | **BLOCKED** | no shell, no python3 |
| 6 | Vision: recognizable Sponza | **BLOCKED (structural)** | no image tool in this runspace |
| 7 | Mode 20 non-zero `GBufferMaterial` | **UNKNOWN** | needs one operator run |

**0 of 7 verified against the patched tree.** Gates 3/4 deliberately NOT carried
forward as PASS from the 2026-08-14 log — that log describes a pre-v183 tree.

## Verification attempt this tick — AD-HOC, PARTIAL

An ad-hoc verification script was written to
`/tmp/hermes-verify-v204-guidescale.py` (not a project suite; no canonical
build/test command is reachable from this runspace). It contains two halves:

**A. Behavioural** — a Python mirror of `GB()` and of the pass's scale
derivation, asserting: primary scale is exactly 2; `GB()` reproduces
`Resolve_cs.hlsl:60`'s `hp*2+1` form across sampled coordinates; the maximum tap
`GB(399,299) = (799,599)` stays inside the 800x600 guide; the control's map is
the identity at four framebuffer extents; and three degenerate guards
(`GuideScale == 0`, null guide, zero dispatch) all fall back to identity rather
than collapsing or dividing by zero.

**B. Structural** — parses the three changed files and asserts the marker
claims mechanically.

**The script COULD NOT BE RUN.** `python3 <path>` was refused by tirith
(`pending_approval / tirith:unknown / exit_code -1`), as was a bare `pwd`
earlier in the same turn — the block is categorical, not command-specific, so
half A is **unexecuted** and none of its assertions may be cited. The temp file
could not be cleaned up for the same reason; it remains at
`/tmp/hermes-verify-v204-guidescale.py`.

**Half B was then carried out manually with file tools**, which is real evidence
rather than an assertion. Results, each from a direct read or query:

- the patched kernel was read **end-to-end** (`:71-131`), not sampled: the
  early-out, both center loads, the 5x5 loop, both neighbour loads, the
  neighbour bounds check and the store were each inspected in place
- primary `t_Depth.Load` → 2 hits, both `GB(...)`; `t_Normal.Load` → 2 hits,
  both `GB(...)`; `t_Input.Load` → 2 hits, both **raw** — fix applied, not
  over-applied
- the neighbour bounds check (`:103`) is evaluated in **dispatch space**, before
  `GB()` is applied — correct, since `outputSize` is the dispatch extent; a tap
  accepted there maps into the guide by `GB()` and the maximum accepted tap
  lands at (799,599) in an 800x600 guide, in bounds
- `u_Output[pixelCoord]` (`:131`) still indexed in **dispatch space** — the
  output is half-res like the dispatch, so the store was correctly not remapped
- primary `GB()` defined once, clamped by `max(int(GuideScale), 1)`
- control: no `GB(` in its `BilateralDenoise_cs.hlsl`; its three load pairs all
  raw — generated SPIR-V unperturbed
- **cbuffer layout compared field-by-field across both copies**: slots 0-4
  byte-identical (`TexelSize`, `DepthSigma`, `NormalSigma`, `SpatialSigma`),
  slot 5 is the guide-scale field in both, equal field counts — **no layout
  drift, the v182/v184 class is not engaged**
- `ConstantsData[5]` written once, into a `float[64]` array
- the C++ idiom `Handle->getDesc().width` on an `nvrhi::TextureHandle` is
  confirmed against same-codebase positive controls (`FGBufferFillPass.cpp:339`
  and `:422`; `FReBLURPass.cpp:155-156` on `Desc.OutputTexture`) — the one
  compile-risk claim checkable without a compiler

**This is ad-hoc structural verification, NOT suite green.** It does not
establish that the HLSL compiles under slangc, that the target links, or that
any pixel is correct. Gates 1-7 remain as tabled above: **0 of 7**.

## What this auditor did NOT do

Did not build, run, compile shaders, validate, or view any image. Did not
commit, push, or touch governance files. Did not fabricate any runtime result.
