# Reference:
# https://stackoverflow.com/questions/72322755/how-do-i-link-botan-in-cmake

find_package(PkgConfig REQUIRED)

if (NOT TARGET luajit::luajit)
    pkg_check_modules(luajit QUIET IMPORTED_TARGET luajit)
    if (TARGET PkgConfig::luajit)
        add_library(luajit::luajit ALIAS PkgConfig::luajit)
    endif ()
endif ()

if (NOT TARGET luajit::luajit)
    find_path(luajit_INCLUDE_DIRS NAMES luajit-2.1/luajit.h
            DOC "The luajit include directory")
    # Include subdirectory so that we can directly #include <lua.hpp>
    set(luajit_INCLUDE_DIRS "${luajit_INCLUDE_DIRS}/luajit-2.1")

    find_library(luajit_LIBRARIES NAMES luajit-5.1
            DOC "The luajit library")
    #set(luajit_LIBRARIES "/home/hangyu5/Documents/Gitrepo-My/vcpkg/installed/x64-linux-dynamic/lib/libluajit-5.1.so")

    mark_as_advanced(luajit_INCLUDE_DIRS luajit_LIBRARIES)

    add_library(luajit::luajit IMPORTED UNKNOWN)
    set_target_properties(
            luajit::luajit
            PROPERTIES
            IMPORTED_LOCATION "${luajit_LIBRARIES}"
            INTERFACE_INCLUDE_DIRECTORIES "${luajit_INCLUDE_DIRS}"
    )
endif ()

message("luajit_LIBRARIES ${luajit_LIBRARIES}")
message("luajit_INCLUDE_DIRS ${luajit_INCLUDE_DIRS}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
        luajit
        REQUIRED_VARS luajit_LIBRARIES luajit_INCLUDE_DIRS
)
