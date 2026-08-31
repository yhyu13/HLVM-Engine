# Pending Commit v130 — v128 Step 0/1/2 patches landed (file-only)

- plan: docs/PENDING_PLAN_v130.md
- files:
  Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl,
  Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl,
  Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp,
  Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp
- source: no bundle — direct edit
- target: branch the parent runspace owns (git topology not touched by cron)
- task: Land v128 Step 0 (bypass-patch), Step 1 (handle-identity log lines), Step 2 (mode 30u sentinel) in both .hlsl copies and both .cpp files. File-only delivery; parent runspace executes build + run + vision + numpy.
- verify:
  ```
  ./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal
  HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20 ./Binary/Debug/TestReSTIR_GI_Temporal
  ```
  Then vision + numpy on `dumps/*_gi_raw_frame8.png` (parent runspace).

- skip_impl_review: no — this commit produces test-grade diagnostic
  patches that are themselves the bisect tool. Reviewer MUST audit them.
- produces_test_files: no — no test files produced this cycle.
- notes: terminal access is structurally blocked in this cron runspace
  per EC-039 / `docs/OVERSEER_ESCALATION.md`. The patches landed in this
  cycle are correct on static analysis but the build/run/verify step
  must execute in the parent runspace.

## Files modified (this cycle)

### 1. Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl

**Step 0 patch (lines 466-480):** added 11 lines (14 new - 4 removed -1
net). Replaces the unconditional early-return with a debugMode-gated
version. The new lines:

```hlsl
// v128 (six-role-pipeline, tick 111, 2026-07-30): bypass the early-return
// for diagnostic modes 20/21/22 that read GBuffer textures directly.
// Without this, when GBufferWorldPos SRV returns zero (the empty-SRV-binding
// hypothesis), length(worldPos) < 0.001 fires and writes Output[pixel] =
// (0,0,0,1) BEFORE the debug-mode switch runs. The diagnostic modes that
// would discriminate "SRV broken" vs "SRV works" are masked.
uint debugModeEarly = (uint)(g_GI.Params5.x + 0.5f);
bool bypassEarlyReturn = (debugModeEarly == 20u
                       || debugModeEarly == 21u
                       || debugModeEarly == 22u);

if (!bypassEarlyReturn && length(worldPos) < 0.001) {
    Output[pixel] = float4(0.0, 0.0, 0.0, 1.0);
    return;
}
```

**Step 2 patch (lines 684-699):** added 16 lines for case 30u sentinel.

### 2. Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/GIPathTracing.hlsl

Identical Step 0 and Step 2 patches as the Private copy above. The Data
copy is the compiled-into-sblob source the test executable links against;
the Private copy is the dev/source-of-truth that gets synced to Data
via the build pipeline. Both must be patched in lockstep.

### 3. Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp

**Step 1 patch (lines 1531-1541):** added handle-identity log line after
the GBuffer MRT-to-ShaderResource transitions at line 1529:

```cpp
// v128 (six-role-pipeline, tick 113, 2026-07-30): handle-identity probe.
// Log the texture handles the GBuffer raster pass just transitioned.
// Compare with FGIPass::DispatchRays's log line at FGIPass.cpp:533 to
// discriminate "handles differ between passes" (binding issue) vs
// "handles match but binding is wrong at descriptor level". Frame-rate
// gated to once-per-N-frames to avoid log spam.
if (FrameCount < 4 || FrameCount % 120 == 0)
{
    HLVM_LOG(LogTest, info, TXT("[handle-id] RenderGBuffer: GBufferMaterial=%p WorldPos=%p Normal=%p"),
        (void*)GBufferMaterial.Get(), (void*)GBufferWorldPos.Get(), (void*)GBufferNormal.Get());
}
```

### 4. Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp

**Step 1 patch (lines 533-543):** added handle-identity log line after
the existing DispatchRays ENTER log at line 531:

```cpp
// v128 (six-role-pipeline, tick 113, 2026-07-30): handle-identity probe.
// Compare with TestReSTIR_GI_Temporal.cpp:1531 (RenderGBuffer) log
// line. Matching handles = binding layer is wrong at descriptor level.
// Differing handles = texture handle identity issue (recreation).
if (Desc.FrameIndex < 4u)
{
    HLVM_LOG(LogGI, info, TXT("[handle-id] FGIPass::DispatchRays: GBufferMaterial=%p WorldPos=%p Normal=%p"),
        (void*)Desc.GBufferMaterial.Get(), (void*)Desc.GBufferWorldPos.Get(), (void*)Desc.GBufferNormal.Get());
}
```

## Plan Deviations (impler fills this in if it deviated from the plan)
No deviations. v130 is the v128 plan executed. The patch positions
match v128's recipe verbatim:
- v128 Step 0 patch: lines 462-481 area, replacing the early-return.
  Landed at lines 466-480 in both .hlsl copies.
- v128 Step 1 patch: TestReSTIR_GI_Temporal.cpp:1519 (after GBuffer
  raster pass). Landed at line 1531 (after the MRT transitions,
  which is functionally equivalent — the handles haven't been
  reassigned between the raster pass and the transitions).
- v128 Step 1 patch: FGIPass.cpp:533 (inside DispatchRays). Landed
  at line 533 (immediately after the existing ENTER log).
- v128 Step 2 patch: case 30u after case 22u. Landed at lines 684-699.

## Acceptance verification (parent runspace)
After build + run:
- Grep log for `[handle-id]` lines. Both files produce log lines.
  Compare handles between RenderGBuffer and FGIPass.
- `validate_restir_gi.py` should pass on the freshest dump group
  IF the patches close the bisect (outcome 0A in v128's recipe).
- If validate fails OR the dump shows uniform black, the bisect
  continues per v128 Steps 3-5.

## Honesty floor
This commit lands patches. It does NOT claim the build succeeded, the
binary ran, or any dump was analyzed. The patches are correct on
static analysis (gating logic, format string syntax, struct field
names all verified). Terminal access remains blocked; the parent
runspace is the only path to verification.