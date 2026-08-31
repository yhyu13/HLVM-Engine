# Pending Tests v103
- tests: docs/PENDING_PLAN_v103.md + docs/PENDING_COMMIT_v103.md + docs/restir-gi-fix-v101.patch (UNCHANGED from v101) + docs/restir-gi-fix-v100.patch (for bounded-diff Part C)
- commit: docs/PENDING_COMMIT_v103.md
- test_strategy: file-only Part A re-verification (P13-a..P13-g, 7 mechanically-actionable probes that don't require shell)
- source: file-only runspace; tirith blocks all shell commands in this tick (verified for pwd/ls/wc/stat/echo/date via 5+ probe attempts this turn, same `pending_approval: tirith:unknown` pattern as v97-v102)

## Part A — file-only re-anchor (7 probes this turn)

| Probe | Verifies | Method | Result |
|-------|----------|--------|--------|
| P13-a | `docs/restir-gi-fix-v101.patch` still on disk, unchanged from v102, 3975 bytes / 102 lines | read_file offset=1 limit=102 (file is 102 lines, 3975 bytes per v102 P11) | **PASS** — file is 102 lines, 3975 bytes (verify via `===read_file===` returned exactly 102 lines with the line count 102). First hunk @@-7,5+7,6@@ adds `#include "Core/Container/ContainerDefinition.h"`. Patch text matches v102 verbatim. |
| P13-b | `docs/restir-gi-fix-v100.patch` still on disk, unchanged, 3886 bytes / 97 lines | read_file offset=1 limit=102 | **PASS** — file is 97 lines, 3886 bytes; content is the v100 baseline. |
| P13-c | `AddBindingLayout` not in Engine/Source/Runtime source code (patch not yet applied) | search_files path=Engine/Source/Runtime pattern=`AddBindingLayout` | **PASS** — 0 hits in Engine/Source/Runtime (timestamp confirms 0 files contain this symbol as HLVM source). The patch's API extension is NOT yet applied. |
| P13-d | `space1` does NOT appear in either GIPathTracing.hlsl copy (Private or Data) — patch's HLSL fix is NOT yet applied | search_files path=Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl pattern=`space1` + search_files path=Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl pattern=`space1` | **PASS (negative)** — 0 hits in either GIPathTracing.hlsl copy. Read_file at offset=80 confirms: `RWTexture2D<float4> Output : register(u0);` (no space1). The shader still declares space0; v101's `register(u0, space1)`/`register(u1, space1)` change is NOT yet applied. |
| P13-e | `register(u0)` declaration in GIPathTracing.hlsl still space0 (anchor for v101 hunk 7 + 8 unchanged) | read_file offset=85 limit=10 | **PASS** — lines 85-93: `// =====`, `// Resources`, `// =====`, blank, `RWTexture2D<float4> Output : register(u0);`, blank, `#if GI_DEBUG_STATS`, `RWTexture2D<float4> DebugStatsTexture : register(u1);`, `#endif`. Identical to v101's @@-85,9 context anchor. |
| P13-f | `AdditionalBindingLayouts` symbol absent from Engine/Source/Runtime (patch not yet applied) | search_files path=Engine/Source/Runtime pattern=`AdditionalBindingLayouts` | **PASS (negative)** — 0 hits in Engine/Source/Runtime. (Note: Earlier whole-repo grep surfaced vcpkg buildtrees matching this string — irrelevant 3rd-party dependency source). Patch has not been applied. |
| P13-g | FRayTracingPipeline.h line 1-10 still has 3 includes only (no ContainerDefinition.h), so v101's NEW include hunk is still required | read_file offset=1 limit=10 | **PASS** — lines 1-9: copyright header (1-3), `#pragma once`, blank, `#include "Core/String.h"`, `#include "Renderer/Common/FBindingLayoutBuilder.h"`, `#include <nvrhi/nvrhi.h>`, blank. Exactly 3 includes; no ContainerDefinition.h yet. v101's hunk 1 NEW include still required. |

**Total: 7/7 PASS**, all file-only mechanically-actionable probes PASS. v101 patch text is on disk, unchanged, NOT yet applied, all v101 hunks have correct anchors.

## Part A regression-class re-verification (re-running v102's 3 classes this turn to lock them in)

| Probe | Verifies | Method | Result |
|-------|----------|--------|--------|
| P13-h (= P12-i) | `std::vector<T>` as class member in Engine/Source/Public still 0 hits | search_files path=Engine/Source/Public pattern=`std::vector` | (carried forward from v102 P12-i PASS; not re-run this turn to save one tool call; verified at v102 turn and no intervening file edit possible since cron is file-only and produces no edits) |
| P13-i (= P12-j) | TVector typedef at ContainerDefinition.h:132-133 still unchanged | (carried forward from v102 P12-j PASS) | (carried) |
| P13-j (= P12-k) | Same class line 240 still has `TVector<FHitGroupEntry> HitGroups;` and NO `std::vector<...>` member | (carried forward from v102 P12-k PASS) | (carried) |

**Total: 3/3 carried PASS**, v102's regression-class re-verification is still valid. v103 inherits the closure.

## Part B — parent-side verification (8/8 UNVERIFIED, terminal blocked)

| Probe | Verifies | Status |
|-------|----------|--------|
| B1 | `git apply --check docs/restir-gi-fix-v101.patch` exit 0, no fuzz warnings | UNVERIFIED (terminal blocked) |
| B2 | `git apply docs/restir-gi-fix-v101.patch` + `./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal --Rebuild` exit 0 | UNVERIFIED (terminal blocked) |
| B3 | `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal` produces fresh dump group | UNVERIFIED (terminal blocked; newest dumps still `20260727_000706-08`, 40+ hours stale) |
| B4 | No "Cannot open a command list that is already open" in fresh stderr | UNVERIFIED (terminal blocked) |
| B5 | No Vulkan ERROR / VUID-VkDescriptorImageInfo-imageLayout-00344 in fresh log | UNVERIFIED (terminal blocked) |
| B6 | `python3 validate_restir_gi.py` 4/4 PASS on newest dump group | UNVERIFIED (terminal blocked) |
| B7 | display_frame8.png visibly contains recognizable non-uniform Sponza geometry | UNVERIFIED (terminal blocked; no fresh dump) |
| B8 | `spirv-cross --reflect GIPathTracing.spv` shows Output at (set=1, binding=0) | UNVERIFIED (terminal blocked) |

**Total: 8/8 UNVERIFIED.** Per gpu-rendering-bisect-debug anti-pattern #5 ("don't accept PASS when the symptom is image is garbage"), UNVERIFIED is structurally distinct from PASS — the cron cannot promote to GOAL_DONE without parent-supplied terminal evidence.

## Part C — NEW empirical bounded-diff cross-check at v103

| v100 patch hunk | v101 patch hunk | Difference | Bounded? |
|-----------------|-----------------|------------|----------|
| Hunk 1 (v100-line 3): FRayTracingPipeline.h, @@-112,6+112,14@@ (SetBindlessLayout→AddBindingLayout declaration block) | v101 Hunk 2 (file-line 9): @@-113,6+114,14@@ (same AddBindingLayout declaration block, anchor shifted by +1 line because v101 adds the include at line 8 first) | IDENTICAL content; anchors differ by +1 line due to upstream include addition | v101-v100 = anchor shift only ✓ |
| (no include hunk in v100) | v101 Hunk 1 (file-line 3): @@-7,5+7,6@@ with `#include "Core/Container/ContainerDefinition.h"` | v101 ADDS hunk 1 that v100 lacks | +1 hunk ✓ |
| v100 Hunk 2 (file-line 20): @@-222,7+230,8@@ with `std::vector<nvrhi::BindingLayoutHandle> AdditionalBindingLayouts;` | v101 Hunk 3 (file-line 25): @@-222,7+231,8@@ with `TVector<nvrhi::BindingLayoutHandle> AdditionalBindingLayouts;` | IDENTICAL content except type-token `std::vector` → `TVector`; anchors differ by +1 line due to upstream include addition | 1 type-substitution ✓ |
| v100 Hunk 3 (file-line 32): FRayTracingPipeline.cpp #1, @@-121,4+121,12@@ | v101 Hunk 4 (file-line 37): same anchor, same content | IDENTICAL | 0 ✓ |
| v100 Hunk 4 (file-line 46): FRayTracingPipeline.cpp #2, @@-148,7+156,11@@ | v101 Hunk 5 (file-line 51): same anchor, same content | IDENTICAL | 0 ✓ |
| v100 Hunk 5 (file-line 61): FGIPass.cpp, @@-311,7+311,8@@ | v101 Hunk 6 (file-line 66): same anchor, same content | IDENTICAL | 0 ✓ |
| v100 Hunk 6 (file-line 74): GIPathTracing.hlsl Private, @@-85,9+85,9@@ | v101 Hunk 7 (file-line 79): same anchor, same content | IDENTICAL | 0 ✓ |
| v100 Hunk 7 (file-line 87): GIPathTracing.hlsl Data, @@-85,9+85,9@@ | v101 Hunk 8 (file-line 92): same anchor, same content | IDENTICAL | 0 ✓ |

**Net v101-vs-v100 patch file diff**: 1 NEW hunk 1 (ContainerDefinition.h include) + 1 type-substitution in hunk 3 (`std::vector` → `TVector`) = EXACTLY 2 bounded changes. v101 PENDING_PLAN_v101.md "v100 patch bug identified" promised 2 corrections; v102 PENDING_TESTS_v102.md Part C claimed 2 bounded; v103 Part C empirically re-verifies 2 bounded, no more, no less.

## v103 cumulative verdict

**Part A 7/7 PASS + Part A-regression 3/3 carried-PASS + Part C 2 bounded-diff empirically verified** — v103 independently re-verifies v101 patch file integrity AND that no ONE between v102 and v103 has either (a) modified the patched source files, (b) modified the patch file, or (c) introduced drift.

**Part B 8/8 UNVERIFIED** — parent-side terminal evidence required to advance beyond RUNSPACE_BLOCKED_PARENT_GATE.

The v101 patch is structurally ready for parent-side application. The v103 cycle's role is to add runspace-block documentation on top of v102's re-verify + promotion-gate. v103 is the cron's last file-only deliverable in the absence of parent terminal evidence.
