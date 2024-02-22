# Reference:
# https://stackoverflow.com/questions/72322755/how-do-i-link-botan-in-cmake

find_path(ADVOBFUSCATOR_INCLUDE_DIRS "Lib/Indexes.h")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
        Advobfuscator
        REQUIRED_VARS ADVOBFUSCATOR_INCLUDE_DIRS
)
