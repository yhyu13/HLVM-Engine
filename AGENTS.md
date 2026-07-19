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

### Ray-Tracing Payloads (slangc)
- **Keep RT payloads compact (<=64 bytes) with every field written+read on BOTH sides of TraceRay.** slangc compiles each entry point independently and can dead-strip payload fields an entry never uses; asymmetric stripping desyncs the raygen/closesthit payload layout and every returned value arrives as garbage (symptom: valid instance IDs/barycentrics but corrupt normals/albedos - red/black noise GI).
- Fully initialize all payload fields in raygen before `TraceRay`.
- Move bounce shading that needs the hit normal INTO closesthit instead of passing the normal through the payload.

### Area Lights vs Geometry
- **Never place an area light exactly coplanar with occluding geometry** (e.g. a ceiling-sized light at the ceiling plane). Normal-offset shadow rays then hit the coplanar geometry just inside `tMax = lightDistance - eps` and systematically self-occlude (floor black with fireflies). Drop the light slightly off the surface (e.g. y=0.99 for a unit box) - see `TestPathTraceGI_Data/CornellBox_Lights.json`.

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

### GExecutableDirectory
```cpp
// Actual directory containing the executable binary (derived from av[0])
FString::Format(TXT("{}/{}.log"), *GExecutableDirectory, *GExecutableName);
```

### GProjectRoot
```cpp
// Use GProjectRoot for paths that must work regardless of CWD
// GProjectRoot is set from compile-time HLVM_ROOT (absolute project root)
FString::Format(TXT("{}/Engine/Source/Runtime/ThirdParty/Imgui/Shader"), *GProjectRoot);
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

### TMap / phmap
```cpp
// TMap<Key, Value, Allocator> — third param is ALLOCATOR, not hash!
// For custom hash, use phmap::node_hash_map directly:
phmap::node_hash_map<FMeshCache::FMeshKey, FMeshCache::FMeshEntry, FMeshCache::FMeshKeyHash> Cache;
```

---

## Core Systems

### Logging
```cpp
DECLARE_LOG_CATEGORY(LogMyModule)
HLVM_LOG(LogMyModule, info, TXT("Message: {}"), val);
HLVM_CLOG(condition, LogMyModule, warning, TXT("Warn: {}"), msg);
```

Test executables automatically register both `FSpdlogConsoleDevice` and `FSpdlogFileDevice`. File logs are written to `{GExecutableDirectory}/{GExecutableName}.log`. On each run the previous log is rotated to `{Name}_1.log` and then `{Name}_2.log`; only the latest 3 runs are kept. There is no per-file size limit.

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

### FShaderLibrary (Shader Permutations)

```cpp
#include "Renderer/Shader/ShaderLibrary.h"

// Load default permutation (no macros)
nvrhi::ShaderHandle Shader = FShaderLibrary::Get().LoadShader(
    Device, ShaderDataDir, TXT("MyShader.sblob"), nvrhi::ShaderType::Compute);

// Load with macros — permutations sorted alphabetically before lookup
TVector<FShaderMacro> Macros;
Macros.push_back({TXT("ENABLE_SSAO"), TXT("1")});
Macros.push_back({TXT("ENABLE_CONTACT_SHADOWS"), TXT("0")});
nvrhi::ShaderHandle Shader = FShaderLibrary::Get().LoadShader(
    Device, ShaderDataDir, TXT("MyShader.sblob"), nvrhi::ShaderType::Compute, Macros);

// Macro order independence: {SSAO=1,CS=0} ≡ {CS=0,SSAO=1}
// Cache key format: "ENABLE_CONTACT_SHADOWS=0 ENABLE_SSAO=1"

// Poll all cached files for modifications
TVector<FPath> Modified = FShaderLibrary::Get().PollAllCachedFiles();
```

---

### FTextureCache (GPU Texture Deduplication)

**NO LONGER a singleton** — owned by `FSceneResourceManager`, persists across scene reloads.

```cpp
// Automatic (preferred): FSceneResourceManager handles lifetime
FSceneResourceManager ResourceManager;
ResourceManager.Initialize(Device, ScenePath);
// ... use ...
ResourceManager.Shutdown(); // Scene released, CACHE PERSISTS for next Initialize()
// ... reload scene ...
ResourceManager.Initialize(Device, ScenePath); // Textures hit cache

// Destructor clears cache (true destruction only)
```

**Key rules**:
- `FTextureCache` is a regular constructable class
- `FAsyncTextureLoader` uses an active cache pointer set by `FSceneResourceManager::Initialize()`
- Cache survives `Shutdown()` — only cleared in destructor

---

### FAsyncTextureLoader (Non-Blocking Async Textures)

```cpp
#include "Renderer/Texture/AsyncTextureLoader.h"

// Begin loading (non-blocking, spawns worker threads)
FAsyncTextureLoader::BeginAsyncLoad(Device, Materials,
    {IMaterial::ETextureType::Albedo,
     IMaterial::ETextureType::Normal,
     IMaterial::ETextureType::Metallic,
     IMaterial::ETextureType::Roughness,
     IMaterial::ETextureType::AmbientOcclusion});

