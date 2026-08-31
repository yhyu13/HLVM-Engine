# Pending Plan v126 — GBuffer SRV binding bisect (REVISED per plan-review v125 FIX)

- task: Continue bisecting GBuffer SRV binding in the GI shader (per docs/DIAGNOSTIC_2026-07-30.md "Recommended next step").
- source: no bundle — direct edit + file inspection
- approach: Five ordered single-variable experiments. Step 0 is the precondition (sblob mtime diff) the plan-criticer flagged as the "single most likely explanation" — if the sblob is stale, the rest of the bisect is closed before any source edits. Steps 1-3 are handle identity (A), single-pixel sentinel (B), and SPIR-V reflection (C) with the C2 fix path committed. Step 4 is the slangc-leak hypothesis test for the v101 `, space1` interaction.
- diff_estimate: +35 / -10 lines (one new debug mode per experiment; one handle-log per dispatch; one mtime-log line)
- skip_plan_review: no — the plan-criticer FIX in v125 was addressed; this v126 plan should be re-reviewed.
- test_strategy: No new test file. Validation is per-experiment: vision analyze + numpy stats on the freshest dump group only.
- risks: All experiments require terminal access. Cron runspace is structurally terminal-blocked per OVERSEER_ESCALATION.md (EC-039).

---

## Step 0 (PRECONDITION) — sblob mtime diff

**Hypothesis (from PIPELINE_HEALTH_2026-07-30_tick105.md):** the on-disk `TestReSTIR_GI_Temporal_Data/GIPathTracing.sblob` pre-dates the v101 patch and was never rebuilt. If true, the modes 20/21/22 dumps were taken against a pre-v101 shader, and the v101 fix is already on-disk but unverified.

**Implementation:** zero source code changes. One terminal command:

```
stat -c '%Y %n' \
  Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal \
  Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.sblob \
  Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl \
  Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl \
  Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp
```

**Predicted outcomes:**
- S1: binary mtime < all sources → sblob is stale. Rebuild + re-run modes 20/21/22. If post-rebuild mode 20 returns non-zero, v101 actually fixed it. Bisect closes.
- S2: binary mtime >= all sources → binary is current. sblob is not necessarily current (sblob is consumed at test init, not linked into binary). Continue to step 1.
- S3: sblob mtime < GIPathTracing.hlsl Data copy → sblob is stale. Rebuild shaders specifically (`Build.sh --Target=TestReSTIR_GI_Temporal` triggers ShaderMake via the cmake target's POST_BUILD step). Re-run modes 20/21/22.

**Time cost:** 5 seconds. **Code cost:** zero lines.

---

## Step 1 (Experiment A) — Handle identity probe

See `docs/PENDING_PLAN_v125.md` Experiment A for the two log lines (RenderGBuffer at TestReSTIR_GI_Temporal.cpp:1519, FGIPass at FGIPass.cpp:533) plus the optional GBuffer-handle early-return guard at FGIPass.cpp:520.

**Predicted outcomes:** See v125 plan. A1 (handles match, binding layer broken) is most likely. A2 (mismatch) requires investigating texture member re-assignment. A3 (null in Desc) requires the early-return guard.

**Time cost:** 1 build + 1 run + log grep = ~30 seconds. **Code cost:** +5 / -0 lines.

---

## Step 2 (Experiment B) — Single-pixel sentinel debug mode

See `docs/PENDING_PLAN_v125.md` Experiment B for the case 30u addition in BOTH GIPathTracing.hlsl copies (Private at line 672, Data at line 672).

**Predicted outcomes:** See v125 plan. B1 (magenta) means binding works at (0,0,0). B2 (zero) means binding universally broken → proceed to step 3. B3 (mixed) means sampler-related.

**Time cost:** 1 slangc rebuild (~0.6s) + 1 build + 1 run + vision/numpy = ~20 seconds. **Code cost:** +8 / -0 lines (4 lines × 2 copies).

---

## Step 3 (Experiment C) — SPIR-V reflection

**Hypothesis:** If A and B both confirm "binding universally broken", the binding layout may declare different binding indices than what the SPIR-V expects.

**Implementation:** One terminal command:

```
spirv-cross --reflect \
  Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.sblob \
  | grep -B 1 -A 3 'GBuffer\|Output\|SceneBVH'
```

If `spirv-cross` is not on disk: try `Build/Debug/_deps/nvrhi-src/thirdparty/spirv-cross/tools/spirv-cross` or install via vcpkg. If absent, fall back to programmatic reflection via `nvrhi::ShaderLibrary::getBindingDesc()` in a tiny test exe.

**Predicted outcomes (with committed fix path):**
- C1: `GBufferWorldPos` at `set=0 binding=1`, `GBufferNormal` at `set=0 binding=2`, `GBufferMaterial` at `set=0 binding=3` → SPIR-V matches C++. Binding layer is correct. Suspect becomes the Vulkan validation layer silently dropping the second binding set (verify by enabling validation layer, but that's a bigger fix).
- C2: GBuffer textures reflected at `set=1 binding=N` → slangc leak from `Output : register(u0, space1)` (line 88) propagated to all RWTexture2D + Texture2D in the shader. **Committed fix:** drop `, space1` from `Output : register(u0, space1)` AND from `DebugStatsTexture : register(u1, space1)`, keep the v22 split binding layouts, rebuild, re-test. If v22 split was correct, dropping `, space1` restores single-binding-set behavior and we lose the nvrhi-deferred-barrier fix, but we can re-add the v22 split differently.
- C3: GBuffer textures not in reflection → slangc DCE'd them. **Committed fix:** add an unconditional dummy read of each GBuffer texture in the raygen (e.g., write `GBufferWorldPos[pixel].r` to `Output[pixel].r` regardless of debugMode), force slangc to emit the SRV reads. Rebuild, re-test.

**Time cost:** 5 seconds for spirv-cross. **Code cost:** depends on outcome; up to +12 / -4 lines.

---

## Step 4 (slangc-leak hypothesis test) — only if C2 triggered

**Hypothesis:** The `, space1` on `Output : register(u0, space1)` and `DebugStatsTexture : register(u1, space1)` causes slangc to also place Texture2D (read-only) bindings in set 1, but the C++ binding layout only declares them in set 0.

**Implementation:** temporarily revert the `, space1` to default (just `register(u0)` and `register(u1)`) in BOTH GIPathTracing.hlsl copies at lines 88 and 91. Rebuild, re-run modes 20/21/22.

**Predicted outcomes:**
- L1: modes 20/21/22 return non-zero → slangc-leak hypothesis confirmed. The fix is to keep `Output` and `DebugStatsTexture` at default space0, then the v22 split binding layout can be combined back into a single binding layout (revert v101 entirely). Or: keep the `, space1` but ensure nvrhi's binding layout actually declares them in set 1 too.
- L2: modes 20/21/22 still return zero → slangc-leak hypothesis falsified. Move to suspect #4 (texture handle identity — already covered by step 1).

**Time cost:** 1 slangc rebuild + 1 build + 1 run = ~15 seconds. **Code cost:** +0 / -8 lines (revert).

---

## FOLLOWUP (tick 110, 2026-07-30) — early-return masks debug-mode diagnostic chain

A new diagnostic insight emerged from tick 110's source analysis that
invalidates the v126 plan's foundational assumption (that modes
20/21/22 actually run). See `docs/PIPELINE_HEALTH_2026-07-30_tick110.md`
for the full analysis.

### The bug
`Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl:466-469`:

```hlsl
if (length(worldPos) < 0.001) {
    Output[pixel] = float4(0.0, 0.0, 0.0, 1.0);
    return;
}
```

The debug-mode switch at line 577+ is **AFTER** this early-return.
When `GBufferWorldPos[pixel].rgb` returns zero — whether because the
SRV binding is broken OR because the raster pass missed pixels — the
early-return fires and Output is written as `(0,0,0,1)` BEFORE the
debug-mode switch runs.

Empirical evidence: log line 121 shows
`gi_raw normalized per-channel — R[0.000,0.000] G[0.000,0.000] B[0.000,0.000]`,
consistent with the early-return path, NOT with mode 20's expected
output (~0.85 Sponza albedo).

### Implication for v126
All of the v126 plan's single-variable experiments (A=handle identity,
B=mode 30u single-pixel sentinel, C=spirv-cross reflection, D=slangc-leak
test) target modes that are masked by the early-return. The diagnostic
chain is broken.

