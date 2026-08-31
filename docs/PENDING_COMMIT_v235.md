# Pending Commit v235 — Restore v176-recipe.sh (minimal honest reconstruction)

- plan: docs/PENDING_PLAN_v235.md
- files: Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/v176-recipe.sh (new), docs/PENDING_COMMIT_v235.md (this file)
- source: no bundle — reconstruction from the `_OPERATOR_RECIPE_v176.sh` shim docstring + lineage references
- target: working tree (no branch — cron runspace is file-only, no git access per agent_3_impler.md step 6 + per user instruction "do not commit")
- task: Restore `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/v176-recipe.sh` to its canonical path. First-hand this turn: file is missing (0 hits in `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/` for `v176-recipe*`). The shim `._OPERATOR_RECIPE_v176.sh` (53 lines) references the missing canonical recipe and documents the expected exit-code table (0=PASS, 1=BUILD, 2=DUMP, 3=VULK, 4=CMDL, 5=VAL, 6=M20, 7=ENV) + usage (.mode20 invocation for gate 7).
- verify: First-hand file-only checks; see PENDING_TESTS_v235.md. The recipe's runtime execution requires terminal which is BLOCKED at the runspace boundary; the file-only verifier confirms structural correctness only.
- skip_impl_review: no — even a restoration cycle benefits from explicit reviewer check (HARD INVARIANT #6: never silently exit).
- produces_test_files: no — restoration cycle. The "test" mechanism is the recipe itself + validate_restir_gi.py.
- notes:
  - **Plan Deviations**: The plan claimed "489 lines" matching the v234 audit's claim. First-hand this turn, the shim `._OPERATOR_RECIPE_v176.sh:48` documents "(six-role-pipeline tick-300 closure audit expected 312 lines)" — contradicting the v234 audit's 489-line claim. The honest disposition is to write a MINIMAL recipe (~80 lines) that satisfies the shim's structural contract (exit codes 0-7, mode20 invocation, all 7 gates) without fabricating 489 or 312 lines of code that may not match the original. The recipe is honest about its scope: it documents each gate, but defers the actual heavy lifting (rebuild + run + dump + validator + SRV probe) to operator-side execution because those require terminal access unavailable to this cron tick.
  - **Source state (first-hand verified this turn)**:
    - v176-recipe.sh: NOT on disk (0 hits in `TestReSTIR_GI_Temporal_Data/`).
    - v176 CVar wiring (`CVar_r_ReSTIR_MaxM`, `HLVM_RGI_MAXM`): NOT on disk (0 hits in `TestReSTIR_GI_Temporal.cpp`).
    - v176 env-var hook (`HLVM_DUMP_RGI`, `HLVM_PT_DEBUG_MODE`): NOT on disk (0 hits in test source).
    - v232 W-clamp + v233 Jacobian clamp + v234 provenance wrap: ON DISK (12/12 v234 verifier rows PASS this turn).
    - v176-recipe.sh shim (`._OPERATOR_RECIPE_v176.sh`): ON DISK (53 lines, 2026-08-19 timestamp).
    - validate_restir_gi.py: ON DISK (404 lines per v234 audit, 4 check_* + 3 extras confirmed).
    - Freshest Debug log (`Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log`): 2026-08-25 07:38, 255 lines, 19.4s clean exit, 0 VUID/ERROR, stats display mean≈0.58 std≈0.07.
  - **Cross-cycle honesty correction**: The v234 PENDING_TEST_AUDIT row 11 explicitly REFUTED the v10 audit's claim that v176-recipe.sh was missing. First-hand this turn: BOTH the v10 audit (said missing) and the v234 audit (said present, 489 lines) are empirically WRONG in this snapshot. The file is missing in this snapshot. The lineage has been carrying a stale-evidence false positive for multiple cycles. This commit + the v235 cycle corrects the record honestly.
  - **What this minimal recipe does NOT include** (because the supporting infrastructure is missing from this snapshot):
    - Gate 1 (Debug build) calls `./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal --Config=Debug`. The build is reproducible.
    - Gate 2 (HLVM_DUMP_RGI=1 dumps) calls the test binary with the env var. The test source has NO env-var hook for HLVM_DUMP_RGI in this snapshot — the gate is documented but cannot produce dumps until the hook is restored. The dumps in the data dir are from earlier work.
    - Gate 5 (validate_restir_gi.py) runs `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py`. The validator IS on disk.
    - Gate 6 (vision) is out of scope for the recipe (no vision tool from a shell).
    - Gate 7 (HLVM_PT_DEBUG_MODE=20) requires the v182-corrected GIPathTracing.hlsl (mode-20 uses gbPixel). The HLSL is on disk; the test source has NO env-var hook for HLVM_PT_DEBUG_MODE in this snapshot. Gate 7 is documented but cannot run until the hook is restored.
  - **Cornell copies verified clean**: `search_files pattern=v176 path=TestCornellBoxGI_Data` returns 0 hits. Cornell algorithm is simpler (no ReSTIR complexity) and doesn't need the v176 CVar wiring.
  - **No governance files touched** (per HARD INVARIANT).
  - **No commits/pushes** (per HARD INVARIANT and per user instruction "do not commit, push, or modify governance files").

## Plan Deviations (impler fills this in if it deviated)
- Plan said "489 lines". Reality: shim references "312 lines". Honest disposition: minimal honest recipe (~80 lines) that satisfies the structural contract without fabricating the line count.
- Plan said restoration of recipe only. Reality: the recipe needs several supporting infrastructure pieces (env-var hooks, CVar wiring) that are also missing. Documented as gaps above; the recipe itself is the verifiable artifact for this cycle.