// Poll once per frame — returns number of textures uploaded
uint32_t Uploaded = FAsyncTextureLoader::Poll(Device);

// Check if loads are still in flight
if (FAsyncTextureLoader::HasPendingLoads()) { /* ... */ }
```

---

### FSceneResourceManager (Unified Scene Lifetime)

Replaces manual `FSceneGPUData` + `FTextureCache::Clear()` + `FAsyncTextureLoader::Poll()`.

```cpp
#include "Renderer/FSceneResourceManager.h"

FSceneResourceManager ResourceManager;
ResourceManager.Initialize(Device, ScenePath);

// Each frame:
auto DrawData = ResourceManager.BuildDrawData();
uint32_t Uploaded = ResourceManager.PollAsyncLoads();

// Cleanup (guaranteed order: scene → texture cache → device)
ResourceManager.Shutdown();
```

**Scene bounds accessors** (no need to BuildDrawData just for camera placement):
```cpp
glm::vec3 Center = ResourceManager.GetSceneCenter();
float Radius = ResourceManager.GetSceneRadius();
```

---

### GPU Profiler

```cpp
FGPUProfiler Profiler;
Profiler.Initialize(Device);

// Each frame:
Profiler.BeginFrame();
Profiler.BeginPass(CmdList, TXT("GBuffer"));
// ... render ...
Profiler.EndPass(CmdList);
Profiler.EndFrame();
```

**Timer query budget**: Default `FDeviceCreationParameters::MaxTimerQueries = 1024`. Increase if tests with many passes crash.

---

### GPU Instancing

Both `FGBufferFillPass` and `FShadowMapPass` support instanced drawing via structured buffer instance matrices + `SV_InstanceID`.

```cpp
// Build instance data (one float4x4 per instance)
nvrhi::BufferDesc InstanceBufferDesc;
InstanceBufferDesc.setByteSize(InstanceCount * sizeof(glm::mat4))
    .setStructStride(sizeof(glm::mat4))
    .setInitialState(nvrhi::ResourceStates::ShaderResource)
    .setKeepInitialState(true);
nvrhi::BufferHandle InstanceBuffer = Device->createBuffer(InstanceBufferDesc);
CmdList->writeBuffer(InstanceBuffer, Matrices.data(), InstanceCount * sizeof(glm::mat4));

// GBuffer instanced draw
FGBufferFillPass::FInstancedMeshDrawItem Item;
Item.VertexBuffer = VB;
Item.IndexBuffer = IB;
Item.InstanceBuffer = InstanceBuffer;
Item.IndexCount = 36;
Item.InstanceCount = 100;
Item.Material = /* ... */;

FGBufferFillPass::FInstancedRenderDesc Desc;
Desc.ViewConstants = /* ... */;
Desc.InstancedItems = &Item;
Desc.InstancedItemCount = 1;
GBufferPass.RenderInstanced(CmdList, Desc);

// Shadow instanced draw (same instance buffer)
FShadowMapPass::FInstancedMeshDrawItem ShadowItem;
ShadowItem.VertexBuffer = VB;
ShadowItem.IndexBuffer = IB;
ShadowItem.InstanceBuffer = InstanceBuffer;  // Reuse!
ShadowItem.IndexCount = 36;
ShadowItem.InstanceCount = 100;

FShadowMapPass::FInstancedRenderDesc ShadowDesc;
ShadowDesc.LightViewProj = /* ... */;
ShadowDesc.InstancedItems = &ShadowItem;
ShadowDesc.InstancedItemCount = 1;
ShadowPass.RenderInstanced(CmdList, ShadowDesc);
```

**Key rules**:
- Instanced shaders (`*InstancedVS.sblob`) are **optional** — if missing, the pass logs a warning and disables instancing
- The instance buffer is a `StructuredBuffer<float4>` in HLSL, indexed by `SV_InstanceID * 4` for matrix rows
- Both passes can coexist with non-instanced draws in the same frame

---

## Key Files

| File | Purpose |
|------|---------|
| `Engine/Source/Runtime/Public/Renderer/SceneGraph/FNode.h` | Scene graph transform hierarchy |
| `Engine/Source/Runtime/Public/Renderer/Deferred/FGBufferFillPass.h` | GBuffer fill pass (instancing supported) |
| `Engine/Source/Runtime/Public/Renderer/Shadow/FShadowMapPass.h` | Shadow map pass (instancing supported) |
| `Engine/Source/Runtime/Public/Renderer/FSceneResourceManager.h` | Unified scene + texture lifetime |
| `Engine/Source/Runtime/Public/Renderer/Texture/TextureCache.h` | GPU texture deduplication cache |
| `Engine/Source/Runtime/Public/Renderer/Texture/AsyncTextureLoader.h` | Non-blocking async texture loading |
| `Engine/Source/Runtime/Public/Renderer/Shader/ShaderLibrary.h` | Shader blob cache with permutations |
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
