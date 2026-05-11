# ShaderMakeBuild.py Documentation

## Overview

**ShaderMakeBuild.py** integrates NVIDIA Donut's ShaderMake tool into HLVM Engine's PyCMake build system. It compiles HLSL shaders to SPIR-V binary blobs with NVSP header + permutation table support.

**Location:** `Engine/Source/Runtime/ShaderMakeBuild.py`

---

## What It Does

1. **Compiles HLSL → SPIR-V** using ShaderMake tool
2. **Wraps SPIR-V in blob format** with NVSP header for runtime permutation support
3. **Generates CMake code** that integrates with PyCMake build system
4. **Handles incremental compilation** based on timestamps

---

## Blob Format

The output `.bin`/`.sblob` files contain:

```
4 bytes:  "NVSP" signature
8 bytes:  permutationSize (0) + dataSize (file size)
N bytes:  SPIR-V binary data
```

Runtime loading via `ShaderMake::FindPermutationInBlob()`:
- If blob has no "NVSP" signature and `numConstants==0` → returns raw SPIR-V
- If blob has "NVSP" signature → searches permutation table

---

## Key Classes

### `ShaderMakeModule`

Constructor parameters:

| Parameter | Type | Description |
|-----------|------|-------------|
| `target_name` | `str` | CMake target name (e.g., "HLVMDeferredShaders") |
| `config_file_cmake` | `str` | Path to `.cfg` config file (CMake variable) |
| `shader_sources_cmake` | `list` | List of HLSL source file paths |
| `output_dir_cmake` | `str` | Output directory for compiled blobs |
| `include_dirs_cmake` | `list` | Include directories for `#include` |
| `project_name` | `str` | Project name passed to ShaderMake |
| `slang_options` | `str` | Additional slangc options |

### Factory Function: `create_deferred_shading_shadermake()`

Pre-configured module for deferred shading shaders:

```python
def create_deferred_shading_shadermake() -> ShaderMakeModule:
    """Factory function to create ShaderMake module for deferred shading shaders."""
```

**Expected files:**
- `Test/TestDeferredShading_Data/fullscreen_vs.hlsl`
- `Test/TestDeferredShading_Data/blit_ps.hlsl`
- `Test/TestDeferredShading_Data/passes/gbuffer_vs.hlsl`
- `Test/TestDeferredShading_Data/passes/gbuffer_ps.hlsl`
- `Test/TestDeferredShading_Data/passes/deferred_lighting_cs.hlsl`

---

## Config File Format (.cfg)

```
<source> -T <profile> -E <entry> [-D <define>]...
```

**Example:**
```
# Blit vertex shader
TestDeferredShading_Data/blit_vs.hlsl -T vs_6_5 -E BlitVS -D SPIRV -D TARGET_VULKAN

# Blit fragment shader
TestDeferredShading_Data/blit_ps.hlsl -T ps_6_5 -E BlitPS -D SPIRV -D TARGET_VULKAN
```

### Profiles

| Stage | Profile |
|-------|---------|
| Vertex Shader | `vs_6_5` |
| Pixel Shader | `ps_6_5` |
| Compute Shader | `cs_6_5` |
| Ray Generation | `lib_6_5` |

### Common Defines

| Define | Purpose |
|--------|---------|
| `SPIRV` | Enable SPIR-V output |
| `TARGET_VULKAN` | Target Vulkan semantics |
| `TARGET_GLSL` | Target OpenGL semantics |

---

## CMake Output

The `dump()` method generates CMake code that:

1. Sets ShaderMake binary path
2. Sets slangc compiler path (from `$ENV{VULKAN_SDK}/bin/slangc`)
3. Validates tool existence
4. Creates `add_custom_target` with ShaderMake command

**Generated command flags:**
```
-p SPIRV                     # Output SPIR-V
--platform SPIRV             # Target platform
-c "${CONFIG_FILE}"          # Config file
-o "${OUTPUT_DIR}"           # Output directory
-B                           # Build (required)
--outputExt .bin             # Output extension
--compiler "${SLANG}"        # slangc path
--slang                      # Enable slang mode
--slangHLSL                  # HLSL input
-D SPIRV                     # Define SPIRV
-D TARGET_VULKAN             # Define TARGET_VULKAN
--tRegShift 0               # Texture register shift
--sRegShift 128              # Sampler register shift
--bRegShift 256              # Buffer register shift
--uRegShift 384              # UAV register shift
--vulkanVersion 1.3          # Vulkan version
--shaderModel 6_5            # Shader model
--project HLVM                # Project name
```

---

## Integration with PyCMake

### Step 1: Import the module

```python
# In Runtime_cmake.py
import sys
sys.path.insert(0, "${CMAKE_SOURCE_DIR}/../Common")
from ShaderMakeBuild import create_deferred_shading_shadermake
```

### Step 2: Create module instance

```python
deferred_shaders = create_deferred_shading_shadermake()
```

### Step 3: Append to project modules

```python
runtime_project.modules.append(deferred_shaders)
```

---

## Manual Compilation (without CMake)

### Using ShaderMake directly

