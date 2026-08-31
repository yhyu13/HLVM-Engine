# Pending Impl Review v218

- plan: docs/PENDING_PLAN_v218.md
- commit: docs/PENDING_COMMIT_v218.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-566)
- timestamp: 2026-08-21

## plan_fidelity_check

The impl matches the plan as amended at the plan gate: four checks run, slot-position used instead of
name-equality for check 3, both findings carded rather than patched. `## Plan Deviations` declares
none, and none is detectable — `files:` claims zero source modified and no `patch` call touched a
source file this cycle. The audit's strongest row is that zero, so the decision to card rather than
patch is what keeps it verifiable. Correct, and consistent with v196/v208.

## Rows I re-derived independently (not read from the marker)

**Finding 2 — reproduces exactly.** `path=FGIPass.cpp pattern="waitForIdle"` → **3 hits: `:177`
(comment), `:197`, `:441`.** Two code hits. `PENDING_COMMIT_v214.md:10` predicts "exactly 1 hit at line
415". Both the count and the line are wrong against a correct tree. Confirmed.

**Card S — reproduces, and is WORSE than the impler stated.** I re-read
`FCommonRenderPasses.cpp` rather than only re-running the count, and the mechanism is live but
misdirected: `SetShaderDataDir` writes `g_ShaderDataDirOverride:44`, which `GetShaderDataDir():67-71`
reads, and the single consumer of that is `:319` → `InitBlitResources(Device, ShaderDataDir):322`,
which loads **`BlitVS.sblob` / `BlitPS.sblob`** (`:210`, `:236`). **It never resolves
`BilateralDenoise_cs.sblob` at all.** So the shared shader's comment is wrong in two independent ways:
the override has no call sites, *and* the mechanism it names governs a different shader set entirely.
`FBilateralDenoisePass` does not consult `FCommonRenderPasses` in any form. Card S text should carry
this; it strengthens the card without changing its severity (still not a defect, still a comment trap).

**Control for the zeros** (v217 rule): `DummyDirectionTexture` → 0 is paired with
`DummyDebugStatsTexture` → 5 in the identical scope and shape, so that scope completed. Every other
load-bearing row in this cycle is a non-zero, which needs no control.

## Security scan

- [x] No hardcoded secrets — no source modified
- [x] No shell injection — no shell executed (terminal refused)
- [x] No eval/exec
- [x] No SQL

## Self-review checklist

- [x] Validation: every count re-derived at file or `Engine/Source/Runtime` scope; no project-root walk
      (v217's false-partial-result class), no `file_glob`, no `|` alternation (tick-526).
- [x] Error handling: n/a, no code path added.
- [x] Tests: role #5 re-runs the carrying rows; `produces_test_files: no`, so HARD INVARIANT #2 does not
      force a different route — but this review ran anyway, as `skip_impl_review: no`.

## One thing the cycle got right that is worth naming

The plan gate's addition was produced by **testing the plan's own framing rather than its conclusion**.
The plan asserted tri-copy divergence was a v182-shaped risk; the gate asked how copy selection
actually works, found the documented mechanism has no call sites, and that question — not the check it
was auditing — produced the cycle's net-new finding. Both findings this cycle came from re-reading a
neighbourhood rather than a cited line, which is the v197 lesson applied twice.

## Feedback for impler

Non-blocking: fold the reviewer's sharpening of card S (the override governs Blit shaders only) into
the card text when it is written to PICK.
