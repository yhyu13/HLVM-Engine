# Phase 9: ReSTIR GI — Advanced Features

## Goal

Add two major improvements:
1. **Environment map sampling** — Replace black miss with sky/environment lighting
2. **5x5 Poisson disk spatial kernel** — Better spatial sample distribution than 3x3 grid

## 1. Environment Map Sampling

### Why

Current miss shader returns black:
```hlsl
void Miss(inout GIPayload payload) {
    payload.flags &= ~0x01;
}
```

This means any ray that escapes the scene contributes nothing. Real scenes have sky light, which is essential for:
- Outdoor scenes (Sponza has open ceiling)
- Realistic bounce lighting
- Better image quality overall

### Approach

**Option A: Procedural sky gradient** (simpler)
- Use a simple analytical sky model (Hosek-Wilkie simplified, or just a gradient)
- No texture assets needed
- Works immediately

**Option B: Cubemap texture** (better quality)
- Load an HDR cubemap
- Sample in miss shader
- Requires texture loading + binding

**Decision**: Option A (procedural sky). No asset dependencies, trivial to implement, still provides huge visual improvement over black.

### Procedural Sky Model

Simple physically-inspired sky:
```hlsl
float3 SampleSky(float3 direction) {
    float3 sunDir = normalize(g_GI.LightDir.xyz);
    float sunDot = dot(direction, sunDir);
    
    // Horizon gradient
    float horizon = 1.0 - abs(direction.y);
    float3 skyColor = lerp(float3(0.5, 0.7, 1.0), float3(0.8, 0.9, 1.0), direction.y * 0.5 + 0.5);
    
    // Sun disk
    float sunDisk = pow(max(sunDot, 0.0), 256.0);
    float3 sunColor = float3(1.0, 0.9, 0.7) * g_GI.LightDir.w;
    
    // Sun glow
    float sunGlow = pow(max(sunDot, 0.0), 8.0) * 0.3;
    
    return skyColor + sunColor * sunDisk + sunColor * sunGlow;
}
```

### Integration

Modify `FewBounceGI.hlsl` miss shader:
```hlsl
[shader("miss")]
void Miss(inout GIPayload payload) {
    float3 skyRadiance = SampleSky(WorldRayDirection());
    payload.radiance += payload.throughput * skyRadiance;
    payload.flags &= ~0x01;
}
```

Also modify closest hit to include sky bounce on final miss:
Currently closest hit adds direct lighting. On the final bounce miss, the sky contribution is added via the miss shader. This is correct.

But wait — the current payload accumulates radiance in closest hit, and miss just clears the hit flag. We need to change the architecture so that miss can contribute radiance.

Looking at the current code:
- `RayGen` initializes `payload.radiance = 0`
- `ClosestHit` adds: `payload.radiance += payload.throughput * albedo * NdotL * intensity`
- `Miss` does nothing to radiance

For environment sampling, we need:
- `Miss` to add: `payload.radiance += payload.throughput * skyRadiance`

This is a minimal change. The miss shader can safely add to `payload.radiance` because `throughput` already contains the attenuation from all previous bounces.

However, there's a subtlety: the current code checks `payload.flags & 0x01` to determine if we hit. After miss, the flag is cleared. Then `RayGen` outputs `payload.radiance` regardless. So adding sky radiance in miss will work correctly — the radiance is accumulated and output.

But wait, the RayGen code does:
```hlsl
if (payload.flags & 0x01) {
    // bounce logic
}
Output[pixel] = float4(payload.radiance, 1.0);
```

So even if miss occurs (flags cleared), the radiance is still output. Adding sky radiance in miss will just increase the output. Good.

### Test Impact

Adding sky light will change the output image. The test might fail if it does pixel-perfect comparison. But looking at the test framework, it likely just checks that the test runs without crashing (it's a rendering test, not an image comparison test).

Let me verify by checking what `RECORD_BOOL(test_FewBounceGI)` does... Actually, from the AGENTS.md, tests use `CheckCondition()` for assertions. The rendering test probably just runs for a few frames and dumps images, without strict pixel comparison.

If the test does compare images, we'll need to update the reference images. But since the user hasn't mentioned reference images, I'll assume it just checks for successful execution.

## 2. 5x5 Poisson Disk Spatial Kernel

### Why

Current 3x3 grid has issues:
- Fixed grid pattern causes structured artifacts
- Only 8 neighbors (small sample count)
- No distance-based falloff

Poisson disk sampling gives:
- Better distribution (no clustering)
- Adjustable radius
- More samples (12-16 instead of 8)
- Rotated pattern per pixel (decorrelation)

### Approach

Replace the 3x3 nested loops with a Poisson disk pattern:
```hlsl
static const float2 PoissonDisk[12] = {
    float2(-0.326, -0.406),
    float2(-0.840, -0.074),
    float2(-0.696,  0.457),
    float2(-0.203,  0.621),
    float2( 0.962, -0.195),
    float2( 0.473, -0.480),
    float2( 0.519,  0.767),
    float2( 0.185, -0.893),
    float2( 0.507,  0.064),
    float2( 0.896,  0.412),
    float2(-0.322, -0.933),
    float2(-0.792, -0.598)
};
```

In the spatial shader:
```hlsl
float rotAngle = hash(pixel, 7u) * 6.2831853;
float sinRot, cosRot;
sincos(rotAngle, sinRot, cosRot);
float2x2 rotator = float2x2(cosRot, -sinRot, sinRot, cosRot);

for (int i = 0; i < 12; i++) {
    float2 offset = mul(PoissonDisk[i], rotator) * spatialRadius;
    int2 neighborPixel = centerPixel + int2(offset + 0.5);
    // ... rest same as before
}
```

### Parameters

Add to `FReSTIRSpatialConstants`:
```cpp
float SpatialRadius;   // Pixel radius for Poisson disk (default 3.0)
```

## Files to Create/Modify

### Create: None

### Modify:
- `FewBounceGI.hlsl` — Add `SampleSky()` to miss shader
- `ReSTIR_Spatial_cs.hlsl` — Replace 3x3 with Poisson disk, add `SpatialRadius`
- `FReSTIRPass.h` — Add `SpatialRadius` to `FReSTIRSpatialConstants`
- `FReSTIRPass.cpp` — Pass `SpatialRadius` constant
- `TestFewBounceGI.cpp` — Set `SpatialRadius` value

## Testing Strategy

1. **Compile**: `./Build.sh --Config=Debug --Target=TestFewBounceGI`
2. **Run**: `./Build.sh --Config=Debug --Target=TestFewBounceGI --Test`
3. **Visual check**: Sky should be visible in Sponza open ceiling areas
4. **Frame dump**: `HLVM_DUMP_GI=1` and inspect — sky should be blue/white gradient, not black

## Success Criteria

- [ ] TestFewBounceGI passes
- [ ] Sky visible in frame dumps (not black)
- [ ] No structured artifacts from spatial kernel
- [ ] Performance maintained (~9s)

## Risks

| Risk | Mitigation |
|------|-----------|
| Sky changes test reference images | Verify test doesn't do pixel-perfect comparison |
| Poisson disk causes sampling artifacts | Use established disk pattern from literature |
| Performance regression with 12 samples vs 8 | 12 samples is still very cheap for compute |
