# Pending Commit v144
- plan: docs/PENDING_PLAN_v144.md
- files: `Engine/Source/Runtime/Runtime_cmake.py`, `Engine/Source/Runtime/CMakeLists.txt`, `Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/test_validate_restir_gi.py`
- source: no bundle — direct edit
- target: current working tree (no commit or push)
- task: Force extraction of the NVRHI validation wrapper while preserving the pre-device Vulkan validation setup.
- verify: `python3 -m unittest discover -s Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data -p 'test_validate_restir_gi.py' -v && ./Build.sh --Rebuild --Target=TestReSTIR_GI_Temporal --Config=Debug && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 ./Binary/Debug/TestReSTIR_GI_Temporal && HLVM_DUMP_RGI=1 HLVM_RGI_ACCUM=8 HLVM_PT_DEBUG_MODE=20 ./Binary/Debug/TestReSTIR_GI_Temporal && python3 Engine/Source/Runtime/Test/TestReSTIR_GI_Temporal_Data/validate_restir_gi.py`
- skip_impl_review: no
- produces_test_files: yes
- notes: The generated Runtime CMake and the PyCMake source are intentionally kept in sync. The whole-archive expression is a CMake 3.24+ feature; this project’s generated Debug build identifies CMake 3.29.3. Runtime verification is required and has not been claimed because terminal execution is denied by tirith and vision is unavailable.

## Implementation

- `Runtime_cmake.py:121-128` now emits `nvrhi_vk` followed by `$<LINK_LIBRARY:WHOLE_ARCHIVE,nvrhi>` so static-library extraction cannot discard the validation wrapper.
- `Runtime/CMakeLists.txt:272` mirrors the generated linkage expression, preventing the current checkout from drifting until CMake is regenerated.
- No renderer, shader, validation lifecycle, or governance file was changed by this cycle.

## Plan Deviations

None. The implementation follows the reviewed plan. The test file is listed in the manifest for the tester role to add in the next pipeline stage.

## Runtime status

- Terminal probes (`pwd && git status --short --branch`, `date`, and `/usr/bin/python3 --version`) were rejected by the host security gate with `pending_approval: tirith:unknown`, `exit_code=-1`.
- No build, Python test, GPU run, validator, log scan, numpy analysis, or vision result is claimed.
