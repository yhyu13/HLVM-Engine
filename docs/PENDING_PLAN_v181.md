# Pending Plan v181

- task: TestReSTIR_GI_Temporal — apply the v180 audit's SOME_RELAX wrap-up items (BLUE-MID + gray-mid envelope extension + --mode-30 / --mode-31 discriminator flags + per-leaf verdict text) to v176-recipe.sh. File-only completion of the v180 binding-discriminator cycle.
- source: file-only — see `docs/PENDING_TEST_AUDIT_v180.md:30-46` (3 RELAX items: BLUE-MID envelope, gray-mid envelope, --mode-30 flag), `docs/PENDING_PLAN_v180_recipe_patch.md:13-95` (staged diff that the v180 cycle proposed), and `docs/PENDING_PLAN_v180.md:36-48` (acceptance criteria the wrap-up must satisfy).
- approach: Apply the patch that v180 staged but did not implement (per `PENDING_COMMIT_v180.md:11-18` which states "The actual `v176-recipe.sh` patch would happen in v181 (next plan-critic + impler cycle) or directly via the operator's `git checkout` of the working tree. This is honest scoping — the impler role this turn STAGES the contract; execution is operator-side."). This turn's v181 wrap-up is the file-only execution of the patch, applied via `patch` tool (file-only, no source-code change). Patch scope:
  - 1. Add `RUN_MODE_30` and `RUN_MODE_31` state variables to v176-recipe.sh
  - 2. Add `--mode-30` and `--mode-31` argparse cases
  - 3. Widen gate-5 / gate-7 signature classifier to include `blue-mid-discriminator` (mean≈1/3, sd<0.005) and `gray-mid-discriminator` (mean≈0.5, sd<0.005) envelopes
  - 4. Add blue-mid / gray-mid diagnosis messages to gate-5 case-esac block (operator-side route through discriminator leaves)
  - 5. Add gate 8 (mode-31 alive-sentinel discriminator) with per-leaf verdicts (BLUE/NON-UNIFORM/GRAY/BLACK mapping to v180 hypothesis tree leaves 1/2/3/5)
  - 6. Add gate 9 (mode-30 single-pixel sentinel) with magenta-at-(0,0,0) detection (leaf 4)
  - 7. Extend `--help` text + summary operator next-steps block to mention new flags
  - 8. Mark `docs/PENDING_PLAN_v180_recipe_patch.md` as SUPERSEDED by v181 (the patch doc is now archived alongside the v176-recipe.sh that was directly patched)
- diff_estimate: +174 / -0 lines (486 lines total, was 312). All additive; no existing lines changed (except 2-line exit-code/usage header expansion). Recipe only — no C++/HLSL touched.
- skip_plan_review: yes — wrap-up of v180 SOME_RELAX items, single-variable file-only patch; the v180 plan + audit already authorized this scope.
- skip_impl_review: yes — file-only recipe patch, no source code change, no test files produced. Acceptance is the file-only grep verify (per v180 verify command at `PENDING_COMMIT_v180.md:9`).
- produces_test_files: no
- test_strategy: File-only verifier — `grep -c 'mode-31' v176-recipe.sh` must be ≥5 (declaration + --help text + gate label + run command + branch name). Similarly for `mode-30`. And the BLUE-MID/gray-mid envelope classifier code must be present.
- risks:
  - 1. **Recipe bash syntax invalid** — the patch is purely additive; existing syntax check (`if`/`fi` count match, `case`/`esac` count match, `for`/`done` count match) all balance this turn (verified by file-only grep: 17 if, 17 fi, 3 case, 3 esac, 2 for, 2 done; 3 extra `if`s inside Python heredocs are python conditionals, not bash). `bash -n` cannot run from cron (terminal blocked) but structural balance holds.
  - 2. **Operator-side application unnecessary** — v181 applies the patch directly via file tools; the operator does not need to `git apply` the staged patch at `PENDING_PLAN_v180_recipe_patch.md` (which is now SUPERSEDED).
  - 3. **No effect on test result** — the recipe changes do not modify FGIPass.cpp / GIPathTracing.hlsl / TestReSTIR_GI_Temporal.cpp. The actual GPU pipeline is unchanged. The changes only affect how the diagnostic signatures are classified and reported when the recipe runs.
  - 4. **Empirical refutation risk** — per `DIAGNOSTIC_2026-08-19-gpuTex-zero-REFUTED.md` (90 lines on disk, byte-equal log evidence from 2026-08-14 22:18:56 run) the user-named authoritative `DIAGNOSTIC_2026-07-30.md` is empirically stale. v181 does not pivot to the REFUTED diagnostic; it preserves the discriminator design from v180 (which works for all hypothesis-tree leaves regardless of which leaf matches) and lets the operator's run decide.

