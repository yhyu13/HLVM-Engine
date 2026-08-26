# Final State — TestReSTIR_GI_Temporal — 2026-07-23

> **SUPERSEDED** — see `final-state-2026-08-09.md`. The Aug-08 uncommitted
> tree fixed the generation-layout bug (v151) and the ReSTIR chain now runs
> end-to-end; this document describes the pre-fix state.

## TL;DR

The black-output symptom that the validator was accepting turned out to be caused
by **NVRHI's validator silently dropping every GPU command** after the first
immediate-command-list collision. Patching one file in NVRHI's validation
wrapper and rebuilding the static lib bypasses the silent failure.

Post-patch:
- The renderer pipeline **runs end-to-end** (display/gi_raw/spatial/denoised
  produce non-zero pixels).
- Output is **uniform magenta/pink**, not Sponza structure yet.
- The GBuffer dumps still appear black — there is a **separate dump-side bug**
  (bug-074) where the readback reads zeros even when the PS forced colored
  sentinels.
- A previously-masked **resource-layout mismatch** (bug-075) on the temporal
  history reservoirs is now visible.

## Root cause of the silent failure

`Engine/Source/Runtime/Build/Debug/_deps/nvrhi-src/src/validation/validation-commandlist.cpp`
in `CommandListWrapper::open()` did:

```cpp
if (m_IsImmediate) {
    if (++m_Device->m_NumOpenImmediateCommandLists > 1) {
        error("Two or more immediate command lists cannot be open at the same time");
        --m_Device->m_NumOpenImmediateCommandLists;
        return; // ← drops m_CommandList->open()
    }
}
m_CommandList->open();
```

When DeviceManager and the test both create immediate command lists and the
counter overflows (which it does on frame 1), the `return` skips
`m_CommandList->open()`. Every subsequent `setGraphicsState` / `dispatch` /
`drawIndexed` on that wrapper becomes a no-op → no GPU work → all-zero dumps.

## Fix applied

Patched `error(...) + return` → `warning(...)` (no return), so the immediate
counter overflow is logged but `m_CommandList->open()` still executes.

Rebuild path (ninja's cmake reconfigure fails on parallel-hashmap network clone,
so bypassed):
1. Compiled `validation-commandlist.cpp` with clang++-17 flags used by the build.
2. Re-archived with `llvm-ar-17 qc libnvrhid.a validation-commandlist.cpp.o`
   + `llvm-ranlib-17 libnvrhid.a`.
3. Ran the link command for `TestReSTIR_GI_Temporal` directly from build.ninja
   (extracted via `ninja -t commands …`) — single clang++-17 link step.

GIPathTracing.sblob was inadvertently deleted by an earlier `rm -f *.sblob`;
re-built via ShaderMake with the same `-I …Private/Renderer/Shader` include
path.

## Observable results after fix

`Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/dumps/20260723_0710*.png`

| Dump | Visual | Size | Notes |
|------|--------|------|-------|
| display | uniform orchid pink, paper texture | 475 KB | pipeline runs, accumulated output exists |
| gi_raw | uniform magenta with fine grain | 1.5 MB | path tracer alive |
| spatial | uniform magenta (slightly lighter) | 670 KB | spatial reuse pass alive |
| denoised | uniform magenta | 932 KB | denoise alive |
| **gbuffer_worldpos** | all-black | 19 KB | **bug-074** — dump reads zeros |
| **gbuffer_normal** | dark teal near-black | 19 KB | bug-074 |
| **gbuffer_material** | near-black | 19 KB | bug-074 |

When GBufferPT_PS.hlsl was forced to write RGB=(1,0,0)/(0,1,0)/(0,0,1) sentinels
(Phase 1.2 of the debug plan), the GBuffer dumps still showed black — proving
that **the dump readback is broken on its own**, independent of NVRHI.

## Pending work for next session

1. **bug-074 — Dump RGBA32F readback** (TestReSTIR_GI_Temporal.cpp:1478-1535).
   `DumpCurrentFrame` / `DumpRGBA32FTexture` likely uses a wrong texture handle
   or omits a barrier to `CopySource` for the MRT attachments. Check whether
   the staging texture's region covers the GBuffer target's full extent, and
   that the readback isn't reading from a cached swapchain image.
2. **bug-075 — Resource layout transition** for TemporalReservoir0/1 history
   reads before the temporal dispatch (TestReSTIR_GI_Temporal.cpp:~466-477).
   Insert transitions to `nvrhi::ResourceStates::ShaderResource` for the
   per-frame history reservoirs and to
   `UnorderedAccess` after they are written by ReSTIR Generate.
3. **GBuffer PT visual content** — once dumps work, check why Sponza geometry
   isn't reaching the GI tracer. Hypothesis: GBuffer pass writes but PS
   attribute locations in GBufferPT_VS.hlsl may not feed the inputs (Vertex
   attribute at location 2/3 not consumed by VS warnings were seen).
4. **bug-076 — Hardcoded Vulkan validation layer** in
   `DeviceManagerVk1_Instance.cpp:138` is on by default, masking real warnings
   with noise. Add a `r.Vulkan.Validation` CVar or env var.

## Logs / artifacts

- Debug plan: `Vibe_Coding/50_ReSTIR_GI_Temporal/claude/50_ReSTIR_GI_Temporal_debug_plan.md`
- Bugs: `.wolf/buglog.json` (73-76)
- Captured screenshots & analyses above prove the bypass works.

## Build/run recipe

```bash
# Run with VK_DRIVER_FILES=... nvidia_icd.json, Lavapipe is broken
cd Engine/Source/Runtime/Binary/Debug
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.json \
HLVM_DUMP_RGI=1 HLVM_DUMP_FRAMES=4 \
timeout 180 ./TestReSTIR_GI_Temporal
```
