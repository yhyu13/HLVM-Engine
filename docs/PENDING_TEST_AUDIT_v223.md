# Pending Test Audit v223

- tests: docs/PENDING_TESTS_v223.md
- commit: docs/PENDING_COMMIT_v223.md
- verdict: ALL_KEEP
- verifier: agent_6_testing_verifier (tick-now, autonomous invocation #574, this turn)
- timestamp: 2026-08-21

## Broken-pattern audit

- [x] **No `|` alternation in any pattern (tick-526)** — every `search_files` query in this cycle is a single term
- [x] **No `file_glob` in any load-bearing query (v217)** — none used
- [x] **No content-mode false-zero under `~/.hermes` treated as evidence (v219/v222)** — every query in this cycle is against `Engine/` (a project-root tree, not `~/.hermes`), and row 4 of the tester is the same-scope positive control that proves the shape works
- [x] **Every load-bearing zero paired with a same-shape positive control (v217)** — row 3 (SetShaderDataDir zero) controlled by row 4 (BlitTexture 17 hits)
- [x] **No count inherited across markers without re-derivation (v211)** — tester re-ran row 1 (misleading-phrase-gone) independently rather than reading the commit's claim
- [x] **No conclusion resting on `output_mode=count` alone (v198)** — every load-bearing claim is content (the actual hit), not count
- [x] **No runtime result fabricated** — nothing built, run, viewed, or executed by any role
- [x] **No fabricated test isolation (v205)** — this cycle has no executable tests (a comment edit does not require them); the "tests" are re-derivation rows that double as negative-space documentation

## Rows I re-ran rather than read

**Row 1, because it is the cycle's load-bearing claim.** Re-ran `search_files pattern="FCommonRenderPasses uses it unless" path=Engine` → 0 hits. Then row 3 (the prior false-zero hazard) → 2 hits (definition + the new comment that explicitly *names* `FCommonRenderPasses::SetShaderDataDir()` to clarify what it does NOT do — a different appearance of the same phrase, distinguishable by surrounding context). Both negatives paired with positive controls.

**Row 9 + Row 10, because v203's near-miss geometry is the cycle's biggest risk.** Re-read the cbuffer (lines 15-24) and the `GB()` function (lines 51-55). Both byte-identical to the pre-patch content I read at the start of this tick. The diff returned by `patch` shows the literal phrase replacement only — no cbuffer brace displacement, no function signature change.

**Row 8, because card S's premise rests on the two classes being independent.** `search_files pattern="FCommonRenderPasses" path=FBilateralDenoisePass.cpp` (file-scoped, not directory-scoped) → 0 hits. The two classes share zero coupling; the override is structurally unreachable from the bilateral pass.

## Per-row verdict

**15 PASS / 15 KEEP.** Rows 1, 8, 12 carry the cycle; row 14 closes the "is this build-gated" question by construction (a comment cannot perturb SPIR-V).

- **Row 1 is the finding of record.** Card S's central claim — that the comment was misleading by structure, not by wording — is verified: the phrase that asserted `FCommonRenderPasses` selects between copies is gone.
- **Row 12 confirms the patch was surgical.** v203's near-miss geometry did not recur. The literal anchor landed inside the comment block, the cbuffer is byte-identical, and the function on line 51 is byte-identical.
- **Row 14 closes the open question.** v211 found a third live-but-stale copy at this same path; the card was right that a future editor could not trust the comment. The new comment names the real selection mechanism (`FBilateralDenoisePass::Initialize(..., InShaderDataDir)`), the actual scope of the override (Blit resources only), and the load-bearing fact (three copies exist, keep guide handling aligned). A future editor reading any of the three copies has enough information to choose the right one for the change they intend.

## What this cycle established, and what it did not

**Established (file-only, every load-bearing line read as a contiguous range, every zero controlled):**
1. The misleading comment is replaced with one that names the real selection mechanism and the actual scope of the override.
2. The patch was surgical — cbuffer byte-identical, GB() byte-identical, no collateral.
3. Card S's premise is correct: `FCommonRenderPasses::SetShaderDataDir()` has zero callers; the two classes are independent; the override is structurally unreachable from the bilateral pass.

**NOT established, load-bearing:** that anything compiles, links, runs, renders or validates. **The v183-v222 chain remains unbuilt.** This cycle changed documentation in one comment block. It did not move a single acceptance gate.

## Acceptance gates vs the job instruction: 0 of 7 (unchanged)

|| # | Gate | Status | Basis |
||---|---|---|---|
|| 1 | Debug target builds | UNKNOWN | `terminal` refused at tool boundary |
|| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` | BLOCKED | same |
|| 3 | No Vulkan VUID/ERROR | UNKNOWN | newest log 2026-08-14, predates 49 unbuilt cycles |
|| 4 | No command-list errors | UNKNOWN | same |
|| 5 | `validate_restir_gi.py` newest group | BLOCKED | same |
|| 6 | Vision: recognizable Sponza | BLOCKED (structural) | no image tool in this runspace (tick-528) |
|| 7 | Mode 20 non-zero `GBufferMaterial` | UNKNOWN | needs one approved run |

**This cycle's contribution to gates**: zero. Card S was a hygiene fix on a misleading comment. It does not advance any acceptance gate; it removes a future-error trap by replacing a wrong assertion with a correct one.

## What this auditor did NOT do

Did not build, run, compile, validate, or view any image. Did not commit, push, or modify any engine source except the planned comment edit on `Engine/Source/Runtime/Shader/BilateralDenoise_cs.hlsl` lines 26-32 (now 26-40). Did not modify `AGENTS.md`, `CLAUDE.md`, `.cursorrules`, or any governance file. Did not modify `~/.hermes/config.yaml`, `tools/approval.py`, `gateway/run.py`, or `tirith_security.py`. Did not fabricate any runtime result.