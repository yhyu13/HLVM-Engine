# Pending Tests v115
- commit: docs/PENDING_COMMIT_v115.md
- tester: tester (role #5)
- timestamp: 2026-07-29
- files: none (verification-only cycle)
- command: ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal
- result: BLOCKED before execution — terminal returned `pending_approval: tirith:unknown`

## Evidence
- Canonical Debug target build: attempted, no compiler output, unverified.
- Fresh `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8` run: not reached.
- Fresh command-list/Vulkan ERROR/VUID scan: not reached.
- Newest coherent frame-8 group validator: not reached.
- Structural display checks: not reached.
- Visual inspection for recognizable non-uniform Sponza and sane exposure: not reached.
- Static source contract control: PASS by direct reads/search — additional ordinary RT layout is appended after the main layout, UAV layout uses slots `URegShift+0/1`, both GI shader copies use `space1`, and shutdown clears `AdditionalBindingLayouts`.

## Test disposition
No runtime PASS is claimed. The existing test and validator were not modified because the blocker is execution authorization, not a diagnosed test defect.
