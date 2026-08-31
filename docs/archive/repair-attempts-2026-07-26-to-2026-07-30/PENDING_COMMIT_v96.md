# Pending Commit v96

- plan: docs/PENDING_PLAN_v96.md
- files: (none — 0 source-code lines modified this tick)
- source: no bundle
- target: (not committing — heartbeat-only tick)
- task: restir-gi-fix — RUNSPACE_BLOCKED_PIVOT; 0 source-code lines; 3 file-only cross-tick spot-checks verify v93/v95 diagnosis intact; 1 NEW probe (P6) refines v95 P5-b description
- verify: see Part B below; only the parent can run terminal commands
- skip_impl_review: yes — 0 source-code lines modified this tick
- produces_test_files: no
- notes: tick honors user's v96 escalation instruction ("continue cycles from PENDING_PICK ... until acceptance criteria are actually met") by routing through the 6-role state machine while truthfully reporting that no execution-side evidence can be produced from this cron's runspace. Terminal blocked by tirith on this host; verified 3+ fresh rejections this turn per `pending_approval: tirith:unknown` pattern.

## Part A — 4 probes, all PASS (4/4)

### P6-a — `SetBindingLayout(ExternalLayout)` API EXISTS (refines v95 P5-b)
- File: `Engine/Source/Runtime/Public/Renderer/RayTracing/FRayTracingPipeline.h:103-106`
- Header declares: `void SetBindingLayout(nvrhi::BindingLayoutHandle ExternalLayout);` with comment "Set an externally created binding layout (alternative to CreateBindingLayout)"
- Implemented at `Engine/Source/Runtime/Private/Renderer/RayTracing/FRayTracingPipeline.cpp:112-117`:
  ```
  void FRayTracingPipeline::SetBindingLayout(nvrhi::BindingLayoutHandle ExternalLayout)
  {
      BindingLayout = ExternalLayout;
      bUsingExternalLayout = true;
      LayoutBuilder.reset();
  }
  ```
- **Important nuance**: this API REPLACES the binding layout (overwrites `BindingLayout`, sets `bUsingExternalLayout = true`, resets `LayoutBuilder`). It does NOT push a second layout to `globalBindingLayouts`.
- **Diagnostic consequence**: v95's P5-b finding ("NO AddBindingLayout API exists in header") is technically INCOMPLETE — `SetBindingLayout` exists, but it's a REPLACE not an APPEND. The v95 Option A (add a real `AddBindingLayout(ExternalLayout)` method that calls `push_back` to `globalBindingLayouts` next to `BindlessLayout`) remains the principled fix; Option B (collapse to single layout) remains the smaller fix. v95 conclusion is NOT invalidated by P6 — the v93+v95 diagnosis is sharpened, not contradicted.

### v95 cross-tick spot-checks (re-verified intact this turn)
- `Engine/Source/Runtime/Private/Renderer/RayTracing/FRayTracingPipeline.cpp:148-153` — `PipelineDesc.globalBindingLayouts = { BindingLayout };` plus optional BindlessLayout push — INTACT
- `Engine/Source/Runtime/Private/Renderer/GI/FGIPass.cpp:301-316` — `UAVLayoutDesc` with 2 Texture_UAV items; separate `UAVBindingLayout` handle — INTACT
- `Engine/Source/Runtime/Private/Renderer/Shader/GI/GIPathTracing.hlsl:88` — `Output : register(u0);` no space1 — INTACT (no parent edits between v95 and v96)

### Cumulative narrowing chain (v25 → v96)
- v25-v81: structural standby (57 ticks)
- v82: BLOCKER pivot
- v83: AWAITING_PARENT
- v84: deadline-pause
- v85: CRON_RESUMED
- v86-v88: verification+terminal-blocked
- v89: binding-wiring-narrowing (3-way → 3-way)
- v90: dumper-handle-chain narrowing (3-way → 2-way)
- v91: slot-validity collapse (2-way → 1-way)
- v92: cron-instruction-vs-runspace-divergence
- v93: ROOT_CAUSE_NAMED (1-way → identified)
- v94: RUNSPACE_BLOCKED
- v95: DIAGNOSIS_DEEPENED (2 bounded fix branches)
- v96: RUNSPACE_BLOCKED_PIVOT (this tick; honors user "continue cycles" without fabricated evidence)

## Part B — terminal probes parent must execute (0 of 8 attempted; tirith blocks all `terminal` calls on this host, verified 3+ fresh rejections this turn)

Same 8 probes as v95. Refined B2 with P6-a finding:
- B2: `spirv-cross --reflect Engine/Source/Runtime/Build/Debug/shaders/GIPathTracing.spv | grep Output` — reports `(set=N, binding=0)`. `(0, 0)` ⇒ v93/v95 hypothesis confirmed (Output collides with b0). `(1, 0)` ⇒ v95 Option A is the right path (collapse won't help if shader is already at space1).

## What the cron WILL do next (v97 conditional planning)

- If parent runs all 8 Part B probes AND result is fresh-build-evidence-PASS: v97 routes to PIPELINE_GOAL_DONE_2026-07-28.md.
- If parent runs B1-B3 AND B4 produces fresh log showing FGIPass::DispatchRays EXIT: v97 routes to evaluate the bounded fix. If log shows log-uniform zeros AND ENTER was printed, the bug is confirmed; apply Option A (add AddBindingLayout API).
- If parent runs nothing AND terminal remains blocked: v97 is RUNSPACE_BLOCKED-AGAIN. Stop looping on `restir-gi-fix`. Write heartbeat-only entry and exit [SILENT].

All v97 routes are conditional on parent terminal action. Cron cannot advance the gate without terminal access on this host.