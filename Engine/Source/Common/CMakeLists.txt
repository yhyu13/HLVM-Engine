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

# package dependency
include(FetchContent)
FetchContent_Declare(
    yalantinglibs
    GIT_REPOSITORY https://github.com/yhyu13/yalantinglibs.git
    GIT_TAG 679cbac8f3c5566a842c91b9d332632d3076f6ac
    GIT_SHALLOW 1 # optional ( --depth=1 )
)
FetchContent_MakeAvailable(yalantinglibs)
find_package(spdlog REQUIRED)
find_package(mimalloc REQUIRED)

# add include path
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Public)

# add a static lib target
add_library(Common STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/Common.cpp
)

# Add dependency packages
target_link_libraries(Common PUBLIC yalantinglibs::yalantinglibs)
target_link_libraries(Common PUBLIC spdlog::spdlog_header_only)
target_link_libraries(Common PUBLIC mimalloc)

# Clang-tidy
# set(CLANG_TIDY_COMMAND clang-tidy-16;
#   --header-filter=${CMAKE_CURRENT_SOURCE_DIR}/Private/Common.cpp;
#   --config-file=${CMAKE_CURRENT_SOURCE_DIR}/.clang-tidy;
# )
# set_target_properties(Common PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}")

add_executable(Test
    ${CMAKE_CURRENT_SOURCE_DIR}/Private/Test.cpp
)
target_link_libraries(Test PRIVATE Common)
