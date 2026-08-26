# Phase 7a: ReSTIR GI — Reservoir Generation Compute Pass

> **CRITIC REVIEW APPLIED**: Scope reduced from full ReSTIR to reservoir generation only. RT shader untouched. Simplified math. Debug visualization added.

## Goal

Create a compute pass that converts the existing few-bounce GI output into **per-pixel reservoirs**. This is the foundation for temporal (Phase 7b) and spatial (Phase 8) reuse.

## Why This Approach

- **Minimal intrusion**: Does not touch the ray tracing pipeline or GI shader
- **Verifiable**: Can visualize reservoir weights directly as grayscale
- **Foundation**: Once reservoirs are generated, temporal/spatial reuse is just merging them

## Architecture

### Pipeline

```
GBuffer Pass
    ↓
GI Ray Trace (3 bounces, existing — untouched)
    ↓
Bilateral Denoise (existing — untouched)
    ↓
[NEW] ReSTIR Generation Compute Pass
    ↓
ReBLUR Temporal Denoiser (existing)
    ↓
Blit to screen
```

### Reservoir Representation (per pixel)

Stored in 2x RGBA16F textures:

| Texture | RGB | A |
|---------|-----|---|
| `Reservoir0` | `y.xyz` = GBuffer world position of primary surface | `W` = unbiased contribution weight |
| `Reservoir1` | `w_sum` (sum of weights) | `M` = sample count (always 1 for generation) |

For Phase 7a, the reservoir is trivial:
- Each pixel generates exactly 1 sample (itself)
- `y` = world position from GBuffer
- `p̂(y)` = radiance from denoised GI output
- `p(y)` = 1.0 (uniform over pixels)
- `w_sum = p̂(y) / p(y) = radiance`
- `M = 1`
- `W = w_sum / (M * p(y)) = radiance`

This is intentionally simple. The complexity comes in Phase 7b when we merge multiple reservoirs.

## HLSL Implementation

### ReSTIR_Generate_cs.hlsl

```hlsl
// Inputs:
//   t0: Denoised HDR radiance (RGB = radiance, A = hit distance)
//   t1: GBuffer world position
//   t2: GBuffer normals
//   t3: Depth
// Outputs:
//   u0: Reservoir0 (worldPos.xyz, W)
//   u1: Reservoir1 (w_sum, M, 0, 0)
//   u2: Debug visualization (grayscale = W)
```

Algorithm per pixel:
1. Read radiance `L` from denoised HDR
2. Read world position `P` from GBuffer
3. Read depth `z`
4. Skip if sky (`z == 0` or `L == 0`)
5. `p_hat = luminance(L)`
6. `w_sum = p_hat`
7. `M = 1`
8. `W = w_sum / M`  // = p_hat since p(y)=1
9. Write reservoirs
10. If `r_ReSTIRDebugVis`, write `float4(W, W, W, 1)` to debug output

### Register Map

| Register | HLSL Name | Type | Purpose |
|----------|-----------|------|---------|
| b0 | `gConstants` | cbuffer | Pass constants |
| t0 | `gRadiance` | Texture2D<float4> | Denoised GI radiance |
| t1 | `gWorldPos` | Texture2D<float4> | GBuffer world position |
| t2 | `gNormals` | Texture2D<float4> | GBuffer normals |
| t3 | `gDepth` | Texture2D<float> | Linear depth |
| s0 | `gPointSampler` | SamplerState | Point sampling |
| u0 | `gReservoir0` | RWTexture2D<float4> | Reservoir data 0 |
| u1 | `gReservoir1` | RWTexture2D<float4> | Reservoir data 1 |
| u2 | `gDebugOutput` | RWTexture2D<float4> | Debug visualization (optional) |

**NVRHI Binding Offsets**: `constantBufferOffset = 0`
- b0 → 256
- t0-t3 → 0-3
- s0 → 128
- u0-u2 → 384-386

## C++ Implementation

### Files

