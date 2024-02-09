
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../Dependency/vcpkg/scripts/buildsystems/vcpkg.cmake"
CACHE STRING "Vcpkg toolchain file")
cmake_minimum_required(VERSION 3.14)
project(Common C CXX)

# set compiler flag
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# set cpp strandard
set(CMAKE_CXX_STANDARD 20)
# set debug postfix for lib
set(CMAKE_DEBUG_POSTFIX d)

# global compile definitions
message("PyCMake: global compile definitions")
add_compile_definitions( $<$<CONFIG:Debug>:HLVM_BUILD_DEBUG=1> $<$<CONFIG:RelWithDebInfo>:HLVM_BUILD_DEVELOPMENT=1> $<$<CONFIG:Release>:HLVM_BUILD_RELEASE=1> $<$<CONFIG:MinSizeRel>:HLVM_BUILD_RELEASE=1>)

####################################################################
# Begin Module

# add module
add_library(Common STATIC ${CMAKE_CURRENT_SOURCE_DIR}/Private/Common.cpp
${CMAKE_CURRENT_SOURCE_DIR}/Private/Platform/GenericPlatform.cpp
${CMAKE_CURRENT_SOURCE_DIR}/Private/Platform/Windows/WindowsPlatformDebuggerUtil.cpp
${CMAKE_CURRENT_SOURCE_DIR}/Private/Platform/LinuxGNU/LinuxGNUPlatformDebuggerUtil.cpp
${CMAKE_CURRENT_SOURCE_DIR}/Private/Utility/Hash.cpp
${CMAKE_CURRENT_SOURCE_DIR}/Private/Core/Log.cpp
${CMAKE_CURRENT_SOURCE_DIR}/Private/Core/Parallel/Lock.cpp)
# Common include dir
message("PyCMake: Common include dir")
target_include_directories(Common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Public)
# Common pch files
message("PyCMake: Common pch files")
target_precompile_headers(Common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Public/Common.shared.pch)

####################################################################
# Begin fetch packages
include(FetchContent)
Set(FETCHCONTENT_QUIET FALSE)
# Fetch yalantinglibs
message("PyCMake: fetching yalantinglibs")
FetchContent_Declare(yalantinglibs
GIT_REPOSITORY https://github.com/yhyu13/yalantinglibs.git
GIT_TAG 679cbac8f3c5566a842c91b9d332632d3076f6ac
GIT_SHALLOW 1 # optional ( --depth=1 )
GIT_PROGRESS TRUE
SYSTEM          # optional, the Backward include directory will be treated as system directory
)
FetchContent_MakeAvailable(yalantinglibs)
# yalantinglibs compile definitions
message("PyCMake: yalantinglibs compile definitions")
target_compile_definitions(yalantinglibs INTERFACE YLT_ENABLE_PMR IGUANA_ENABLE_PMR ENABLE_STRUCT_PACK_OPTIMIZE)
# Fetch backward
message("PyCMake: fetching backward")
FetchContent_Declare(backward
GIT_REPOSITORY https://github.com/yhyu13/backward-cpp.git
GIT_TAG 51f0700452cf71c57d43c2d028277b24cde32502
GIT_SHALLOW 1 # optional ( --depth=1 )
GIT_PROGRESS TRUE
SYSTEM          # optional, the Backward include directory will be treated as system directory
)
FetchContent_MakeAvailable(backward)
# Fetch parallel-hashmap
message("PyCMake: fetching parallel-hashmap")
FetchContent_Declare(parallel-hashmap
GIT_REPOSITORY https://github.com/yhyu13/parallel-hashmap.git
GIT_TAG 67c24619e4f5ab2097b74cc397732c17a25d6944
GIT_SHALLOW 1 # optional ( --depth=1 )
GIT_PROGRESS TRUE
SYSTEM          # optional, the Backward include directory will be treated as system directory
)
FetchContent_MakeAvailable(parallel-hashmap)

# Finish fetch packages
####################################################################

####################################################################
# Begin find packages
# Find package spdlog
message("PyCMake: Find package spdlog")
find_package(spdlog CONFIG REQUIRED )
# Find package mimalloc
message("PyCMake: Find package mimalloc")
find_package(mimalloc CONFIG REQUIRED )
# Find package magic_enum
message("PyCMake: Find package magic_enum")
find_package(magic_enum CONFIG REQUIRED )
# Find package Boost
message("PyCMake: Find package Boost")
find_package(Boost  REQUIRED COMPONENTS filesystem system thread date_time regex timer chrono)

# Finish find packages
####################################################################
# Common include dir
message("PyCMake: Common include dir")
target_include_directories(Common PUBLIC ${parallel-hashmap_SOURCE_DIR})
target_include_directories(Common PUBLIC ${Boost_INCLUDE_DIRS})
# Common link dir
message("PyCMake: Common link dir")
target_link_directories(Common PUBLIC ${Boost_LIBRARY_DIRS})
# Common link libs
message("PyCMake: Common link libs")
target_link_libraries(Common PUBLIC yalantinglibs::yalantinglibs)
target_link_libraries(Common PUBLIC Backward::Interface ${CMAKE_DL_LIBS})
target_link_libraries(Common PUBLIC spdlog::spdlog)
target_link_libraries(Common PUBLIC mimalloc)
target_link_libraries(Common PUBLIC yalantinglibs::yalantinglibs)
target_link_libraries(Common PUBLIC ${Boost_LIBRARIES})

# add exe
add_executable(Test3rdParty ${CMAKE_CURRENT_SOURCE_DIR}/Test/Test3rdParty.cpp)
# Test3rdParty link libs
message("PyCMake: Test3rdParty link libs")
target_link_libraries(Test3rdParty PRIVATE Common)
# Test3rdParty pch files
message("PyCMake: Test3rdParty pch files")
target_precompile_headers(Test3rdParty REUSE_FROM Common)

# add exe
add_executable(TestLogger ${CMAKE_CURRENT_SOURCE_DIR}/Test/TestLogger.cpp)
# TestLogger link libs
message("PyCMake: TestLogger link libs")
target_link_libraries(TestLogger PRIVATE Common)
# TestLogger pch files
message("PyCMake: TestLogger pch files")
target_precompile_headers(TestLogger REUSE_FROM Common)

# add exe
add_executable(TestParallel ${CMAKE_CURRENT_SOURCE_DIR}/Test/TestParallel.cpp)
# TestParallel link libs
message("PyCMake: TestParallel link libs")
target_link_libraries(TestParallel PRIVATE Common)
# TestParallel pch files
message("PyCMake: TestParallel pch files")
target_precompile_headers(TestParallel REUSE_FROM Common)

# add exe
add_executable(TestException ${CMAKE_CURRENT_SOURCE_DIR}/Test/TestException.cpp)
# TestException link libs
message("PyCMake: TestException link libs")
target_link_libraries(TestException PRIVATE Common)
# TestException pch files
message("PyCMake: TestException pch files")
target_precompile_headers(TestException REUSE_FROM Common)

# add exe
add_executable(TestUtility ${CMAKE_CURRENT_SOURCE_DIR}/Test/TestUtility.cpp)
# TestUtility link libs
message("PyCMake: TestUtility link libs")
target_link_libraries(TestUtility PRIVATE Common)
# TestUtility pch files
message("PyCMake: TestUtility pch files")
target_precompile_headers(TestUtility REUSE_FROM Common)

# End Module
####################################################################
