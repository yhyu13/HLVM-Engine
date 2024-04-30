# Reference:
# https://stackoverflow.com/questions/72322755/how-do-i-link-botan-in-cmake

find_package(PkgConfig REQUIRED)

if (NOT TARGET Botan::Botan)
    pkg_check_modules(Botan QUIET IMPORTED_TARGET botan-3)
    if (TARGET PkgConfig::Botan)
        add_library(Botan::Botan ALIAS PkgConfig::Botan)
    endif ()
endif ()

if (NOT TARGET Botan::Botan)
    find_path(Botan_INCLUDE_DIRS NAMES botan/botan.h
            PATH_SUFFIXES botan-3
            DOC "The Botan include directory")

    find_library(Botan_LIBRARIES NAMES botan botan-3
            DOC "The Botan library")

    mark_as_advanced(Botan_INCLUDE_DIRS Botan_LIBRARIES)

    add_library(Botan::Botan IMPORTED UNKNOWN)
    set_target_properties(
            Botan::Botan
            PROPERTIES
            IMPORTED_LOCATION "${Botan_LIBRARIES}"
            INTERFACE_INCLUDE_DIRECTORIES "${Botan_INCLUDE_DIRS}"
    )
endif ()

message("Botan_LIBRARIES ${Botan_LIBRARIES}")
message("Botan_INCLUDE_DIRS ${Botan_INCLUDE_DIRS}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
        Botan
        REQUIRED_VARS Botan_LIBRARIES Botan_INCLUDE_DIRS
)