# HLVM-Engine Agent Guidelines

## Build & Test Commands

```bash
./GenerateCMakeProjects.sh    # Generate CMakeLists.txt from PyCMake
./Build.sh --Test              # Run ALL tests
./Build.sh --Config=Debug --Target=TestSceneGraphNode --Test  # Single test
./Build.sh --Rebuild --Test --TestRepeatNum=2  # Rebuild + repeat
```

**Options**: `--Config=<Debug|RelWithDebInfo|Release>`, `--Target=<name>`, `--Test`, `--TestRepeatNum=N`, `--Jobs=N`, `--Clean`, `--Rebuild`, `--Verbose`, `--GraphViz`, `--GPerf`

**WARNING**: Don't use `ctest -j N` - causes mallocator errors.

---

## Project Structure

```
HLVM-Engine/
├── Engine/Source/
│   ├── Common/              # Shared utilities
│   │   ├── Public/         # Header-only interfaces
│   │   ├── Private/        # Implementation
│   │   └── Test/           # Common tests
│   └── Runtime/            # Engine runtime
│       ├── Public/         # RHI, Window, SceneGraph
│       ├── Private/        # DeviceManagerVk, etc.
│       └── Test/           # Runtime tests
├── Binary/                 # Build outputs
├── Build/                  # CMake build dirs
└── Samples/               # Asset files
```

**Public/Private separation** (Unreal Engine pattern).

---

## Code Style

### Include Order
```cpp
#pragma once
#include "Core/String.h"           // 1. Project headers
#include "Platform/PlatformDefinition.h"

#include <atomic>                   // 2. Standard library
#include <memory>

#include <boost/container/vector.hpp>  // 3. Third-party
```

### Naming Conventions
| Element | Convention | Example |
|---------|------------|---------|
| Classes | PascalCase | `FString`, `FVulkanDevice` |
| Functions | camelCase | `GetCurrentThreadId` |
| Member vars | camelCase | `GMallocator`, `Width` |
| Macros | UPPER_CASE | `DECLARE_LOG_CATEGORY` |
| Types/Templates | PascalCase + T prefix | `TFString`, `TSmallVector<T,N>` |

### Types (from `TypeDefinition.h`)
```cpp
typedef std::uint8_t TUINT8;
typedef std::int32_t TINT32;
typedef std::uint64_t TUINT64;
typedef double TFLOAT;  // Note: double precision!
typedef std::basic_string<char8_t> FString;
using FByteBuffer = std::span<TBYTE>;
```

### Error Handling
**NO exceptions** - disabled for performance.

```cpp
HLVM_ASSERT(condition);                    // Dev only
HLVM_ASSERT_F(condition, TXT("msg {}"), x); // Dev only with format
HLVM_ENSURE(condition);                    // Always evaluated
HLVM_LOG(LogCat, error, TXT("Error: {}"), val);
```

### Formatting
- 4 spaces indent, Allman braces (breaking), ~120 char line length
- Trailing commas in struct/class definitions, no trailing whitespace

---

## Critical Gotchas (MUST REMEMBER)

### NVRHI Rendering
1. **`constantBufferOffset` defaults to 256** - Set explicitly to 0 for GLSL binding 0:
   ```cpp
   VulkanBindingOffsets offsets;
   offsets.constantBufferOffset = 0;
   ```
2. Buffers require `isConstantBuffer=true` + `keepInitialState=true`
3. Depth clear: `ClearDepthStencilAttachment(cmd, fb, 1.0f, 0u)` NOT `ClearDepthAttachment`

### FNode Scene Graph
```cpp
auto& child = parentNode.AddChild<FNode>(TXT("ChildName"));
child.SetPosition(FVec3(1, 2, 3));
child.UpdateWorldTransform();  // MUST call after Set*
```
**Use `IsUpdating` flag** to prevent infinite recursion in transform propagation.

### Camera Coordinates
- Camera looks in **-Z direction** by default
- For camera to see scene at z=-39, camera must be at z > -39
- Use `MoveToAndLookAt(cameraPos, targetPos)` for orientation

### GExecutablePath
```cpp
// WRONG: Points to CWD, not executable
boost::filesystem::current_path()  // This IS GExecutablePath

// CORRECT shader path:
FString::Format(TXT("{}/../../Test/{}_Data"), *GExecutablePath, *GExecutableName);
```