- `Engine/Source/Runtime/Public/Renderer/PostProcess/FReSTIRPass.h`
- `Engine/Source/Runtime/Private/Renderer/PostProcess/FReSTIRPass.cpp`

### Interface

```cpp
namespace ReSTIR {
    struct FReSTIRConstants {
        float OutputSize[2];
        float RcpOutputSize[2];
        float FrameIndex;
        float DebugVis;       // 1.0 = debug, 0.0 = normal
        float Pad[2];
    };

    class FReSTIRPass {
    public:
        struct FDesc {
            nvrhi::TextureHandle RadianceTexture;
            nvrhi::TextureHandle WorldPosTexture;
            nvrhi::TextureHandle NormalTexture;
            nvrhi::TextureHandle DepthTexture;
            nvrhi::TextureHandle OutReservoir0;
            nvrhi::TextureHandle OutReservoir1;
            nvrhi::TextureHandle OutDebugTexture; // optional
            uint32_t OutputWidth = 0;
            uint32_t OutputHeight = 0;
        };

        FReSTIRPass() = default;
        ~FReSTIRPass() { Shutdown(); }

        FReSTIRPass(const FReSTIRPass&) = delete;
        FReSTIRPass& operator=(const FReSTIRPass&) = delete;
        FReSTIRPass(FReSTIRPass&&) = delete;
        FReSTIRPass& operator=(FReSTIRPass&&) = delete;

        bool Initialize(nvrhi::IDevice* InDevice, const FString& InShaderDataDir);
        void Dispatch(nvrhi::ICommandList* CmdList, const FDesc& Desc, const FReSTIRConstants& Constants);
        void Shutdown();

    private:
        nvrhi::IDevice* Device = nullptr;
        nvrhi::ShaderHandle ComputeShader;
        nvrhi::BindingLayoutHandle BindingLayout;
        nvrhi::ComputePipelineHandle Pipeline;
        nvrhi::BufferHandle ConstantBuffer;
        nvrhi::SamplerHandle PointSampler;
        FString ShaderDataDir;
        bool bIsInitialized = false;
    };
}
```

### Integration into TestFewBounceGI.cpp

1. **Include**: `#include "Renderer/PostProcess/FReSTIRPass.h"`

2. **Member variables**:
```cpp
ReSTIR::FReSTIRPass ReSTIRPass;
nvrhi::TextureHandle Reservoir0Texture;
nvrhi::TextureHandle Reservoir1Texture;
nvrhi::TextureHandle ReSTIRDebugTexture;
bool bReSTIRInitialized = false;
```

3. **Initialize** (in `FFewBounceGIPass::Initialize`):
```cpp
// Create reservoir textures
nvrhi::TextureDesc ResDesc;
ResDesc.width = CurrentFBInfo.width;
ResDesc.height = CurrentFBInfo.height;
ResDesc.format = nvrhi::Format::RGBA16_FLOAT;
ResDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
ResDesc.isUAV = true;
ResDesc.debugName = "Reservoir0";
Reservoir0Texture = NvrhiDevice->createTexture(ResDesc);
ResDesc.debugName = "Reservoir1";
Reservoir1Texture = NvrhiDevice->createTexture(ResDesc);
ResDesc.debugName = "ReSTIRDebug";
ReSTIRDebugTexture = NvrhiDevice->createTexture(ResDesc);

// Initialize pass
if (!ReSTIRPass.Initialize(NvrhiDevice, DataDir)) { ... }
bReSTIRInitialized = true;
```

