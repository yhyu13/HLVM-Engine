# Pending Tests v65
- task: structural standby — verify 22-patch cumulative inventory INTACT

## Part A — File-only static probes (this tick)
- T1: FGIPass.h:106 contains `UAVBindingLayout` v22 split member — PASS (verified)
- T2: FGIPass.cpp:183 contains `UAVBindingLayout = nullptr` v22 split init — PASS (verified)
- T3: FGIPass.cpp:311 contains `UAVBindingLayout = Device->createBindingLayout` v22 split create — PASS (verified)
- T4: FGIPass.cpp:612 contains `UAVBuilder.Build(), UAVBindingLayout` v22 split use-site — PASS (verified)
- T5: FImageDump.cpp:27 contains `std::clamp(rgbaData[i * 4 + 3] * 255.0f, 0.0f, 255.0f)` v41 alpha-encoder fix — PASS (verified)
- T6: FGIPass.cpp:487 contains `std::cerr << "[RGI] FGIPass::WriteConstants: DebugMode effective="` v38 cerr DebugMode log — PASS (verified)
- T7: Private/Renderer/Shader/GI/GIPathTracing.hlsl:593 contains `case 6u:` v13 — PASS (verified)
- T8: Private/Renderer/Shader/GI/GIPathTracing.hlsl:604 contains `case 7u:` v17 — PASS (verified)
- T9: Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl:604 contains `case 7u:` v17 (data-dir copy) — PASS (verified)
- T10: Private/Renderer/Shader/GI/GIPathTracing.hlsl:694 contains `Output[pixel].w = max(Output[pixel].w, 0.99994f);` v28 — PASS (verified)
- T11: Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl:694 contains same v28 alpha sentinel (data-dir copy) — PASS (verified)
- T12: Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py contains `check_alpha_sentinel` v37 — PASS (verified)
- T13: Test/TestReSTIR_GI_Temporal_Data/dump_pixelstats.py contains `compute_alpha_stats` v40 — PASS (verified)

13/13 Part A probes PASS.

## Part B — Runtime probes (parent-driven; terminal blocked in cron)
- B1: Run `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Test` — UNVERIFIED (terminal blocked)
- B2: Run `cd Engine/Source/Runtime/Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal 2>stderr.log` — UNVERIFIED
- B3: Verify v12 cerr fires (`[RGI] Render() entry:` + `[RGI] FGIPass::DispatchRays() entry:`) — UNVERIFIED
- B4: Verify v38 cerr DebugMode-effective line decodes via decode_v38_evidence.py — UNVERIFIED
- B5: Inspect `display_frame8.png` alpha channel (saturated → v28 sentinel compiled + dispatch ran) — UNVERIFIED
- B6: Run `validate_restir_gi.py` (validator should pass 4/4 if v37 alpha-check + v38 cerr + v41 encoder all functional) — UNVERIFIED
- B7: Vision-analyze `display_frame8.png` for recognizable Sponza geometry — UNVERIFIED
- B8: Verify `grep stderr.log VUID-VkDescriptorImageInfo-imageLayout-00344` returns 0 hits (v22 fix effective) — UNVERIFIED
