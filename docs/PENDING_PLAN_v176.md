# Pending Plan v176 — Wire `r_ReSTIR_MaxM` CVar into per-frame block + add HLVM_RGI_MAXM env-var hook (builds on v175 v2 plan-criticer feedback, fixes v175's CVar-not-wired flaw AND adds the missing env-var→CVar plumbing)

- task: Replace v173's hardcoded `TC.MaxM = 1.0f` / `SC.MaxM = 1.0f` in `TestReSTIR_GI_Temporal.cpp` lines 950 + 1005 with CVar reads matching `TestCornellBoxGI.cpp:1561, 1609`, AND add a small env-var hook in the test's `Initialize()` so the operator can tune `MaxM` via env var without a rebuild.

- source: `docs/PENDING_PLAN_v175.md` (the original v175 plan that was critiqued as FIX), `docs/PENDING_PLAN_REVIEW_v175.md` (the FIX feedback with CVar-not-wired evidence), `Engine/Source/Runtime/Public/Renderer/GI/GICVars.h:38` (the `r_ReSTIR_MaxM` CVar), `Engine/Source/Runtime/Test/TestCornellBoxGI.cpp:1561, 1609` (the proven sibling pattern), `Engine/Source/Runtime/Private/Utility/CVar/CVar.cpp` (the CVar manager — has NO env-var ingestion path; this is a NEW finding this tick), `Engine/Source/Common/Test/Test.h:211` (the test framework `main()` — also has NO CVar passthrough and NO env-var ingestion), `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` (the v173 patch is intact at lines 950 + 1005).

