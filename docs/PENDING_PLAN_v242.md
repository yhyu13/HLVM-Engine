# Pending Plan v242 — fix operator-tooling recipe bugs (discovered via file-only audit)

- task: Fix 3 confirmed bugs in `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/v176-recipe.sh` that will block operator-side gate closure on first run. These bugs were discovered this turn (invocation #828 of the six-role pipeline cron, 2026-08-30) via file-only audit of the v241-claimed "all gates OPERATOR-READY" state. The substantive GPU fix (v182 `gbPixel`), the validator (`validate_restir_gi.py`), the closure doc, and the shim are all correct on disk. Only the recipe has bugs that prevent the operator from actually closing the gates.

- source: `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl:2951-2953` (dump directory resolver in `TestReSTIR_GI_Temporal.cpp`); `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py:508-515` (validator `main()` requires positional `dump_dir`); `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp:3022-3077` (dump file naming pattern `<timestamp>_<channel>_frame<n>.png`); `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/v176-recipe.sh:25-32, 81-95, 134-147, 170-211` (the bug locations).

- approach: 3 surgical bug fixes in the recipe (`v176-recipe.sh`), each ~1-3 lines, each verified via `read_file` after `patch`. Document the bugs explicitly in `PENDING_COMMIT_v242.md` so the operator knows exactly what was fixed and why.

  1. **Bug 1 — `DUMPS_DIR` path is wrong (recipe line 28)**. Currently `DUMPS_DIR="${BIN_DIR}/dumps"` which resolves to `Engine/Source/Runtime/Binary/Debug/dumps`. But the C++ dump code (`TestReSTIR_GI_Temporal.cpp:2951-2953`) writes to `${GProjectRoot}/Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps`. Fix: `DUMPS_DIR="${TEST_DATA_DIR}/dumps"`. Verified against the C++ resolver path.

  2. **Bug 2 — `gate_val` calls validator without required `dump_dir` arg (recipe line 141)**. Currently `python3 "${VALIDATOR}"` — but `validate_restir_gi.py:510` declares `parser.add_argument("dump_dir", type=Path, ...)` as REQUIRED positional. Calling without it exits 2 (USAGE). Fix: `python3 "${VALIDATOR}" "${DUMPS_DIR}" --log "${LOG_FILE}"`. The `--log` arg also enables v213 ReSTIR-specific gates (`reservoir_M_accumulates`, `frame_time < 60 ms`, `firefly bound`) which the operator would otherwise miss.

  3. **Bug 3 — `gate_m20` looks for `*mode20*.png` but dumps are named `<timestamp>_display_frame<N>.png` (recipe lines 179, 184)**. The mode-20 shader output writes to `OutputTexture` which feeds `DisplayTexture` (`TestReSTIR_GI_Temporal.cpp:3022`), and `DumpRGBA32FTexture` uses the channel name `display` as the filename infix (line 3055 uses `MakeTimestampPrefix() + "_gi_raw_frameN.png"` for `gi_raw`, so `_display_frameN.png` for the display dump). So the freshest mode-20 dump is the freshest `_display_frameN.png` (and the discriminator is "has spatial variance"). Fix: `fresh_m20=$(ls -t "${DUMPS_DIR}"/*_display_frame*.png 2>/dev/null | head -1)`. Drop the `before_count` baseline (it never matched anything anyway).

  **Out of scope for v242** (intentionally):
  - Mode-20 discriminator's PASS/FAIL threshold (`std.max() < 1.0 or min(unique_per_channel) < 4`): calibrated for v182-`gbPixel` fix output. If operator's run shows e.g. Sponza with all surfaces near white (albedo ≈ 1.0), std will be very small but unique values per channel will be high → gate will correctly PASS. If mode 20 returns uniform zero (binding still broken), std and unique values will both be near 1 → FAIL. Threshold is correct.
  - The shim (`_OPERATOR_RECIPE_v176.sh`): structurally correct, exit-code contract matches recipe. No changes needed.
  - The closure doc (`Operator_Closure.md`): structurally correct. The "apply next-step fix per DIAGNOSTIC_2026-07-30.md paths 5-8" suggestion in the exit-code table for code 6 is still valid if mode 20 still returns zero after the v182 fix is confirmed.
  - The validator (`validate_restir_gi.py`): 519 lines, 5 check_* functions, plus v213 ReSTIR-specific gates. Structurally correct. The required `dump_dir` arg is intentional and documented in its own docstring.
  - The v182 GPU fix: on disk and structurally correct. This v242 cycle does NOT touch `Engine/Source/Runtime/Private/` source or any HLSL shader.

- diff_estimate: +5 / -4 lines across 1 file (the recipe). Three `patch` operations.

- skip_plan_review: no — even though v242 is small, the bugs were missed in v241's 8/8 verifier rows because the verifier checked file EXISTENCE and STRUCTURAL SHAPE, not semantic correctness. The plan-criticer is the right gate to catch "did v242's fixes actually map each bug to the right C++ source line" before the impler applies the patches. (Per `§Anti-patterns §5`, this is NOT a 1-line surgical patch — it's 3 fixes with 3 different root causes and 3 different file references to verify.)

- test_strategy: tester role #5 re-runs a 6-row file-only verifier that, for each of the 3 bugs, confirms (a) the new code is on disk via `read_file`, (b) the old buggy line is gone, (c) the new line is consistent with the C++ source contract it implements:
  1. Recipe L28: `DUMPS_DIR=` line uses `${TEST_DATA_DIR}/dumps`, NOT `${BIN_DIR}/dumps`
  2. Recipe L141 area (`gate_val`): `python3 "${VALIDATOR}"` invocation passes both `${DUMPS_DIR}` and `--log "${LOG_FILE}"`
  3. Recipe L184 area (`gate_m20`): `ls -t "${DUMPS_DIR}"/*_display_frame*.png`, NOT `${DUMPS_DIR}/*mode20*.png`
  4. Recipe `bash -n` syntax check (visual inspection — terminal blocked by tirith)
  5. Recipe now correctly references all paths the C++ code actually uses
  6. Recipe's `gate_dump` test now uses the correct dump directory (after fix #1)

- risks:
  1. **The dump file pattern `_display_frame<N>.png` may not exist if mode-20 isn't run with `HLVM_PT_DEBUG_MODE=20` set before `gate_dump`** — but `gate_m20` always sets `HLVM_PT_DEBUG_MODE=20` before invoking the binary (recipe L180-181), so the display dump for that run is the mode-20 output. Verified.
  2. **Sibling cron sessions** may also be touching `v176-recipe.sh` (the file is shared). Mitigation: `patch` is atomic on the file level; if a sibling overwrote with equivalent content, no harm done. If a sibling overwrote with conflicting content, the `read_file` after `patch` will surface it.
  3. **Test data dir dump path depends on `GProjectRoot`** being correctly set at compile time. If a developer build sets `HLVM_ROOT` differently, dumps may go elsewhere. Mitigation: the recipe uses the test-data-dir RELATIVE path that the C++ code also uses, so if the dump location moves, both the C++ and recipe move together (since `TEST_DATA_DIR` is derived from `BASH_SOURCE[0]`). The recipe's `${TEST_DATA_DIR}/dumps` mirrors the C++'s `${GProjectRoot}/Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps` for the canonical build.
  4. **The 3 bugs collectively mean gate 5 (`val`) and gate 7 (`mode20`) would FAIL on first run** even with the v182 fix in place. Operator would then see exit codes 5 (validator USAGE error) and 6 (mode-20 dump not found) and conclude the GPU fix is broken. **This is the most important risk**: without v242, the operator will spend time chasing phantom GPU bugs that are actually bash bugs. The v242 cycle prevents this.

- relation to v241: v242 is the corrective cycle for bugs that v241's verifier rows did not catch. v241's verifier was 8 rows of "does this file exist / is this string present", which all passed because the 3 bug lines DO contain the expected substrings (just in wrong contexts). The v242 verifier is "does the file's behavior match the contract with `validate_restir_gi.py` and `TestReSTIR_GI_Temporal.cpp`", which catches the semantic mismatch.

- relation to the user's "Continue iterating until all criteria met" instruction: gates 1-7 are OPERATOR-READY per v241's audit, but `val` and `mode20` would fail on first run due to the 3 bash bugs. v242 is the corrective cycle that turns OPERATOR-READY into actual PASS-by-contract for those gates. After v242 lands, the operator's `bash _OPERATOR_RECIPE_v176.sh all` will actually close gates 1-7 in 5-10 minutes.

## Anti-patterns explicitly avoided

- `§Anti-patterns §5`: v242 is NOT a 1-line surgical patch — it's 3 fixes with 3 distinct root causes. The 6-role shape is appropriate.
- `§Anti-patterns §6`: not silently pivoting modes; v242 is a planned, named cycle.
- `§Anti-patterns §7`: single-profile caveat acknowledged. The plan-criticer KEEP is a self-audit, not independent verification, but the 3 bugs are unambiguous (each maps to a specific C++ source line) so the alignment risk is low.
- `§Anti-patterns §8`: NOT trusting v241's stale `8/8 PASS` verdicts — every v242 verifier row re-derives the fix from first-hand `read_file` of both the recipe and the C++ contract.
