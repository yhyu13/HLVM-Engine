cmake_minimum_required(VERSION 3.14)
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../Dependency/vcpkg/scripts/buildsystems/vcpkg.cmake"
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

# add dependency packages
include(FetchContent)
Set(FETCHCONTENT_QUIET FALSE)
FetchContent_Declare(
    yalantinglibs
    GIT_REPOSITORY https://github.com/yhyu13/yalantinglibs.git
    GIT_TAG 679cbac8f3c5566a842c91b9d332632d3076f6ac
    GIT_SHALLOW 1 # optional ( --depth=1 )
    GIT_PROGRESS TRUE
    SYSTEM
)
FetchContent_MakeAvailable(yalantinglibs)
target_compile_definitions(yalantinglibs INTERFACE YLT_ENABLE_PMR IGUANA_ENABLE_PMR ENABLE_STRUCT_PACK_OPTIMIZE)
target_link_libraries(Common PUBLIC yalantinglibs::yalantinglibs)

# Also requires one of: libbfd (gnu binutils), libdwarf, libdw (elfutils)
# Either vckpg install elfutils libdwarf or sudo apt-get install libdwarf-dev libdw-dev -y
FetchContent_Declare(
    backward
    GIT_REPOSITORY https://github.com/yhyu13/backward-cpp
    GIT_TAG 51f0700452cf71c57d43c2d028277b24cde32502  # or a version tag, such as v1.6
    GIT_SHALLOW 1 # optional ( --depth=1 )
    GIT_PROGRESS TRUE
    SYSTEM          # optional, the Backward include directory will be treated as system directory
)
FetchContent_MakeAvailable(backward)
# Add Backward to your target (either Backward::Interface, Backward::Object, or Backward::Backward)
target_link_libraries(Common PUBLIC Backward::Interface)
target_link_libraries(Common PUBLIC ${CMAKE_DL_LIBS})

find_package(spdlog CONFIG REQUIRED)
target_link_libraries(Common PUBLIC spdlog::spdlog_header_only)

find_package(mimalloc CONFIG REQUIRED)
target_link_libraries(Common PUBLIC mimalloc)

find_package(magic_enum CONFIG REQUIRED)
target_link_libraries(Common PUBLIC magic_enum::magic_enum)

find_package(Boost REQUIRED)
if (Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    link_directories(${Boost_LIBRARY_DIRS})
endif ()
target_link_libraries(Common PUBLIC ${Boost_LIBRARIES})

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

####################################################################
# target 4
add_executable(TestParallel
        ${CMAKE_CURRENT_SOURCE_DIR}/Test/TestParallel.cpp
)
target_link_libraries(TestParallel PRIVATE Common)
target_precompile_headers(TestParallel REUSE_FROM Common)

####################################################################
# target 5
add_executable(TestException
        ${CMAKE_CURRENT_SOURCE_DIR}/Test/TestException.cpp
)
target_link_libraries(TestException PRIVATE Common)
target_precompile_headers(TestException REUSE_FROM Common)