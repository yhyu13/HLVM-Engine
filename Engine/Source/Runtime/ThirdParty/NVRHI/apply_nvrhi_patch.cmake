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
    message(STATUS "nvrhi patch already applied (exact), skipping: ${_hlvm_patch}")
else()
    execute_process(
        COMMAND git apply "${_hlvm_patch}"
        RESULT_VARIABLE _hlvm_apply_result
    )
    if(NOT _hlvm_apply_result EQUAL 0)
        # The working tree may already carry FUNCTIONALLY equivalent edits
        # with different comment text (e.g. the v169 variant of the 08608
        # fix in the Release checkout). Verify each hunk's functional marker
        # before failing; if all are present, treat the patch as applied.
        execute_process(
            COMMAND grep -q "add_library(nvrhi STATIC" CMakeLists.txt
            RESULT_VARIABLE _hlvm_h1
            ERROR_QUIET
        )
        execute_process(
            COMMAND sh -c "grep -q 'include_validation' CMakeLists.txt || grep -q '\\${src_validation}' CMakeLists.txt"
            RESULT_VARIABLE _hlvm_h2
            ERROR_QUIET
        )
        execute_process(
            COMMAND grep -q "HLVM bypass: continuing" src/validation/validation-commandlist.cpp
            RESULT_VARIABLE _hlvm_h3
            ERROR_QUIET
        )
        execute_process(
            COMMAND sh -c "grep -q 'bindPipeline(vk::PipelineBindPoint::eGraphics, GfxPso->pipeline)' src/vulkan/vulkan-raytracing.cpp || grep -q 'm_CurrentCmdBuf->cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, GfxPso->pipeline)' src/vulkan/vulkan-raytracing.cpp"
            RESULT_VARIABLE _hlvm_h4
            ERROR_QUIET
        )
        if(_hlvm_h1 EQUAL 0 AND _hlvm_h2 EQUAL 0 AND _hlvm_h3 EQUAL 0 AND _hlvm_h4 EQUAL 0)
            message(STATUS "nvrhi patch functionally already applied (markers present), skipping: ${_hlvm_patch}")
        else()
            message(FATAL_ERROR
                "apply_nvrhi_patch.cmake: git apply failed (${_hlvm_apply_result}) and functional markers are incomplete: "
                "CMakeLists=${_hlvm_h1}/${_hlvm_h2} validationCL=${_hlvm_h3} raytracing=${_hlvm_h4} — ${_hlvm_patch}")
        endif()
    else()
        message(STATUS "nvrhi patch applied: ${_hlvm_patch}")
    endif()
endif()
