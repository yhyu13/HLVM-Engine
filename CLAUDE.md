# OpenWolf

@.wolf/OPENWOLF.md

This project uses OpenWolf for context management. Read and follow .wolf/OPENWOLF.md every session. Check .wolf/cerebrum.md before generating code. Check .wolf/anatomy.md before reading files.


# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Generate CMakeLists.txt from PyCMake
./GenerateCMakeProjects.sh

# Build all tests
./Engine/Source/Common/Build.sh --Test
./Engine/Source/Runtime/Build.sh --Test

# Build single test (preferred over parallel ctest)
./Engine/Source/Runtime/Build.sh --Config=Debug --Target=TestSceneGraphNode --Test

# Available options
--Config=<Debug|RelWithDebInfo|Release>
--Target=<name>
--Test
--Clean
--Rebuild
--Verbose
--GraphViz
--GPerf
--Jobs=N
--TestRepeatNum=N
--DumpFrames=N        # Enable frame dump: N frames per test
--DumpDir=<path>      # Override frame dump output directory
```

**WARNING**: Do NOT use `ctest -j N` directly — causes mallocator errors.

---

## Architecture

HLVM-Engine is a game engine inspired by Unreal Engine 5, focused on core infrastructure (memory management, rendering, telemetry).

### Directory Structure
```
Engine/Source/
├── Common/           # Shared utilities (Public/Private/Test)
│   ├── Public/      # Header-only interfaces (Core/, Platform/, Template/)
│   └── Private/     # Implementation
├── Runtime/         # Engine runtime
│   ├── Public/      # RHI, SceneGraph, Window, Mesh, Material, Texture
│   ├── Private/     # Vulkan device implementation
│   └── Test/        # Runtime tests
└── Dependency/
    └── vcpkg/       # Forked vcpkg for dependencies

Binary/              # Build outputs
Samples/            # Asset files
```

### Public/Private Separation
Following UE5 pattern: `Public/` contains header-only interfaces, `Private/` contains implementation details.

### Core Types (`TypeDefinition.h`)
```cpp
typedef std::uint8_t   TUINT8;
typedef std::int32_t   TINT32;
typedef std::uint64_t  TUINT64;
typedef double         TFLOAT;  // double precision
typedef std::basic_string<char8_t> FString;
using FByteBuffer = std::span<TBYTE>;
```

### NVRHI Integration
Uses forked NVRHI from `https://github.com/yhyu13/NVRHI.git`

---

## Critical Gotchas

### NVRHI Rendering
- `constantBufferOffset` defaults to 256 — set explicitly to 0 for GLSL binding 0:
  ```cpp
  VulkanBindingOffsets offsets;
  offsets.constantBufferOffset = 0;
  ```
- Depth clear: `ClearDepthStencilAttachment(cmd, fb, 1.0f, 0u)` NOT `ClearDepthAttachment`

### FNode Scene Graph
```cpp
auto& child = parentNode.AddChild<FNode>(TXT("ChildName"));
child.SetPosition(FVec3(1, 2, 3));
child.UpdateWorldTransform();  // MUST call after Set*
```
Use `IsUpdating` flag to prevent infinite recursion in transform propagation.

### Camera Coordinates
- Camera looks in **-Z direction** by default
- For camera to see scene at z=-39, camera must be at z > -39
- Use `MoveToAndLookAt(cameraPos, targetPos)` for orientation

### GExecutablePath
`GExecutablePath` is `boost::filesystem::current_path()` — points to CWD, NOT executable directory.
```cpp
// Shader path example:
FString::Format(TXT("{}/../../Test/{}_Data"), *GExecutablePath, *GExecutableName);
```

### MiMalloc2 Thread Cleanup
```cpp
mi::Mallocator::thread_done();  // MUST call on thread exit
```

### Thread Safety
```cpp
LOCK_GUARD(flag);               // RAII lock
FWorkStealThreadPool pool(8);
auto future = pool.Submit([]() { return result; });
```

### Logging Macros
Available: `HLVM_LOG`, `HLVM_CLOG`, `HLVM_CLOG_ELSE_FATAL`
NOT available: `HLVM_LOG_F`, `HLVM_LOGF`
```cpp
HLVM_CLOG(condition, LogMyModule, warning, TXT("Warn: {}"), msg);
```

### Matrix Operations
- **COPY** matrices before comparing (not reference)
- `SetFov()` modifies matrix in-place

### shared_from_this Trap
Only works on `shared_ptr`-owned objects. `std::make_unique` objects cannot use `shared_from_this()`.

---

## Code Style

### Naming Conventions
| Element | Convention | Example |
|---------|------------|---------|
| Classes | PascalCase | `FString`, `FVulkanDevice` |
| Functions | camelCase | `GetCurrentThreadId` |
| Member vars | camelCase | `GMallocator`, `Width` |
| Macros | UPPER_CASE | `DECLARE_LOG_CATEGORY` |
| Types/Templates | PascalCase + T prefix | `TFString`, `TSmallVector<T,N>` |

### Include Order
1. Project headers (`Core/`, `Platform/`)
2. Standard library (`<atomic>`, `<memory>`)
3. Third-party (`<boost/container/vector.hpp>`)

