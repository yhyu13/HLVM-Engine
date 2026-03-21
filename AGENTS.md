# HLVM-Engine Agent Guidelines

## Build & Test Commands

### Primary Build Script
```bash
./GenerateCMakeProjects.sh    # Generate CMakeLists.txt files
./Build.sh [OPTIONS]          # Build/test project
```

### Running Tests
```bash
# Run ALL tests (all configurations)
./Build.sh --Test

# Run single test by name
./Build.sh --Config=Debug --Target=TestParallel --Test

# Rebuild + test with repetition
./Build.sh --Rebuild --Test --TestRepeatNum=2

# Performance profiling
./Build.sh --Config=RelWithDebInfo --GPerf --Target=TestMemory
```

### Available Options
| Option | Description | Example |
|--------|-------------|---------|
| `--Config=<type>` | Debug/RelWithDebInfo/Release | `--Config=Debug` |
| `--Target=<name>` | Specific test executable | `--Target=TestLogger` |
| `--Test` | Run CTest after build | standalone flag |
| `--TestRepeatNum=<N>` | Number of repetitions | `--TestRepeatNum=3` |
| `--Jobs=<N>` | Parallel compilation jobs | `--Jobs=8` |
| `--Verbose` | Show CMake output | standalone flag |

**Warning**: Don't use `ctest -j N` (parallel ctest) - causes mallocator errors.

---

## Code Style Guidelines

### Project Structure
```
HLVM-Engine/
├── Engine/
│   ├── Source/Common/Public/     # Header-only interfaces
│   ├── Source/Common/Private/    # Implementation
│   └── Source/Runtime/           # Engine subsystems
├── Binary/<Config>/              # Build outputs
┗── Samples/                      # Demo applications
```

**Key**: Public/Private separation for modules (inspired by Unreal Engine)

### Include Order
```cpp
// 1. Project's own headers (in defined order)
#pragma once
#include "Core/String.h"
#include "Platform/PlatformDefinition.h"

// 2. Standard Library
#include <atomic>
#include <memory>
#include <string>

// 3. Third-party (Boost, external libs)
#include <boost/container/vector.hpp>
#include <spdlog/spdlog.h>
```

### Naming Conventions
- **Classes**: PascalCase (`FString`, `FVulkanDevice`)
- **Functions**: camelCase (`GetCurrentThreadId`, `RenderFrame`)
- **Member Variables**: camelCase prefixed with `g_` or no prefix (`GMallocator`, `Width`)
- **Macros**: UPPER_CASE (`DECLARE_LOG_CATEGORY`, `HLVM_LOG`)
- **Types/Templates**: PascalCase with T prefix (`TFString`, `TSmallVector<T,N>`)
- **Templates**: PascalCase with F prefix on free functions (`FFmtString`)

### Types & Types Aliases
Use engine-defined type aliases (from `TypeDefinition.h`):
```cpp
typedef std::byte TBYTE;
typedef std::uint8_t TUINT8;
typedef std::int32_t TINT32;
typedef std::uint64_t TUINT64;
typedef double TFLOAT;        // Note: double precision!
typedef std::basic_string<char8_t> FString;

using FByteBuffer = std::span<TBYTE>;
```

### Memory Patterns
- Use `std::shared_ptr`/`TSharedPtr` for reference-counted objects
- Prefer stack allocation where possible
- Override global `new/delete` with custom allocator
- Use `TMiMallocator` (mimalloc wrapper) as thread-local default
- Avoid heap allocation in hot paths - use `FStackAllocator` for temporary allocations

### Error Handling
- **NO exceptions** - disabled for performance
- Use assertion macros instead:
  ```cpp
  HLVM_ASSERT(condition);                    // Development only
  HLVM_ASSERT_F(condition, TXT("msg {}"), x);
  HLVM_ENSURE(condition);                    // Always evaluated
  HLVM_LOG(category, error, TXT("Error"));   // Runtime logging
  ```
- Custom exception throws program, prints stack trace

### Formatting (based on clang-format implicit rules)
- Indentation: 4 spaces per level
- Brace style: Allman/breaking braces
- Line length: ~120 chars (soft limit)
- Trailing commas in struct/class definitions
- No trailing whitespace

---

## Core Systems Reference

### Logging System (UE5-style)
```cpp
// Declare log category
DECLARE_LOG_CATEGORY(LogMyModule)

// Use in code
HLVM_LOG(LogMyModule, info, TXT("Message value: {}", val));
HLVM_CLOG(cond, LogMyModule, warning, TXT("Warn: {}"), msg);
```

### Console Variables (CVar)
```cpp
// Define CVar (auto-generated from macro)
AUTO_CVAR_BOOL(r_VSync, true, "Enable VSync", Saved)
AUTO_CVAR_INT(r_MaxAnisotropy, 8, "Max anisotropic filtering", Saved)

// Access value
if (CVar_r_VSync) { /* ... */ }
int32_t maxAniso = CVar_r_MaxAnisotropy;
CVar_r_VSync.SetValue(false);
```

