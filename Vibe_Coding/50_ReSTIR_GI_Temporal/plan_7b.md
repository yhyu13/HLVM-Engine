# Phase 7b: ReSTIR GI — Temporal Reservoir Reuse

> **CRITIC REVIEW APPLIED**: Scope focused on temporal merge only. No shading changes. Debug visualization of M/W verifies correctness.

## Goal

Merge per-pixel reservoirs temporally across frames using reprojection. Each pixel combines its current reservoir with the reprojected previous frame's reservoir.

## Architecture

### Pipeline

```
GBuffer Pass
    ↓
GI Ray Trace (3 bounces, untouched)
    ↓
Bilateral Denoise (untouched)
    ↓
ReSTIR Generation (Phase 7a — writes Reservoir0/1)
    ↓
[NEW] ReSTIR Temporal Reuse (reads current + history, writes merged + new history)
    ↓
ReBLUR (untouched — still uses DenoisedHDRTexture)
    ↓
Blit to screen
```

### Why No Shading Change Yet?

Phase 7b only builds and verifies the temporal merge infrastructure. The merged reservoir is not yet used for final shading — that comes in Phase 8 (spatial reuse + evaluation). This prevents visual regressions and makes debugging easier.

### Ping-Pong Reservoirs

| Frame N | Current Reservoirs | History Reservoirs |
|---------|-------------------|-------------------|
| Even | Reservoir0/1 | Reservoir0History/1History |
| Odd | Reservoir0History/1History | Reservoir0/1 |

After temporal pass, swap pointers.

### Reservoir Merge Algorithm

For each pixel:
1. Read current reservoir `r_c` (worldPos_c, W_c, w_sum_c, M_c)
2. Reproject pixel to previous frame UV using `PrevViewProj` + depth
3. If reprojected UV is valid, read history reservoir `r_h`
4. **Merge**:
   ```
   M_merged = min(M_c + M_h, MAX_M)  // MAX_M = 30
   w_sum_merged = w_sum_c + w_sum_h  // p(y)=1 simplifies Jacobian to 1.0
   
   // Select winner proportional to weights
   if random() < w_sum_h / w_sum_merged:
       y_merged = y_h
   else:
       y_merged = y_c
   
   W_merged = w_sum_merged / max(M_merged, 1e-6)
   ```
