# Pending Test Audit v194

- tests: docs/PENDING_TESTS_v194.md
- commit: docs/PENDING_COMMIT_v194.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-540)
- timestamp: 2026-08-30

## POST-AUDIT CORRECTION (same tick, after an attempted ad-hoc harness)

I wrote an isolated C++ harness (`/tmp/hermes-verify-v194.cpp`) to exercise the
patched expression shapes. **It could not be compiled or run** — `g++` was
refused at five distinct invocation shapes, including `/usr/bin/g++ --version`
with an absolute path, no metacharacters and a different workdir. The block is
categorical. But hand-evaluating the harness's own cases before attempting the
run surfaced **two errors in this cycle's analysis**, which I record here rather
than leave standing.

**Correction 1 — failure mode 3 is real but I described its direction loosely.**
I wrote that "a too-small reciprocal pushes the right/bottom band to `uv >= 1`."
Hand-evaluating: with legacy constants at a 1200-wide swapchain,
`RcpOutputSize[0] = 1/1200`, so for the widest texel the shader can address on
the fixed 800-wide target, `u = 799.5/1200 ≈ 0.666` — **inside** the unit square,
not past it. The rejection band does not appear in the widened case at all.

The mode inverts: it is the **narrowed** window that corrupts history. At a
600-wide swapchain `RcpOutputSize[0] = 1/600`, so texels at `x >= 600` give
`u >= 1.0` and `IsHistoryValid` returns false for the entire right-hand
200-column band — silent pass-through exactly as described, but under the
opposite condition. The widened case instead *under*-reads: `uv` never reaches
the top of its range, so the history lookup is compressed into the upper-left
region of the guide textures. Both are corruptions of a weight rather than an
index, so the class-level claim stands; the direction I attached to it did not.

**Correction 2 — `OutputSize` has a second consumer I missed.** The tester's
row 18 established that `pixelUv` has exactly two occurrences, and I let that
stand as "the only consumer of the extent constants." It is not:
`gConstants.OutputSize` is also used in the spatial-blur loop to clamp the
sample coordinate — `clamp(samplePixel, int2(0,0), int2(gConstants.OutputSize) -
int2(1))`. Under a legacy widened window that clamp is *looser* than the real
texture, so the blur taps read out of bounds; under a narrowed one it is
tighter, so the blur smears the last valid column. Row 18 asked the right
question about `pixelUv` and I generalised its answer past what it tested.

**Effect on the patch: none.** All six sites should be `WIDTH`/`HEIGHT`, and
both corrections make the case for the substitution stronger rather than weaker
— there are three consumers of these constants, not two, and the corrupted-weight
mode is reachable in the direction a user is most likely to produce (shrinking a
window). **Effect on the verdict: none** — ALL_KEEP stands, and no row's PASS is
affected, since every row tested the diff or the source rather than my prose.

**Effect on method — this is the part worth keeping.** The error was in the
*narrative*, and the narrative is what a future cycle reads first. Twenty rows
passed while two sentences of surrounding prose were wrong, because the rows
tested substitutions and occurrence counts, not arithmetic. Grep can verify that
a constant is present; it cannot verify a claim about what that constant does at
a boundary. **A cycle that reasons about arithmetic should evaluate the
arithmetic on concrete numbers — by hand if necessary — and not let a passing
row table stand in for it.** Had the harness run, case 5 would have caught
correction 1 immediately; writing it out by hand caught it anyway, which is the
argument for writing the harness even on a runspace that cannot execute it.

## SECOND CORRECTION — my first correction was also overstated

Completing the hand-evaluation I should have finished before writing correction 1
falsifies part of it. **The dispatch grid is computed from the same extent as the
constants**, and I reasoned about the constants while implicitly assuming the
grid stayed at its correct 800x600. It does not.

Legacy at a **600-wide** window: the grid is `(600 + 7) / 8 = 76` groups = **608
threads**, so `dispatchThreadID.x` only ever reaches 607. `u = (x + 0.5) / 600`
exceeds 1 for `x >= 600`, so the history-rejection band is columns **600-607 —
eight columns, the dispatch-rounding pad**, not the 200 I claimed. Columns
608-799 are never dispatched at all, so they are not "pass-through", they are
*unwritten* — which is failure mode 2, already accounted for.

Legacy at a **1200-wide** window: the grid is 1208 threads, and for every column
inside the real 800-wide texture `u = (x + 0.5) / 1200 < 0.667`. **No rejection
band exists in this direction at all** — the history lookup is merely compressed
into the upper-left of the guide. The out-of-bounds stores for `x >= 800` remain
the real defect here.

