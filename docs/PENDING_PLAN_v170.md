# Pending Plan v170
- task: Identify the static knob that flattens TestReSTIR_GI_Temporal display variation to std≈0.046 (~12/255) and propose a fix
- source: file-only bisect of `docs/PENDING_PICK.md` v170 card (added 2026-11-15 tick1544); evidence from `Binary/Debug/TestReSTIR_GI_Temporal.log` lines 232-258 (frame-8 stats); source inspection of `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp:2489-2733` (display composition + dump glue), `FGIPass.h:62-70` (FGIPassDesc), `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:440-490` (GI pass shader constants update). Cross-reference: `docs/DIAGNOSTIC_2026-08-01-v25.md` (premise stale, observations still relevant for the renderer wiring), `docs/DIAGNOSTIC_2026-07-30.md` (deprecated by 7+ intervening cycles).
- approach: Three-step file-only bisect to localize the display-flattening bug:
  1. **Read `TestReSTIR_GI_Temporal.cpp` lines 2489-2733 verbatim** to identify what code populates `DisplayTexture`. The `LogFloatStats` call site (line 2600-2614) reads `DisplayTexture` from a `nvrhi::TextureHandle`; `DumpRGBA32FTexture` reads it again to PNG. The TEXTURE is the same handle for both calls. We need to find what upstream pass writes to DisplayTexture. Candidates to grep for: `DisplayTexture` writes/clears, `Compose`, `Present`, `SwapChain`, `BlitTexture`, `RenderTarget`, `RenderTexture`.
  2. **Read `TestReSTIR_GI_Temporal.cpp` lines 750-825 (the per-frame Render / Compositor section)** to find the per-frame chain. The test runs `RenderGBuffer → FGIPass → Denoise → Compose`. Identify the Compose pass — it should produce display output.
  3. **Grep for static knobs** that could flatten display: `Desc.Exposure`, `Desc.DirectIntensity`, `Desc.AccumScale`, `Desc.IndirectIntensity`, `DisplayScale`, `MipLevel`, `ContributionShaping`, blend-mode constants in the compose/present path. Identify which one(s) are set as constants regardless of incoming gi_raw / denoised radiance.

  **Hypothesis by elimination**: Since (a) gi_raw std=0.046 and (b) display std=0.046 are nearly identical, the display pass adds almost no structure beyond the gi_raw signal. Either:
  - (A) The display pass is `display = gi_raw` (uniform composition — no material/normal modulation added) → DisplayTexture == gi_raw copy at frame 8
  - (B) The display pass is `display = tonemap(gi_raw * Exposure)` where tonemap has near-zero derivative at gi_raw's range → display stays close to gi_raw
  - (C) The display pass is `display = denoised * DirectIntensity + gi_raw * (1-DirectIntensity)` where `DirectIntensity` is set such that the direct/GI split collapses them → if DirectIntensity=1, display == denoised; if DirectIntensity=0, display == gi_raw
  - (D) A `BlendState` or `BlendFactor` in the test's display compose is multiplying by ~0.5 with clamping → uniform compression

  **Diagnostic action (file-only, no terminal required)**: read the display compose pass source and identify the static knob that explains display.std == gi_raw.std + bright monochrome mean=0.46.

- diff_estimate: +50/-10 lines max if a fix is found in the test config; +5/-5 lines if a one-line FGIPassDesc field is the culprit; +0 lines if the bug is in the compose shader (out of v170 file-only bisect scope)
- skip_plan_review: no — this is a multi-file bisect touching test + possibly shader path; warrants fresh-eyes review
- test_strategy: The acceptance criterion #4 (vision-confirmed recognizable Sponza with sane exposure) requires the operator-side rebuild + run with `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20` and visual inspection of `dumps/<timestamp>_display_frame8.png`. The file-only bisect can identify the candidate knob; the operator-side rebuild confirms the fix. Validator: `python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py` on the post-fix dump group — expects 6/6 PASS (currently 3/6 FAIL predicted from log stats per `docs/PIPELINE_HEALTH_2026-08-15.md`).
- risks:
  1. **Display compose shader is in the SDIR or similar source** — out of file-only bisect scope. If identified, escalate to operator-side for shader recompile.
  2. **Test harness changed between v166 and v168 cycles** — `HLVM_RGI_ACCUM=8` was added later; if its interaction with ComposeDisplay was poorly understood, the test may produce a too-monochrome stable state by design. Source-side evidence in `TestReSTIR_GI_Temporal.cpp` lines 2730-2830 (post-dump cleanup) and 2700-2720 (display dump wrapper).
  3. **`Desc.Exposure` field may not exist on FGIPassDesc** — verify `Engine/Source/Runtime/Public/Renderer/GI/FGIPass.h:25-60` to see the exact field set; if Exposure is missing, the test's intended `Exposure=1.5` from line 49-50 of `DIAGNOSTIC_2026-08-01-v25.md` would be silently dropped.
  4. **File-only runspace cannot rebuild + run + vision-check** — bisect plan delivers an evidence-backed diagnostic recipe. Empirical confirmation requires operator-side terminal+numpy+vision.
  5. **Direct lights added by the test** — if the test adds Directional/Point/Spot lights, then per-pixel Lambert contribution would yield std ≥ 0.1; current std=0.046 means either no lights OR lights added but composing wrong. Check `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp:800-810` for `LightCount` and `DirectionalLight` setup.

