cmake_minimum_required(VERSION 3.14)
set(CMAKE_TOOLCHAIN_FILE "~/Documents/Gitrepo-My/HLVM-Engine/Engine/Source/Dependency/vcpkg/scripts/buildsystems/vcpkg.cmake"
  CACHE STRING "Vcpkg toolchain file")
project(common C CXX)

# set compile flag
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# set cpp strandard
set(CMAKE_CXX_STANDARD 20)

# Using vcpkg repo with cmake adding include paths
# https://github.com/microsoft/vcpkg/issues/798#issuecomment-287219089
find_path(SYSTEM_INCLUDE_DIR spdlog)
# add include path
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR},
    ${SYSTEM_INCLUDE_DIR}
)

# set debug postfix for static lib
set(CMAKE_DEBUG_POSTFIX d)
# add a static lib target
add_library(common STATIC
    common.cpp
)

# Add dependency packages
find_package(spdlog)
target_link_libraries(common PRIVATE spdlog::spdlog_header_only)