```bash
# Full path to ShaderMake binary
SHADERMAKE=/path/to/ShaderMake
SLANGC=$VULKAN_SDK/bin/slangc

$SHADERMAKE \
    -p SPIRV \
    --platform SPIRV \
    -c TestDeferredShading_shaders.cfg \
    -o Test/TestDeferredShading_Data/ \
    -B \
    --outputExt .bin \
    --compiler $SLANGC \
    --slang \
    --slangHLSL \
    -D SPIRV \
    -D TARGET_VULKAN \
    --tRegShift 0 --sRegShift 128 --bRegShift 256 --uRegShift 384 \
    --vulkanVersion 1.3 \
    --shaderModel 6_5 \
    --project HLVM
```

### Using slangc directly

```bash
SLANGC=$VULKAN_SDK/bin/slangc

# Vertex shader
$SLANGC -target spirv -D SPIRV -D TARGET_VULKAN \
    -profile vs_6_5 -entry BlitVS \
    -o blit_vs.spv blit_vs.hlsl

# Fragment shader
$SLANGC -target spirv -D SPIRV -D TARGET_VULKAN \
    -profile ps_6_5 -entry BlitPS \
    -o blit_ps.spv blit_ps.hlsl
```

---

## NVRHI Binding Offsets

ShaderMake uses register shifts to match NVRHI defaults:

| Register Type | Shift | NVRHI Default |
|--------------|-------|---------------|
| Textures (t) | 0 | `tRegShift = 0` |
| Samplers (s) | 128 | `sRegShift = 128` |
| Buffers (b) | 256 | `bRegShift = 256` |
| UAVs (u) | 384 | `uRegShift = 384` |

**Note:** HLSL shader bindings use `register(t0)`, `register(s0)`, etc. These map to SPIR-V bindings after applying shifts.

---

## HLSL to GLSL Differences

When writing HLSL for Vulkan:

| HLSL | GLSL | Notes |
|------|------|-------|
| `Texture2D tex : register(t0)` | `layout(binding=0) uniform texture2D tex` | Need `[[vk::binding(...)]]` or `-fvk-t-shift` |
| `SamplerState samp : register(s0)` | `layout(binding=0) uniform sampler samp` | Need `[[vk::binding(...)]]` |
| `cbuffer CB : register(b0)` | `layout(binding=0) uniform CB { ... }` | Need `[[vk::binding(...)]]` |
| `RWTexture2D tex : register(u0)` | `layout(binding=0, rgba16f) uniform writeonly image2D tex` | UAV syntax differs |
| `tex.Load(int3(coord, 0))` | `texelFetch(tex, coord, 0)` | Samplerless load |
| `GroupMemoryBarrier()` | `groupMemoryBarrier()` | Compute sync |

### Example: Corrected HLSL with Vulkan bindings

```hlsl
// Explicit Vulkan binding
[[vk::binding(0, 0)]] Texture2D inputTexture : register(t0);
[[vk::binding(1, 0)]] SamplerState samp : register(s0);

float4 BlitPS(BlitVS_Output Input) : SV_Target0
{
    return inputTexture.Sample(samp, Input.UV);
}
```

---

## File Structure

```
HLVM-Engine/
├── Engine/
│   ├── Source/
│   │   ├── Common/
│   │   │   └── ShaderMakeBuild.py      # PyCMake integration module
│   │   └── Runtime/
│   │       ├── Test/
│   │       │   ├── TestDeferredShading_shaders.cfg  # Config file
│   │       │   └── TestDeferredShading_Data/
│   │       │       ├── blit_vs.hlsl     # HLSL source
│   │       │       ├── blit_ps.hlsl     # HLSL source
│   │       │       ├── blit_vs.bin      # Compiled blob
│   │       │       └── blit_ps.bin      # Compiled blob
│   │       └── Runtime_cmake.py         # Build integration
│   └── Binary/
│       └── GNULinux-x64/
│           └── ShaderMake/
│               └── bin/
│                   └── ShaderMake       # NVIDIA tool
└── ThirdParty/
    └── vulkansdk/
        └── Bin/
            └── slangc                   # Slang compiler
```

---

## Troubleshooting

### "ShaderMake not found"

```bash
# Verify ShaderMake exists at expected path
ls "$CMAKE_SOURCE_DIR/../../Binary/GNULinux-x64/ShaderMake/bin/ShaderMake"
```

### "slangc not found"

```bash
# Verify VULKAN_SDK is set
echo $VULKAN_SDK
ls $VULKAN_SDK/Bin/slangc
```

### "undefined identifier" in HLSL

- Check entry point name matches `-E` flag
- Verify `-D SPIRV -D TARGET_VULKAN` defines are set

### Binding mismatch at runtime

- HLSL `register(t0)` maps to SPIR-V binding after `-fvk-t-shift`
- Default shifts: `--tRegShift 0 --sRegShift 128 --bRegShift 256 --uRegShift 384`
- Use `[[vk::binding(binding, set)]]` for explicit Vulkan bindings

---

## References

- [NVIDIA Donut ShaderMake](https://github.com/NVIDIAGameWorks/donut)
- [Slang Compiler](https://shaderlang.org/)
- [SPIR-V Specification](https://registry.khronos.org/SPIR-V/)
