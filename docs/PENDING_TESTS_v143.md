# Pending Tests v143
- commit: docs/PENDING_COMMIT_v143.md
- tester: tester (single-profile self-check)
- timestamp: 2026-08-03

## Tests written

- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/test_validate_restir_gi.py`: five module-direct/source-contract regression tests for no-anchor passthrough, two multi-second runs (including same-second channel ordering), stale partial-file exclusion, the current dump directory's latest-display timestamp contract, and pre-device validation/RT option ordering.
- `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp:2008-2018`: source/lifecycle assertion surface. The single `DeviceParams` block configures dimensions, swapchain, both validation flags, and RT extensions before `CreateWindowDeviceAndSwapChain`; there is no duplicate late block.
- Behavioral target recipe (not runnable here): build Debug; default `HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8`; newest-group 4-check validator; fresh VUID/error scan; mode 20; fresh display inspection.

## Coverage summary

- Module-direct/source-contract: 5 Python behaviors
- Source lifecycle: covered by the source-contract case above
- GPU integration: 6 acceptance checks specified, execution externally blocked
- TestClient-layer: 0 (not applicable)
- Router-wiring: 0 (not applicable)

## Static cases audited

For a sorted synthetic path list:

1. No display path -> helper returns the original list (caller later reports no display for display-specific checks).
2. One run whose dumps span multiple seconds -> all files at/after its display timestamp remain.
3. Two complete runs -> only files at/after the second display timestamp remain.
4. Older partial files before the newest display timestamp -> excluded.
5. Same-second current-run channels that sort before `display` -> retained.
6. Current on-disk list -> every selected file is at/after the latest display timestamp and includes both display and gi_raw channels.

## TDD red-phase notes

- Before v143, case 3 failed: `load_frames` returned all historical PNGs and display checks used the first lexicographic display.
- Before v143, lifecycle assertion failed: the only explicit `bEnableDebugRuntime = true` appeared after `CreateWindowDeviceAndSwapChain`; fresh log line 13 consequently listed no Vulkan layers.
- If either validation flag moves below device creation again, the source-order check would fail and a fresh log would again have an empty layer section.
- If grouping regresses to the raw glob list, synthetic cases 3/4 and newest-group acceptance fail.

## Testability gaps

Terminal, numpy execution, GPU run, and vision are blocked in this scheduled runspace. Tests are specified but no runtime PASS is claimed. A capable worker must execute `PENDING_COMMIT_v143.md::verify` and inspect the newest default-mode display (not the mode-20 material diagnostic) before closure.