### MiMalloc2 Thread Cleanup
```cpp
mi::Mallocator::thread_done();  // MUST call on thread exit
```

### Logging Macros
**Available**: `HLVM_LOG`, `HLVM_CLOG`, `HLVM_CLOG_ELSE_FATAL`
**NOT available**: `HLVM_LOG_F`, `HLVM_LOGF`
```cpp
HLVM_LOG(LogMyModule, info, TXT("Value: {}"), val);
spdlog::level::critical  // correct enum, NOT 'crit'
```

### Matrix Operations
- **COPY** matrices before comparing (not reference)
- `SetFov()` modifies matrix in-place

### shared_from_this Trap
```cpp
// WRONG: Object not owned by shared_ptr
auto n = std::make_unique<Node>();
n->shared_from_this(); // CRASH!

// CORRECT: Use shared_ptr ownership
auto sp = std::make_shared<Node>();
sp->shared_from_this(); // OK
```

---

## Core Systems

### Logging
```cpp
DECLARE_LOG_CATEGORY(LogMyModule)
HLVM_LOG(LogMyModule, info, TXT("Message: {}"), val);
HLVM_CLOG(condition, LogMyModule, warning, TXT("Warn: {}"), msg);
```

### Console Variables (CVar)
```cpp
AUTO_CVAR_BOOL(r_VSync, true, "Enable VSync", Saved)
AUTO_CVAR_INT(r_MaxAnisotropy, 8, "Max anisotropic filtering", Saved)
if (CVar_r_VSync) { /* ... */ }
CVar_r_VSync.SetValue(false);
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

ShaderMake compiles HLSL shaders to SPIR-V blobs with NVSP header.

**Key files**:
- `Engine/Source/Runtime/ShaderMakeBuild.py` - PyCMake integration
- `Test/TestName_Data/ShaderMake.cfg` - Shader compilation config
- `Test/TestName_Data/*.hlsl` - HLSL shader sources

**Config format**:
```ini
[options]
entry = vsMain
profile = vs_6_7

[reflection]
...

[make]
shader0 0 * vsMain vs vs_6_7
```

**Usage**:
```cpp
#include "ShaderMake/ShaderMakeLoader.h"
ShaderMake::FindPermutationInBlob(blob, "vsMain", &outEntry, &outSize);
```

---

## Key Files

| File | Purpose |
|------|---------|
| `Engine/Source/Runtime/Public/Renderer/SceneGraph/FNode.h` | Scene graph transform hierarchy |
| `Engine/Source/Runtime/Private/Renderer/DeviceManagerVk*.cpp` | NVRHI Vulkan device |
| `Engine/Source/Runtime/Test/TestSceneGraphNode.cpp` | 12 FNode/camera tests |
| `Engine/Source/Runtime/Test/TestCubeOnPlane.cpp` | NVRHI rendering example |
| `Engine/Source/Runtime/Test/TestFullDeferredShading2.cpp` | Deferred shading with GBuffer |
| `Engine/Source/Common/Public/Core/CVar/CVarMacros.h` | CVar system |
| `Engine/Source/Common/Public/Test/Test.h` | Testing framework |

---

## Build System

- **PyCMake** (`Runtime_cmake.py`) generates CMakeLists.txt
- **vcpkg** fork at `Engine/Source/Dependency/vcpkg` for dependencies
- **NVRHI**: Forked at `https://github.com/yhyu13/NVRHI.git`

---

## Pre-Flight Checklist
1. Read `AGENTS.md` and relevant skills in `.opencode/skills/`
2. Build single test: `./Build.sh --Config=Debug --Target=<Test> --Test`
3. Verify compilation before proceeding

---

## Quick Reference

### Thread Safety
```cpp
mi::Mallocator::thread_done();  // On thread exit
LOCK_GUARD(flag);               // RAII lock
```

### Parallelism
```cpp
FWorkStealThreadPool pool(8);
auto future = pool.Submit([]() { return result; });
```

### File Paths
```cpp
FString dataDir = FString::Format(
    TXT("{}/../../Test/{}_Data"), *GExecutablePath, *GExecutableName);
```