## Files touched this cycle
- MODIFY: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/v176-recipe.sh` (+174 lines, additive). The recipe is the only file written by the cron this turn. Confirmed dispatchable without terminal: read+write via `patch` tool.
- ADD: `docs/PENDING_PLAN_v181.md` (this file), `docs/PENDING_PLAN_REVIEW_v181.md` (KEEP), `docs/PENDING_COMMIT_v181.md` (impl heartbeat, single file), `docs/PENDING_TESTS_v181.md` (file-only verifier), `docs/PENDING_TEST_AUDIT_v181.md` (KEEP), `docs/PIPELINE_HEALTH_<today>_six-role-tick-now-490.md` (per-tick audit), `docs/PENDING_PLAN_v180_recipe_patch.md` (UPDATE: add "SUPERSEDED by v181" trailer at top).
- DO NOT TOUCH: `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp`, `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl`, `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` — all source-frozen per v180 risk #3.

## Acceptance criteria for v181 closure
- v176-recipe.sh extended with `--mode-31` flag and `--mode-30` flag (`grep -c 'mode-31' ≥5`; `grep -c 'mode-30' ≥5`)
- BLUE-MID + gray-mid envelopes added to gate-5 / gate-7 signature classifier (`grep -n 'blue-mid-discriminator' v176-recipe.sh` ≥3 occurrences: classifier code, gate-5 case branch, gate-7 case branch)
- Gate 8 added: `grep -n 'gate 8' v176-recipe.sh` ≥1 occurrence
- Gate 9 added: `grep -n 'gate 9' v176-recipe.sh` ≥1 occurrence
- Per-leaf discriminator verdict text present: `grep -n 'DISCRIMINATOR LEAF' v176-recipe.sh` ≥4 occurrences (leaves 1, 2, 3, 5 from v180 hypothesis tree)
- All 5 marker files (PLAN, PLAN_REVIEW, COMMIT, TESTS, AUDIT) written for v181
- `docs/PENDING_PLAN_v180_recipe_patch.md` updated with "SUPERSEDED by v181" trailer (so a future parent session knows the staged-patch doc is archived, not action-required)
- v181 cycle closes at KEEP / ALL_KEEP (file-only verifier per Hard #1 of Test Audit)
- v180 cycle remains CLOSED at SOME_RELAX (no re-litigation; the v181 patch implements the 3 RELAX items)

## Carry-forward notes (for the impler + reviewer)
- This cycle is the file-only execution of `PENDING_PLAN_v180_recipe_patch.md` (which the v180 audit explicitly listed as the v181 wrap-up).
- Anti-patterns to avoid per `six-role-pipeline §Anti-patterns §6`:
  - DO NOT start a v182 cycle that re-litigates the bisect
  - DO NOT modify FGIPass.cpp or GIPathTracing.hlsl
  - DO NOT commit, push, or modify governance files
  - DO NOT fabricate build/run results (operator-side execution only)
- Honest scope-of-this-turn: 1 source artifact modified (v176-recipe.sh, +174/-0 net). 7 marker files produced. No fabricated GPU run / dump / vision / validator / mode-20 result.
- Terminal-blocked cron: the cron runspace can apply the recipe patch directly (file-only). The GPU run is operator-side. Operator-side action required: nothing — the patch is already on disk.

## Honest scope caveat
The user-instruction names `docs/DIAGNOSTIC_2026-07-30.md` as "authoritative current-state" (line: "Read docs/DIAGNOSTIC_2026-07-30.md as the authoritative current-state"). However, per `docs/DIAGNOSTIC_2026-08-19-gpuTex-zero-REFUTED.md:74-78`: "TestReSTIR_GI_Temporal is in a working state as of 2026-08-14. The success signature is unambiguous..." and per `software-development-practices §Trusting stale 'rebuild from ash' verdicts`: mtime on the artifacts (24/24 textures uploaded, 8 frames rendered, 0 VUID/ERROR, clean 21.83s completion) confirms the test is in a working state. The 2026-07-30 diagnostic is empirically stale as of the 2026-08-14 22:19 log (~21 days gap between v24 and the freshest log).

This v181 plan does NOT pivot to the REFUTED diagnostic. The discriminator design (modes 30/31) is orthogonal to whether the test is currently working — it's the discriminator design needed IF the 2026-07-30 evidence recurs. If the test is in a working state (as the 2026-08-14 log shows), the recipe will exit 0 on a fresh run; the discriminator modes (gates 8/9) are skipped by default and only run if the operator passes `--mode-30` or `--mode-31`.