### Error Handling
No exceptions. Use:
```cpp
HLVM_ASSERT(condition);                         // Dev only
HLVM_ASSERT_F(condition, TXT("msg {}"), x);    // Dev only with format
HLVM_ENSURE(condition);                         // Always evaluated
HLVM_LOG(LogCat, error, TXT("Error: {}"), val);
```

### Formatting
- 4 spaces indent, Allman braces (breaking), ~120 char line length
- Trailing commas in struct/class definitions, no trailing whitespace

### Console Variables (CVar)
```cpp
AUTO_CVAR_BOOL(r_VSync, true, "Enable VSync", Saved)
AUTO_CVAR_INT(r_MaxAnisotropy, 8, "Max anisotropic filtering", Saved)
if (CVar_r_VSync) { /* ... */ }
CVar_r_VSync.SetValue(false);  // Modify at runtime
```
**Flags**: `Saved` (persist to INI), `ReadOnly`, `Cheat`, `Console`

---

## Testing

```cpp
DECLARE_LOG_CATEGORY(LogTest)
RECORD(test_name, true) {
    HLVM_LOG(LogTest, info, TXT("Running..."));
    CheckCondition(value == expected);
}
```

Test locations: `Engine/Source/Common/Test/`, `Engine/Source/Runtime/Test/`

---

## ShaderMake (HLSL → SPIR-V)

ShaderMake compiles HLSL shaders to SPIR-V blobs.

**Config** (`Test/TestName_Data/ShaderMake.cfg`):
```ini
[options]
entry = vsMain
profile = vs_6_7

[make]
shader0 0 * vsMain vs vs_6_7
```

**Usage**:
```cpp
#include "ShaderMake/ShaderMakeLoader.h"
ShaderMake::FindPermutationInBlob(blob, "vsMain", &outEntry, &outSize);
```

---

## Frame Dump (Generic Render Test Debugging)

Generic frame dump utility for render tests via `FRenderPassDumper`.

**Header**: `Engine/Source/Runtime/Public/Image/FRenderPassDumper.h`

### Usage in Render Tests

```cpp
#include "Image/FRenderPassDumper.h"

class FSponzaDeferredPass : public IRenderPass {
    FRenderPassDumper mFrameDumper;  // RAII - auto cleanup

    bool Initialize(...) {
        mFrameDumper.Initialize(NvrhiDevice, nvrhi::Format::RGBA16_FLOAT);
        mFrameDumper.SetTestName(TXT("TestSponzaDeferred"));
    }

    void Render(...) {
        // ... render to HDRTexture ...

        if (mFrameDumper.IsEnabled()) {
            mFrameDumper.BeginDump(NvrhiDevice, HDRTexture.Get(), width, height);
        }

        // ... more rendering ...

        if (mFrameDumper.IsEnabled()) {
            if (mFrameDumper.EndDump(CmdList)) {
                HLVM_LOG(LogTest, info, TXT("Dumped: frame {}"), mFrameDumper.GetCurrentFrame());
            }
            if (mFrameDumper.IsLastFrame()) {
                return;  // Skip blit on last frame
            }
        }

        // ... blit to screen ...
    }
};
```

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `HLVM_DUMP_RT` | (none) | Enable frame dump when set |
| `HLVM_DUMP_FRAMES` | 4 | Number of frames to dump |
| `HLVM_DUMP_DIR` | (auto) | Output directory override |

### Example

```bash
# Direct env var (any test binary)
HLVM_DUMP_RT=1 ./Binary/.../TestSponzaDeferred
HLVM_DUMP_RT=1 HLVM_DUMP_FRAMES=8 HLVM_DUMP_DIR=/tmp ./Binary/.../TestSponzaDeferred

# Via Build.sh (runs tests with env vars set automatically)
./Engine/Source/Runtime/Build.sh --Config=Debug --Target=TestRTShadowsGBuffer --Test --DumpFrames=4
./Engine/Source/Runtime/Build.sh --Config=Debug --Target=TestRTShadowsGBuffer --Test --DumpFrames=2 --DumpDir=/tmp/my_dumps
```

Output goes to `Engine/Source/Runtime/Test/TestName_Data/` (or `--DumpDir` if specified) with timestamp filenames.

---

## Key Files

| File | Purpose |
|------|---------|
| `Engine/Source/Runtime/Public/Renderer/SceneGraph/FNode.h` | Scene graph transform hierarchy |
| `Engine/Source/Runtime/Private/Renderer/DeviceManagerVk*.cpp` | NVRHI Vulkan device |
| `Engine/Source/Runtime/Test/TestSceneGraphNode.cpp` | FNode/camera tests |
| `Engine/Source/Runtime/Test/TestCubeOnPlane.cpp` | NVRHI rendering example |
| `Engine/Source/Runtime/Test/TestFullDeferredShading2.cpp` | Deferred shading with GBuffer |
| `Engine/Source/Runtime/Public/Image/FRenderPassDumper.h` | Frame dump utility |
| `Engine/Source/Common/Public/Core/CVar/CVarMacros.h` | Console variable system |
| `Engine/Source/Common/Test/Test.h` | Testing framework |

---

## Dependencies

**vcpkg** fork at `Engine/Source/Dependency/vcpkg` manages graphics (Vulkan, GLFW3, GLM), compression (zstd, Botan3), scripting (LuaJIT, sol2), and profiling (Tracy, gperftools).
