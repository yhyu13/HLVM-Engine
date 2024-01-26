cmake_minimum_required(VERSION 3.14)
set(CMAKE_TOOLCHAIN_FILE "~/Documents/Gitrepo-My/HLVM-Engine/Engine/Source/Dependency/vcpkg/scripts/buildsystems/vcpkg.cmake"
  CACHE STRING "Vcpkg toolchain file")
project(common C CXX)

# set compile flag
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# set cpp strandard
set(CMAKE_CXX_STANDARD 20)
# set debug postfix for static lib
set(CMAKE_DEBUG_POSTFIX d)

# package dependency
include(FetchContent)
FetchContent_Declare(
    yalantinglibs
    GIT_REPOSITORY https://github.com/yhyu13/yalantinglibs.git
    GIT_TAG 679cbac8f3c5566a842c91b9d332632d3076f6ac
    GIT_SHALLOW 1 # optional ( --depth=1 )
)
FetchContent_MakeAvailable(yalantinglibs)

find_package(spdlog)

# Using vcpkg repo with cmake adding include paths
# https://github.com/microsoft/vcpkg/issues/798#issuecomment-287219089
find_path(SYSTEM_INCLUDE_DIR spdlog)
# add include path
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR},
    ${SYSTEM_INCLUDE_DIR}
)

# add a static lib target
add_library(common STATIC
    common.cpp
)

# Add dependency packages
target_link_libraries(common PRIVATE spdlog::spdlog_header_only)
target_link_libraries(common PRIVATE yalantinglibs::yalantinglibs)