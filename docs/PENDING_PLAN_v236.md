# Pending Plan v236 — Runtime closure of the 2026-07-30 GI shader GBuffer SRV binding diagnostic

- task: Operator-side runtime closure of the 2026-07-30 diagnostic that the GI shader's `Texture2D<float4> GBufferMaterial : register(t3)` SRV read returns zero (HLVM_PT_DEBUG_MODE=20 sentinel). The diagnostic's recommended fix-surface has been verified on disk this tick (v235 audit). The remaining work is operator-side terminal execution of the test binary with the env vars set, then dump validation.
- source: no bundle — pure documentation + operator-side execution.
- approach:
  1. **Document the on-disk closure surface** for the v236 task. First-hand this tick, the following is verified to be on disk:
     - `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl:653` compiles the debug switch behind `#ifdef HLVM_RGI_DEBUG_VIS`.
     - `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ShaderMake.cfg:1` passes `-D HLVM_RGI_DEBUG_VIS` for GIPathTracing.hlsl → the sblob has the debug switch.
     - `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:516-521` reads `r_GI_DebugMode` CVar + `HLVM_PT_DEBUG_MODE` env var and writes the value to `g_GI.Params5[0]`.
     - `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl:660` reads `g_GI.Params5.x` into `debugMode`.
     - `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl:764-766` has the v182 mode-20/21/22 fix (uses `gbPixel` instead of `pixel`).
     - `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:613-634` builds the SRV binding set with `SetTextureSRV(1, Desc.GBufferWorldPos)`, `SetTextureSRV(2, Desc.GBufferNormal)`, `SetTextureSRV(3, Desc.GBufferMaterial)`.
     - `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp:614-616` hooks `HLVM_DUMP_RGI` env var to the dump machinery.
     - `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp:2842-2970` dumps gi_raw + gbuffer_* + denoised + spatial + display via `DumpCurrentFrame()` at the last frame.
     - `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/v176-recipe.sh` (restored by v235) has a `gate_m20()` function (lines 207-243) that runs the binary with `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20` and validates gi_raw via numpy pixel-stats.
  2. **Document the operator-side closure command** for the operator who has terminal access:
     ```bash
     cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
     ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild
     bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/v176-recipe.sh mode20
     # exit 0 → mode-20 SRV returns non-zero GBufferMaterial → binding-broken hypothesis REFUTED
     # exit 6 → mode-20 gi_raw is mostly black → binding-broken hypothesis CONFIRMED → diagnostic re-opens
     ```
  3. **Document the runtime verification gap** — this cron tick is file-only (terminal denied at tirith boundary; no vision_analyze tool; no cronjob registration). The v236 closure requires operator-side execution which cannot happen from this runspace.
- diff_estimate: +0 / -0 lines (no source changes; pure documentation of the closure surface).
- skip_plan_review: yes — pure documentation cycle for a runtime closure that requires operator-side terminal; the design is "what's already documented + what the v235-restored recipe invokes".
- test_strategy: tester role #5 runs an 8-row file-only verifier confirming the closure surface listed in approach step 1 is on disk. Acceptance: every file path + line number exists as documented.
- risks:
  1. **The on-disk closure surface may have a subtle bug** (e.g., Params5.x is double-clamped somewhere; the shader's debugMode read has a type mismatch; the binding set's SRV is silently dropped). These are runtime risks that only operator-side execution can surface.
  2. **The `HLVM_RGI_DEBUG_VIS` compile flag in ShaderMake.cfg** may not have been honored in the sblob on disk. If slangc silently dropped the `#ifdef` block for any reason, the debug switch won't be in the compiled shader, and mode 20 will silently no-op. Verify via `spirv-cross --reflect` on the sblob — but that requires terminal.
  3. **The 0x52e800cb440 handle identity** observed in the 2026-08-25 07:38 log (RenderGBuffer ↔ FGIPass::DispatchRays) confirms the handle is the SAME object across the boundary — refuting DIAGNOSTIC option 4 (handle mismatch). This is the strongest empirical signal that the binding chain is intact; mode-20 SRV probe should return non-zero.
  4. **Terminal access denial** — the v236 task is intrinsically operator-side. This cron tick's disposition is to document the surface and mark the card as "ready for operator-side closure" rather than to fabricate a runtime result.
- relation to existing queued cards: v236 retires the original GBuffer SRV binding diagnostic. After v236 closes (file-only documentation only; runtime closure is operator-side), the v232/v233/v234/v235/v236 cycle chain constitutes the complete load-bearing fix for the 2026-07-30 diagnostic.
- relation to v232 + v233 + v234 + v235: v236 is the final cycle in this chain. It documents the closure surface that v232-v235 have collectively established. No new code; pure documentation.