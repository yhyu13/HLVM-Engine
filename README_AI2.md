# HLVM Engine – Quick Reference Guide for AI Agents
**Purpose**: This document provides a concise, structured overview of the HLVM Engine codebase so that LLMs (and new developers) can rapidly locate, understand, and work with key components, modules, and development workflows.

---  

## 1. Repository Layout (Root)

```
/home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
├── Engine/                     # Core engine source tree
│   ├── Source/                 # All C/C++ source files
│   │   ├── Common/             # Shared utilities, platform abstractions
│   │   │   ├── Public/         # Public headers (interface only)
│   │   │   ├── Private/        # Implementation files
│   │   │   └── ThirdParty/     # vcpkg dependencies (isolated)
│   │   ├── Runtime/             # Runtime‑specific components
│   │   ├── Editor/              # Editor tooling (if present)
│   │   ├── Plugin/              # Plugin system scaffolding
│   │   └── Scripts/             # Python build utilities (PyCMake)
│   ├── Binary/                  # Build output & intermediate binaries
│   ├── Document/                # Design docs, API reference, tutorials
│   ├── Samples/                 # Example code snippets
│   ├── build/                   # CMake build directory (generated)
│   └── env.yaml                 # Conda environment specification
├── Document/                    # Additional documentation files
├── Samples/                     # Sample applications and shaders
├── build/                       # CMake output (generated)
├── .github/                     # GitHub Actions CI configs
├── .git/                        # Git repository metadata
├── .vscode/                     # VS Code workspace settings
├── LICENSE / LICENSE_1_0.txt    # Licensing files
├── README_AI.md                 # AI‑focused guide (this file)
├── README.md                    # Human‑readable project overview
└── Setup.sh / GenerateCMakeProjects.sh  # Boot‑strap scripts
```

---  

## 2. Core Modules & Responsibilities  

| Module | Directory (source) | Primary Responsibilities | Representative Files |
|--------|-------------------|--------------------------|----------------------|
| **Auth / Security** | `Engine/Source/Common/Public/Core/Security/` | Token handling, RSA/OAEP encryption, secure string obfuscation | `Encrypt/RSA.h`, `Template/Obfuscate/MetaString.tpp` |
| **Memory Management** | `Engine/Source/Common/Public/Core/Mallocator/` | Global allocator interface, TLS mimalloc, stack allocator, VM‑mallocator (WIP) | `Mallocator.cpp`, `VMArena.h`, `SmallBinnedMallocator.h` |
| **Parallelism & Concurrency** | `Engine/Source/Common/Public/Core/Parallel/` | Spin locks, rival locks, MPMC queues, work‑steal thread pool, fiber pool | `Lock.h`, `ConcurrentQueue.h`, `WorkStealThreadPool.h` |
| **File System & Packaged Assets** | `Engine/Source/Common/Public/Platform/FileSystem/Packed/` | Token‑based file descriptors, packed container fragments, 4 MB fragment loading | `PackedToken.h`, `PackedContainerFragment.h` |
| **Logging System** | `Engine/Source/Common/Public/Core/Log.h` | UE‑style macros, async/sync log routing, spdlog integration | `Log.h` |
| **String Handling** | `Engine/Source/Common/Public/Core/String.h` | `chat8_t` type, `FString`, `FPath`, `FName` pooling, TXT macro | `String.h` |
| **Template Utilities** | `Engine/Source/Common/Public/Template/` | Meta‑string obfuscation, GlobalTemplate utilities (scoped variables, printf‑free formatting) | `GlobalTemplate.tpp`, `MetaString.tpp` |
| **Platform Abstraction** | `Engine/Source/Common/Public/Platform/` | Atomic operations, debugger detection, platform‑specific metrics | `GenericPlatform.h`, `GenericPlatformDebuggerUtil.h` |
| **Compression / Encryption** | `Engine/Source/Common/Public/Core/Compress/`, `Encrypt/` | Zstd wrapper, Botan3 RSA/EMSA wrappers, PKCS#8 key handling | `Zstd.h`, `RSA.h` |
| **Build System** | `Engine/Scripts/pycmake/` | PyCMake – object‑oriented CMake wrapper, project generation, dependency fetching | `pycmake/` scripts, `GenerateCMakeProjects.sh` |

---  

## 3. Key Cross‑Module Concepts  

| Concept | Description | Where to Find |
|---------|-------------|----------------|
| **Global Mallocator (GMallocator)** | TLS‑based allocator that can swap between mimalloc, stack allocator, or VM allocator at runtime. | `Mallocator.cpp`, `VMArena.h`, `SmallBinnedMallocator.h` |
| **HLVM_LOG Macro** | UE‑style logging with compile‑time level elimination; async for trace/debug/info, sync for warn/error/critical. | `Log.h` |
| **Token‑Based Pak File** | Files are described by tokens (offset, size, compression) stored in `PackedToken.h`; actual data resides in `PackedContainerFragment.h`. | `PackedToken.h`, `PackedContainerFragment.h` |
| **String Literal Macro `TXT()`** | Enables UE‑style macros for compile‑time string handling; works with `chat8_t`. | `String.h` |
| **FString / FPath / FName** | Custom string types: `FString` (replace `std::string`), `FPath` (Boost filesystem wrapper with cached hash), `FName` (string pooling – WIP). | `String.h`, `Platform/FileSystem/Packed/*.h` |
| **Template Meta‑Programming** | `GlobalTemplate.tpp` provides scoped variables, optional removal, memory‑free `printf`‑style formatting. | `Template/GlobalTemplate.tpp` |
| **Parallel Work Steal** | Thread‑pool that uses MPMC queues and work‑stealing algorithm; fiber pool built on `boost::fibers`. | `Parallel/Async/WorkStealThreadPool.h`, `WorkStealFiberPool.h` |

