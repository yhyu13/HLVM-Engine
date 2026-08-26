# ReSTIR GI Iterations — Retrospective (2026-06-06)

**Why this exists**: We spent ~15 iterations chasing "ReSTIR temporal dimming" before realising the actual bug was upstream (broken normals, hardcoded gray albedo, 1-SPP). This document records what worked, what failed, and the meta-pattern that kept producing failure. Future sessions on ReSTIR / few-bounce GI / Sponza lighting should read this BEFORE debugging.

---

## What We Did Right (in rough order of impact)

| Fix | Impact |
|-----|--------|
| Removed TAA lerp() from temporal pass | Eliminated progressive dimming (~33 → ~19 → ~14 → ~12) |
| Fixed spatial reading `DenoisedHDRTexture` instead of uninitialised `TemporalRadianceTexture` | Eliminated black output |
| Restored 3×3 spatial neighbour merge | ReSTIR actually does spatial reuse now |
| Added vertex buffer bindings (RTVertices/RTIndices/RTInstanceInfo) to hit shader | ClosestHit gets real normals instead of `normalize(SV_HitT)` |
| Added per-instance AlbedoColor from PBRMaterial | Surfaces are no longer uniform gray |
| Added primary direct lighting in RayGen | Base image is recognisable before GI even starts |
| SPP loop (8 samples) + stronger bilateral (SpatialSigma=4) | Reduced Monte Carlo noise |
| ACES filmic tonemap + saturate() | Eliminated HDR clipping to white (12.6% → 0%) |
| Deterministic generation hash (removed FrameIndex) | Frame-to-frame variation dropped to ~0.4/255 |

---

## What We Did Wrong (in order of occurrence)

### 1. Anchored on "ReSTIR dimming" and ignored upstream GI for too long

We spent ~15 iterations tweaking `w_sum`, M caps, ping-pong swaps, and debug visualisations in ReSTIR shaders. The actual root cause was upstream: `SV_HitT` declared as `float3`, hardcoded `albedo=0.7`, no real normals.

**Why**: The user's context said "temporal dimming bug", so we assumed the bug was in ReSTIR. We should have first asked: "does the raw GI output look reasonable?"

### 2. Trusted the compacted context over the actual files on disk

The user's code_state showed a spatial shader with 3×3 neighbour-merge logic. The actual file on disk was a 34-line passthrough copy shader. We debugged merge math that wasn't even running for several iterations.

**Why**: Compacted context showed what the user thought was latest, but the file had been overwritten/reverted.

### 3. Debug visualisation became production code temporarily

We added `gOutRadiance = red/green` for `historyValid` debug to the temporal shader. This debug output got compiled into the build and influenced our perception of what was happening.

**Why**: We modified the shader to debug, then forgot to fully revert before moving to the next hypothesis.

### 4. Double-offset vertex index bug in ClosestHit

C++ adjusted indices by `+vertexOffset` when building unified buffers. The shader then added `+info.VertexOffset` again: `RTVertices[info.VertexOffset + i0]`. This caused out-of-bounds reads → zero normals → black output.

**Why**: We changed the shader to remove `VertexOffset` after seeing black output, rather than reasoning through the index math upfront.

### 5. Tonemapping whiplash: Reinhard → too dark, Exposure=2 → clipping, then ACES

- Added `Reinhard x/(1+x)` → output collapsed to mean ~11/255 (too dark for this scene)
- Switched to raw `Exposure=2` → brought back 12.6% white pixel clipping
- Finally settled on ACES filmic curve

**Why**: We were fixing the symptom (white pixels in PNG dump) without understanding that the cause was `light intensity 3.0 + no tonemap`. A better first step would have been "reduce light intensity to 1.0 and add a proper tonemap".

### 6. Late analysis of pixel distribution

We should have run `np.percentile()` and black/white pixel counts on frame 1. Instead we stared at mean values (~33, ~19, etc.) for 10+ iterations. The percentile analysis immediately revealed the bimodal distribution (63% black / 35% at 255) that told us the real story.

**Why**: We treated this as a "ReSTIR temporal weight bug" instead of a "HDR pipeline + broken normals + no colour" bug.

---

## Why We Kept Making Mistakes (root cause)

| Pattern | How It Hurt Us |
|---------|----------------|
| **Hypothesis-driven without isolation** | We kept guessing (w_sum scaling, buffer swap timing, matrix order) instead of first rendering without ReSTIR to see if GI itself was broken |
| **Modifying instead of measuring** | We changed shaders (gray output, M visualisation, debug colours) to "see what's happening" rather than adding structured logging or inspecting intermediate textures |
| **Fixing symptoms incrementally** | Each fix addressed one visible issue (black screen → dimming → white pixels → darkness) without a coherent picture of the whole pipeline |
| **Ignoring the critic until late** | The user provided a detailed critic that correctly identified `SV_HitT`, hardcoded albedo, and 1-SPP as the real issues. We should have read that first instead of treating it as background noise |
| **Compacted context ≠ file on disk** | A 15-iteration compacted summary is not a substitute for reading the current shader source. The source is the truth |

---

## The One Thing That Would Have Saved Time

**Disable ReSTIR, dump `DenoisedHDRTexture` directly, and inspect it.**

If we had done this on iteration 2 instead of iteration 15, we would have seen:
- 90% black pixels (broken normals → NdotL was random)
- Zero colour variation (hardcoded albedo = 0.7)
- Salt-and-pepper from 1-SPP

That would have immediately pointed us to `FewBounceGI.hlsl` instead of chasing ghosts in ReSTIR merge math.

---

## How to Apply This (next session on a similar bug)

1. **Read the user's critic BEFORE touching code.** If they say "the GI is broken", the GI is broken — not the post-process.
2. **Disable the post-process and look at the upstream.** If ReSTIR output is noisy, dump `DenoisedHDRTexture`. If that's noisy, dump `HDRTexture` (raw GI). If that's noisy, the bug is in the GI shader.
3. **Run `np.percentile()` on the dumped frame** before any mean/std analysis. Bimodal distributions (lots of 0s and 255s) tell you the pipeline is producing either nothing or saturated output — that immediately narrows the search to "input is zero" vs "tonemap is missing".
4. **Treat compacted context as a hint, not as the file.** When in doubt, Read the actual source. The harness tracks which files have been read; if you re-read, you'll get the latest.
5. **Don't keep debug visualisations in the build.** `#ifdef DEBUG_VIS` or comment them out before iterating. Confirmed by iteration 3 above.
6. **Mark critics as `STATUS: SUPERSEDED` when a new one replaces them.** Don't accumulate three critics in one directory without a current pointer.

---

## Cross-references

- Bug history: `.wolf/buglog.json` entries `bug-045` (pre-fix) and `bug-046` (post-fix)
- Current critic: `Vibe_Coding/50_ReSTIR_GI_Temporal/critic_2026-06-06_snowflower_v2.md`
- Implementation critics: `impl_critic.md`, `impl_7b_critic.md`, `impl_8_critic.md`, `impl_9_critic.md` (in same directory)
- Code: `Engine/Source/Runtime/Test/TestFewBounceGI.cpp` + `Engine/Source/Runtime/Test/TestFewBounceGI_Data/{FewBounceGI,ReSTIR_*_cs}.hlsl`
