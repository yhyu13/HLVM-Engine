# Pending Tests v114
- commit: docs/PENDING_COMMIT_v114.md
- tester: tester (role #5)
- timestamp: 2026-07-29
- files: no new test files; existing TestReSTIR_GI_Temporal harness/validator retained
- command: ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal
- result: BLOCKED — terminal invocation returned pending_approval (tirith:unknown)

## Static contract checks
- PASS: UAV layout slots are `FBindingLayoutBuilder::URegShift + 0/1` (384/385).
- PASS: per-frame UAV set uses `FBindingSetBuilder::SetTextureUAV(0/1)`, whose implementation emits `URegShift + index`.
- PASS: `AddBindingLayout(UAVBindingLayout)` occurs during GI layout creation before pipeline finalization.
- PASS: `FinalizePipeline()` orders main layout, additional ordinary layouts, then optional bindless layout.
- PASS: both GI shader copies declare `register(u0/u1, space1)`.
- PASS: `Shutdown()` clears `AdditionalBindingLayouts`.

## Runtime evidence
Build, fresh ACCUM=8 run, fresh-log scan, newest-group validator, and visual PNG inspection are UNVERIFIED. No success is claimed.
