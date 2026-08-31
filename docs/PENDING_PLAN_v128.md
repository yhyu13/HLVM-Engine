# Pending Plan v128 — GBuffer SRV binding bisect, final consolidation (incorporates tick 110 insight)

- task: Continue bisecting GBuffer SRV binding in the GI shader (per docs/DIAGNOSTIC_2026-07-30.md "Recommended next step", plus the tick-110 early-return masking insight at docs/PIPELINE_HEALTH_2026-07-30_tick110.md). Land a fix that satisfies the dispatcher's seven acceptance criteria end-to-end.
- source: no bundle — direct edit + file inspection
- approach: **Single critical pre-step (0) first**: bypass the early-return at `GIPathTracing.hlsl:466-469` for modes 20/21/22 so those diagnostic modes can actually run. Then run the v126 bisect ladder (A handle-identity, B single-pixel sentinel mode 30u, C spirv-cross reflection, D slangc-leak test). The bypass-patch is the missing variable from 109+ prior ticks; v126's experiments all assume the diagnostic modes run, which they do not today.
- diff_estimate: +28 / -4 lines. Bypass patch: +6 lines × 2 hlsl copies = +12 lines. Case 30u: +8 lines × 2 hlsl copies = +16 lines. Handle log lines: +4 lines × 2 cpp files = +8 lines (offset by -4 lines removing the existing `if (length(worldPos) < 0.001)` early-return block to be replaced with the gated version, and net zero for handle-log edits since they just append).
- skip_plan_review: no — this is a fundamental revision of the v126 plan per the tick-110 insight; needs fresh plan-criticer review.
- test_strategy: No new test file. Validation is per-experiment: vision + numpy per-pixel stats on the freshest dump group only. The 4-check structural validator (`docs/DIAGNOSTIC_2026-07-30.md` §"4-check structural validator") replaces the scalar mean-luma gate.
- risks: All experiments require terminal access. Cron runspace is structurally terminal-blocked per EC-039 / `docs/OVERSEER_ESCALATION.md`. This plan assumes **a parent runspace with terminal executes it**.

---

## Step 0 (PRECONDITION, NEW per tick 110) — bypass the early-return for diagnostic modes 20/21/22

**Hypothesis (from `docs/PIPELINE_HEALTH_2026-07-30_tick110.md`):** the early-return at `GIPathTracing.hlsl:466-469` fires when `GBufferWorldPos[pixel].rgb` returns zero (whether because the SRV is broken OR because the raster pass missed pixels). When it fires, the entire debug-mode switch at line 577+ is bypassed. Modes 20/21/22 NEVER execute today. Every v126 experiment is masked.

**Implementation:** patch `GIPathTracing.hlsl` in BOTH copies (Private + Data) to add a `bypassEarlyReturn` flag that disables the early-return for the diagnostic modes that read GBuffer textures directly.

In `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl` (line 462 area, immediately after `float3 diffuse = GBufferMaterial[pixel].rgb;` and before the early-return at line 466), insert:

```hlsl
// v128 (six-role-pipeline, tick 111, 2026-07-30): bypass the early-return
// for diagnostic modes 20/21/22 that read GBuffer textures directly.
// Without this, when GBufferWorldPos SRV returns zero (the empty-SRV-binding
// hypothesis), length(worldPos) < 0.001 fires and writes Output[pixel] =
// (0,0,0,1) BEFORE the debug-mode switch runs. The diagnostic modes that
// would discriminate "SRV broken" vs "SRV works" are masked.
uint debugModeEarly = (uint)(g_GI.Params5.x + 0.5f);
bool bypassEarlyReturn = (debugModeEarly == 20u
                       || debugModeEarly == 21u
                       || debugModeEarly == 22u);

if (!bypassEarlyReturn && length(worldPos) < 0.001) {
    Output[pixel] = float4(0.0, 0.0, 0.0, 1.0);
    return;
}
```

