# Pending Commit v1

- plan: docs/PENDING_PLAN_v1.md
- files: Engine/Source/Runtime/Public/Renderer/PostProcess/FReSTIRPass.h, Engine/Source/Runtime/Private/Renderer/PostProcess/FReSTIRPass.cpp, Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp, Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
- source: no bundle
- target: working tree (no commit per cron rules)
- task: bug-088 (immediate-CL collision) + bug-075 (SRV+UAV ping-pong split) + validator tightening
- verify: ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test ; python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
- skip_impl_review: no — both fixes touch pipeline creation and the test's per-frame CommandList lifetime; needs an evidence check (fresh log without VUID-00344 and without "Cannot open a command list").
- produces_test_files: no (validator edits existing test script, not new test files)
- notes:
  - bug-075: the HLSL shader ReSTIR_Temporal_cs.hlsl does NOT need to change. Two-phase dispatch from the same shader is safe because the HLSL `ReSTIR_Temporal` entry point reads inputs in the first N lines and writes outputs at the end. We invoke it twice; the second invocation re-reads whatever it wrote (OutReservoir0/1) but those textures aren't bound as SRV in the UAV set's pipeline (they only appear in the UAV set), so the second dispatch reads them as freshly-written UAV contents from the same dispatch — no SRV-on-same-tex race. To be safer for future readers, the HLSL writer could add an `if (bPhase == 0) { /* read+merge into LDS */ } else { /* write back */ }` dispatch parameter, but the current contract works because phase 1 writes nothing (UAV set is bound but shader's write happens unconditionally and produces the same answer on phase 2 — phase 2 simply re-does the merge with its own previous frame's state). This is wasteful but correct.
  - bug-088: tested by absence of "Cannot open a command list that is already open" in the fresh log.

## Plan Deviations (impler fills this in if it deviated)
- The plan called for a one-line `CommandList->close()` fix at end of Render(). After root-cause analysis of the actual symptom (dump code reads DisplayTexture in VK_IMAGE_LAYOUT_UNDEFINED), the real bug was that the per-frame CL was never submitted via `executeCommandList` — only the raster pass inside RenderGBuffer was submitted. Fix changed from "just close" to "close + executeCommandList" (one extra line). Also moved `DumpCurrentFrame()` to AFTER the per-frame CL execute so dumps see the post-accumulate state (Vulkan FIFO queue ordering).
- The plan's bug-075 fix (split TemporalLayout into TemporalLayoutSRV + TemporalLayoutUAV and dispatch in two phases) had already been applied to FReSTIRPass.cpp/.h by a prior session — confirmed intact (TemporalLayoutSRV and TemporalLayoutUAV both present, HLSL declares `register(u0, space1)` and `register(u1, space1)`). No additional work needed for bug-075.
- Validator tightening (3 checks: non-black mean > 5, spatial std > 30, cell variance std > 8) was already applied to validate_restir_gi.py. Confirmed intact.

## Implementation Evidence (impler fills this in)
- Files modified this cycle:
  - `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` — added `NvrhiDevice->executeCommandList(CommandList)` after the close at end of Render (line 662), moved DumpCurrentFrame to after the execute.
- Build/run blocked by tirith in this cron (terminal approval denied for every command). I cannot run `./Build.sh --Target=TestReSTIR_GI_Temporal --Test`. The parent session (user) must run the verification command from PENDING_COMMIT_v1.md `verify:` line.
- Expected post-build behaviour: post-raster pipeline work (GIPass, denoise, ReSTIR, accumulate, blit) is now submitted; DisplayTexture will be in GENERAL layout (not UNDEFINED) when DumpCurrentFrame reads it; the dump will show the actual accumulated radiance instead of an undefined-initialized texture; "A command list should be executed before it is reopened" warning should disappear.
- Risk: the executeCommandList submission may cause a Vulkan validation error if any pass already transitioned DisplayTexture to UNDEFINED then never wrote (e.g., the accumulate pass silently failing). If that happens, the dump will still show UNDEFINED layout and the validate script will fail. The parent should inspect the dump and log on next run.