---  

## 4. Build & Development Workflow  

1. **Initial Setup**  
   ```bash
   ./Setup.sh                # Installs conda env, git submodules, vcpkg deps
   ```
2. **Generate CMake Projects** (PyCMake wrapper)  
   ```bash
   ./GenerateCMakeProjects.sh
   ```
3. **Build** (from project root or `Engine/` directory)  
   ```bash
   # Release build
   ./build.sh --config=Release
   # Debug build with tests
   ./build.sh --config=Debug --test
   ```
4. **Run Tests**  
   ```bash
   ctest -C BuildDir --output-on-failure
   ```
5. **Common Development Tasks**  
   - **Add a new component**: create a public header under `Common/Public/<Category>/`, implementation under `Common/Private/<Category>/`, and update `Common.cmake.py` if needed.  
   - **Memory‑allocator swap**: use `SwapMallocator(new_allocator)` from anywhere after `InitAssertionStackMallocator()`.  
   - **Logging**: use `HLVM_LOG(Category, Level, TXT("msg"), var)`; categories can be disabled via compile‑time flags.  
   - **String literals**: prefix with `TXT("...")`; compatible with `chat8_t`.  
   - **Parallel tasks**: enqueue work via `WorkStealThreadPool::enqueue(task)`; retrieve results via `std::future`.  

---  

## 5. Frequently Used Commands & Utilities  

| Command | Purpose | Example |
|---------|---------|---------|
| `grep -R "class.*Parser" Engine/Source/Common/Public/` | Find parser class definitions. | `grep -R "class.*Parser" Engine/Source/Common/Public/` |
| `rg "await.*compile"` Engine/Source/ | Locate async compilation calls. | `rg "await.*compile" Engine/Source/` |
| `ast-grep -p "ExportNamedFunction[$$]" Engine/Source/Common/Public/` | AST‑aware search for exported functions. | `ast-grep -p "ExportNamedFunction[$$]" Engine/Source/Common/Public/` |
| `python -m pip install -r requirements.txt` | Install Python dependencies for PyCMake. | `python -m pip install -r requirements.txt` |
| `ctest -C Build/` | Run unit/integration tests. | `ctest -C Build/` |
| `lsp_goto_definition <file>:<line>:<col>` | Jump to symbol definition (IDE/LSP). | `lsp_goto_definition Engine/Source/Common/Public/Core/Mallocator/VMArena.h:45` |
| `lsp_find_references <symbol>` | Find all usages of a symbol. | `lsp_find_references "GMallocator"` |

---  

## 6. Coding Conventions & Style  

* **Naming** – UE‑style prefixes: `F` for classes/structs, `TXT` for string literals, `chat8_t` for generic characters.  
* **Headers** – Public headers contain **only declarations**; implementations live in private sub‑folders.  
* **Memory** – Prefer custom allocators (`GMallocator`, stack allocator) over `new/delete`.  
* **Logging** – Use `HLVM_LOG` macros; configure levels via compile‑time constants.  
* **String Handling** – Use `FString`, `FPath`, `FName` where appropriate; avoid raw `std::string` in public interfaces.  
* **Templates** – Leverage `GlobalTemplate.tpp` for scoped resources (file handles, timers).  

---  

## 7. Agent Interaction Guide  

When an LLM needs to **modify** or **extend** code:  

1. **Locate the target file** using `lsp_goto_definition` or `grep/rg` patterns listed above.  
2. **Read the surrounding context** (typically 5–10 lines before/after) to understand imports and surrounding logic.  
3. **Create a minimal change** that respects existing patterns (e.g., add new enum to `Common/Public/Core/String.h` and update the corresponding `*.cpp`).  
4. **Run diagnostics** (`lsp_diagnostics <file>`) to ensure no lint errors.  
5. **Commit** only after verifying that the change passes `lsp_diagnostics` and does not break existing tests.  

---  

## 8. Reference Cheat‑Sheet (One‑Page)

```
Engine/
├─ Source/
│   ├─ Common/
│   │   ├─ Public/      ← public headers (interfaces)
│   │   ├─ Private/     ← implementations
│   │   └─ ThirdParty/  ← vcpkg deps
│   ├─ Runtime/
│   ├─ Editor/
│   └─ Scripts/
│        └─ pycmake/    ← PyCMake wrapper
├─ Document/            ← design docs
├─ Samples/
├─ build/
├─ env.yaml
└─ README_AI.md        ← this file
```

**Core Subsystems**  
- **Memory**: `Mallocator.cpp`, `VMArena.h`, `SmallBinnedMallocator.h`  
- **Parallel**: `Lock.h`, `ConcurrentQueue.h`, `WorkStealThreadPool.h`  
- **FileSystem**: `PackedToken.h`, `PackedContainerFragment.h`  
- **Logging**: `Log.h`  
- **String**: `String.h` (`FString`, `FPath`, `FName`)  
- **Build**: `pycmake/` + `Setup.sh` + `GenerateCMakeProjects.sh`

---  

*End of document.*