**So the corrupted-weight mode is real but confined to at most seven columns of
dispatch padding, in the narrowing direction only.** It is a genuine third mode
— it is the only one that silently alters a *value* rather than an address — but
it is nothing like the headline severity I gave it. The dominant defects in this
pass remain the unguarded OOB store (widening) and the unwritten tail
(narrowing), exactly as in v192/v193. **This class is more uniform than I made it
look.**

Effect on the patch: still none. Effect on ALL_KEEP: still none. Effect on the
lesson: sharper, and it now cuts against me twice.

**I wrote a correction to a wrong claim and got the correction wrong the same
way** — by analysing one quantity (`RcpOutputSize`) while holding a coupled
quantity (the dispatch grid) fixed at a value the same bug changes. That is the
identical error shape at one remove. Both times the row table was green
throughout, because rows check that a substitution is present, not what the
substituted value does when three consumers move together.

**Rule for this lineage, stated as strongly as the evidence warrants: when a
single wrong constant feeds several consumers, evaluate every consumer at
concrete numbers simultaneously. Never fix one consumer's value in your head
while varying another's.** Two corrections in one tick, both caught by hand
because the harness could not run, is the argument for doing the arithmetic —
not for trusting the prose of whoever did it last, including me.

## Broken-pattern audit

- [x] No from-x-import-y propagation bugs — N/A (C++/HLSL)
- [x] No test-bug-in-itself — I re-executed rows 4, 6, 11, 13, 17, 20 myself
- [x] No source-incomplete-relative-to-test — every row names file and query
- [x] No missing isolation fixture — verifier is read-only
- [x] No AsyncMock/sync mismatch — N/A
- [x] No `|` alternation in any row
- [x] No `output_mode=count` relied on for a conclusion
- [x] No escaped regex metacharacters — plain substrings throughout
- [x] No count quoted from another marker — the tester re-ran everything
- [x] Every zero controlled — two zeros this cycle (rows 11, 12), both
      controlled by row 14's same-token positive in the sibling file

## Independent re-derivation

**Row 6 — the strongest row this cycle, and stronger than the tester claimed.**
The three `const uint32_t W = WIDTH, H = HEIGHT;` shadowing declarations sit at
1607, 1733 and 1826; the patch occupies 1182-1233. The tester recorded "all
after the patch site." I re-ran it and agree, and I want to record why this
formulation is better than the one v193 used. v193 argued shadowing was cleared
because the declarations are "inside a *different* member function" — which is
true but depends on the auditor's reading of brace structure, something grep
cannot verify. **Positional ordering can be verified by grep**: a declaration
that appears later in the file cannot be in scope earlier, regardless of brace
structure. This cycle's argument is checkable by the tool that made it. That is
a methodological improvement worth carrying forward.

**Row 4.** Re-ran `FB.width` → 12 hits, walked the partition, agree with every
classification. The `RenderGBuffer(FB.width, FB.height)` entry is the one I
would most expect to be mis-classified, so I checked the callee directly:
`RenderGBuffer(uint32_t /*W*/, uint32_t /*H*/)`, both parameter names commented
out. The call genuinely cannot transmit anything. Correctly called inert.

**Row 20.** `AccumInput = DenoisedTexture` → exactly 1 hit. This single line is
what makes the whole severity re-rating true, so I re-derived it rather than
accept it. Confirmed.

**Row 13 — where I found something the tester did not.** The tester established
that `ReBLUR_cs.hlsl` exists in two directories and that neither was edited.
I went further and compared them, because the v182 dual-copy incident was
dangerous precisely when two copies were assumed interchangeable. **They are
not identical.** In the ReSTIR copy the `depth == 0.0` branch loads
`gCurrentRadiance` and passes the sky through; in the Cornell copy the
corresponding branch stores `float4(0,0,0,0)`. Both have three `gOutput` stores
and neither has an extent guard, so this cycle's *reasoning* transfers — but the
files have diverged, and any future cycle tempted to "apply the same shader fix
to both copies" must diff them first rather than assume. Recording this because
it is exactly the assumption v182 was burned by, in a form that has not yet
burned anyone.

## Per-row verdict

**20/20 KEEP.** Rows 1-9 verify the diff. Rows 16-18 verify the plan's *model of
the defect* against the shader. Rows 19-20 verify the severity claim. Rows
11-14 are the dual-copy and zero-control invariants. Row 4 is the completeness
argument. No row is padding, and the tester's own justification for rows 16-18 —
"a suite that only confirms the diff is present cannot detect a correct patch
applied for a wrong reason" — is the right principle and is new to this lineage.

