# HLVM Shader Compilation System
# Equivalent to Donut's compileshaders.cmake for HLVM Engine
#
# Copyright 2026 HLVM Engine
# MIT License

# ShaderMake path - can be overridden
if(NOT DEFINED HLVM_SHADERMAKE_PATH)
    set(HLVM_SHADERMAKE_PATH "$ENV{VULKAN_SDK}/../Tools/ShaderMake/bin/ShaderMake" CACHE PATH "Path to ShaderMake executable")
endif()

# ShaderMake compiler options for SPIR-V via slangc
if(NOT DEFINED HLVM_SLANG_PATH)
    set(HLVM_SLANG_PATH "$ENV{VULKAN_SDK}/Bin/slangc" CACHE PATH "Path to slangc compiler")
endif()

# Default register shifts for Vulkan descriptor sets
set(HLVM_VULKAN_REGISTER_OFFSETS
    --tRegShift 0
    --bRegShift 256)

# Main shader compilation function for HLVM
# Usage: hlvm_compile_shaders(
#     TARGET <build-target-name>
#     CONFIG <shader-config-file.cfg>
#     OUTPUT_DIR <output-directory>
#     SOURCES <source-dir>
#     SHADERMAKE_OPTIONS <options>)

function(hlvm_compile_shaders)
    set(options HEADER BINARY BLOB)
    set(oneValueArgs TARGET CONFIG OUTPUT_DIR SOURCES SHADERMAKE_OPTIONS)
    set(multiValueArgs)
    cmake_parse_arguments(PARSE_ARGV 0 HLVM params "${options}" "${oneValueArgs}" "${multiValueArgs}")
    
    if(NOT params_TARGET)
        message(FATAL_ERROR "hlvm_compile_shaders: TARGET argument required")
    endif()
    if(NOT params_CONFIG)
        message(FATAL_ERROR "hlvm_compile_shaders: CONFIG argument required")
    endif()
    if(NOT params_OUTPUT_DIR)
        message(FATAL_ERROR "hlvm_compile_shaders: OUTPUT_DIR argument required")
    endif()
    if(NOT params_SOURCES)
        message(FATAL_ERROR "hlvm_compile_shaders: SOURCES argument required")
    endif()
    
    # Build ShaderMake command
    set(SHADERMAKE_CMD "${HLVM_SHADERMAKE_PATH}")
    
    # Platform and compiler
    list(APPEND SHADERMAKE_CMD -p SPIRV)
    list(APPEND SHADERMAKE_CMD --slang)
    list(APPEND SHADERMAKE_CMD --compiler "${HLVM_SLANG_PATH}")
    
    # Register shifts for Vulkan
    foreach(OPT IN LISTS HLVM_VULKAN_REGISTER_OFFSETS)
        list(APPEND SHADERMAKE_CMD ${OPT})
    endforeach()
    
    # Output format
    if(params_BINARY)
        list(APPEND SHADERMAKE_CMD --binary)
    endif()
    if(params_HEADER)
        list(APPEND SHADERMAKE_CMD --header)
    endif()
    if(params_BLOB)
        list(APPEND SHADERMAKE_CMD --binaryBlob)
    endif()
    
    # Additional ShaderMake options from caller
    if(params_SHADERMAKE_OPTIONS)
        separate_arguments(OPTIONS_LIST NATIVE_COMMAND "${params_SHADERMAKE_OPTIONS}")
        list(APPEND SHADERMAKE_CMD ${OPTIONS_LIST})
    endif()
    
    # Config and output
    list(APPEND SHADERMAKE_CMD -c "${params_CONFIG}")
    list(APPEND SHADERMAKE_CMD -o "${params_OUTPUT_DIR}")
    
    # Add custom command for ShaderMake
    add_custom_command(
        OUTPUT ${params_TARGET}
        COMMAND ${SHADERMAKE_CMD}
        DEPENDS ${params_CONFIG}
        WORKING_DIRECTORY ${params_SOURCES}
        COMMENT "Compiling shaders using ShaderMake"
        VERBATIM)
    
    add_custom_target(${params_TARGET} ALL DEPENDS ${params_TARGET})
endfunction()

# Wrapper for compiling all HLVM shaders
# Usage: hlvm_compile_all_shaders(TARGET hlvm_shaders)
function(hlvm_compile_all_shaders TARGET)
    set(options)
    set(oneValueArgs TARGET)
    set(multiValueArgs)
    cmake_parse_arguments(PARSE_ARGV 0 HLVM params "${options}" "${oneValueArgs}" "${multiValueArgs}")
    
    hlvm_compile_shaders(
        TARGET ${params_TARGET}
        CONFIG ${ARGN}
        OUTPUT_DIR ${HLVM_SHADER_OUTPUT_DIR}
        SOURCES ${HLVM_SHADER_SOURCE_DIR}
        BINARY)
endfunction()
