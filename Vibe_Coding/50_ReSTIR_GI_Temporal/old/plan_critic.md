# CRITIC REVIEW — Phase 7: ReSTIR GI Plan

**Iteration**: 1  
**Reviewer Persona**: Senior Staff Engineer reviewing a junior AI agent's plan  
**Verdict**: REVISE (Major issues found)

---

## Concerns

### [CRITICAL] Risk: Ray Tracing Shader UAV Expansion Breaks Binding Layout

**Dimension**: Correctness / Architecture  
**Summary**: Adding `u1` and `u2` UAVs to `FewBounceGI.hlsl` requires updating the RT binding layout in C++, which is complex and error-prone. The current RT pipeline has a carefully constructed binding set.  
**Suggestion**: Instead of modifying the RT shader to output reservoirs, **generate reservoirs in a separate compute pass that reads the existing GI output and GBuffer**. This avoids touching the RT pipeline entirely.  
**Verdict**: Must fix.

### [CRITICAL] Scope Too Large for Single Phase

**Dimension**: Minimalism  
**Summary**: The plan tries to do: (1) reservoir generation, (2) temporal merge, (3) shading evaluation, (4) RT shader modification, (5) new C++ pass — all in one go. If any piece fails, debugging is extremely hard.  
**Suggestion**: Split into **two sub-phases**:
- **Phase 7a**: Reservoir generation compute pass (read GI output + GBuffer, build reservoirs)
- **Phase 7b**: Temporal reuse compute pass (merge reservoirs with history)  
Do 7a first, verify it works, then 7b.

**Verdict**: Must fix. Scope down to 7a only for this session.

### [MAJOR] Missing Motion Vector Buffer

**Dimension**: Correctness  
**Summary**: The plan mentions "reprojection using PrevViewProj + depth" but depth-based reprojection alone cannot track moving objects. Without motion vectors, temporal reuse will ghost on camera movement.  
**Suggestion**: For Phase 7a, skip temporal reuse entirely. Just build the reservoir generation pass and output reservoirs as debug colors. Temporal reuse (7b) requires either motion vectors or accepting ghosting as a known limitation.

**Verdict**: Accept for now, but document clearly.

### [MAJOR] Reservoir Target PDF is Ill-Defined

**Dimension**: Correctness  
**Summary**: The plan says "target PDF p(y) = cos(theta)/PI" and "w = radiance" but ReSTIR requires the target function p̂(y) to be proportional to the integrand. For diffuse GI: p̂(y) = f_s(x,ω_i,y) * L_e(y) * G(x↔y). Using just radiance as weight without knowing the PDF at generation time breaks the unbiased weight calculation W = w_sum / (M * p(y)).  
**Suggestion**: Simplify. Since we're reading the **already-traced** GI output, treat each pixel's path as a sample with:
- `y` = primary hit direction (or just store pixel index as sample ID)
- `p̂(y)` = the radiance value already computed by the GI shader
- `p(y)` = 1.0 (since we're treating each pixel as 1 uniform sample)
- `W = w_sum / M` (since p(y) = 1)

This is technically "resampled importance resampling on a grid" rather than true ReSTIR, but it's the correct stepping stone.

**Verdict**: Revise math.

### [MAJOR] No Visibility Check for Temporal Samples

**Dimension**: Correctness  
**Summary**: The plan says "skip visibility ray for simplicity" but temporal reuse without visibility checks causes severe ghosting when objects move.  
**Suggestion**: For Phase 7a (no temporal yet), this is N/A. For 7b, add a note that temporal reuse MUST have visibility validation before production.

**Verdict**: Deferred to Phase 7b.

### [MINOR] Double Ping-Pong Confusion

**Dimension**: Architecture  
**Summary**: The plan proposes ping-pong for both GI output AND reservoir data. That's 4 textures to manage.  
**Suggestion**: For Phase 7a, reservoir data is write-once (no ping-pong needed). Only introduce ping-pong in 7b.

**Verdict**: Acknowledge.

### [MINOR] Missing Register Layout Documentation

**Dimension**: Idiomatics  
**Summary**: The HLSL register assignments need to be carefully documented to match NVRHI binding layout offsets (constantBufferOffset=0).  
**Suggestion**: Add a register map table to the plan.

**Verdict**: Nit, fix in implementation.

### [NIT] Naming Convention

**Dimension**: Idiomatics  
**Summary**: `FReSTIRPass` is okay, but per AGENTS.md classes use PascalCase without namespace prefix in names. `ReSTIR::FPass` might be cleaner.  
**Suggestion**: Keep `ReSTIR::FReSTIRPass` for clarity, but ensure file names match.

**Verdict**: Accept as-is.

---

## Revised Plan Summary

**Phase 7a: Reservoir Generation Compute Pass** (This Session)

Instead of modifying the RT shader, create a compute shader that:
1. Reads the existing `DenoisedHDRTexture` (or `HDRTexture`) — this is our "sample contribution"
2. Reads GBuffer (world pos, normal, depth)
3. Builds a per-pixel reservoir where:
   - `y` = GBuffer world position + primary ray direction (derived from normal)
   - `p̂(y)` = radiance from GI output
   - `p(y)` = 1.0 (uniform over pixel grid)
   - `W = radiance` (since p(y)=1, w_sum = radiance, M=1)
4. Outputs:
   - `Reservoir0`: `float4(worldPos, W)`
   - `Reservoir1`: `float4(w_sum, M=1, pdf=1, hitT)`
   - `DebugOutput`: `float4(W, W, W, 1)` for visual verification

C++ pass: `FReSTIRPass` that wraps this compute shader.

Integration:
```
GI Ray Trace → Bilateral Denoise → ReSTIR Generation → Debug Blit → ReBLUR → Blit
```

Temporarily add a CVAR `r_ReSTIRDebugVis` to output reservoir weight as grayscale.

**Phase 7b: Temporal Reuse** (Future Session)
- Add history reservoir textures (ping-pong)
- Reproject and merge
- Add visibility validation

---

**Confidence Score**: 7/10 after revision (was 4/10 before)
