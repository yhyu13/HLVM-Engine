# Pending Plan Review v229

- plan: docs/PENDING_PLAN_v229.md
- verdict: FIX
- reviewer: agent_2_plan_criticer (this tick)
- timestamp: 2026-08-22 (this turn)

## Design soundness

The plan correctly identifies the defect (14 ReSTIR textures sized at init, dispatched at `CurrentFBInfo`, never recreated), names the fix shape (move/recreate on resize), and proposes a helper that satisfies the symmetry-with-init requirement. **But the plan is missing three load-bearing pieces and has one structural ambiguity that needs to be resolved before the impler writes code.** None of these are show-stoppers, but each is the kind of thing that, if it surfaces at the impl gate, would force a fix-loop and waste a cycle.

## Plan completeness

**Missing piece #1 — `search_files` already returns the wrong positive control.** The test_strategy row (i) says: "`search_files pattern='Reservoir0Texture = NvrhiDevice->createTexture'` in `TestCornellBoxGI.cpp` → must be inside the resize branch (i.e. ≥1 occurrence inside the `if (!GBufferNormalsTexture || CurrentFBInfo.width != LastWidth ...)` block)". But the resize branch's `if (!GBufferNormalsTexture ...)` at `:1160` uses the GBuffer MRT creation as the trigger, NOT the ReSTIR textures. After the fix, the SAME if-statement will contain the ReSTIR texture creation, but the if-guard itself does not change. So this row tests "is the createTexture call inside the function body that starts at `:1160`" — which is a body-span test, not a structural test. Better row: count the number of `NvrhiDevice->createTexture` calls in the resize branch BEFORE the patch (should be 9: 6 GBuffer MRTs + HDR + Denoised HDR + Staging — actually 8 since staging uses `createStagingTexture`) vs AFTER (should be 8 + 14 = 22). That is a count-delta the tester can verify with one query.

**Missing piece #2 — release semantics are unspecified.** The plan says "Deletion of stale handles is mandatory: the existing `Reservoir0Texture = ...; Reservoir0HistoryTexture = ...` members must be released before reassignment." But it does not say HOW. The existing code at `:1180`-`:1208` reassigns GBufferDiffuseTexture etc. WITHOUT explicit release — NVRHI's handle semantics appear to be that reassignment drops the old handle. But the ReSTIR textures have a creation site at `:967-988` that uses member handles `Reservoir0Texture` etc., and those handles are read at `:1510`-`:1511`, `:1529`-`:1530`, `:1545`-`:1546`, etc. **If the plan's helper just reassigns `Reservoir0Texture = NvrhiDevice->createTexture(...)`, the nvrhi::TextureHandle is reference-counted internally — the old handle drops on reassignment, so no explicit release is needed.** The plan should state this explicitly so the impler doesn't write 14 unnecessary `Reservoir0Texture = nullptr;` lines (or, worse, write `m_Reservoir0Texture.Reset()` calls that don't exist in this fork).

**Missing piece #3 — the `ReSTIRPass.Shutdown` lifecycle.** Line `:1028-1029` records `bReSTIRInitialized = true` after `Initialize` succeeds. If the fix moves the texture creation into the resize branch, then on shutdown, the current code at `:1023` `Initialize` succeeded but the resize branch never ran (first frame). The shutdown path needs to know that the ReSTIR textures ARE bound. This is the kind of detail that the impler should not have to reverse-engineer.

**Structural ambiguity — "extract a helper" vs "duplicate the block".** The plan says "extract a `CreateReSTIRTextures(uint32_t W, uint32_t H)` helper, call it once at the end of init, and call it again inside the resize branch." This is cleaner than duplicating, but it adds a method to the class which changes the header (presumably `TestCornellBoxGI.h` if it's a class member, or static if it's a free function). The plan does not say which. The impler needs to choose. **This is the kind of choice that should be resolved at the plan gate so the impler does not pick wrong and the reviewer cannot fault it for picking wrong.**

## Feedback for planner (FIX)

1. **Replace test_strategy row (i) with a count-delta test.** Before patch: count `NvrhiDevice->createTexture` in the resize branch at `:1160-1256`. After patch: that count + 14. Verify the delta is exactly 14.

2. **Specify the release semantics explicitly.** NVRHI handles are reference-counted; reassigning drops the old. Do not write explicit release calls. State this in the plan's `risks` section.

3. **Choose helper visibility.** Free static function (no header change) is preferred — `static void CreateReSTIRTextures(nvrhi::IDevice*, uint32_t, uint32_t, ...)`. Member function on the test class is acceptable but requires header edits to `TestCornellBoxGI.h`. The plan should pick one.

4. **Note `bReSTIRInitialized` ordering.** Init block currently sets `bReSTIRInitialized = true` at `:1028` after `ReSTIRPass.Initialize` succeeds, BEFORE the resize branch could ever have run. After the fix, `bReSTIRInitialized` should remain set at the same point if the helper is called from init, AND from resize, but Shutdown needs to handle "texture was created" vs "pass was initialized" separately or both together. State the rule.

5. **Add a build verification note in the plan** (not just in the risks section). The verify command should be `./Build.sh --Config=Debug --Target=TestCornellBoxGI --Rebuild && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestCornellBoxGI` per the canonical recipe — note this is `TestCornellBoxGI`, NOT `TestReSTIR_GI_Temporal`, since the cycle touches the control.

6. **Acknowledge the v198 sibling positive control.** Plan risks row says "v198 sibling pattern" but doesn't quote the v198 finding's exact line. Add: "`TestRTReflections.cpp:892-984` recreates all extent-sized resources on resize — positive control for the procedure this fix implements."

## Single-profile caveat

Same head as planner, so KEEP is a self-check. The five corrections above are mechanical and resolvable in one cycle's worth of changes. Returning FIX so the planner picks one of the helper-visibility options (free vs member), tightens the test_strategy, and resolves the release semantics.