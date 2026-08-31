# AI Agent Navigation Map — HLVM-Engine

**Read this first.** 5-minute orientation for any AI agent (or human) entering this repo.
Dated 2026-09-01 — Phase 2 of the four-phase autonomous run.

## What this is

A Vulkan/NVRHI game engine written in modern C++ (C++23, Clang-17). It has a deferred renderer, GI / path tracing, scene graph, shader make pipeline, and ~57 test executables.

- **Branch:** `rhi2`
- **Root:** `/home/hangyu5/Documents/Gitrepo-My/HLVM-Engine`
- **Docs:** `docs/` (this dir is the new home for architecture / nav / policy)
- **Build:** `./Build.sh --Config=Debug --Target=<name> --Test`
- **Tests live as binaries:** `Engine/Source/{Common,Runtime}/Binary/<Config>/<TestName>`

## Module map (one line per area)

When you need to:
| Touch | Look at |
|---|---|
| The test framework, `RECORD`/`HLVM_TEST_EXPECT_*` | `Engine/Source/Common/Test/Test.h` |
| A renderer pass (GBuffer, shadow, etc.) | `Engine/Source/Runtime/Public/Renderer/<Name>/` |
| The scene graph / FNode | `Engine/Source/Runtime/Public/Renderer/SceneGraph/FNode.h` |
| Cameras, math | `Engine/Source/Runtime/Public/Math/`, `Renderer/SceneGraph/PerspectiveCameraNode.h` |
| Window + Vulkan device | `Engine/Source/Runtime/Private/Renderer/DeviceManagerVk*.cpp` |
| Shaders (HLSL → SPIR-V) | `Engine/Source/Runtime/Public/Renderer/ShaderMake/`, `Private/ShaderMakeBuild.py` |
| Texture cache, async loader | `Engine/Source/Runtime/Public/Renderer/Texture/` |
| Scene GPU data, resource lifetime | `Engine/Source/Runtime/Public/Renderer/FSceneResourceManager.h` |
| Materials (PBR) | `Engine/Source/Runtime/Public/Renderer/Material/` |
| GI / path tracing / ReSTIR | `Engine/Source/Runtime/Private/Renderer/Shader/GI/`, `Test/TestReSTIR_GI_Temporal*.cpp` |

## Test authoring (the rule, also in `TEST_GUIDELINES.md`)

**Use `HLVM_TEST_EXPECT_*` for assertions. Use `HLVM_ENSURE` only in production invariants.**

```cpp
#include "Test.h"
DECLARE_LOG_CATEGORY(LogMyModule)

RECORD(my_test_name)
{
    // arrange
    auto obj = MakeThing();
    // act
    auto r = obj.Do();
    // assert (RED → GREEN loop, one slice at a time)
    HLVM_TEST_EXPECT_TRUE(r.IsValid());
    HLVM_TEST_EXPECT_EQ(r.GetValue(), 42);
}
```

The test framework logs `EXPECT FAILED at <file>:<line>` and exits **non-zero** if any assertion fails. Pre-Phase 1 tests that mixed `HLVM_ENSURE` and the new macros are tracked in `PHASE_1_SDD_TDD_AUDIT.md`.

## Public/Private split (Unreal-style)

Every module has:
- `Public/` — header-only interfaces (`#pragma once`, includes nothing from `Private/`)
- `Private/` — implementation; never included from outside the module
- `Test/` — tests for the module's public API

**When editing `Public/`** you are changing the API; rebuild all dependents.
**When editing `Private/`** only the module's test needs to be rebuilt.

## Common gotchas (also in `AGENTS.md`)

- `constantBufferOffset` defaults to 256 — set to 0 for GLSL binding 0 (NVRHI Vulkan)
- RT payloads: keep ≤ 64 bytes, init all fields in raygen, move hit-shading into closesthit
- Area lights: never coplanar with occluding geometry (normal-offset shadow rays self-occlude)
- `FNode::UpdateWorldTransform()` must be called after `Set*`
- Camera looks in `-Z`; position the camera *above* the target Z
- `GExecutablePath` = CWD; use `GProjectRoot` for paths that must work regardless of CWD
- `mi::Mallocator::thread_done()` on thread exit
- Logging: `HLVM_LOG(LogCat, critical, ...)` — `critical` is the correct enum (not `error`)
- `TCHAR` is `char8_t` on Linux, `wchar_t` on Windows. `TXT("…")` makes a `TCHAR` literal
- `FString` (basic_string<TCHAR>) ≠ std::string; `c_str()` returns `const char8_t*` on Linux

## Pipeline / build

CMakeLists.txt is **generated** from PyCMake scripts:
- `GenerateCMakeProjects.sh` (root) — runs `Engine/Source/{Common,Runtime}/*_cmake.py`
- A test target = `Test<Name>` is detected from `Test/Test<Name>.cpp`
- Adding a new test file: drop it in the right `Test/`, rerun `./GenerateCMakeProjects.sh`, build

Build artifact directory:
```
Engine/Source/Common/Binary/<Config>/<Test>
Engine/Source/Common/Build/<Config>/         # ninja intermediates
Engine/Source/Runtime/Binary/<Config>/<Test>
Engine/Source/Runtime/Build/<Config>/
```

## Phase 1 (already done) — what changed

`Test.h` got `HLVM_TEST_EXPECT_*` macros with real failure detection + non-zero exit. Two high-visibility test files migrated:
- `Engine/Source/Runtime/Test/TestSceneGraphNode.cpp` (78 assertions migrated, copy-paste dupes removed)
- `Engine/Source/Common/Test/TestString.cpp` (6 new real assertions on FName / FString::Format)

## What is in scope for Phase 2 cleanup

- ✅ This navigation map (`docs/AI_NAVIGATION.md`)
- ✅ `docs/TEST_GUIDELINES.md` — assertion policy
- ⏸ Other "AI-native" refactors (per-file intent headers, CVar↔Test conventions, etc.) are deferred to specific feature work in Phase 4.

## How to ask the human about intent (when blocked)

Default: do not ask. The user explicitly authorized autonomous execution on 2026-09-01 ("all go non stop no queston ask all permission"). Only pause on real blockers:
- Build error you cannot diagnose after one round
- API/behavior contract ambiguity (silent vs. fail-fast)
- Data-loss risk (deletes, force-pushes)

For minor decisions, pick the HLVM-style-consistent option and document it in your commit message.