## Concrete bisect plan

### Step 1: Identify DisplayTexture producer
```bash
# File-only: read all "DisplayTexture" assignment sites
grep -n "DisplayTexture" Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp
# Expect: definition + LogFloatStats read + DumpRGBA32FTexture read + 1-N assignments upstream
```

### Step 2: Identify Compose pass
```bash
# Find the Compose / Present / Blit chain that writes DisplayTexture
grep -n "Compose\|Present\|SwapChain\|BlitTexture" Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp
```

### Step 3: Identify the static knob
```bash
# Read the FGIPassDesc field set
cat Engine/Source/Runtime/Public/Renderer/GI/FGIPass.h | grep -A40 "struct FGIPassDesc"
# Read the Desc initialization in the test (likely lines 750-820)
sed -n '750,820p' Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp
```

### Step 4: Compose the fix
Based on Step 1-3 evidence, identify the field/knob causing display.std == gi_raw.std and write a one-line override in the test's `Desc` initialization. Concrete examples (decided by Step 3 evidence):
- If `Desc.Exposure` is missing → add `Desc.Exposure = 1.0f;` (or what v25 diagnostic recommended)
- If `Desc.AccumScale` is missing → add `Desc.AccumScale = 0.5f;`
- If ComposeDisplay is reading gi_raw verbatim → insert intermediate ComposePass call
- If Denormalize-on-dump is enabled but `Desc.DirectIntensity=0` → set `Desc.DirectIntensity=1.0f`

### Step 5: Verify (operator-side)
```bash
cd Engine/Source/Runtime/Build/Debug
cmake -S ../../.. -B . -DCMAKE_BUILD_TYPE=Debug  # if not already configured
./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild
HLVM_PT_DEBUG_MODE=0 HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal
python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py
# Then vision-confirm display_frame8.png is recognizable Sponza
```

## Empirical baseline (file-only, this tick)
- **Display std=0.046** vs **gbuffer_material std=0.162** → 3.5× collapse from gbuffer to display
- **Display mean=0.458** (bright monochrome) → likely a `DisplayScale` multiplier pushing everything to mid-tone
- **Frame-8 stats derived from Binary/Debug/TestReSTIR_GI_Temporal.log lines 232-258**
- **All prior patches operational**: v137 (binding-offset 0), v140 (AmbientColor), v151 (binding-layout split), v166+v168 (graphics-pipeline rebind), v169 (cross-tree port) — confirmed intact on disk

## Acceptance bar for v170
- **Plan output**: identifies the specific static knob (file, line) responsible for display.std ≪ gbuffer.std
- **Commit output** (impler): changes ≤ 5 lines in the test's `Desc` initialization, OR introduces a 1-config CVar that the operator can override at runtime
- **Tests output**: validate_restir_gi.py exits 0 (6/6 PASS)
- **Vision output**: display_frame8.png shows recognizable Sponza walls with directional shadows (gate: operator-side)
- **Log output**: post-fix log frame-8 stats show display.std ≥ 0.1, gbuffer_material.std ≈ 0.16, gi_raw.std ≈ 0.05 — the per-step std collapse from gbuffer → gi_raw → display is reduced from 3.5× to ≤ 2×

## Critical caveat (file-only runspace reality)
This tick's output is **planner + plan-criticer markers only**. The next role (impler) requires rebuild + run + verify, blocked by tirith terminal gate. The bisect recipe above is concrete enough that an operator (with terminal access) can execute it within 1 hour:
1. cat lines 750-825 of TestReSTIR_GI_Temporal.cpp
2. cat FGIPass.h:25-60 (FGIPassDesc struct)
3. grep for Exposure/DirectIntensity/AccumScale/DisplayScale
4. Identify the missing/zero knob
5. One-line fix to test's Desc init
6. Rebuild + run + verify per the 5-step recipe

If this tick is accepted (KEEP by plan-criticer), the next cron tick — IF the operator has run the recipe and made a candidate fix — can produce PENDING_COMMIT_v170.md. Otherwise the bisect continues cycle-stopped.
