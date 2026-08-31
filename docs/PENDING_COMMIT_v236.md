# Pending Commit v236 — Runtime closure documentation

- plan: docs/PENDING_PLAN_v236.md
- files: (none — this is a documentation-only cycle; no source changes)
- source: no bundle — direct edit of `docs/` markers only
- target: working tree (no branch — cron runspace is file-only, no git access per agent_3_impler.md step 6 + per user instruction "do not commit")
- task: Document the on-disk closure surface for the 2026-07-30 GBuffer SRV binding diagnostic. Verify first-hand that every component needed for runtime closure is on disk: shader compiled with debug switch, FGIPass reads CVar+env, shader reads Params5, v182 mode-20 gbPixel fix on disk, binding set binds GBufferMaterial to t3, test source hooks HLVM_DUMP_RGI, v235-restored recipe has gate_m20 SRV probe.
- verify: First-hand file-only checks; see PENDING_TESTS_v236.md. The runtime closure requires operator-side terminal + vision which is BLOCKED at the runspace boundary.
- skip_impl_review: no — even a documentation cycle benefits from explicit reviewer check (HARD INVARIANT #6: never silently exit).
- produces_test_files: no — documentation cycle.
- notes:
  - **Plan Deviations**: None. The plan said "pure documentation" and the commit has no code change.
  - **Source state (first-hand verified this turn)**:
    - `GIPathTracing.hlsl:653` `#ifdef HLVM_RGI_DEBUG_VIS` — ON DISK
    - `TestReSTIR_GI_Temporal_Data/ShaderMake.cfg:1` `-D HLVM_RGI_DEBUG_VIS` — ON DISK
    - `FGIPass.cpp:516-521` CVar+env-var→Params5 plumbing — ON DISK
    - `GIPathTracing.hlsl:660` debugMode = (uint)(g_GI.Params5.x + 0.5f) — ON DISK
    - `GIPathTracing.hlsl:764-766` v182 mode-20/21/22 gbPixel fix — ON DISK
    - `FGIPass.cpp:613-634` SRV binding set with t1/t2/t3 — ON DISK
    - `TestReSTIR_GI_Temporal.cpp:614-616` HLVM_DUMP_RGI hook — ON DISK
    - `TestReSTIR_GI_Temporal.cpp:2842-2970` DumpCurrentFrame machinery — ON DISK
    - `v176-recipe.sh:207-243` gate_m20 SRV probe — ON DISK (restored by v235)
  - **Cross-cycle independence**: v236 documents but does not modify any of v232/v233/v234/v235's work. The closure surface was assembled by those cycles; v236 just enumerates it.
  - **No governance files touched** (per HARD INVARIANT).
  - **No commits/pushes** (per HARD INVARIANT and per user instruction "do not commit, push, or modify governance files").

## Plan Deviations (impler fills this in if it deviated)
None.