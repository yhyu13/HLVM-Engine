# HLVM Engine: apply the engine's nvrhi patch set to the fetched nvrhi source.
#
# nvrhi-rhi2.patch contains three engine workarounds:
#   1. v134 (2026-07-30): build the validation TUs as first-class sources so
#      ninja incremental rebuilds cannot silently skip them.
#   2. HLVM (2026-07-30): immediate command-list reopen bypass for the
#      SingleCL-SingleFrame pattern used by the engine tests.
#   3. v168 (2026-08-14): VUID-vkCmdTraceRaysKHR-None-08608 false-positive
#      workaround for the system Vulkan Validation Layer 1.3.280 (re-bind the
#      current graphics pipeline before the RT bind to reset VVL's
#      "since last bound pipeline" dynamic-state mask).
#
# Idempotent: if the patch is already applied, the reverse-apply check
# succeeds and this script does nothing (so re-configures are safe).

if(NOT DEFINED HLVM_NVRHI_PATCH_DIR)
    message(FATAL_ERROR "apply_nvrhi_patch.cmake: HLVM_NVRHI_PATCH_DIR is not set")
endif()

set(_hlvm_patch "${HLVM_NVRHI_PATCH_DIR}/nvrhi-rhi2.patch")

execute_process(
    COMMAND git apply --reverse --check "${_hlvm_patch}"
    RESULT_VARIABLE _hlvm_already_applied
    ERROR_QUIET
)

if(_hlvm_already_applied EQUAL 0)
    message(STATUS "nvrhi patch already applied, skipping: ${_hlvm_patch}")
else()
    execute_process(
        COMMAND git apply "${_hlvm_patch}"
        RESULT_VARIABLE _hlvm_apply_result
    )
    if(NOT _hlvm_apply_result EQUAL 0)
        message(FATAL_ERROR
            "apply_nvrhi_patch.cmake: git apply failed (${_hlvm_apply_result}): ${_hlvm_patch}")
    endif()
    message(STATUS "nvrhi patch applied: ${_hlvm_patch}")
endif()