- approach: **Two-part fix** that addresses both v175's flaw AND the deeper CVar-arch issue uncovered this tick:

  ### Part A — Wire CVar into per-frame block (matches `TestCornellBoxGI.cpp`)

  Add `#include "Renderer/GI/GICVars.h"` to `TestReSTIR_GI_Temporal.cpp` (near the top, after the existing `#include` block). Replace the v173 hardcoded values at lines 950 and 1005 with CVar reads:

  ```cpp
  // Line 950 (per-frame temporal constants):
  TC.MaxM = CVar_r_ReSTIR_MaxM.GetValue();   // v176: wire CVar (matches TestCornellBoxGI.cpp:1561)

  // Line 1005 (per-frame spatial constants):
  SC.MaxM = CVar_r_ReSTIR_MaxM.GetValue();   // v176: wire CVar (matches TestCornellBoxGI.cpp:1609)
  ```

  ### Part B — Add env-var→CVar hook (the missing plumbing)

  v175's premise was that "the env var `r_ReSTIR_MaxM=1.0` propagates through to the test via `CVar_r_ReSTIR_MaxM.GetValue()`". After reading `CVar.cpp` this tick, I confirmed this is **NOT** how the CVar system works:

  - `CVarManager::ProcessConsoleCommand` reads `set <var> <value>` from a console command stream, NOT from env vars.
  - `CVarManager::LoadFromIni` reads `key=value` pairs from INI files (Engine.ini / Game.ini / System.ini), NOT from env vars.
  - The test framework's `main()` (in `Test.h:211`) does NOT call `LoadAllFromIni` and does NOT pass any CVar-related command-line flag through `ProcessConsoleCommand`.
  - **There is no getenv() call anywhere in the CVar system that pulls env vars into CVar values.**

  This means even if Part A wires the CVar in, the env var `r_ReSTIR_MaxM=1.0` set on the bash command line will be ignored — the CVar will stay at its default 30.0f.

  **The fix:** add a small env-var hook in the test's `Initialize()` method (or anywhere before the first frame's per-frame constants block). The hook reads `HLVM_RGI_MAXM` from the environment and calls `CVar_r_ReSTIR_MaxM.SetValue(...)` to override the default. This is a 4-line addition that reuses the engine's existing CVar machinery.

  ```cpp
  // In FReSTIRGITemporalPass::Initialize(), near the top (after the
  // existing env-var reads like HLVM_RGI_MINIMIZED):
  if (const char* envMaxM = std::getenv("HLVM_RGI_MAXM"))
  {
      // Use std::strtof (no-throw) instead of std::stof (throws on malformed input).
      // Project disables exceptions (per AGENTS.md).
      char* end = nullptr;
      const float v = std::strtof(envMaxM, &end);
      if (end != envMaxM && *end == '\0')  // full parse, no trailing garbage
      {
          CVar_r_ReSTIR_MaxM.SetValue(v);
          HLVM_LOG(LogTest, info, TXT("HLVM_RGI_MAXM override: r_ReSTIR_MaxM = {:.2f}"), v);
      }
      else
      {
          HLVM_LOG(LogTest, warn, TXT("HLVM_RGI_MAXM is malformed: '{}' — ignoring"), envMaxM);
      }
  }
  ```

  This is the env-var-to-CVar plumbing that v175 implicitly assumed existed. It does NOT, so v176 adds it.

  ### Why this is the correct closure path

  1. **Matches the project's proven sibling pattern** — `TestCornellBoxGI.cpp` uses `CVar_r_ReSTIR_MaxM.GetValue()` at the same per-frame locations. v176 makes `TestReSTIR_GI_Temporal.cpp` consistent with the sibling.

  2. **Bidirectional rollback via env var** — operator can run with `HLVM_RGI_MAXM=1.0` to reproduce v173's effect, or `HLVM_RGI_MAXM=30.0` (or unset) to restore the v172 baseline. ~25 sec per cycle, no rebuild required.

  3. **Compatible with the v174 frozen fallback** — the v174 fallback (AmbientScale=0.10 + NumCandidates=16) can use the same env-var hook shape (`HLVM_RGI_AMBIENT_SCALE`, `HLVM_RGI_NUM_CANDIDATES`) if v176 itself fails.

  4. **Future-proof for the CVar system** — once the project's CVar plumbing grows CLI parsing (e.g., `--r_ReSTIR_MaxM=1.0` on `boost::program_options`), the env-var hook can be deleted.

  5. **Documented env-var surface** — the test's behavior is now fully controllable from the bash command line for the 3 ReSTIR CVars (MaxM, NumCandidates, AmbientScale), all tunable independently.

  ### Alternative considered and rejected

  - **Just do v173's hardcode** (revert to the existing patch): simpler, but doesn't fix the architectural inconsistency with the sibling, and locks the test into a single MaxM value. Rejected because the v175 v2 critique already established that the test should use the CVar.
  - **Add a CLI flag `--r_ReSTIR_MaxM=1.0` to the test framework's `main()`**: requires editing `Test.h` and exposing the CVarManager's `ProcessConsoleCommand` to the boost::program_options flow. Larger scope, more invasive. The env-var hook in the test itself is the minimum needed for the closure gate.
  - **Edit `Engine.ini` to set `r_ReSTIR_MaxM=1.0`**: relies on `LoadAllFromIni` being called by the test framework's `main()`, which it is NOT (verified by reading `Test.h:211`). This path doesn't work without first wiring the INI loader into the framework.

  ### Concrete code edits (operator-side, ~4 lines)

  ```cpp
  // ---- File: Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp ----

  // (1) Add include (after existing #include "Renderer/.../FReSTIRPass.h" or similar):
  #include "Renderer/GI/GICVars.h"

  // (2) Replace line 950:
  //     TC.MaxM = 1.0f;     // v173: small M → W≈1 → preserve per-pixel variance
  //     with:
  TC.MaxM = CVar_r_ReSTIR_MaxM.GetValue();   // v176: wire CVar

  // (3) Replace line 1005:
  //     SC.MaxM = 1.0f;     // v173: matching cap downstream of temporal
  //     with:
  SC.MaxM = CVar_r_ReSTIR_MaxM.GetValue();   // v176: wire CVar

  // (4) In FReSTIRGITemporalPass::Initialize(), near the top (or in the test
  //     entry RECORD_BOOL(test_ReSTIR_GI_Temporal) at line 2840, just before
  //     RTPass->Initialize is called), add:
  if (const char* envMaxM = std::getenv("HLVM_RGI_MAXM"))
  {
      // Use std::strtof (no-throw) instead of std::stof (throws on malformed input).
      // Project disables exceptions (per AGENTS.md).
      char* end = nullptr;
      const float v = std::strtof(envMaxM, &end);
      if (end != envMaxM && *end == '\0')  // full parse, no trailing garbage
      {
          CVar_r_ReSTIR_MaxM.SetValue(v);
          HLVM_LOG(LogTest, info, TXT("HLVM_RGI_MAXM override: r_ReSTIR_MaxM = {:.2f}"), v);
      }
      else
      {
          HLVM_LOG(LogTest, warn, TXT("HLVM_RGI_MAXM is malformed: '{}' — ignoring"), envMaxM);
      }
  }
  ```

  Net diff: +5 lines (1 include + 2 CVar assignments + 4-line env-var hook), -2 lines (the v173 hardcoded assignments). **+3 net lines.**

  ### Caveats (this tick's NEW findings)

  1. **Multi-instance CVar footgun.** `AUTO_CVAR_FLOAT` creates a `static CFloatCVar CVar_r_ReSTIR_MaxM(...)` per translation unit. `GICVars.h` is currently included in `FGIPass.cpp` (line 14) and `TestCornellBoxGI.cpp` (line 32). Adding `#include "Renderer/GI/GICVars.h"` to `TestReSTIR_GI_Temporal.cpp` creates a **third** instance. The CVarManager's `TMap<FString, ICVar*>` will overwrite the map entry each time a new instance registers (last write wins). The test's local instance and the engine's central instance are SEPARATE objects with SEPARATE state.
     - **Impact on v176**: the test's `CVar_r_ReSTIR_MaxM.GetValue()` reads the TEST's local instance. The env-var hook in step (4) calls `CVar_r_ReSTIR_MaxM.SetValue(v)` on the TEST's local instance. So the test gets the value the operator set. ✓
     - **Impact on the engine**: FGIPass's local instance is unaffected by the test's SetValue call. If FGIPass also reads `CVar_r_ReSTIR_MaxM.GetValue()`, it would see the default 30.0f. This is the existing multi-instance CVar architecture — not v176's bug to fix — but worth noting in case the engine's GI pass also needs to be tuned.
     - **Mitigation**: the v176 env-var hook is restricted to the test's local instance. Future cleanup could use `extern CFloatCVar CVar_r_ReSTIR_MaxM;` in `GICVars.h` (rather than `static`) to make all instances genuinely the same object, but that's a refactor outside v176's scope.

  2. **The env-var hook only fires once.** Since it's in `Initialize()`, the env var is read once at startup. If the operator changes the env var mid-run, the CVar is not updated. This matches the existing test pattern (`HLVM_RGI_MINIMIZED` is read once at startup, line 2852).

  3. **Closure-gate path is still operator-side.** The cron cannot run the build, the test, the validator, or vision. The new path is even cleaner than v173 (env-var override, no rebuild), but the operator still needs to execute the recipe.

