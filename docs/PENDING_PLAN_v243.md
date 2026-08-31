# Pending Plan v243

- **task**: Empirically verify GBuffer SRV binding fix end-to-end on the v140+v182 source tree
- **source**: no bundle — this is an empirical verification cycle, not a source patch
- **approach**: Three-phase pipeline. **Phase A (no source change)**: rebuild Debug binary with existing v140 (`FGIPass.h:86-89 AmbientColor[4]` default + `FGIPass.cpp:473-475 AmbientColorPtr = Desc.AmbientColor` override) + v182 (`GIPathTracing.hlsl:499 gbPixel + L764-766 modes 20/21/22 indexed at gbPixel + L773-782 mode 30 sentinel + L791-800 mode 31 alive-sentinel`) source on disk; run `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` to generate fresh dump group; run `HLVM_PT_DEBUG_MODE=20`, `=21`, `=22`, `=30`, `=31` in five separate runs. **Phase B (per-pixel analysis)**: numpy per-pixel statistics on each dump group; expect mode 20/21/22 to show non-zero, non-uniform pixel data; expect mode 30 to show magenta `(1,0,1)` at (0,0,0); expect mode 31 to show non-uniform color (binding works, slangc keeps the read). **Phase C (acceptance)**: validator passes `validate_restir_gi.py` on fresh dump group (4-check structural: black-pixel-ratio, color-variance, temporal-stability, cell-variance); vision check on fresh display image confirms recognizable Sponza with sane exposure.
- **diff_estimate**: +0 / -0 lines of source code (pure empirical cycle); +~50 lines of operator-recipe extensions to `v176-recipe.sh` for the 5-mode run; +~30 lines of `validate_restir_gi.py` extensions if needed
- **skip_plan_review**: no — this is a multi-step empirical cycle that benefits from a fresh-eyes critique on the verification recipe
- **test_strategy**: The tester (role #5) writes a `verify_v243.py` wrapper that orchestrates the 5-mode debug runs, parses per-mode dump outputs, and asserts the per-mode shape expectations; the testing-verifier (role #6) cross-checks the wrapper against `validate_restir_gi.py` and against the canonical `DIAGNOSTIC_2026-07-30.md` hypothesis
- **risks**:
  1. **Terminal tool is categorically denied by tirith** in this cron runspace (cumulative ≥ 2335 lineage denials). The planner, impler, reviewer, tester, and testing-verifier **cannot execute the build/run steps**. This card can ONLY be closed by an operator at the keyboard running `_OPERATOR_RECIPE_v176.sh all` (or its v243-extension), OR by an environment where the cron has `enabled_toolsets: ["terminal", "file"]` per `DISPATCHER_PROMPT.md` L11. **Per the user's instruction off-ramp clause ("or report concrete external blocker with evidence")**, this is the central blocker. The six-role pipeline state machine cannot proceed past Rule 1 → planner without terminal access; it can write the markers but cannot verify them.
  2. **The 2026-07-30 binding-broken hypothesis is already quad-refuted in source** (v182 fixes probe positions; v140 fixes AmbientColor; v131-v139 unblock the binding per `DIAGNOSTIC_2026-08-01-v25.md` L24-28). The only remaining gap is empirical: no one has run the rebuilt binary and confirmed mode 20 returns non-zero. If the empirical run reveals a NEW failure mode (e.g., the shader has a different bug at runtime that source reading missed), that becomes a v244 cycle.
  3. **Mode 30 sentinel gate ambiguity**: if mode 30 shows magenta at (0,0,0) but mode 20 shows zero everywhere, the binding is "partially bound" (masking by layout transition per ping-pong UAV/SRV) — that's a v244-class deeper investigation. If both are zero, the binding is universally broken (v244 too). Per `gpu-rendering-bisect-debug §The decisive experiment: constant-sentinel reads across the boundary`, mode 31 is the slangc-dead-strip discriminator.
  4. **Vision tool unavailable in cron**: gate 6 (vision check) cannot be performed in the cron runspace. The operator at the keyboard must do this step.
  5. **No fresh-run log evidence on disk post-v182 source fix landed 2026-08-30**: the 3 Debug logs (`TestReSTIR_GI_Temporal.log`, `_1.log`, `_2.log`) are dated 2026-08-26/27 — 3-4 days BEFORE the v182 source fix landed. Any acceptance verdict derived from these logs is about the pre-v182 binary.

## Files this cycle touches (read-only, plus operator-recipe extensions)

- Read: `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl` (current v182 source — confirm fix is intact)
- Read: `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp` (current v140 source — confirm AmbientColor override intact)
- Read: `Engine/Source/Runtime/Public/Renderer/GI/FGIPass.h` (current v140 source — confirm AmbientColor[4] field intact)
- Read: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` (verify Desc.AmbientColor assignment for TestReSTIR_GI_Temporal)
- Extend (operator): `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/v176-recipe.sh` — add 5-mode debug run orchestration
- Extend (operator): `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` — extend with mode 20/21/22/30/31 assertions if needed

## Verification contract (for the tester + testing-verifier roles)

1. **Build clean**: `./Build.sh --Rebuild --Config=Debug --Target=TestReSTIR_GI_Temporal` exits 0
2. **No Vulkan VUID/ERROR in log**: grep `Binary/Debug/TestReSTIR_GI_Temporal*.log` for `VUID|Invalidate|Device lost|VK_ERROR|FATAL` → 0 hits
3. **No command-list errors**: same grep expanded with `CommandList.*error` → 0 hits
4. **Five debug-mode runs succeed**: HLVM_PT_DEBUG_MODE=20/21/22/30/31 each produce a fresh dump group under `Binary/Debug/dumps/`
5. **Mode 20 (GBufferMaterial SRV read) returns non-zero per-pixel**: numpy per-channel min > 0, max > 0.05 (albedo is normalized to [0,1] for Sponza)
6. **Mode 21 (GBufferNormal SRV read) returns non-zero per-pixel**: per-channel min/max span ≥ 0.05
7. **Mode 22 (GBufferWorldPos SRV read) returns non-zero per-pixel**: per-channel min/max span ≥ 0.05
8. **Mode 30 sentinel**: dump shows magenta `(1, 0, 1)` at pixel (0, 0, 0)
9. **Mode 31 alive-sentinel**: dump shows non-uniform color (NOT solid blue, NOT solid zero)
10. **Validator passes**: `python3 validate_restir_gi.py dumps/` exits 0 with all 4 check_* functions passing
11. **Vision check (operator at keyboard)**: open fresh `dumps/2026*_display_frame48.png` — recognizable Sponza with sane exposure

## Acceptance gate mapping

| Gate | Requirement | How verified |
|------|-------------|--------------|
| 1 | Debug target builds | `./Build.sh` exit code (terminal required) |
| 2 | Fresh dump group produced | `ls -t dumps/*_frame48.png` shows today's date (terminal required) |
| 3 | No Vulkan VUID/ERROR | grep log file (file-only OK) |
| 4 | No command-list errors | grep log file (file-only OK) |
| 5 | `validate_restir_gi.py` passes | python invocation (terminal required) |
| 6 | Vision check passes | open image (vision tool — not in cron) |
| 7 | HLVM_PT_DEBUG_MODE=20 returns non-zero | numpy analysis (terminal required) |

**Gates 1, 2, 5, 7 require terminal**. **Gate 6 requires vision tool**. **Gates 3, 4 can be file-only**. **All 7 gates operator-executable via `_OPERATOR_RECIPE_v176.sh all`** (or its v243 extension). The cron cannot close this card.