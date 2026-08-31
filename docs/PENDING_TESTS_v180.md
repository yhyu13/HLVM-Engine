# Pending Tests v180

- commit: docs/PENDING_COMMIT_v180.md
- plan: docs/PENDING_PLAN_v180.md
- cycle: v180 — TestReSTIR_GI_Temporal GBuffer SRV binding discriminator
- produces_test_files: no (this cycle is a recipe-extension + GPU-experiment heartbeat; no tests/ paths touched)
- timestamp: 2026-08-21

## Test scenarios (1 per hypothesis-tree leaf)

Each scenario is a single dump-PNG inspection after a single recipe run. The recipe is the operator's `v176-recipe.sh --skip-build --mode-31` (post-patch).

### Scenario 1 — Discriminator LEAF: "binding works, values zero" (mode 31 BLUE)
- **Run**: `HLVM_PT_DEBUG_MODE=31 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 timeout 300 ./TestReSTIR_GI_Temporal`
- **Expected dump**: latest `_gi_raw_frame*.png` shows uniform blue `(0,0,1)` per pixel
- **Hypothesis confirmed**: SRV binding IS alive; the upstream raster pass (or material sampler, or some C++ sentinel) is producing zero. Bisect pivots to `TestReSTIR_GI_Temporal.cpp:RenderGBuffer` (lines 2022-2267) and/or `FillGBufferSentinels` if present.
- **Recipe binding**: gate-5 probe catches `mean=0.333, sd<0.005` → `blue-mid` envelope (NEW, post-v180 patch)

### Scenario 2 — Discriminator LEAF: "binding works, reads real data" (mode 31 non-uniform)
- **Run**: same as above with mode 31
- **Expected dump**: latest `_gi_raw_frame*.png` shows non-uniform RGB ≈ `read_value*0.5+0.1` per pixel (Sponza structure visible, slightly muted due to the `*0.5+0.1` transform)
- **Hypothesis confirmed**: SRV binding is correct; the modes 20/21/22 result was misleading. The problem is in the v176 patch's per-mode downstream code. Bisect pivots to the modes-20/21/22 writes themselves.
- **Recipe binding**: gate-5 `variance` branch fires normally

### Scenario 3 — Discriminator LEAF: "slangc dead-strip" (mode 31 GRAY)
- **Run**: same as above with mode 31
- **Expected dump**: latest `_gi_raw_frame*.png` shows uniform gray `(0.5, 0.5, 0.5)` per pixel
- **Hypothesis confirmed**: slangc compiled the `default:` branch as the catch-all because all the case labels were unreachable (or `HLVM_RGI_DEBUG_VIS` macro did not pre-process). Verify shader compile flags; check the dispatcher `nvrhi::ShaderMake` log.
- **Recipe binding**: gate-5 probe classifies as `variance-failure` (gray is a mean=0.5, sd=0 envelope) — needs explicit `gray-mid` discriminator branch in v181

### Scenario 4 — Discriminator LEAF: "binding partially bound (works at (0,0,0))"
- **Run**: `HLVM_PT_DEBUG_MODE=30 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 timeout 300 ./TestReSTIR_GI_Temporal`
- **Expected dump**: latest `_gi_raw_frame*.png` shows magenta `(1,0,1)` at (0,0,0) AND black elsewhere
- **Hypothesis confirmed**: binding layout's per-region transition is dropping data post-frame-0. Bisect pivots to `FGIPass.cpp` ping-pong logic (TempReservoir 2/3 vs 0/1 aliasing).
- **Recipe binding**: requires extending recipe to support `--mode-30` (NOT in v180 scope; v181 follow-up)

### Scenario 5 — Discriminator LEAF: "binding universally broken" (mode 31 BLACK + mode 30 BLACK)
- **Run**: BOTH modes 31 AND 30 in separate invocations
- **Expected dump**: mode 31 → black `(0,0,0)`; mode 30 → also black (sentinel `(0,0,0) > 0.001` is FALSE so else-branch returns black)
- **Hypothesis confirmed**: binding never reaches the shader. Bisect pivots to `FGIPass.cpp:FBindingSetBuilder::ValidateAgainstLayout` (line 722) — assert should fire at the HLVM_ENSURE if layout structurally mismatched set; since it doesn't fire, the binding is structurally valid but `setTextureState` calls (lines 557-565) are racing with the dispatch. Bisect to Vulkan validation layer (currently stubbed; would need to re-enable per `DeviceManagerVk4_LifeCycle.cpp`).
- **Recipe binding**: gate-5 catches both as `v24-uniform-zero`

## Test isolation
- Each scenario runs in a fresh process invocation (no shared state)
- Dump PNG timestamps are compared to the pre-run snapshot; only NEW stamps are inspected
- Tests are gated behind the operator's `v176-recipe.sh --mode-31` execution

## What this DOES NOT cover
- **The mode 30 magenta-everywhere case** (binding works for ALL pixels, not just (0,0,0)) is not testable via mode 30 alone; it requires post-bisect re-execution. Out of scope for v180.
- **The mode 31 non-uniform case at frame 0 only** (sentinel pattern that decays) requires multi-frame stability analysis; deferred to v181 if mode 31 scenario 2 fires.
- **Vulkan validation layer warnings** were stubbed at `DeviceManagerVk4_LifeCycle.cpp` (per DIAGNOSTIC_2026-07-30.md). Scenario 5's "binding universally broken" branch would benefit from re-enabling the layer; not in v180 scope.

## Carry-forward
- Tester role (this turn): scoped as file-only-discriminator-document with 5 leaf scenarios
- Testing-verifier (v180): audit each scenario's discriminator criterion against the v176-recipe.sh --mode-31 patch (after operator applies); one bullet per scenario with PASS/FAIL/N-A
- The cron runspace this turn STAGED THE TEST CONTRACT. The actual GPU runs are operator-side.
- After operator runs `--mode-31` and `--mode-30` and reports the 5 signatures, v180 closes at SOME_RELAX (likely) or ALL_KEEP (if all 5 scenarios discriminate cleanly)

— tester, dispatch from tick-now-467, 2026-08-21, file-only, single-profile host, terminal-blocked, autonomous invocation #467 in lineage. **v180 test contract staged: 5 discriminator scenarios, no test files produced (this is a recipe-extension contract, not a code test). Operator action required to apply v176-recipe.sh patch and run modes 30/31.**
