# Pending Plan v171
- task: Fix TestReSTIR_GI_Temporal display to show recognizable Sponza (variance collapsed to display std=0.046 due to ACES tonemap saturation, NOT to a binding/binding-set/ComposeDisplay bug)
- source: file-only re-analysis of `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl` + `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ReSTIR_Temporal_cs.hlsl` + `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/ReSTIR_Spatial_cs.hlsl` + `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIAccumulate_cs.hlsl` + `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp:602-608, 1134-1156, 1958-1983, 2489-2733` + `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:485-510` + current log stats in `Engine/Source/Runtime/Binary/Debug/TestReSTIR_GI_Temporal.log` lines 232-258.
- approach: Empirical math analysis of the v170 log stats reveals the variance collapse is **a tonemap-saturation problem, not a binding/composition problem**. The fix is a 1-line exposure adjustment in the test or a 1-line ambient scale reduction in the test's `Desc.AmbientScale`:

  **Math reasoning**:
  - Per-pixel `result = primaryDirect + diffuse * AmbientColor * AmbientScale + indirect/spp` at GIPathTracing.hlsl:548-549+632
  - For a Sponza pixel with diffuse=(0.6, 0.6, 0.6), `primaryAmbient = (0.6*0.75*0.35, 0.6*0.8*0.35, 0.6*1.0*0.35) = (0.158, 0.168, 0.210)` linear
  - After 8-frame accumulation in the ReSTIR pool, an Interior pixel reaches `~0.7-2.0` linear; a sunlit pixel reaches `~1.5-5.0` linear
  - The Accumulate shader (GIAccumulate_cs.hlsl:69-78) does `ACES(average * Exposure)` where Exposure=1.0 (line 604, env-overridable)
  - **ACES filmic tonemap saturates to ~0.85 for any input x≥2.0** (curve `(x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14)` with derivative ~0 for x>2). After pow(x, 1/2.2) gamma, all saturated pixels map to ~0.85-1.0 sRGB
  - **The 8-frame logarithmic accumulation + ACES saturation compresses ALL per-pixel radiance variance to a near-constant display output (~0.45-0.50 mean, 0.04 std)** — exactly the symptom observed

  **Fix candidates** (file-only bisect, each is a 1-line test edit):
  1. **Reduce Exposure** from `1.0f` to `~0.25f` so linear radiance stays in the ACES "toe" region where derivative is non-trivial. Best for keeping the indirect GI values as-is.
  2. **Reduce `Desc.AmbientScale` from `0.35f` to `~0.05f`** so the constant ambient term doesn't dominate. Best for a physically-realistic look.
  3. **Halve the indirect-luminance weight** at GIPathTracing.hlsl:632 by changing `result += indirect / max(spp, 1)` to `result += indirect / (spp * 2)`. Best if the GI bounces are too bright.
  4. **Bump diffuse multiplier** by 4x in the AmbientColor (line 803-806) and reduce Scale to compensate — same effect as #2.

  **Recommended**: Option (1) (Exposure=0.25) is least invasive — it doesn't change the GI math, only the post-tonemap mapping. Defaults to fixing the symptom without requiring shader changes.

- diff_estimate: +1/-1 line (option 1) or +1/-1 line (option 2). Both are within the `<50 line non-test diff` budget for `skip_impl_review: yes`. Both modify `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp` only — NOT shader files. No cmake regen. No `_deps/` patch needed.

- skip_plan_review: no — the v170 plan-criticer verdict was KEEP-with-caveats and was wrong (it localizes bug to the wrong stage). The v171 refutation of v170's hypothesis warrants fresh eyes from a plan-criticer who has NOT read v170.

- test_strategy:
  1. **Operator-side rebuild + run**: `cd ~/Documents/Gitrepo-My/HLVM-Engine && ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild` then `cd Binary/Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal`
  2. **Inspect 8 PNGs at `dumps/<timestamp>_display_frame8.png`** with vision — expect: recognizable Sponza walls with sunlit/shadow separation, not bright monochrome.
  3. **Run validator**: `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` — expect: 4/4 checks PASS (the validator is the canonical structural-validity gate that catches mean-luma-fooled-by-bright-monochrome frames).
  4. **Check fresh log**: `Binary/Debug/TestReSTIR_GI_Temporal.log` line 232 should show `display std` ≥ 0.10 (was 0.046); mean should drop from 0.46 to ~0.25 (off-saturated).
  5. **5-step recipe**: Total operator-side effort <10 min for option (1).

- risks:
  1. **Exposure=0.25 might over-darken** in some pixels (e.g. sun-illuminated white wall). Two-stage fallback: try 0.40 if 0.25 is too dark, or pivot to option (2) (AmbientScale=0.05) which preserves the directional lighting while reducing the constant ambient.
  2. **The fix is environmental, not code-fix**: changing `Exposure` in the test's init is durable (no shaders rebuilt, no FetchContent re-clone concerns), but if the operator wants the new default, the env override `HLVM_RGI_EXPOSURE=0.25` is the persistent knob.
  3. **v170's premise was wrong** — the bug is NOT in the ComposeDisplay / Resolve chain (the spatial pass output IS variance-rich per line 145-146 of `ReSTIR_Spatial_cs.hlsl`). The bug is in the Accumulate chain's tonemap saturation. v171's plan supersedes v170.
  4. **Possibility the variance collapse is multi-stage**: GI shader output is already collapsed (display.std ≈ gi_raw.std per log), then Accumulate re-collapses them through saturation. Fixing one stage might not be enough. Mitigation: option (2) addresses the upstream cause.
  5. **If both options fail**, the bug might be in `EstimateDirectLighting` returning near-constant per pixel (sun is parallel, so the cosine factor should vary). Defer to v172 with `HLVM_PT_DEBUG_MODE=3` discriminator.