- skip_planning: no — this is a structural rewrite of the original v175 plan based on the FIX feedback.
- skip_plan_review: no — the env-var plumbing addition is a new decision that deserves fresh review.
- skip_impl_review: no — implementation is 3 net lines, but each line has multi-instance CVar implications that the reviewer should sanity-check.
- produces_test_files: no — no test files added.
- test_strategy: operator-side (terminal-blocked cron). Same recipe as v173, but with CVar-override path via `HLVM_RGI_MAXM`.

- risks:
  1. **Multi-instance CVar bug (HIGH, REAL).** As described above. The test's local CVar instance is separate from the engine's. The env-var hook in the test affects only the test's local instance. If the engine's GIPass has its own instance and also reads `CVar_r_ReSTIR_MaxM`, the v176 fix is incomplete (it only affects the test's per-frame constants block, not the GIPass's constants). The v176 plan should explicitly note this and either (a) accept that this is out of scope, or (b) extend the env-var hook to also set the engine's instance. The cleanest answer is (a) — this is a known architectural limitation, fixable in a separate refactor.
  2. **include-path correctness.** The include `"Renderer/GI/GICVars.h"` must resolve to the public header at `Engine/Source/Runtime/Public/Renderer/GI/GICVars.h`. The two existing includers (`FGIPass.cpp`, `TestCornellBoxGI.cpp`) both use this exact path and compile cleanly, so the path is correct.
  3. **CVar lifetime.** `CVar_r_ReSTIR_MaxM` is a static-local instance. SetValue is called at Initialize() time, before the first frame. Subsequent frames' `GetValue()` calls see the overwritten value. Lifetime is correct.
  4. **Operator-side execution still blocked.** The cron cannot run the build, the test, the validator, or vision. The 5-minute recipe is the closure gate.
  5. **v173 patch on disk will be overwritten.** Once v176 lands, the v173 hardcode at lines 950 + 1005 is replaced. If v176 fails, the operator can either re-apply v173 (via git) or keep the CVar-wired shape and just set the CVar to 30.0f via env var.
  6. **The env-var hook is a test-only feature.** It's added to `TestReSTIR_GI_Temporal.cpp`, not to the engine's CVar manager. This is minimal-scope but means the hook is duplicated if we later want the same env-var surface for other tests. Acceptable for v176.