Apply the SAME patch to `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl` (line numbers will differ slightly; locate by the comment `if (length(worldPos) < 0.001)`).

Then rebuild shaders (POST_BUILD step in cmake) + binary:

```bash
./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal
```

Then run mode 20:

```bash
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20 \
  ./Binary/Debug/TestReSTIR_GI_Temporal
```

Then vision + numpy on `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/*_gi_raw_frame8.png`.

**Predicted outcomes (THE discriminating experiment):**
- **0A:** Mode 20 dump shows GBufferMaterial albedo (white/cream Sponza materials, mean luma > 0.5, color variance > 0.05) → SRV binding works correctly. The empty-SRV-binding hypothesis is REJECTED. The actual bug is downstream (likely the early-return masking the path-trace logic for pixels where GBufferWorldPos is non-zero but somehow `worldPos` calc gives zero — see also: maybe the raster pass's per-pixel material write is happening AFTER the GBufferWorldPos/SRV bind order, leaving some pixels with SRV=0 even after rasterization). Bisect moves to investigating why `length(worldPos)` returns 0 for Sponza pixels.
- **0B:** Mode 20 dump is still uniform `(0,0,0,255)` → SRV IS broken. Proceed to v126 plan's Steps 1-4 (handle-identity, mode 30u sentinel, spirv-cross reflection, slangc-leak test).
- **0C:** Mode 20 dump shows PARTIAL data (some pixels have GBufferMaterial albedo, others are black) → SRV binding works for some pixels but not others. This is the textbook symptom of a ping-pong UAV/SRV layout transition bug (see `nvrhi-deferred-barrier-ordering.md` reference). Proceed to nvrhi split-binding investigation.

**Time cost:** ~60 seconds (patch + rebuild + run + vision + numpy). **Code cost:** +12 / -4 lines (across both .hlsl copies; -4 lines for the 4-line early-return block being replaced with the gated version).

---

## Step 1 (Experiment A) — Handle identity probe

**Hypothesis:** the texture handles the GI pass receives (`Desc.GBufferWorldPos/Normal/Material`) are different objects than what the raster pass wrote to. If true, the SRV reads return zero because the SRV points to an uninitialized texture.

**Implementation:** in `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` line 1519 (inside `RenderGBuffer()`, AFTER the GBuffer raster pass finishes), add:

```cpp
HLVM_LOG(LogTest, info, TXT("[handle-id] RenderGBuffer: GBufferMaterial=%p WorldPos=%p Normal=%p"),
    (void*)GBufferMaterial.Get(), (void*)GBufferWorldPos.Get(), (void*)GBufferNormal.Get());
```

In `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp` line 533 (inside `DispatchRays`, where `Desc` is consumed), add:

```cpp
HLVM_LOG(LogGI, info, TXT("[handle-id] FGIPass::DispatchRays: GBufferMaterial=%p WorldPos=%p Normal=%p"),
    (void*)Desc.GBufferMaterial.Get(), (void*)Desc.GBufferWorldPos.Get(), (void*)Desc.GBufferNormal.Get());
```

Add an optional GBuffer-handle early-return guard at `FGIPass.cpp:520`:

```cpp
if (!Desc.GBufferWorldPos || !Desc.GBufferNormal || !Desc.GBufferMaterial) {
    HLVM_CLOG(!Desc.GBufferWorldPos, LogGI, error, TXT("FGIPass: null GBufferWorldPos"));
    HLVM_CLOG(!Desc.GBufferNormal,   LogGI, error, TXT("FGIPass: null GBufferNormal"));
    HLVM_CLOG(!Desc.GBufferMaterial, LogGI, error, TXT("FGIPass: null GBufferMaterial"));
    return;
}
```

Then rebuild, run, and grep the log for `[handle-id]` lines.

**Predicted outcomes:**
- A1: handles MATCH in both lines → binding layer is broken (the handle is correct, the descriptor is not pointing at the right memory). Proceed to Step 3 (SPIR-V reflection).
- A2: handles MISMATCH (RenderGBuffer logs one set, FGIPass logs different) → texture member re-assignment somewhere between passes. Likely cause: `keepInitialState=true` + dynamic resize or recreation. Fix: store the handles in a member variable on the test class, log on every GBuffer operation.
- A3: any handle is null in FGIPass's log → the early-return guard catches it. Fix: investigate why the texture is null when the raster pass clearly wrote to it (likely a stale handle reference).

**Time cost:** ~30 seconds (rebuild + run + grep). **Code cost:** +8 / -0 lines.

---

## Step 2 (Experiment B) — Single-pixel sentinel mode 30u

**Hypothesis:** if A1 confirmed "handles match", the binding layout is correct but the SRV reads return zero universally. Mode 30u tests "is the binding universally broken or only at certain pixels".

**Implementation:** add `case 30u:` to the debug-mode switch in BOTH GIPathTracing.hlsl copies (Private + Data) immediately after the case 22u entry at line 672:

```hlsl
case 30u:
{
    // v128 single-pixel sentinel: read GBufferMaterial at (0,0,0) only.
    // If mode 30 shows albedo at (0,0,0) but mode 20 shows zero everywhere,
    // the binding works at (0,0,0) but is masked elsewhere (e.g., layout
    // transition per ping-pong UAV/SRV). If mode 30 also shows zero, the
    // binding is universally broken.
    float3 sentinelColor = GBufferMaterial.Load(int3(0, 0, 0)).rgb;
    if (any(sentinelColor > float3(0.001, 0.001, 0.001))) {
        debugColor = float3(1.0, 0.0, 1.0); // magenta: binding works at (0,0,0)
    } else {
        debugColor = float3(0.0, 0.0, 0.0); // black: binding universally broken
    }
    break;
}
```

Then rebuild shaders + binary, run with `HLVM_PT_DEBUG_MODE=30`.

**Predicted outcomes:**
- B1: gi_raw_frame8 dump shows magenta pixels (mode 30 sentinel triggered) → binding works for at least one pixel. Bug is layout-transition per ping-pong (see nvrhi-deferred-barrier-ordering.md reference).
- B2: dump shows uniform black → binding is universally broken. Proceed to Step 3.
- B3: dump shows mix → layout transition is partial (some pixels transitioned, others didn't).

**Time cost:** ~20 seconds. **Code cost:** +16 / -0 lines (8 lines × 2 copies).

---

## Step 3 (Experiment C) — SPIR-V reflection

**Hypothesis:** if A1 + B2 both confirm "binding universally broken", the binding layout may declare different binding indices than what the SPIR-V expects.

**Implementation:** one terminal command:

```bash
spirv-cross --reflect \
  Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.sblob \
  | grep -B 1 -A 3 'GBuffer\|Output\|SceneBVH'
```

If `spirv-cross` is not on disk: try `Build/Debug/_deps/nvrhi-src/thirdparty/spirv-cross/tools/spirv-cross` or install via vcpkg.

**Predicted outcomes (with committed fix paths):**
- C1: `GBufferWorldPos` at `set=0 binding=1`, `GBufferNormal` at `set=0 binding=2`, `GBufferMaterial` at `set=0 binding=3` → SPIR-V matches C++ binding layout. Binding layer is correct. Suspect becomes the Vulkan validation layer silently dropping the second binding set (verify by enabling validation layer, but that's a bigger fix — see Step 5).
- C2: GBuffer textures reflected at `set=1 binding=N` → slangc leak from `Output : register(u0, space1)` (line 88) propagated to all RWTexture2D + Texture2D in the shader. **Committed fix:** drop `, space1` from `Output : register(u0, space1)` AND from `DebugStatsTexture : register(u1, space1)`, keep the v22 split binding layouts, rebuild, re-test. If v22 split was correct, dropping `, space1` restores single-binding-set behavior and we lose the nvrhi-deferred-barrier fix, but we can re-add the v22 split differently.
- C3: GBuffer textures not in reflection → slangc DCE'd them. **Committed fix:** add an unconditional dummy read of each GBuffer texture in the raygen (e.g., write `GBufferWorldPos[pixel].r` to `Output[pixel].r` regardless of debugMode), force slangc to emit the SRV reads. Rebuild, re-test.

**Time cost:** 5 seconds for spirv-cross. **Code cost:** depends on outcome; up to +12 / -4 lines.

---

## Step 4 (slangc-leak hypothesis test) — only if C2 triggered

**Hypothesis:** the `, space1` on `Output : register(u0, space1)` and `DebugStatsTexture : register(u1, space1)` causes slangc to also place Texture2D (read-only) bindings in set 1, but the C++ binding layout only declares them in set 0.

**Implementation:** temporarily revert the `, space1` to default (just `register(u0)` and `register(u1)`) in BOTH GIPathTracing.hlsl copies at lines 88 and 91. Rebuild, re-run modes 20/21/22.

**Predicted outcomes:**
- L1: modes 20/21/22 return non-zero → slangc-leak hypothesis confirmed. Keep the revert (no `, space1`) AND revert v22 (combine the two binding layouts back into one). The single-binding-layout configuration is the v1 state that originally produced the nvrhi-deferred-barrier warnings, but with v101's additional fixes (e.g., the CommandList isolation at line 1531-1537) those warnings may not fire anymore.
- L2: modes 20/21/22 still return zero → slangc-leak hypothesis falsified. Move to suspect #4 (texture handle identity — already covered by Step 1).

**Time cost:** ~15 seconds. **Code cost:** +0 / -8 lines (revert).

---

## Step 5 (FINAL FIX LANDING, after bisect yields root cause)

Once Step 0's outcome (or any subsequent step) identifies the root cause, the impler lands the fix:
- If root cause is the early-return masking (outcome 0A — "SRV works, downstream issue"): keep the bypass-patch from Step 0 as a permanent debug-mode tool, then bisect downstream from `length(worldPos) < 0.001`. Likely fix: change the early-return condition to be more specific (e.g., `if (all(worldPos == 0))` to allow NaN/inf values to pass through, or remove the early-return entirely and let the path-trace handle the zero-pos case).
- If root cause is binding handle identity (outcome A2): patch the handle propagation (likely a per-frame `keepInitialState` recreation issue).
- If root cause is slangc DCE (outcome C3): keep the unconditional dummy read.
- If root cause is slangc-leak (outcome C2/L1): revert `, space1` and consolidate binding layouts.

Verification: rerun `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=0` and check:
1. No Vulkan VUID/ERROR in log.
2. No nvrhi command-list errors.
3. `validate_restir_gi.py` passes on the freshest dump group.
4. Fresh display image (vision) shows recognizable Sponza with sane exposure.
5. `HLVM_PT_DEBUG_MODE=20` returns non-zero GBufferMaterial.

All five must pass before the bisect closes.

---

## Step 6 (post-fix cleanup) — remove debug-mode sentinels once verified

Per `software-development-practices §Code Review §Destructive Action Protocol`: "Don't leave unconditional sentinels in code that runs every frame forever — they're load-bearing, but their load-bearing purpose is debugging, not production." If the bypass-patch from Step 0 was landed as a fix (not a permanent tool), revert it after the bisect closes.

Specifically: remove the `bypassEarlyReturn` block from BOTH GIPathTracing.hlsl copies once the bug is closed and the original early-return condition is verified correct.

---

## Acceptance gate (per dispatcher instructions)

Same as prior plans: each step has concrete falsifiable outcomes. The full seven-criteria acceptance check:

1. **Debug target builds.** `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal` exits 0 with no warnings.
2. **Run env vars work.** `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal` produces 8 frames of dumps.
3. **No Vulkan VUID/ERROR.** Grep log for `VUID` and `ERROR`; both must return 0 matches.
4. **No command-list errors.** Grep log for `CommandList`; must return 0 matches (other than informational log lines).
5. **`validate_restir_gi.py` passes.** Run on the newest dump group only; 4-check structural validator must report PASS.
6. **Fresh display image shows Sponza.** Vision analysis on `dumps/*_display_frame8.png`; must show recognizable Sponza geometry with sane exposure (mean luma 0.05-0.5, color variance > 0.05, cell variance > 0.02).
7. **`HLVM_PT_DEBUG_MODE=20` returns non-zero GBufferMaterial.** After Step 0's bypass-patch lands, mode 20 dump must show real Sponza material albedo (white/cream, not uniform black).

The dispatcher's full acceptance criteria cannot be satisfied in the file-only runspace. Step 0 alone takes 60 seconds and may resolve the entire bisect (the early-return masking hypothesis may be the root cause, in which case the fix is to either remove the early-return or change its gating condition).

---

## What this plan does NOT change (from v126 + tick 110 insights)

- No commits, pushes, history rewrites (the parent runspace owns git topology end-to-end).
- No governance-file edits.
- No new test files.
- No re-architecture of the binding layout plumbing (Step 5 commit, not Step 0-4 bisect).

## Plan Deviations policy (per six-role-pipeline skill)

Same as v126/v127. The reviewer must audit any deviation from this plan during implementation. Deviations should land in `PENDING_COMMIT_v<N>.md`'s `## Plan Deviations` section.

## Cycle map (this v128)

- **v128 (this plan):** Six ordered steps. Step 0 = bypass-patch precondition (THE critical fix from tick 110). Steps 1-3 = bisect A/B/C. Step 4 = slangc-leak test (conditional). Step 5 = final fix landing. Step 6 = post-fix cleanup.
- **v128-review (plan-criticer):** evaluate whether the bypass-patch is the right fix or just a diagnostic tool. If the bypass-patch is itself the fix, KEEP and route to impler. If the bypass-patch should be temporary, FIX with feedback.
- **v129 (impler, parent runspace with terminal):** apply Step 0 patch + rebuild + run mode 20. Discriminating outcome determines next steps.
- **v130+ (impler):** depending on Step 0 outcome, apply Steps 1-5 in order.
- **v<N+1 (reviewer):** audit the impler's work and the bisect outcomes.
- **v<N+2 (tester):** run the seven-criteria acceptance gate; vision + numpy on fresh dumps.
- **v<N+3 (testing-verifier):** verdict on the test audit (KEEP or DELETE based on whether the seven criteria all passed).

---

## What unblocks this plan (parent-session responsibility)

Per EC-039 (parent must intervene), three options:

(a) Reconfigure cron `enabled_toolsets` to actually grant terminal, then verify with one manual probe BEFORE recreating the cron.

(b) Parent executes the live-evidence recipe directly (60-180 seconds):
    1. Apply the Step 0 bypass-patch to `GIPathTracing.hlsl` (Private + Data copies).
    2. `./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal`.
    3. `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20 ./Binary/Debug/TestReSTIR_GI_Temporal`.
    4. Vision + numpy on `dumps/*_gi_raw_frame8.png`.
    5. If non-zero (outcome 0A), bisect closes; remove the bypass-patch, fix downstream root cause.
    6. If zero (outcome 0B), proceed to Step 1 (handle-identity log, ~30s) → Step 2 (mode 30u, ~20s) → Step 3 (spirv-cross, ~5s) → Step 4 (slangc-leak test, conditional, ~15s).
    7. Once root cause identified, apply Step 5 fix. Re-run seven-criteria acceptance gate.

(c) Pause the six-role cron and continue interactive debugging.

The seven-criteria acceptance gate can be evaluated on a single fresh `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=0` run with vision + numpy + `validate_restir_gi.py`. If all seven pass, the dispatcher exits with KEEP verdicts and the cycle closes. If any fails, the bisect continues with diagnostic info from the failure.