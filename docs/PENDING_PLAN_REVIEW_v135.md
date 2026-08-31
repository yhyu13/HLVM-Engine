# Pending Plan Review v135 — KEEP (with structural caveat)

- plan: docs/PENDING_PLAN_v135.md
- verdict: KEEP
- reviewer: plan-criticer (file-only runspace; freshness-degraded per anti-pattern #7)
- timestamp: 2026-07-30

## Design soundness

The design is **mechanically sound** but **probability-correct is moderate, not high**. The plan correctly identifies that `createBindingSet()` calls `vkUpdateDescriptorSets` which records the descriptor's `imageLayout` parameter (SHADER_READ_ONLY_OPTIMAL). However, nvrhi internally passes the EXPECTED layout (per the binding layout's resource type), not the image's current physical layout. So the descriptor metadata is correct regardless of when commitBarriers fires. **The actual root cause of zero SRV reads may NOT be the barrier ordering.**

Three reasons the design is still worth doing:

1. **It's defense-in-depth.** The existing commitBarriers at line 668 (v131) keeps the image in SHADER_READ_ONLY_OPTIMAL by dispatch time. Adding an early commitBarriers doesn't break anything; it just ensures the layout transition happens before any descriptor manipulation. Low risk.

2. **It bisects a real possible cause.** The image being in COLOR_ATTACHMENT_OPTIMAL at descriptor-update time is a documented anti-pattern in the nvrhi-deferred-barrier-ordering guide. Even if nvrhi internally handles this correctly, putting the explicit commitBarriers earlier rules it out as the root cause for the next iteration.

3. **It produces observable evidence.** If after rebuild, `HLVM_PT_DEBUG_MODE=20` (GBufferMaterial SRV read) STILL returns zero, the barrier ordering was NOT the root cause, and we move to v136 with a tighter hypothesis. If mode 20 returns non-zero, the bisect closes.

## Plan completeness

The plan is **complete for a file-only cycle**:
- Clear task description.
- Specific file path (`Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp`).
- Concrete patch (+3 / -0 lines).
- 8 file-only tests + 2 parent-runspace tests deferred to terminal.
- Honest risk section acknowledging 6 distinct hypotheses for the zero-SRV-read symptom.
- Parent-runspace verification recipe (8 steps).

**Missing**:
- The plan does not explicitly call out that v131's commitBarriers at line 668 will be PRESERVED (defense-in-depth). This is mentioned in the patch section but should be in the summary. **Minor — not a blocker.**
- The plan does not discuss what happens if `vkCmdPipelineBarrier` is called when no barriers are pending (nvrhi's commitBarriers should be a no-op in that case). **Acknowledged in risk #2.**

## Feedback for planner (FIX)

No FIX-required items. The plan is approved as-is.

Optional minor suggestions (NOT blocking):
- Consider adding `gbuffer_material`, `gbuffer_normal`, `gbuffer_worldpos` dump file presence checks as parent-runspace tests in addition to the validator.
- Consider documenting that this patch supersedes the v131 patch's location (not removes it).

## Decision

KEEP. The plan is well-scoped, file-only, low-risk (additive), and produces observable evidence either way.

The impler should proceed with the patch as specified.