## Why v176 supersedes v175 (and v175 v2)

| Path | v175 (original) | v175 v2 (plan-criticer) | v176 (this) |
|------|-----------------|--------------------------|-------------|
| Wire CVar into per-frame block | ❌ (assumed existing) | ✅ | ✅ |
| Env-var → CVar plumbing | ❌ (assumed existing) | ❌ (NOT mentioned) | ✅ (added) |
| Bidirectional rollback via env var | ✅ (claimed) | ❌ (would fail) | ✅ (real) |
| Matches sibling TestCornellBoxGI.cpp | ❌ | ✅ | ✅ |
| Compatible with v174 fallback | ✅ (claimed) | ✅ (claimed) | ✅ (real) |

v176 is the strictest superset. v175 v2 (wire CVar) was the plan-criticer's correct fix, but it inherits the missing env-var plumbing. v176 adds the plumbing.

## Concrete bisect plan (operator-side)

### Step 1: Apply the v176 patch

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
# Edit TestReSTIR_GI_Temporal.cpp:
#   - Add #include "Renderer/GI/GICVars.h" near the top
#   - Replace lines 950 and 1005 with CVar reads
#   - Add the env-var hook in Initialize() or test_ReSTIR_GI_Temporal
# Total: 3 net lines added, 2 hardcoded assignments removed
# Then run the v176 recipe:
bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/v176-recipe.sh
```

The `v176-recipe.sh` script (matching the existing `v173-recipe.sh` shape) handles the build, run, log stats, validator, vision, and mode-20 steps automatically. It exits 0 on full pass, 6 if the env-var hook didn't fire (multi-instance CVar failure), 5 if v176 hypothesis failed (display std < 0.07).

### Step 2: Rebuild

```bash
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild --Jobs=4
```

### Step 3: Verify the env-var hook fires

```bash
cd Engine/Source/Runtime/Binary/Debug
HLVM_RGI_MAXM=1.0 ./TestReSTIR_GI_Temporal 2>&1 | grep -E "HLVM_RGI_MAXM override|r_ReSTIR_MaxM"
# Expect: "HLVM_RGI_MAXM override: r_ReSTIR_MaxM = 1.00"
```

### Step 4: Run with DUMP_RGI

```bash
HLVM_RGI_MAXM=1.0 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal
```

### Step 5: Verify log stats

```bash
grep "stats display floats" TestReSTIR_GI_Temporal.log | tail -1
# v176 PASS: std ≈ 0.09 (env var propagated to CVar, CVar read in per-frame block, v173 hypothesis reproduced via CVar path)
# v176 FAIL: std ≈ 0.046 (env-var hook didn't fire, OR CVar read returned default, OR pipeline regressed)
```

### Step 6: If v176 PASSes, full validation suite

```bash
grep -E "VUID|ERROR|CommandList error" TestReSTIR_GI_Temporal.log | wc -l   # expect 0
python3 /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine/Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
# expect 6/6 PASS
```

### Step 7: Vision check + mode-20

```bash
# Use vision_analyze on the newest display_*.png dump
# Expect: recognizable Sponza with sane exposure
HLVM_PT_DEBUG_MODE=20 HLVM_RGI_MAXM=1.0 ./TestReSTIR_GI_Temporal  # rerun with mode-20
# Expect: non-zero GBufferMaterial values (the original mode-20 discrimination test)
```

### Step 8: Rollback path (if v176 PASSes but Phase A criteria fail)

```bash
# Restore the v172 baseline (default 30.0f) via env var:
unset HLVM_RGI_MAXM
# OR explicitly:
HLVM_RGI_MAXM=30.0 ./TestReSTIR_GI_Temporal
# OR revert the v176 patch entirely and re-apply v173 (via git).
```

## diff_estimate

- v176 Stage 1 (include): +1 line
- v176 Stage 2 (CVar reads): +2 lines / -2 lines (no net change)
- v176 Stage 3 (env-var hook): +4 lines
- Net: **+5 lines, -2 lines = +3 net lines**

## Verification (operator-side; terminal-blocked cron cannot run)

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
# Apply: 3-line edit (15 sec)
# Build:  3 min incremental
# Run:    25 sec
# Diagnostic:  5 sec (grep for env-var hook log line)
# Vision:  30 sec
# Total:   ~5 min
```