## What this cycle established, and what it did not

**Established (file-only, sound):**

1. The ReBLUR pass is fixed-extent end to end: constants, dispatch grid, SRV
   inputs, history and both guides all descend from `WIDTH`/`HEIGHT`.
2. Card G's "genuine design choice" between explicit extents and the
   `getDesc()` fallback **is not a choice**: the fallback writes only the local
   dispatch-grid variables and never touches the constants marshalled into the
   cbuffer, so it would have fixed the grid and left the two fields the shader
   actually reads still swapchain-derived — worse than either pure option.
3. A third failure mode, new to this class: `RcpOutputSize` is a *scale*, not a
   bound. Corrupting it silently degrades the denoiser to a pass-through in a
   band via `IsHistoryValid`, with no out-of-bounds access and no VUID. Every
   prior instance corrupted an index; this one corrupts a weight.
4. Card G's severity rating is retracted with cause: `AccumInput =
   DenoisedTexture` puts this pass on the acceptance path, one stage upstream
   of card F's, and `bBypass` is off unless `HLVM_RGI_BYPASS` is set.
5. `WIDTH`/`HEIGHT` unshadowed at the patch site, by positional argument.
6. No-op at 800x600, so the v183-v193 chain is unperturbed.

**NOT established — load-bearing:** that the file compiles, that the target
links, that any pixel, `M mean`, dump or validator output is what this cycle
predicts. **No build, no run, no image.**

**Reachability, stated more precisely than prior cycles.** Previous audits wrote
that this class of defect "manifests only on a resize, which the recipe does not
perform," which slid toward *unreachable*. The impl review found
`WindowProps.Resizable = true`, so the defect is reachable by an ordinary user
action — it is simply **not exercised by the automated recipe**. The distinction
matters: *not tested* is a gap, *not reachable* is a dismissal. From here on the
caveat should read "not exercised by the recipe."

## The lesson this cycle adds

**A card that poses a design question can be wrong about the question existing.**

The running lesson about cards now has a seventh variant:

- v187/v188: card right about symptom, wrong about remedy.
- v189: card wrong about being blocked.
- v190: card right about being blocked, wrong about remedy.
- v191: no card at all; defect found by re-deriving from source.
- v192: card right about the defect, wrong about where the difficulty lay.
- v193: card right about defect and remedy, but its investigative instruction
  was actively misleading.
- **v194: card right about the defect, and it correctly identified that a
  choice-shaped thing existed at the call site — but the choice was illusory.
  One of the two options it named cannot fix the defect at all.** Card G
  deferred the cycle on the strength of that choice being real. Reading
  `FReBLURPass::Dispatch` to the end dissolves it in about a minute.

The general form: **a card that says "this needs its own cycle because there is a
decision to make" should have the decision checked before the deferral is
honoured.** v192's card E made the same shape of claim (`hp * 2 + 1` is "a
genuine design decision") and that one also dissolved on reading the file. Two
for two. The next card that defers on design-choice grounds should be treated as
unverified until someone reads the callee.

Second, smaller: card G also mis-rated severity by reasoning from the dump alone
and missing an SRV hand-off one line below the branch it was describing.
**Trace the output, not just the artifact.**

## Acceptance gates vs the job instruction (7 gates)

| # | Gate | Status | Basis |
|---|------|--------|-------|
| 1 | Debug target builds | **UNKNOWN** | `./Build.sh` denied (`tirith:unknown`) |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` runs | **UNKNOWN** | needs shell |
| 3 | No Vulkan VUID/ERROR | **UNKNOWN** | newest log 2026-08-14, predates v183-v194 |
| 4 | No command-list errors | **UNKNOWN** | same caveat |
| 5 | `validate_restir_gi.py` newest group | **BLOCKED** | no shell, no python3 |
| 6 | Vision: recognizable Sponza | **BLOCKED (structural)** | no image tool in this runspace |
| 7 | Mode 20 non-zero `GBufferMaterial` | actionable since v182 | needs one operator run |

**0 of 7 verified against the patched tree.** Gates 3/4 deliberately NOT carried
forward as PASS from the 2026-08-14 log: that log says nothing about a tree
patched on 2026-08-30.

## What this auditor did NOT do

Did not build, run, compile shaders, validate, or view any image. Did not commit,
push, or touch governance files. Did not fabricate any runtime result.