4. **Render** (after bilateral denoise, before ReBLUR):
```cpp
// Transition textures
CmdList->setTextureState(DenoisedHDRTexture, ..., ShaderResource);
CmdList->setTextureState(GBufferWorldPosTexture, ..., ShaderResource);
CmdList->setTextureState(GBufferNormalsTexture, ..., ShaderResource);
CmdList->setTextureState(GBufferDepthTexture, ..., ShaderResource);
CmdList->setTextureState(Reservoir0Texture, ..., UnorderedAccess);
CmdList->setTextureState(Reservoir1Texture, ..., UnorderedAccess);
CmdList->setTextureState(ReSTIRDebugTexture, ..., UnorderedAccess);

ReSTIR::FReSTIRPass::FDesc ReSTIRDesc;
ReSTIRDesc.RadianceTexture = DenoisedHDRTexture;
ReSTIRDesc.WorldPosTexture = GBufferWorldPosTexture;
ReSTIRDesc.NormalTexture = GBufferNormalsTexture;
ReSTIRDesc.DepthTexture = GBufferDepthTexture;
ReSTIRDesc.OutReservoir0 = Reservoir0Texture;
ReSTIRDesc.OutReservoir1 = Reservoir1Texture;
ReSTIRDesc.OutDebugTexture = ReSTIRDebugTexture;
ReSTIRDesc.OutputWidth = CurrentFBInfo.width;
ReSTIRDesc.OutputHeight = CurrentFBInfo.height;

ReSTIR::FReSTIRConstants ReSTIRConstants;
ReSTIRConstants.OutputSize[0] = (float)CurrentFBInfo.width;
ReSTIRConstants.OutputSize[1] = (float)CurrentFBInfo.height;
ReSTIRConstants.RcpOutputSize[0] = 1.0f / CurrentFBInfo.width;
ReSTIRConstants.RcpOutputSize[1] = 1.0f / CurrentFBInfo.height;
ReSTIRConstants.FrameIndex = (float)FrameCount;
ReSTIRConstants.DebugVis = CVar_r_ReSTIRDebugVis ? 1.0f : 0.0f;

ReSTIRPass.Dispatch(CmdList, ReSTIRDesc, ReSTIRConstants);
```

5. **Blit**: Use `ReSTIRDebugTexture` for debug visualization, or skip and feed existing pipeline to ReBLUR.

6. **Cleanup** (in `BackBufferResizing` and destructor):
```cpp
Reservoir0Texture = nullptr;
Reservoir1Texture = nullptr;
ReSTIRDebugTexture = nullptr;
if (bReSTIRInitialized) ReSTIRPass.Shutdown();
```

### CVAR for Debug Visualization

```cpp
AUTO_CVAR_BOOL(r_ReSTIRDebugVis, false, "Visualize ReSTIR reservoir weights as grayscale", Console)
```

## Build Integration

1. **ShaderMake.cfg**:
```
FewBounceGI.hlsl -T lib
BilateralDenoise_cs.hlsl -T cs
ReBLUR_cs.hlsl -T cs
ReSTIR_Generate_cs.hlsl -T cs
GBufferSponzaVS.hlsl -T vs
GBufferSponzaPS.hlsl -T ps
```

2. **CMakeLists.txt**: Add `Private/Renderer/PostProcess/FReSTIRPass.cpp`

## Testing Strategy

1. **Compile**: `./Build.sh --Config=Debug --Target=TestFewBounceGI`
2. **Run**: `./Build.sh --Config=Debug --Target=TestFewBounceGI --Test`
3. **Debug vis**: `r_ReSTIRDebugVis=1` — should show grayscale image where bright = high radiance, dark = low/no radiance
4. **Baseline comparison**: Without debug vis, output should match previous pipeline (ReSTIR generation pass is invisible — it just writes to reservoir textures that aren't consumed yet)

## Success Criteria

- [ ] Compiles without errors
- [ ] TestFewBounceGI passes
- [ ] Debug visualization shows plausible grayscale weights (sky = black, lit surfaces = bright)
- [ ] No performance regression (compute pass is cheap)
- [ ] Reservoir textures are valid RGBA16F

## Phase 7b: Temporal Reuse (Next Session)

- Add history reservoir textures (ping-pong)
- Reproject using `PrevViewProj` + depth
- Merge reservoirs with `M` cap
- Visibility validation ray
- Feed merged reservoir into shading

## Phase 8: Spatial Reuse (Future)

- 3x3 neighbor reservoir merge
- Normal/plane/roughness rejection
- Final integration with ReBLUR