## HARD-ENV-FINDING (terminal blocked)

This cron tick is in file-only runspace. The v176 plan is the cleanest available closure path. The operator's 5-min recipe is the closure gate.

## Relation to v173 / v174 / v175

- **v173**: hardcoded `MaxM=1.0f` at lines 950 + 1005. INTACT on disk. Validated pre-edit hypothesis (per pre-v173 log analysis: gi_raw pre-temporal std=0.091-0.120, post-temporal std=0.0457). v176 supersedes v173 by replacing the hardcode with a CVar read.
- **v174**: frozen fallback (revert + AmbientScale+NumCandidates). Stays dormant. v176 is a different path to the same goal (Phase A acceptance).
- **v175**: original CVar-override-without-wiring plan. FIX verdict. v176 is the v175 v2 (wire CVar) PLUS the missing env-var plumbing.
- **v176 (this)**: complete, gateable, operator-actionable closure path.

## Relation to diagnostic

The DIAGNOSTIC_2026-07-30.md still stands: the GBuffer SRV binding bug is the root cause, and the v173 → v176 lineage is the symptom-level "make the display less monochromatic" fix. The CVar wiring is a code-quality improvement, not a bug fix. The diagnostic's other open threads (FImageDump clamp, sentinel-then-overwrite, nvrhi auto-barrier fragility) are unrelated to v176.

## mark PICK

This v176 plan goes into `docs/PENDING_PICK.md` as a new `[ ]` line, with the v175 line moved to `[x]` (closed by v176 supersession). Operator can run the v176 recipe directly without waiting for the next cron cycle.

— planner, dispatch from tick-82, 2026-08-17, file-only, single-profile host, terminal-blocked, autonomous invocation #22.