5. Write merged reservoir to output (becomes next frame's history)

### Random Number Generation

Use a simple hash based on pixel coord + frame index:
```hlsl
float hash(uint2 pixel, uint frame)
```

## HLSL Implementation

### ReSTIR_Temporal_cs.hlsl

```hlsl
// Inputs:
//   t0: Current Reservoir0 (worldPos.xyz, W)
//   t1: Current Reservoir1 (w_sum, M, pdf, hitDist)
//   t2: History Reservoir0
//   t3: History Reservoir1
//   t4: Depth texture
// Outputs:
//   u0: Merged Reservoir0 (for next frame's history)
//   u1: Merged Reservoir1
//   u2: Debug output (M as grayscale, or W)
```

**Constants**:
```hlsl
struct FReSTIRTemporalConstants {
    float4x4 InverseCurrViewProj;
    float4x4 PrevViewProj;
    float2 OutputSize;
    float2 RcpOutputSize;
    float FrameIndex;
    float MaxM;
    float DebugVis;  // 1.0 = show M, 2.0 = show W
    float Pad;
};
```

**Reprojection**:
```hlsl
float2 ReprojectUV(float2 uv, float depth) {
    float4 clipPos = float4(uv * 2 - 1, depth * 2 - 1, 1);
    float4 worldPos = mul(InverseCurrViewProj, clipPos);
    worldPos /= worldPos.w;
    float4 prevClip = mul(PrevViewProj, worldPos);
    prevClip /= prevClip.w;
    return prevClip.xy * 0.5 + 0.5;
}
```

**Validation**:
- Reprojected UV must be in [0, 1]
- History reservoir must have M > 0 (not sky/invalid)

## C++ Implementation

### Modified FReSTIRPass

Extend `FReSTIRPass` with a second dispatch method, or create `FReSTIRTemporalPass`.

**Decision**: Extend `FReSTIRPass` with a `DispatchTemporal` method. Keeps all ReSTIR logic in one class.

```cpp
namespace ReSTIR {
    struct FReSTIRTemporalConstants {
        TFP32 InverseCurrViewProj[16];
        TFP32 PrevViewProj[16];
        TFP32 OutputSize[2];
        TFP32 RcpOutputSize[2];
        TFP32 FrameIndex;
        TFP32 MaxM;
        TFP32 DebugVis;
        TFP32 Pad;
    };

    class FReSTIRPass {
    public:
        // Phase 7a: Generation
        void Dispatch(nvrhi::ICommandList*, const FDesc&, const FReSTIRConstants&);
        
        // Phase 7b: Temporal
        struct FTemporalDesc {
            nvrhi::TextureHandle CurrentReservoir0;
            nvrhi::TextureHandle CurrentReservoir1;
            nvrhi::TextureHandle HistoryReservoir0;
            nvrhi::TextureHandle HistoryReservoir1;
            nvrhi::TextureHandle DepthTexture;
            nvrhi::TextureHandle OutReservoir0;
            nvrhi::TextureHandle OutReservoir1;
            nvrhi::TextureHandle OutDebugTexture;
            uint32_t OutputWidth, OutputHeight;
        };
        void DispatchTemporal(nvrhi::ICommandList*, const FTemporalDesc&, const FReSTIRTemporalConstants&);
    };
}
```

### Integration into TestFewBounceGI.cpp

1. **Add history textures**:
```cpp
nvrhi::TextureHandle Reservoir0History;
nvrhi::TextureHandle Reservoir1History;
```

2. **Create history textures** alongside current reservoirs in `Initialize()`.

3. **In Render()**, after Generation pass:
```cpp
// ReSTIR Temporal Reuse
CmdList->setTextureState(Reservoir0Texture, ..., ShaderResource);
CmdList->setTextureState(Reservoir1Texture, ..., ShaderResource);
CmdList->setTextureState(Reservoir0History, ..., ShaderResource);
CmdList->setTextureState(Reservoir1History, ..., ShaderResource);
CmdList->setTextureState(GBufferDepthTexture, ..., ShaderResource);
CmdList->setTextureState(Reservoir0Texture, ..., UnorderedAccess);  // Reuse as output
CmdList->setTextureState(Reservoir1Texture, ..., UnorderedAccess);

ReSTIR::FReSTIRPass::FTemporalDesc TemporalDesc;
// ... bind textures ...

ReSTIR::FReSTIRTemporalConstants TemporalConstants;
// ... fill matrices ...
TemporalConstants.MaxM = 30.0f;

ReSTIRPass.DispatchTemporal(CmdList, TemporalDesc, TemporalConstants);

// Swap: merged output becomes next frame's history
std::swap(Reservoir0Texture, Reservoir0History);
std::swap(Reservoir1Texture, Reservoir1History);
```

**Note**: We swap the **current** and **history** pointers. The merged output is written to what was previously the current reservoir texture, and then we swap so it becomes the history for next frame.

Wait, actually we need to be careful. The generation pass writes to `Reservoir0Texture`. Then the temporal pass reads from `Reservoir0Texture` (current) and `Reservoir0History` (history), and writes to... where?

Option A: Write back to `Reservoir0Texture` (overwriting generation output). Then swap.
Option B: Write to a separate `MergedReservoir0` texture. Then swap.

Option A is simpler and saves texture memory:
1. Generation writes to `Reservoir0/1`
2. Temporal reads `Reservoir0/1` (current) + `Reservoir0/1History` (history)
3. Temporal writes to `Reservoir0/1` (overwriting current with merged)
4. Swap `Reservoir0/1` <-> `Reservoir0/1History`

For next frame:
- `Reservoir0/1` now contains the merged data (from step 3)
- `Reservoir0/1History` now contains the old current (unmerged, from before step 3)

Wait, that's wrong. After step 3, `Reservoir0` contains merged. After swap, `Reservoir0History` contains merged, and `Reservoir0` contains the old unmerged current. Then next frame's generation pass overwrites `Reservoir0` with new current. That works!

Actually no — next frame needs the merged data as history. Let's trace carefully:

Frame N:
1. Generation writes to `Reservoir0` (current)
2. Temporal reads `Reservoir0` (current) + `Reservoir0History` (history from N-1)
3. Temporal writes merged to `Reservoir0` (overwriting current)
4. Swap: `Reservoir0` <-> `Reservoir0History`
   - Now `Reservoir0History` = merged (for frame N+1 history)
   - Now `Reservoir0` = old history from N-1 (will be overwritten by generation in N+1)

Frame N+1:
1. Generation writes to `Reservoir0` (overwriting old N-1 history)
2. Temporal reads `Reservoir0` (current N+1) + `Reservoir0History` (merged N)
3. Temporal writes merged N+1 to `Reservoir0`
4. Swap

Yes, this works! The merged result from frame N becomes the history for frame N+1.

## Shader Register Map

| Register | Name | Type | Purpose |
|----------|------|------|---------|
| b0 | `gConstants` | cbuffer | Temporal constants |
| t0 | `gCurrReservoir0` | Texture2D | Current reservoir 0 |
| t1 | `gCurrReservoir1` | Texture2D | Current reservoir 1 |
| t2 | `gHistReservoir0` | Texture2D | History reservoir 0 |
| t3 | `gHistReservoir1` | Texture2D | History reservoir 1 |
| t4 | `gDepth` | Texture2D | Linear depth |
| u0 | `gOutReservoir0` | RWTexture2D | Merged reservoir 0 |
| u1 | `gOutReservoir1` | RWTexture2D | Merged reservoir 1 |
| u2 | `gDebugOutput` | RWTexture2D | Debug visualization |

**NVRHI Binding Offsets**: `constantBufferOffset = 0`
- b0 → 256
- t0-t4 → 0-4
- u0-u2 → 384-386

## Build Integration

- `ShaderMake.cfg`: Add `ReSTIR_Temporal_cs.hlsl -T cs`
- `CMakeLists.txt`: Already has `FReSTIRPass.cpp`

## Testing Strategy

1. **Compile**: `./Build.sh --Config=Debug --Target=TestFewBounceGI`
2. **Run**: `./Build.sh --Config=Debug --Target=TestFewBounceGI --Test`
3. **Debug vis M**: `g_ReSTIRDebugVis = true` — static scenes should show M increasing over first 30 frames (saturates at MAX_M=30). Moving camera should show M resetting at edges.
4. **Debug vis W**: Show merged W as grayscale — should be smoother than single-frame W.

## Success Criteria

- [ ] Compiles without errors
- [ ] TestFewBounceGI passes
- [ ] Debug M visualization shows increasing values on static pixels (up to MAX_M)
- [ ] Debug M visualization shows low values at screen edges / during camera motion
- [ ] No performance regression
- [ ] Ping-pong swap works correctly across frames

## Risks & Mitigations

| Risk | Mitigation |
|------|-----------|
| Reprojection math wrong → M doesn't accumulate | Visualize M; static pixels should reach MAX_M |
| Reservoir merge corrupts data | Visualize W; should be stable and non-zero |
| Ping-pong swap wrong → feedback loop | Log frame 0 vs frame 10 M values |
| Read-after-write hazard | Ensure proper texture state transitions |

## Phase 8 Preview

- Spatial reuse: 3x3 neighbor merge on merged reservoirs
- Shading evaluation: proper RGB output from selected samples
- Integration with ReBLUR: feed spatially-reused output into temporal denoiser
