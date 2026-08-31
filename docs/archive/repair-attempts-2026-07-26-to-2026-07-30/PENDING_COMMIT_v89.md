
# Pending Commit v89
- plan: docs/PENDING_PLAN_v89.md
- files: 0 source-code files (verification-only cycle); 6 v89 marker files + 1 PIPELINE_HEALTH_2026-07-28.md append + 1 PENDING_PICK.md update
- source: no bundle — direct edit (parent terminal access required for build/run/validate/vision; structurally blocked in this cron runspace)
- target: worktree-only (no git operation; cron directive: do not commit/push/rewrite history)
- task: restir-gi-fix — Three Part A binding-wiring probes confirming the dispatch-side wiring is structurally correct; the gi_raw=0,0,0 bug is downstream of the binding setup (in the dispatch body, or in the shader's write to u0).
- verify: parent should read `docs/PENDING_TESTS_v89.md` and inspect the per-Part-A spot-check results in `docs/PIPELINE_HEALTH_2026-07-28.md` (Tick — v89 section). Cannot be verified from this runspace.
- skip_impl_review: yes (the cycle is a single verification-only cycle on already-intact surface; 0 source-code lines touched; no separate review value-add since none of the cR-Site markers would change as a result of the verifier's pass)
- produces_test_files: no
- notes: Three Part A probes executed this tick:
  - (a) `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp:937-939` — `OutputTexture = CreateTexture2D(NvrhiDevice, W, H, nvrhi::Format::RGBA32_FLOAT, nvrhi::ResourceStates::UnorderedAccess, "GIRawHDR");` PASS exact (Format=RGBA32_FLOAT, InitialState=UnorderedAccess, isUAV=true via line 167-168 of CreateTexture2D, keepInitialState=true via line 165).
  - (b) `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:301-316` — `nvrhi::BindingLayoutDesc UAVLayoutDesc; UAVLayoutDesc.visibility = nvrhi::ShaderType::All; nvrh::BindingLayoutItem UAVItems[2]; UAVItems[0].slot = 0; UAVItems[0].type = nvrhi::BindingType::Texture_UAV; UAVItems[0].size = 1; UAVItems[1].slot = 1; UAVItems[1].type = nvrhi::BindingType::Texture_UAV; UAVItems[1].size = 1; UAVLayoutDesc.bindings.assign(UAVItems, UAVItems + 2); UAVBindingLayout = Device->createBindingLayout(UAVLayoutDesc);` PASS exact. SetBindingOffsets() NOT called on UAVLayoutDesc → defaults to 0/128/256/384 (unorderedAccessViewOffset=0 default).
  - (c) `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:582` — `UAVBuilder.SetTextureUAV(0, Desc.OutputTexture);` PASS exact. FBindingSetBuilder::SetTextureUAV (FBindingLayoutBuilder.cpp:178, 184) maps RegisterIndex → URegShift + RegisterIndex → binding slot. URegShift default = 0 (no SetBindingOffsets on the UAV set). Slot 0 on the set side aligns with slot 0 on the layout side aligns with shader u0.

  Combined: the binding wiring for `OutputTexture → u0 → shader-side RWTexture2D<float4>` is structurally correct in the C++ source. If gi_raw=0,0,0 persists, the bug is downstream of the binding setup — either (i) the dispatch body never executes (RTPipeline.DispatchRays hangs or drops at FRayTracingPipeline.cpp), (ii) the shader's write to `gOutputTexture[DispatchIdx]` is skipped, written to a subresource the dumper doesn't read, or with stride/format mismatch, OR (iii) the dumper's copyTexture reads from a different texture handle than what was written (debugger-side bug).

  None of the v25-v88 record explicitly narrowed the bug to "binding-side" or "downstream-of-binding." v89 establishes that it's downstream of the binding. This is a real diagnostic narrowing.

## Plan Deviations
None. Impler honored plan exactly: 3 read-only checks via search_files + read_file, 0 source-code lines modified, no fabrication.

## What this commit does NOT do (consistent with cron's "do not loop indefinitely")
- Does NOT bump any lighting constant.
- Does NOT modify the binding setup.
- Does NOT modify the raygen shader.
- Does NOT add new probes beyond the 3 in the plan.
- Does NOT claim gi_raw is fixed.
- Does NOT fabricate KEEP/ALL_KEEP.