### Platform Layer
- Linux: GCC/Clang with ptrace debugging
- Windows: MSVC compatibility layer
- Common interface via `GenericPlatform` + platform-specific implementations

---

## Testing Guide

### Test File Locations
- `/Engine/Source/Common/Test/`     - Common module tests
- `/Engine/Source/Runtime/Test/`    - Runtime module tests

### Sample Test Pattern
```cpp
// Top of test file
DECLARE_LOG_CATEGORY(LogTest)
RECORD(test_name, true) {
    // Test setup and assertions
    HLVM_LOG(LogTest, info, TXT("Test running..."));
    
    // Actual test
    CheckCondition();
}
```

### CTest Integration
Tests auto-register via CMake function:
```cmake
add_test(NAME ${TEST_TARGET} COMMAND $<TARGET_FILE:${TEST_TARGET}> ${ARGN})
```

**Run with logs**:
```bash
./Build.sh --Config=Debug --Target=TestParallel --Test
# Logs saved to Testing/build_test_Debug_TestParallel.log
```

---

## Dependencies & Tooling

### Build Requirements
- clang-16 or later
- CMake 3.28+
- vcpkg dependency manager (forked at `/Engine/Source/Dependency/vcpkg`)

### Key Frameworks
- **vcpkg.json** at root of each module - dependency declarations
- **PyCMake** - Python-based cmake generator (custom build system)
- **CTest** - Unit testing framework

### External Libraries (selected)
- Boost (containers, filesystem, fibers)
- spdlog (logging)
- mimalloc (memory allocator)
- Vulkan Memory Allocator
- glslang/glsl (shader compilation)
- assimp (3D model import)
- Botan3 (encryption)
- Zstd (compression)

---

## Git Workflow

### Commit Practices
- Follow conventional commits: `feat()`, `fix()`, `refactor()`, `docs()`
- Keep commits focused and atomic
- Write clear messages describing WHY not WHAT

### Pre-push Checklist
1. Run `./Build.sh --Rebuild --Test` (all configs)
2. Ensure clean compiler warnings (`-Werror` enabled)
3. Verify all tests pass

---

## Additional Resources

- `/DOC_Coding_Style.md`         - Full coding standards document
- `/README.md`                    - Project overview and features
- `/Engine/Scripts/pycmake/`      - PyCMake source code
- `/Engine/Source/Dependency/README.md` - vcpkg upgrade guide

---

# HLVM-Engine Documentation for Vibe Coding

## Project Overview
A UE5-inspired game engine framework focused on infrastructure development with enhanced pak file handling, memory management, and tooling. Prioritizes Linux development workflow, C++20 features, and performance optimizations.

## Core Architecture
- **Engine/**: Contains core engine systems and subsystems
- **Source/**: Main source code repository
  - **Common/**: Shared utilities and cross-platform components
  - **Platform/**: OS-specific implementations
  - **Core/**: Fundamental engine systems
- **Build/**: Build system and configuration files
- **ThirdParty/**: External dependencies and libraries

## Build & Setup
- **Prerequisites**: 
  - Anaconda3
  - git
  - clang-17
  - cmake 3.29
- **Key Scripts**:
  - `Setup.sh`: Environment setup and dependency installation
  - `GenerateCMakeProjects.sh`: CMake project generation

## Key Features
- **Custom Build System**: PyCMake Python package generating CMake projects
- **Memory Management**: 
  - Global mallocator interface with TLS mimallocator
  - Stack allocator (2/3 cost of mimalloc)
  - Virtual memory arena with binned allocators
- **String Handling**: 
  - `chat8_t` char type with UE5-style string handling
  - `FString` and `FPath` implementations
  - String pooling system
- **Compression**: Zstd wrapper with custom interface
- **Encryption**: Botan3-based RSA with PKCS8 obfuscation
- **Logging**: UE5-style macros with compile-time level elimination
- **Parallelism**: 
  - Spin/Rival locks
  - Work-steal thread pool
  - MPMC queues with 1.5x Boost performance

## Dependencies
- **Core Libraries**:
  - Boost (filesystem, hashing)
  - Botan3 (encryption)
  - Zstd (compression)
  - RapidJSON (JSON parsing)
  - Protobuf (serialization)
- **Development Tools**:
  - Backward.cpp (stack traces)
  - Ctre (compile-time regex)
  - Magic Enum (enumeration handling)
  - Mimalloc (memory allocator)

## Technical Highlights
- **Obfuscation**: AdvoObfuscator integration for sensitive strings
- **File System**: Packed token/containers with hash-based indexing
- **Debugging**: PTrace (Linux) and WinAPI (Windows) debugger detection
- **Concurrency**: Lock-free queues with rival lock optimization
- **Memory**: Custom VM allocator with sharded free lists (WIP)

This additional vibe coding info focuses on infrastructure components critical for vibe coding: core engine architecture, build system, memory management patterns, and key library integrations. Omitted: test files, temporary configurations, and rendering-related components.