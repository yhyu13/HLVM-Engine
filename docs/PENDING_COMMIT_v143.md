# Pending Commit v143
- plan: docs/PENDING_PLAN_v143.md
- files: `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal.cpp`, `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py`, `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/test_validate_restir_gi.py`
- source: no bundle — direct edit
- target: current working tree (no commit or push)
- task: Request Vulkan/NVRHI validation before device creation and restrict structural validation to the newest dump group.
- verify: `python3 -m unittest discover -s Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data -p 'test_validate_restir_gi.py' -v && ./Build.sh --Config=Debug --Target=TestReSTIR_GI_Temporal && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal && python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20 ./Binary/Debug/TestReSTIR_GI_Temporal`
- skip_impl_review: no
- produces_test_files: yes
- notes: The validator is an executable regression-test artifact, so review is mandatory. Runtime verification was attempted but denied by the host security gate (`pending_approval: tirith:unknown`).

## Implementation

### Pre-device validation setup

In `TestReSTIR_GI_Temporal.cpp`, one pre-creation `DeviceParams` block now sets dimensions, swapchain options, validation, and RT extensions in the proven sibling-test order:

```cpp
DeviceParams.bEnableDebugRuntime = true;
DeviceParams.bEnableNVRHIValidationLayer = true;
DeviceParams.bEnableRayTracingExtensions = true;
```

The duplicate ineffective post-creation parameter block was removed. `DeviceManagerVk1_Instance.cpp` now sees the debug-runtime request while constructing the Vulkan instance, enabling `VK_LAYER_KHRONOS_validation` and its debug messenger. `DeviceManagerVk4_LifeCycle.cpp` sees the explicit NVRHI wrapper request while creating the NVRHI device.

### Newest-group validator scope

`validate_restir_gi.py` now exposes `select_newest_dump_group(files)`. It parses each filename's `YYYYMMDD_HHMMSS` prefix, anchors on the maximum display timestamp, and keeps every file at or after that timestamp. Timestamp comparison (rather than slicing at the display's lexicographic list position) preserves same-second channels such as `denoised` that sort before `display`. Because the C++ dumper writes display first for each run, this selects exactly the latest run even when later channels span adjacent seconds. `load_frames` applies this selector before loading images and before all four checks.

## Plan Deviations

None. Both planned harness edits and the planned focused Python regression tests were applied. The Python helper uses inline documentation because its grouping contract depends on dump order.

## Runtime status

- `terminal`: three invocations denied by tirith, including the target build and validator command.
- Therefore no fresh build/run/mode-20/vision result is claimed.
- Existing post-v142 logs remain supporting evidence only, not v143 verification.
