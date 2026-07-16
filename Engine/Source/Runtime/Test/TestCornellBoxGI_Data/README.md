# TestCornellBoxGI Validation

This standalone test renders a Cornell Box through the engine's GI pipeline
(GBuffer → ray-traced GI → ReBLUR/ReSTIR denoising → blit) and verifies the
output for classic color-bleed behaviour.

## Running

```bash
./Engine/Source/Runtime/Build.sh --Config=Debug --Target=TestCornellBoxGI --Test
HLVM_DUMP_GI=1 ./Engine/Source/Runtime/Binary/Debug/TestCornellBoxGI
python3 Engine/Source/Runtime/Test/TestCornellBoxGI_Data/validate_cornell.py
```

## What is validated

The `validate_cornell.py` script checks the four frames dumped to
`dumps/`:

- **Black pixels** < 5%
- **Temporal stability**: max mean frame-to-frame Δ < 5 (0-255), mean
  per-pixel temporal std < 20% of mean intensity
- **Intensity range**: high/low mean-intensity ratio < 5
- **Color bleeding**: red and green tinted pixels visible on the floor
  (lower half of the image)

## Implementation notes

- The left and right walls are made emissive so the test produces a strong,
  stable color-bleed signal without requiring many bounces/samples.
- SPP is kept at 2 and max bounces at 1 to avoid the high-variance black-pixel
  artifacts seen with larger sample counts in this path-tracing setup.
- A `hashUint` fix in `CornellBoxGI.hlsl` avoids truncating the per-bounce
  random seed from `float` to `uint`.
- The dumped frame is the final denoised output (`ReSTIROutputTexture` or
  `DenoisedHDRTexture`), not the raw GI buffer.
