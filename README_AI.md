# README_AI.md - HLVM Engine Guide for AI Agents

## Project Overview

HLVM-Engine is a personal game engine project inspired by Unreal Engine 5, focusing on building core infrastructure components rather than rendering/animation/game content workflows. The project aims to create more user-friendly pak file handling, memory management, and telemetry systems compared to UE5 while sharpening Linux development toolchain and C++20 skills.

**Primary Language**: C++20
**Build System**: PyCMake (custom Python-based CMake wrapper)
**Target Platform**: Linux-x64 (with Windows support planned)
**Dependency Management**: vcpkg

## Directory Structure

```
HLVM-Engine/
├── Engine/                    # Main engine source code
│   ├── Source/                # All C++ source files
│   │   ├── Common/            # Core shared functionality
│   │   │   ├── Public/        # Public headers
│   │   │   ├── Private/       # Implementation files
│   │   │   └── ThirdParty/    # Third-party libraries
│   │   ├── Runtime/           # Runtime-specific components
│   │   ├── Editor/            # Editor functionality
│   │   ├── Plugin/            # Plugin system
│   │   └── Dependency/        # vcpkg dependencies
│   └── Scripts/               # Python build and utility scripts
│       ├── pycmake/           # Custom OOP CMake wrapper
│       └── pyarg/             # Argument parsing utilities
├── Binary/                    # Build output and toolchain binaries
├── Document/                  # Documentation
├── Samples/                   # Example code
├── build/                     # CMake build directory
├── env.yaml                   # Conda environment specification
├── Setup.sh                   # Initial setup script
└── GenerateCMakeProjects.sh   # Build generation script
```

## Core Systems

### Memory Management
Located in `Engine/Source/Common/Public/Core/Mallocator/`

- **Global Mallocator Interface**: Abstract interface allowing runtime allocator switching
- **TLS Mimalloc Integration**: Thread-local storage with mimalloc as default
- **Stack Allocator**: Optimized stack allocator (2/3 cost of mimalloc)
- **VM Mallocator (WIP)**: Custom mimalloc-like allocator with:
  - Virtual memory arena management
  - Multiple heaps per arena
  - Lock-free small binned allocator

Key Files:
- `Engine/Source/Common/Private/Core/Mallocator/Mallocator.cpp` - Implementation
- `Engine/Source/Common/Public/Core/Mallocator/VMMallocator/VMArena.h` - VM arena

### Parallelism
Located in `Engine/Source/Common/Public/Core/Parallel/`

- **Locks**: 
  - Spin lock using `std::atomic_flag`
  - Rival lock for read/write scenarios
- **Concurrent Queues**: SPSC/MPSC/MPMC implementations (1.5x Boost performance)
- **Work Steal Thread Pool**: Efficient task scheduling with work stealing
- **Fiber Pool**: Experimental implementation using boost::fibers

Key Files:
- `Engine/Source/Common/Public/Core/Parallel/Lock.h` - Lock implementations
- `Engine/Source/Common/Public/Core/Parallel/ConcurrentQueue.h` - Queues
- `Engine/Source/Common/Public/Core/Parallel/Async/WorkStealThreadPool.h` - Thread pool

### File System
Located in `Engine/Source/Common/Public/Platform/FileSystem/`

- **Boost Integration**: File mapping and I/O operations
- **Packed File System**: Custom pak-like file system
  - Token files describing file structure (offsets, sizes, compression)
  - Container files with tightly packed data
  - Runtime fragment loading (4MB chunks)

Key Files:
- `Engine/Source/Common/Public/Platform/FileSystem/Packed/PackedToken.h` - File tokens
- `Engine/Source/Common/Public/Platform/FileSystem/Packed/PackedContainerFragment.h` - Containers

### Logging System
Located in `Engine/Source/Common/Public/Core/Log.h`

- **UE5-style Logging**: `HLVM_LOG(LogXXX, level, TXT(...), ...)`
- **Log Categories**: Compile-time elimination of unwanted levels
- **Device Redirector**: Using spdlog as backend device
  - Async logging for trace/debug/info
  - Sync flushing for warn/error/critical

### String Handling
Located in `Engine/Source/Common/Public/Core/String.h`

- **Custom Char Type**: `chat8_t` compatible with `char`
- **UE5-style Macros**: `TXT("...")` for string literals
- **FString Class**: Replacement for `std::string`
- **FPath Class**: Boost::filesystem wrapper with cached hash
- **FName System**: String pooling (WIP)

### Template Utilities
Located in `Engine/Source/Common/Public/Template/`

