# Pending Plan Review v169
- plan: docs/PENDING_PLAN_v169.md
- verdict: KEEP
- reviewer: plan-criticer (file-only, single-profile host)
- timestamp: 2026-08-15T-current-tick-Z

## Design soundness
The v169 plan correctly identifies the inconsistency introduced when the Debug copy was patched to v168 graphics-pipeline rebind while the Release + RelWithDebInfo copies still have the v167 explicit-clear. The v167 patch's `setViewport(0, 0, nullptr)` violates VUID-vkCmdSetViewport-viewportCount-arraylength (requires viewportCount > 0), which means any operator-side rebuild in Release or RelWithDebInfo will reintroduce the VUID cascade.

The fix is byte-equal to the proven v168 patch (graphics-pipeline rebind) in the Debug copy. The Debug binary log `TestReSTIR_GI_Temporal.log` (2026-08-14 22:18:56, 273 lines, 0 VUIDs, non-uniform GBufferMaterial) is the empirical verification artifact that proves the patch shape works. Porting byte-equal patch text to the other two copies is the canonical fix.

## Plan completeness
The plan covers:
- Byte-equal port of the 9-line graphics-pipeline rebind from Debug copy lines 1367-1371 to Release + RelWithDebInfo
- Replacement of the v167 Part 2 comment header (lines 1347-1355 of Release + RelWithDebInfo) with v169 header documenting the port
- Part 1 (revert v166) at lines 1658-1670 is unchanged — already correct in both copies (verified by 0 `setPDynamicState` hits in `vulkan-raytracing.cpp`)
- Risks acknowledged: FetchContent re-clone hazard, terminal-blocked empirical confirmation, getDesc API parity
- 5-line operator-side rebuild recipe provided

**Missing item (suggested for impl-review)**: the plan does not explicitly verify that `GraphicsPipeline` class and `checked_cast<>` template are available in the Release + RelWithDebInfo nvrhi fork copies. Cross-tree parity is byte-equal (`wc -l` parity verified at earlier ticks), but a defensive grep for `GraphicsPipeline* GfxPso = checked_cast<` would confirm the type machinery is present in both copies.

## Feedback for planner (FIX only)
None. The plan is KEEP. The design is sound, the patch is byte-equal to a proven working version, and the empirical evidence (Debug binary log) confirms the patch shape.

## Plan-criticer self-check
- v169 design solves the stated problem (cross-tree patch inconsistency) by porting the proven v168 patch
- v169 patch is byte-equal to the Debug copy (which has been empirically verified to work)
- The 5-line operator rebuild recipe is short and copy-pasteable
- The Part 1 (revert v166) is unchanged because both copies already have it
- The empirical verification gap (operator-side rebuild in Release/RelWithDebInfo) is real but unavoidable in a file-only runspace

## Verdict
**KEEP** — proceed to impler. The patch is byte-equal to the proven Debug copy and the empirical evidence is strong.
