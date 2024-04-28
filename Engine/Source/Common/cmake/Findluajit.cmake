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
    find_path(luajit_INCLUDE_DIRS NAMES luajit-2.1/lua.hpp
            PATH_SUFFIXES luajit
            DOC "The luajit include directory")

    find_library(luajit_LIBRARIES NAMES libluajit libluajit-5.1
            DOC "The luajit library")

    mark_as_advanced(luajit_INCLUDE_DIRS luajit_LIBRARIES)

    add_library(luajit::luajit IMPORTED UNKNOWN)
    set_target_properties(
            luajit::luajit
            PROPERTIES
            IMPORTED_LOCATION "${luajit_LIBRARIES}"
            INTERFACE_INCLUDE_DIRECTORIES "${luajit_INCLUDE_DIRS}"
    )
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
        luajit
        REQUIRED_VARS luajit_LIBRARIES luajit_INCLUDE_DIRS
)
message("luajit_LIBRARIES ${luajit_LIBRARIES}")
message("luajit_INCLUDE_DIRS ${luajit_INCLUDE_DIRS}")