- **MetaString.tpp**: Compile-time string obfuscation
- **GlobalTemplate.tpp**: Common template utilities
  - Scoped variables (file handles, timers)
  - Optional/reference removal
  - Memory allocation-free printf

### Platform Abstraction
Located in `Engine/Source/Common/Public/Platform/`

- **Atomic Pointers**: Platform-specific CAS operations
- **Debugger Utilities**: Debugger detection for Linux/Windows
- **Memory Management**: Platform-specific memory stats

## Build System

### PyCMake - Custom OOP CMake Wrapper
Located in `Engine/Scripts/pycmake/`

Python-based CMake wrapper providing object-oriented project description.

Features:
- Project/Module abstraction
- Package management (fetch/find)
- OOP relationships between build objects

Usage:
```bash
# Initial setup
./Setup.sh
# Generate projects
./GenerateCMakeProjects.sh
```

### Dependencies
Managed through vcpkg in `Engine/Source/Dependency/vcpkg/`

Key packages:
- **Core**: spdlog, mimalloc, magic_enum, Boost
- **Compression/Encryption**: botan3, zstd
- **Networking**: protobuf, grpc, curl
- **Serialization**: rapidjson, nlohmann_json
- **Telemetry**: opentelemetry
- **Third-party fetch**: yalantinlibs, backward, parallel_hashmap, ctre, string_pool

### Environment Setup
Uses conda environment defined in `env.yaml` with Python 3.11 and required packages.

## Key Files Reference

### Core Headers
- `Engine/Source/Common/Public/Core/Assert.h` - Exception handling with stack trace
- `Engine/Source/Common/Public/Core/Log.h` - Logging system
- `Engine/Source/Common/Public/Core/Memory.h` - Memory management interfaces
- `Engine/Source/Common/Public/Core/String.h` - String handling
- `Engine/Source/Common/Public/Core/Delegate.h` - Delegate system

### Platform Headers
- `Engine/Source/Common/Public/Platform/GenericPlatform.h` - Platform abstraction
- `Engine/Source/Common/Public/Platform/GenericPlatformAtomicPointer.h` - Atomic operations
- `Engine/Source/Common/Public/Platform/GenericPlatformDebuggerUtil.h` - Debug utilities

### Utility Headers
- `Engine/Source/Common/Public/Utility/Timer.h` - Timing utilities
- `Engine/Source/Common/Public/Utility/Hash.h` - Hash functions (MD5, SHA1)

### Configuration Files
- `Engine/Source/Common/Public/Common.h` - Common configurations
- `Engine/Source/Common/Public/Global.h` - Global definitions
- `Engine/Source/Common/Public/UserPredefined.gen.h` - User predefines

## Agent Guidelines

### Code Conventions
- Follow UE5-style naming (F prefixes for classes, TXT macros for strings)
- Use `chat8_t` instead of `char` for compatibility
- Prefer custom allocators over direct new/delete
- Use HLVM_LOG macros for logging
- Leverage template utilities for common patterns

### Module Interaction Patterns
- **Common Module**: Shared functionality used across all modules
- **Public Headers**: Interface definitions only
- **Private Implementations**: Actual code implementation
- **ThirdParty**: Isolated external dependencies

### Common Workflows
1. **Adding New Component**:
   - Public header in `Engine/Source/Common/Public/[Category]/`
   - Implementation in `Engine/Source/Common/Private/[Category]/`
   - Add to `Common.cmake.py` if needed

2. **Memory Management**:
   - Use `GMallocator` (TLS mimalloc) by default
   - Consider stack allocator for temporary allocations
   - Implement scoped allocators for controlled lifetimes

3. **Logging**:
   - Define log categories where needed
   - Use appropriate log levels (trace/debug/info/warn/error/critical)
   - Prefer async logging for non-critical messages

### Testing
Test implementations in `Engine/Source/Common/Test/` using custom test framework.

### Current Development Focus
(WIP items from TODO.md)
- Completing VM mallocator implementation
- Bullet3 physics integration
- Vulkan scene graph framework
- Tracy profiler integration
- Lua scripting integration

## Build Commands Reference

```bash
# Initial setup (installs conda env, git submodules, vcpkg)
./Setup.sh

# Generate CMake projects using PyCMake
./GenerateCMakeProjects.sh

# Build specific configuration (from Engine directory)
# Uses PyCMake backend with various options:
# --config=Release/Debug/RelWithDebInfo
# --test (run tests after build)
# --target=<specific_target>
```

This README_AI.md serves as a comprehensive guide for AI agents working with the HLVM-Engine codebase, providing essential context about the architecture, build system, and development practices.