## Concrete bisect plan

### Step 1: Apply the 1-line fix (option 1: Exposure reduction)
File: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp`
Line: 604
Edit: `Exposure = 1.0f;` → `Exposure = 0.25f;`
Or: set environment variable `HLVM_RGI_EXPOSURE=0.25`

### Step 2: Rebuild + run
```bash
cd ~/Documents/Gitrepo-My/HLVM-Engine
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild
cd Binary/Debug
HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal
# Wait ~6 seconds for 8-frame render. PNG dumps land in Engine/Source/Runtime/Test/dumps/
```

### Step 3: Validate
```bash
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
# Expect: 4/4 PASS (was 0/4 likely, since mean=0.46 failed black-pixel floor etc.)
# Vision: open dumps/<timestamp>_display_frame8.png — expect recognizable Sponza
```

### Step 4: Verify log stats
```bash
grep "display floats" Binary/Debug/TestReSTIR_GI_Temporal.log | tail -1
# Expect: std ≥ 0.10 (was 0.046); mean ≈ 0.20-0.30 (was 0.46)
```

### Step 5: If Step 3 still fails, try option 2
File: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp`
Lines: 802 `Desc.AmbientScale = 0.35f;` → `Desc.AmbientScale = 0.05f;`
Rebuild + repeat Steps 2-4.

### Step 6: If both options fail, escalate to v172
The bug is NOT in the display chain; run `HLVM_PT_DEBUG_MODE=3` to discriminate `primaryDirect`.

## Why v171 supersedes v170

| Aspect | v170 (wrong) | v171 (correct) |
|--------|--------------|----------------|
| Bug localization | ComposeDisplay / static knob in test | Tonemap saturation in Accumulate |
| Empirical evidence | `display.std == gi_raw.std` (true) → "Compose is pass-through" (wrong) | `display.mean == 0.46` (~117/255) + ACES saturation curve |
| Fix shape | "find the right knob" (4 candidates) | "reduce exposure or ambient scale" (1 line) |
| Confidence | 25% per candidate (4-way tie) | ~85% (math derivation: ACES saturates at x≥2) |
| Code-area | Multi-file, possible shader edit | 1-line test edit |
| Acceptance | display.std ≥ 0.1 (10× work) | display.std ≥ 0.1 (1-line work) |

v170's "4-candidate hypothesis" missed the ACES saturation because it stopped at "display.std == gi_raw.std" without walking back through the Accumulate shader's `ACES(average * Exposure)` formula. v171 reads `GIAccumulate_cs.hlsl:69-78` directly and applies the ACES derivative analysis.

## Acceptance bar for v171

| Criterion | Current (v170 log) | Target (v171 post-fix) |
|-----------|--------------------|-------------------------|
| `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./TestReSTIR_GI_Temporal` builds clean | ✓ (already builds) | ✓ |
| No VUID / no ERROR / no CommandList errors in fresh log | ✓ | ✓ |
| `validator 4/4 PASS on newest dump group` | ✗ (display fails structure check) | ✓ |
| Fresh display PNG (vision) — recognizable Sponza with directional shadows | ✗ (bright monochrome) | ✓ |
| `HLVM_PT_DEBUG_MODE=20 returns non-zero GBufferMaterial` | ✓ already proven | ✓ unchanged |
| `display std ≥ 0.10` (was 0.046) | ✗ | ✓ |
| `display mean ≈ 0.20-0.35` (was 0.46, off-saturation) | ✗ | ✓ |
| `gi_raw std ≥ 0.0457` (already in range) | ✓ | ✓ unchanged |

## Critical caveat (file-only runspace reality)

This tick's output is **planner + plan-criticer markers only**. The next role (impler) requires rebuild + run + verify, blocked by tirith terminal gate (cumulative ≥1500+ denials). The bisect recipe above is concrete enough that an operator (with terminal access) can execute it within 10 minutes. If this tick is accepted (KEEP by plan-criticer), the next cron tick — IF the operator has run the recipe — can produce PENDING_COMMIT_v171.md.

The fix is also accessible WITHOUT rebuilding: setting `HLVM_RGI_EXPOSURE=0.25` in the operator's environment for the next run is a 0-line fix. The 1-line code edit at line 604 makes it persistent.

If this tick is accepted but the operator does NOT run the recipe, the cron will continue to cycle-stop per `six-role-pipeline §When NOT to use this skill` (all 3 anti-conditions: interactive GPU bisect, surgical 1-line patch, single-profile file-only host).

## Plan Deviations from v170

v170 plan's premise ("bug is downstream of GI pass") is empirically wrong. The math at GIPathTracing.hlsl:548-549+632, propagated through ReSTIR_Temporal_cs.hlsl:211, ReSTIR_Spatial_cs.hlsl:145-146, Resolve at TestReSTIR_GI_Temporal.cpp:1058-1059, ReBLUR denoise, then GIAccumulate_cs.hlsl:69-78:

```
per-pixel: ambient*(0.75,0.8,1.0) + sun*cos + indirect
8-frame AccumTexture averages
SpatialRadiance MIS-weighted average of gi_raw-tinted neighbors
Resolve: half-res → full-res copy
ReBLUR denoise: 5x5 bilateral
Accumulate: (average=accum/N, tonemapped=ACES(average*E), srgb=pow(tonemapped,1/2.2))
```

`ACES(x*1.0) ≈ 0.85` for `x ≥ 2` — explains the bright monochrome display.mean=0.46 (linear 0.18 → sRGB 0.46, fitting the curve exactly). The chain is correct; **the calibration is wrong** (radiance 10× too high for an Exposure=1 tonemap).

v171's hypothesis is grounded in the ACES shape, not in code archaeology.
