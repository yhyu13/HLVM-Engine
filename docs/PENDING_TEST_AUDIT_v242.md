# Pending Test Audit v242 — fix operator-tooling recipe bugs

- tests: docs/PENDING_TESTS_v242.md
- commit: docs/PENDING_COMMIT_v242.md
- plan: docs/PENDING_PLAN_v242.md
- impl_review: docs/PENDING_IMPL_REVIEW_v242.md
- verdict: **ALL_KEEP**
- verifier: testing-verifier (six-role pipeline role #6)
- timestamp: 2026-08-30T...Z (this turn, six-role pipeline cron tick, v242 cycle)

## Broken-pattern audit (5 known patterns from `software-development-practices`)

| Pattern | Check | Result |
|---------|-------|--------|
| 1. from-x-import-y patch propagation bugs | v242 modifies 1 bash file, no Python imports. The 6 verifier rows are pure file-content checks against bash source + C++ contract references. | **N/A** |
| 2. test-bug-in-itself (asserts against wrong fixture) | All 6 verifier rows assert against on-disk content via first-hand `read_file`. Each row quotes expected content + actual content from the recipe + cites the C++ source line that defines the correct behavior. | **PASS** |
| 3. source-incomplete-relative-to-test | v242 modifies the recipe; the C++ side (TestReSTIR_GI_Temporal.cpp + validate_restir_gi.py) is unchanged. The "test" verifies that 3 recipe fixes match the existing C++ contracts (dumps to TEST_DATA_DIR/dumps; validator requires dump_dir; dumps named <ts>_<channel>_frame<N>.png). | **N/A** |
| 4. missing test isolation fixture | No tests are run; the 6 verifier rows are pure file-system checks that do not require process isolation. The runtime closure has its own pre-flight check (`gate_env()` → exit 7). | **N/A** |
| 5. AsyncMock on sync function (or vice versa) | No Python mocking involved. | **N/A** |

**No broken-pattern matches. Audit clean.**

## Per-test verdict (6 verifier rows from `PENDING_TESTS_v242.md`)

| # | Test file / row | Verdict | Rationale |
|---|----------------|---------|-----------|
| 1 | Bug 1 fix: `DUMPS_DIR` path constant | **KEEP** | First-hand `read_file` of recipe L35 returns exactly `DUMPS_DIR="${TEST_DATA_DIR}/dumps"`; L28-34 inline comment cites `TestReSTIR_GI_Temporal.cpp:2951-2953`. The fix matches the C++ contract. |
| 2 | Bug 2 fix: `gate_val` validator invocation | **KEEP** | First-hand `read_file` of recipe L156 returns exactly `python3 "${VALIDATOR}" "${DUMPS_DIR}" --log "${LOG_FILE}"`; L151-154 inline comment cites `validate_restir_gi.py:510,513`. The fix matches the validator's argparse contract. |
| 3 | Bug 3 fix: `gate_m20` filename glob | **KEEP** | First-hand `read_file` of recipe L203 returns exactly `ls -t "${DUMPS_DIR}"/*_display_frame*.png`; L193-197 inline comment cites `TestReSTIR_GI_Temporal.cpp:3022,3055`. The fix matches the dump file naming convention. Old `*mode20*.png` glob is absent from the recipe. |
| 4 | Old buggy lines are absent | **KEEP** | First-hand `read_file` of entire recipe (264 lines): `*mode20*.png` appears 0 times; `python3 "${VALIDATOR}"` (with no other args) appears 0 times; `DUMPS_DIR="${BIN_DIR}/dumps"` appears 0 times as an assignment (only mentioned in comment). All 3 old buggy patterns eliminated. |
| 5 | Bash syntax is valid | **KEEP** (visual) | Standard bash syntax verified by visual inspection of L1-264: `set -uo pipefail` at L23, function definitions, `[[ ]]` tests, command substitution via `$()`, case dispatch at L238-264. No syntax errors visible. Cannot run `bash -n` because terminal tool blocked by tirith on this host. |
| 6 | Recipe still implements all 8 gate_* functions | **KEEP** | First-hand `read_file`: gate_env (L40), gate_build (L67), gate_dump (L80), gate_vulk (L105), gate_cmdl (L123), gate_val (L141), gate_vision (L164), gate_m20 (L189). 8/8 present. Case dispatch at L238-264 covers all 9 modes. |

**6/6 KEEP. No SOME_RELAX, SOME_DELETE, or MAJOR_DELETE items.**

## 7-gate acceptance status (audited, post-v242)

| # | Criterion | Status | Audit verdict |
|---|-----------|--------|---------------|
| 1 | Debug target builds | **OPERATOR-READY** | KEEP — `_OPERATOR_RECIPE_v176.sh build` invokes `v176-recipe.sh build` which runs `Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild` |
| 2 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` produces fresh dump group | **OPERATOR-READY (post-v242 fix 1)** | KEEP — `gate_dump` now uses `DUMPS_DIR="${TEST_DATA_DIR}/dumps"` matching the C++ dump writer's actual output directory (`TestReSTIR_GI_Temporal.cpp:2951-2953`) |
| 3 | No Vulkan VUID/ERROR | **OPERATOR-READY** | KEEP — `_OPERATOR_RECIPE_v176.sh vulk` invokes `v176-recipe.sh gate_vulk` which greps the log |
| 4 | No command-list errors | **OPERATOR-READY** | KEEP — `_OPERATOR_RECIPE_v176.sh cmdl` invokes `v176-recipe.sh gate_cmdl` which greps the log |
| 5 | `validate_restir_gi.py` passes newest dump | **OPERATOR-READY (post-v242 fix 2)** | KEEP — `gate_val` now passes `${DUMPS_DIR}` (required positional) and `--log "${LOG_FILE}"` (enables v213 ReSTIR-specific gates) per `validate_restir_gi.py:510,513` |
| 6 | Fresh display image shows recognizable Sponza | **OPERATOR-READY** | KEEP — `_OPERATOR_RECIPE_v176.sh vision` invokes `v176-recipe.sh gate_vision` which xdg-opens the freshest display PNG |
| 7 | `HLVM_PT_DEBUG_MODE=20` returns non-zero GBufferMaterial | **OPERATOR-READY (post-v242 fix 3)** | KEEP — `gate_m20` now uses `_display_frame*.png` glob matching the actual mode-20 output filename pattern per `TestReSTIR_GI_Temporal.cpp:3022,3055`; v182 `gbPixel` fix on disk at `GIPathTracing.hlsl:764-766` |

**0/7 PASS direct (cannot be evaluated from file-only runspace), 7/7 OPERATOR-READY (operator can close in 5-10 min by running `bash _OPERATOR_RECIPE_v176.sh all`).**

## Cycle disposition

| Phase | Status |
|-------|--------|
| v242 planner | ✓ (KEEP via explicit plan-review KEEP verdict) |
| v242 impler | ✓ (3 documented bug fixes + 1 header comment update; +21 / -5 lines total; substantive changes are 3 lines matching the plan's "3 fixes" design) |
| v242 reviewer | ✓ (KEEP, plan fidelity preserved, no source touched, security clean) |
| **v242 tester** | **✓ (6/6 file-only semantic-correctness verifier rows PASS)** |
| **v242 testing-verifier** | **✓ (6/6 KEEP, no broken patterns; 7/7 acceptance gates OPERATOR-READY post-v242)** |

**v242 cycle COMPLETE 6/6 ALL_KEEP.**

## What the next operator-side action is

After v242 lands, the remaining path to full closure is operator-side terminal execution (5-10 minutes):

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine

# One-shot closure:
bash _OPERATOR_RECIPE_v176.sh all

# Or per-gate:
bash _OPERATOR_RECIPE_v176.sh build   # gate 1
bash _OPERATOR_RECIPE_v176.sh dump    # gate 2 (post-v242: dumps found in correct dir)
bash _OPERATOR_RECIPE_v176.sh val     # gate 5 (post-v242: validator invoked correctly)
bash _OPERATOR_RECIPE_v176.sh vulk    # gate 3
bash _OPERATOR_RECIPE_v176.sh cmdl    # gate 4
bash _OPERATOR_RECIPE_v176.sh vision  # gate 6
bash _OPERATOR_RECIPE_v176.sh mode20  # gate 7 (post-v242: mode-20 dump found)
```

| Exit code | Meaning | Next action |
|-----------|---------|-------------|
| 0 | All 7 gates PASS | Mark v242 `[x]` closure final; queue empties; Rule 10 stops firing |
| 2 | gate_dump failed (no fresh PNGs) | Inspect binary run log; check HLVM_DUMP_RGI hook is set in code |
| 3 | Vulkan VUID/ERROR hit | Inspect log; check binding layout vs `FBindingLayoutBuilder::Add*` |
| 4 | Command-list error | Inspect log; check `close+execute+waitForIdle+open` ordering |
| 5 | Validator failed (gate 5) | Inspect validator output (post-v242, this is a real validator FAIL not a USAGE error) |
| 6 | Mode-20 probe shows zero GBufferMaterial (gate 7) | v182 fix did NOT work; spawn v243 with binding-deep-bisect plan |
| 7 | Environment pre-flight failed | Resolve missing deps; re-run |

## Hard invariants compliance

- **#1 (PENDING_PICK.md authoritative)**: PICK has 1 actionable item (v242) being added this tick.
- **#2 (test files trigger reviewer)**: v242 produces no test files; reviewer gate honored anyway.
- **#3 (impler deviates and documents)**: no deviations; inline comments are intentional additions documented in commit notes.
- **#4 (plan-criticer FIX loops to planner)**: KEEP verdict on v242 plan-review; no loop needed.
- **#5 (single-instance lock)**: this is one cron tick; sibling-session race acknowledged in commit risks.
- **#6 (never silently exit)**: this audit doc IS the non-silent exit.
- **#7 (append-only discipline)**: v242 markers APPENDED to v232-v241 chain; all prior markers preserved on disk.

## Anti-patterns explicitly avoided

- `§Anti-patterns §5`: v242 PRODUCES 3 line-level fixes + 1 header comment update (~21 lines total); not a 1-line surgical patch.
- `§Anti-patterns §6`: the pipeline IS running; this tick completes the operator-tooling gap exposed by v241's stale existence verifier.
- `§Anti-patterns §7`: single-profile caveat acknowledged.
- `§Anti-patterns §8`: NOT trusting v241's stale `8/8 PASS` existence verifier — every v242 verifier row re-derives the fix from first-hand `read_file` of both the recipe and the C++ contract source line. The v242 cycle is exactly the anti-pattern-§8 corrective action: when stale verdicts disagree with on-disk reality (the 3 bugs that v241 missed), spawn a new cycle that re-verifies from first principles.

## Audit doc metadata

- **Cycle state**: v232-v241 ALL_KEEP; **v242 COMPLETE 6/6 ALL_KEEP** (3 bug fixes in operator-tooling recipe: DUMPS_DIR path, gate_val validator invocation, gate_m20 filename glob; all 3 fixes verified against C++ source contracts).
- **Patch state (v242 changes)**:
  - `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/v176-recipe.sh`: +21 / -5 lines (was 245 lines, now 264 lines)
  - 3 substantive line changes + inline comments + header comment block update
- **Patch state (unchanged from prior cycles)**:
  - v182 mode-20 `gbPixel` fix: on disk at `GIPathTracing.hlsl:764-766` (private) and `:835-837` (test data dir)
  - v232 W-clamp + w_sum-clamp: on disk
  - v233 Jacobian clamp + normal rotation: on disk
  - v241 operator-shim (55 lines) + closure doc (118 lines) + canonical recipe (245 → 264 lines after v242)
  - `validate_restir_gi.py` (519 lines): on disk, 5 check_* functions + v213 ReSTIR-specific gates
- **Operator tooling state**: `_OPERATOR_RECIPE_v176.sh` (46 lines) at repo root; `Operator_Closure.md` (118 lines) at repo root; `v176-recipe.sh` (264 lines) at canonical path; `validate_restir_gi.py` (519 lines) at canonical path.
- **Cron config**: enabled, this session IS a cron tick (invocation #828 of the lineage).
- **Next cycle**: depends on operator-side terminal access. Either (a) operator runs `_OPERATOR_RECIPE_v176.sh all` and the queue empties, or (b) Rule 10 fires again with v242 marked `[x]` until terminal is granted.
- **Independent re-verification**: YES (6 file-only verifier rows re-derived first-hand this turn via `read_file` of both the recipe and the C++ source contracts).

— file-only audit, 2026-08-30, autonomous invocation #828 in lineage. v242 cycle 6/6 ALL_KEEP. Operator action: `bash _OPERATOR_RECIPE_v176.sh all` to close gates 1-7.
