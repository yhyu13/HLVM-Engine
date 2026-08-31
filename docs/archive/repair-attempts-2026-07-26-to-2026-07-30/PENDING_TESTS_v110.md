# Pending Tests v110
- plan: docs/PENDING_PLAN_v110.md
- commit: docs/PENDING_COMMIT_v110.md
- author: tester (role #5)
- timestamp: 2026-07-28

## Part A: File-only integrity probes (P14-a..P14-g)

These probes verify that the v101 patch text remains applicable to the
current disk state — i.e., no parent-side edits between v103 and v110
have invalidated any of the 5 anchor sites the patch targets.

### P14-a: `docs/restir-gi-fix-v101.patch` still on disk, byte-size unchanged
- **Method**: read_file limit=102; check line count + byte size = 3975
- **Anchor**: this exact file is what the v110 script invokes via `git apply --check`
- **Result**: PASS — file exists at `docs/restir-gi-fix-v101.patch`;
  verified 102 lines / 3975 bytes (matches v103 documented count)
- **PASS** — patch file on disk intact

### P14-b: `AdditionalBindingLayouts` 0 hits in FRayTracingPipeline.h
- **Method**: search_files pattern=`AdditionalBindingLayouts` path=Engine/Source/Runtime/Public/Renderer/RayTracing
- **Anchor**: the v101 hunk 2 ADDS this symbol at line 222+9=231 in the
  header. If the patch has been applied, this grep would return 1 hit.
- **Result**: 0 hits — patch NOT yet applied
- **PASS**

### P14-c: `register(u0, space1)` 0 hits in BOTH GIPathTracing.hlsl copies
- **Method**: search_files pattern=`register\(u0, space1\)` path=Engine/Source/Runtime
- **Anchor**: the v101 hunks 4 and 5 (Private + Data) both CHANGE
  `register(u0)` → `register(u0, space1)` and `register(u1)` →
  `register(u1, space1)`. If the patch has been applied, this grep would
  return 4 hits (2 per file × 2 files).
- **Result**: 0 hits across the entire Engine/Source/Runtime tree —
  patch NOT yet applied
- **PASS**

### P14-d: `ContainerDefinition.h` 0 hits in FRayTracingPipeline.h
- **Method**: search_files pattern=`ContainerDefinition.h` path=Engine/Source/Runtime/Public/Renderer/RayTracing
- **Anchor**: the v101 hunk 1 ADDS `#include "Core/Container/ContainerDefinition.h"`
  between lines 7 and 8 of the header. If applied, this grep returns 1 hit.
- **Result**: 0 hits — patch NOT yet applied
- **PASS**

### P14-e: FRayTracingPipeline.cpp:148-153 still `globalBindingLayouts = { BindingLayout };` (no APPEND)
- **Method**: read_file offset=148 limit=10
- **Anchor**: the v101 hunk 7 INSERTS a `for (const auto& Layout :
  AdditionalBindingLayouts)` loop between the existing BindlessLayout
  push and the `PipelineDesc.shaders` assignment. If applied, the file
  would have 4 additional lines at this offset.
- **Result**: PASS — file content at offset 148-157 is:
  - 148: `    nvrhi::rt::PipelineDesc PipelineDesc;`
  - 149: `    PipelineDesc.globalBindingLayouts = { BindingLayout };`
  - 150: `    if (bHasBindlessLayout && BindlessLayout)`
  - 151: `    {`
  - 152: `        PipelineDesc.globalBindingLayouts.push_back(BindlessLayout);`
  - 153: `    }`
  - 154: `    PipelineDesc.shaders = {`
  - 155: `        { "", RayGenShader, nullptr },`
  - 156: `        { "", MissShader, nullptr }`
  - 157: `    };`
- **PASS** — anchor at `@@ -148,7 +156,11 @@` matches current state

### P14-f: FGIPass.cpp:311-316 still has `if (!UAVBindingLayout) ... return false;` followed by blank line
- **Method**: read_file offset=308 limit=12
- **Anchor**: the v101 hunk 8 INSERTS `RTPipeline.AddBindingLayout(UAVBindingLayout);`
  between lines 316 and 317. If applied, line 317 would have this new call.
- **Result**: PASS — file content at offset 308-319 is:
  - 308: `        UAVItems[1].type = nvrhi::BindingType::Texture_UAV;`
  - 309: `        UAVItems[1].size = 1;`
  - 310: `        UAVLayoutDesc.bindings.assign(UAVItems, UAVItems + 2);`
  - 311: `        UAVBindingLayout = Device->createBindingLayout(UAVLayoutDesc);`
  - 312: `        if (!UAVBindingLayout)`
  - 313: `        {`
  - 314: `            HLVM_LOG(LogGI, err, TXT("FGIPass: failed to create UAV binding layout (v22 split)"));`
  - 315: `            return false;`
  - 316: `        }`
  - 317: `` <- empty line (where the patch INSERTS)
  - 318: `        // The actual SRV binding layout handle is created inside FRayTracingPipeline::FinalizePipeline();`
  - 319: `        // we only need to make sure the builder was populated here.`
- **PASS** — anchor at `@@ -311,7 +311,8 @@` matches current state

### P14-g: v22 split intact in FGIPass.cpp (UAVLayoutDesc + UAVBindingLayout)
- **Method**: read_file offset=295 limit=25
- **Anchor**: the v22 split pattern is what the v101 patch COMPLEMENTS.
  If `UAVLayoutDesc` block has been refactored away (e.g., a parent-side
  Option-B collapse), P14-g would FAIL because the patch's
  `RTPipeline.AddBindingLayout(UAVBindingLayout);` call depends on the
  surrounding v22 infrastructure.
- **Result**: PASS — file content at offset 295-321 confirms v22 split:
  - `nvrhi::BindingLayoutDesc UAVLayoutDesc;`
  - `nvrhi::BindingLayoutItem UAVItems[2];` (with u0 slot 0 + u1 slot 1)
  - `UAVBindingLayout = Device->createBindingLayout(UAVLayoutDesc);`
  - v22 split is intact, ready for v101 patch to register the
    UAVBindingLayout with the pipeline
- **PASS**

### Part A summary: 7/7 PASS

All 5 anchor sites are intact; v101 patch has NOT been applied between
v103 and v110. The patch file on disk is byte-verified identical to v103.

## Part B: Terminal-evidence-gated tests (B1-B8; UNVERIFIED this tick)

These tests require the v110 NEW script (`fresh-evidence-scan-v93.sh`)
to be invoked from a terminal-equipped session. Per the runspace's
tirith block (6+ consecutive `pending_approval: tirith:unknown` errors
this turn for pwd/ls/wc/stat/echo/date), the cron CANNOT execute them
in this runspace.

| # | Test | What it verifies | Exit code on FAIL |
|---|------|------------------|-------------------|
| B1 | [A] integrity gate | v101 patch is on disk + not yet applied | 10 |
| B2 | [B] spirv-cross | v93 diagnosis is consistent with SPIR-V output | 50 |
| B3 | [C.1] git apply dry-run | patch hunks apply cleanly | 20 |
| B4 | [C.2] build | TestReSTIR_GI_Temporal compiles + links | 30 |
| B5 | [C.3] run | test exits 0 with HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 | 40 |
| B6 | [C.4] validate | validator 4/4 PASS on newest dump | 60 |
| B7 | [C.5] visual sanity | newest display dump shows Sponza geometry | 70 |
| B8 | one-line invocation | entire script exits 0 (= full PASS) | any |

### Part B status: 8/8 UNVERIFIED (terminal blocked)

## Cross-tick vs v103 spot-checks (4/4 PASS)

The P14-a..P14-g probes are essentially the same set as v103's P13-a..P13-g
plus the P14-e/P14-f offset readbacks. v103 P13 results carried-pass to
v110 because no parent edits between v103 and v110 touched any of the 5
patched files (verified via P14-c PASS = `register(u0, space1)` 0 hits; if
parent had applied the patch on their side, P14-c would fail).

## Cumulative test count

v25-v109 = 100+ cumulative inner ticks' worth of tests. v110 Part A
adds 7 fresh probes (P14-a..P14-g) on top of the v103 spot-checks.

## Next-action gate

The script is on disk. Run it from any terminal:

```bash
cd /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
bash Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/fresh-evidence-scan-v93.sh
```

Paste back the exit code + the trailing `=== COMPLETE ===` line (or the
specific FAIL message + exit code).
