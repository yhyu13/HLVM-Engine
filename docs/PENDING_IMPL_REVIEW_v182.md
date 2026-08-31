# Pending Impl Review v182

- plan: docs/PENDING_PLAN_v182.md
- commit: docs/PENDING_COMMIT_v182.md
- verdict: KEEP
- reviewer: agent_4_reviewer (tick-529)
- timestamp: 2026-08-30

## plan_fidelity_check

Impl matches the plan exactly. Plan said "align modes 20/21/22/31 in both shader
copies to gbPixel, +12/-4"; the diff is precisely that plus the stale-comment fix
the plan's risk #2 implied. `## Plan Deviations` correctly reports None, and
independent verification confirms no undeclared deviation: `search_files
pattern="int3.gbPixel"` returns exactly 8 hits (4 sites × 2 copies, same line
numbers 764/765/766/793 in both files), and `pattern="int3.pixel"` returns 24
hits none of which are in GIPathTracing.hlsl (all in sibling ReSTIR/denoise
shaders that legitimately dispatch at GBuffer resolution and are out of scope).

## Correctness review

- **gbPixel scope**: declared at :499, used at :764-793 in the same function
  body. In scope. The `#ifdef HLVM_RGI_DEBUG_VIS` block at :653 is nested inside
  the same function, so no scoping hazard.
- **Semantics preserved**: each case keeps its original arithmetic transform
  (`*0.5+0.5`, `*0.25+0.5`, `*0.5+0.1`). Only the address changed. Per
  `gpu-rendering-bisect-debug` anti-pattern #7, the non-trivial arithmetic that
  protects case 31u from slangc dead-strip is intact.
- **Blast radius**: all 4 sites are inside `#ifdef HLVM_RGI_DEBUG_VIS`, and all
  are inside `if (debugMode != 0u)`. With the default `HLVM_PT_DEBUG_MODE` unset,
  none of this code executes. The production path (:501-503, :553, :556) is
  byte-unchanged. Confirmed by inspection: no line outside the debug switch was
  touched in either copy. Regression risk on the display/validator gates: none.
- **Copy parity**: ShaderMake.cfg:1 compiles the Test/..._Data copy; the
  Private/Renderer copy is the source of truth. Both patched byte-equal, so the
  next build cannot pick up a stale probe. This is the exact trap that would
  have silently produced another false "still black" reading.

## Security scan

- [x] No hardcoded secrets
- [x] No shell injection (shader + one comment line in a bash file)
- [x] No eval/exec
- [x] No SQL injection

## Self-review checklist

- [x] Validation: address-space change verified against the two runtime
  dimensions read from the freshest log (400x300 dispatch, 800x600 GBuffer).
- [x] Error handling: n/a — `Texture2D.Load` on an out-of-range coord returns 0
  in HLSL, which is exactly the old failure mode being removed. The new coord is
  in range by construction (gbPixel ≤ RenderTargetSize).
- [x] Tests: file-only structural verification in PENDING_TESTS_v182.md.

## Feedback for impler (FIX only)

n/a — KEEP. One note carried to the audit: the commit is appropriately honest
that the fix is *proved correct by construction* but *not yet re-measured at
runtime*. Do not let a later tick upgrade that to "gate 7 passes."
