cmake_minimum_required(VERSION 3.14)
set(CMAKE_TOOLCHAIN_FILE "~/Documents/Gitrepo-My/HLVM-Engine/Engine/Source/Dependency/vcpkg/scripts/buildsystems/vcpkg.cmake"
  CACHE STRING "Vcpkg toolchain file")
project(Common C CXX)

# set compile flag
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# set cpp strandard
set(CMAKE_CXX_STANDARD 20)
# set debug postfix for static lib
set(CMAKE_DEBUG_POSTFIX d)

# project macro
add_compile_definitions("$<$<CONFIG:Debug>:HLVM_BUILD_DEBUG=1>")
add_compile_definitions("$<$<CONFIG:RelWithDebInfo>:HLVM_BUILD_DEVELOPMENT=1>")
add_compile_definitions("$<$<CONFIG:Release>:HLVM_BUILD_RELEASE=1>")
add_compile_definitions("$<$<CONFIG:MinSizeRel>:HLVM_BUILD_RELEASE=1>")

# add project include path
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Public)

####################################################################
# target 1
file(GLOB_RECURSE PRIVATE_CPPs ${CMAKE_CURRENT_SOURCE_DIR}/Private/*.cpp)
add_library(Common STATIC ${PRIVATE_CPPs})

# package dependency
include(FetchContent)
FetchContent_Declare(
    yalantinglibs
    GIT_REPOSITORY https://github.com/yhyu13/yalantinglibs.git
    GIT_TAG 679cbac8f3c5566a842c91b9d332632d3076f6ac
    GIT_SHALLOW 1 # optional ( --depth=1 )
)
FetchContent_MakeAvailable(yalantinglibs)
find_package(spdlog CONFIG REQUIRED)
find_package(mimalloc CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)

# add dependency packages
target_link_libraries(Common PUBLIC yalantinglibs::yalantinglibs)
target_link_libraries(Common PUBLIC spdlog::spdlog_header_only)
target_link_libraries(Common PUBLIC mimalloc)
target_link_libraries(Common PUBLIC magic_enum::magic_enum)

# pch
target_precompile_headers(Common
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/Public/Common.shared.pch
)

# Clang-tidy (Slow!)
# set(CLANG_TIDY_COMMAND clang-tidy-16;
#   --header-filter=${CMAKE_CURRENT_SOURCE_DIR}/Private/Common.cpp;
#   --config-file=${CMAKE_CURRENT_SOURCE_DIR}/.clang-tidy;
# )
# set_target_properties(Common PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}")

####################################################################
# target 2
add_executable(Test3rdParty
    ${CMAKE_CURRENT_SOURCE_DIR}/Test/Test3rdParty.cpp
)
target_link_libraries(Test3rdParty PRIVATE Common)
target_precompile_headers(Test3rdParty REUSE_FROM Common)

####################################################################
# target 3
add_executable(TestLogger
        ${CMAKE_CURRENT_SOURCE_DIR}/Test/TestLogger.cpp
)
target_link_libraries(TestLogger PRIVATE Common)
target_precompile_headers(TestLogger REUSE_FROM Common)
