# Pending Commit v237 — empirical closure of the 2026-07-30 GI shader GBuffer SRV binding diagnostic

- plan: docs/PENDING_PLAN_v237.md
- files: (none — documentation-only cycle; no source changes; no `Engine/Source/Runtime/` edits)
- source: no bundle — direct synthesis of v232-v236 cycle chain + DIAGNOSTIC_2026-07-30.md + DIAGNOSTIC_2026-08-30-state-machine-617.md + freshest Debug log artifact
- target: working tree (no branch — cron runspace is file-only, no git access per agent_3_impler.md step 6 + per user instruction "do not commit, push, or modify governance files")
- task: Document the empirical closure surface for the 2026-07-30 GBuffer SRV binding diagnostic. Verify first-hand (file-only) that every component needed for runtime closure is on disk + that the production-path empirical evidence (handle identity, gi_lo non-zero, 0 VUID, 0 CommandList errors) refutes the binding-broken hypothesis.
- verify: First-hand file-only checks; see PENDING_TESTS_v237.md. Runtime closure requires operator-side terminal + vision which is BLOCKED at the runspace boundary.
- skip_impl_review: no — even a documentation cycle benefits from explicit reviewer check (HARD INVARIANT #6: never silently exit).
- produces_test_files: no — documentation cycle.
- notes:
  - **Plan Deviations**: None. The plan said "pure documentation + first-hand re-verification" and the commit has no code change.
  - **Source state (first-hand verified this turn via `read_file` against actual on-disk source)**:
    - `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl:764-766` — mode-20/21/22 use `gbPixel` (the v182 fix is on disk). Comment block at :755-763 documents the rationale (modes previously indexed with `pixel` in DISPATCH space; production reads at :501-503 use `gbPixel` in FULL-RES GBuffer space; v182 aligned probes to be faithful).
    - `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl:773-782` — mode 30 sentinel discriminator on disk (single-pixel sentinel at (0,0,0); magenta if binding works at (0,0,0), black if universally broken).
    - `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl:785-800` — mode 31 slangc-dead-strip discriminator on disk (Candidate A probe).
    - `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:516-521` — CVar+env-var→Params5 plumbing on disk.
    - `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:583-585` — handle-identity log statement for frame index < 4.
    - `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:617-619` — SRV binding set with t1/t2/t3 GBufferMaterial on disk.
    - `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ShaderMake.cfg:1` — `-D HLVM_RGI_DEBUG_VIS` define on disk.
    - `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp:614-616` — `HLVM_DUMP_RGI` env-var hook on disk.
    - `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp:2644, 2668, 2461, 2467` — handle-id log statements at RenderGBuffer on disk.
    - `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/v176-recipe.sh:207-243` — `gate_m20()` SRV probe function on disk.
    - `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` — 4-check structural validator on disk (481 lines, all 4 user-stated check functions present).
    - `_OPERATOR_RECIPE_v176.sh` — operator-side closure shim on disk.
  - **Freshest Debug log state (first-hand verified this turn via `read_file` against `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log`)**:
    - Lines 196, 202, 208 (RenderGBuffer) and lines 200, 206, 212, 216 (FGIPass::DispatchRays) — GBufferMaterial handle `0x52e800cb440` byte-equal across the boundary in 4 frames. WorldPos `0x52e800cb7c0` and Normal `0x52e800cd040` likewise byte-equal.
    - Lines 198, 204, 210, 214, 218, 221, 224, 227 — CommandList handle `0x52e81946e00` consistent across all 8 frames; Pre-GIPass and Post-GIPass matched for every frame.
    - 0 VUID hits across the entire log (`search_files pattern="VUID"` returns 0 matches in this log).
    - Line 230 — display stats mean=[0.5789, 0.5766, 0.5931] std=[0.0681, 0.0697, 0.0685] cv_lit=0.1179 — non-zero structured data consistent with recognizable Sponza.
    - Line 233 — gi_lo stats mean=[0.1388, 0.1395, 0.1535] std=[0.0406, 0.0405, 0.0413] cv_lit=0.2822 — the production path's GI light output is non-zero, which proves GBufferMaterial SRV reads work in production (diffuse = GBufferMaterial[gbPixel].rgb per `:501-503`; if t3 read returned zero, gi_lo would be zero).
    - Line 247 — test completed cleanly in 19.4 seconds.
  - **Freshest dump group (first-hand verified this turn via `search_files pattern="20260825_073403_*"`)**:
    - 9 PNGs in `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/`: gbuffer_{depth, material, normal, worldpos} + gi_{lo, raw} + denoised + spatial + display. All frame 48.
  - **0 dumps in dumps/ with `_m20_`, `_m30_`, or `_m31_` filename markers** — the v182 fix has been on disk since early in the v182-v214 cohort (2026-08-30) but has never been runtime-exercised with the debug-mode probes that would directly verify the binding hypothesis.
  - **Cross-cycle independence**: v237 documents but does not modify any of v232/v233/v234/v235/v236's work. The closure surface was assembled by those cycles; v237 just re-verifies it first-hand and frames the 7 user-stated acceptance gates against it.
  - **No governance files touched** (per HARD INVARIANT and per user instruction "do not commit, push, or modify governance files").
  - **No commits/pushes** (per HARD INVARIANT and per user instruction "do not commit, push").

## Plan Deviations (impler fills this in if it deviated)

None.