### Required revision: new Step 0.5 BEFORE the existing Step 0

Add a `bypassEarlyReturn` flag in `GIPathTracing.hlsl` (Private + Data
copies) so that modes 20/21/22 (and 30u when added) skip the early-return:

```hlsl
uint debugModeEarly = (uint)(g_GI.Params5.x + 0.5f);
bool bypassEarlyReturn = (debugModeEarly == 20u || debugModeEarly == 21u || debugModeEarly == 22u);

if (!bypassEarlyReturn && length(worldPos) < 0.001) {
    Output[pixel] = float4(0.0, 0.0, 0.0, 1.0);
    return;
}
```

Then re-run `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20`.

### Discriminating outcome
- **A:** Mode 20 dump shows Sponza albedo (white/cream) → SRV binding
  works correctly. The empty-SRV-binding hypothesis is REJECTED. Bug
  is then downstream (raster pass missing pixels, or worldPos calc).
- **B:** Mode 20 dump is still uniform `(0,0,0,255)` → SRV IS broken.
  Proceed to v126 plan Steps 1-4.

### Cross-reference
- `docs/PIPELINE_HEALTH_2026-07-30_tick110.md` — full analysis.
- `docs/DIAGNOSTIC_2026-07-30.md` — older diagnostic (predates this
  insight; mode 20 conclusion is now ambiguous).

---

## Cycle map (revised)

- **v126 (this plan):** Five ordered steps. Step 0 = precondition. Steps 1-3 = bisect A/B/C. Step 4 = slangc-leak test (conditional).
- **v127 (impler, parent runspace with terminal):** run step 0 (5 seconds). If outcome S1 or S3, rebuild + re-test modes 20/21/22; if they return non-zero, plan closes. Otherwise proceed.
- **v128 (impler):** step 1, log handle addresses.
- **v129 (impler):** step 2, slangc rebuild + case 30u.
- **v130 (impler):** step 3, spirv-cross reflection.
- **v131 (impler, conditional):** step 4, slangc-leak test.
- **v132 (plan-criticer/review):** evaluate results, route to next iteration or finalize.

---

## Acceptance gate (per dispatcher instructions)

Same as v125: each step has concrete falsifiable outcomes.

The dispatcher's full acceptance criteria still cannot be satisfied in the file-only runspace. Step 0 alone takes 5 seconds and may resolve the entire bisect (the v101 patch may already be working and the .sblob was never picked up). Parent runspace with terminal access can close this in 5-30 seconds.

---

## What this plan does NOT change (from v125)

- No commits, pushes, history rewrites.
- No governance-file edits.
- No new test files. `skip_impl_review: yes` is honest.

---

## Plan Deviations policy (per six-role-pipeline skill)

Same as v125. The reviewer must audit any deviation from this plan during implementation. Deviations should land in `PENDING_COMMIT_v<N>.md`'s `## Plan Deviations` section.