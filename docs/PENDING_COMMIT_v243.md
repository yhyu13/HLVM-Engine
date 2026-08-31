# Pending Commit v243

- **plan**: docs/PENDING_PLAN_v243.md
- **files**: (no source change — this is a pure empirical verification cycle)
  - Read: `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl`
  - Read: `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp`
  - Read: `Engine/Source/Runtime/Public/Renderer/GI/FGIPass.h`
  - Read: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp`
  - Extend (operator): `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/v176-recipe.sh`
  - Extend (operator): `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py`
- **source**: no bundle — pure empirical verification
- **target**: `main` (no commit, no push — per the dispatcher HARD rule)
- **task**: Empirically verify the v140+v182 source fixes close the GBuffer SRV binding + AmbientColor issues described in `DIAGNOSTIC_2026-07-30.md` and `DIAGNOSTIC_2026-08-01-v25.md`
- **verify**: `cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine && bash _OPERATOR_RECIPE_v176.sh all` (or its v243 extension) — exits 0 on success, >0 on any gate failure
- **skip_impl_review**: no — this card produces test files (extensions to v176-recipe.sh and validate_restir_gi.py)
- **produces_test_files**: yes — `verify_v243.py` wrapper script + extensions to `validate_restir_gi.py`
- **notes**:
  1. **Cannot execute from cron runspace**: terminal tool denied by tirith. This card is operator-executable only. Cron produces markers; the operator at the keyboard produces the runtime evidence.
  2. The reviewer (role #4) should explicitly check that the operator-recipe extensions do not regress the v242 bug fixes (`DUMPS_DIR` path, `gate_val` validator invocation, `gate_m20` filename glob).
  3. The five debug-mode runs are independent — order does not matter. Suggested order: 20 (the original failing case) → 21 → 22 → 30 → 31 (the discriminators).

## Plan Deviations (impler fills this in if it deviated)

*None yet — no source change attempted in this card. Deviations will only exist if the empirical run reveals a defect requiring source patches (which would become v244).*

## Acceptance gate status (to be filled in by reviewer, tester, testing-verifier)

| Gate | Status | Evidence |
|------|--------|----------|
| 1 (build) | OPERATOR-READY | terminal required |
| 2 (fresh dumps) | OPERATOR-READY | terminal required |
| 3 (no VUID) | PENDING | file-only grep, runs after build |
| 4 (no CL errors) | PENDING | file-only grep, runs after build |
| 5 (validator) | OPERATOR-READY | python + terminal |
| 6 (vision) | OPERATOR-READY | vision tool required |
| 7 (mode 20 non-zero) | OPERATOR-READY | numpy + terminal |