from PyCMake.cmakecpp import *

# Create a VcpkgContext object with the specified path for vcpkg root and version
vcpkg_ctx_runtime = VcpkgContenxt(vcpkg_root_path='../Dependency/vcpkg',
                          vcpkg_config=VcpkgConfigModel(name='Runtime',
                                                        version='0.2.1',
                                                        dependencies=[
                                                            "glfw3",
                                                            "glm",
                                                            "vulkan-headers",
                                                            "vulkan-memory-allocator",
                                                            "assimp",
                                                            "bullet3",
                                                        ]))

# 导入 Common_cmake.py 中的 vcpkg_ctx_common 变量
import sys
from os import path
sys.path.append( path.dirname( path.dirname( path.abspath(__file__) ) ) )
from Common import Common_cmake
vcpkg_ctx_runtime.merge_vckpkg_context(Common_cmake.vcpkg_cxt_common)


# Find the glfw package with the specified options
glfw3 = FindPackage(name='glfw3',
                    config=True,
                    required=True,
                    dependant_target_link_libs=[
                        DomainValueModel(domain=DomainEnum.PUBLIC, values=['glfw'])])

# Find the glm package with the specified options
glm = FindPackage(name='glm',
                  config=True,
                  required=True,
                  dependant_target_link_libs=[
                      DomainValueModel(domain=DomainEnum.PUBLIC, values=['glm::glm'])])

# Find the vulkan-headers package with the specified options
vulkan_header = FindPackage(name='VulkanHeaders',
                            config=True,
                            required=False,
                            dependant_target_link_libs=[
                                DomainValueModel(domain=DomainEnum.PUBLIC, values=['Vulkan::Headers'])])

# Find the vulkan-headers package with the specified options
vulkan_memory_allocator = FindPackage(name='VulkanMemoryAllocator',
                            config=True,
                            required=True,
                            dependant_target_link_libs=[
                                DomainValueModel(domain=DomainEnum.PUBLIC, values=['GPUOpen::VulkanMemoryAllocator'])])

"""
Global Config :
"""
bThreadSanitizer = False
bBuildShared = False

# Create a RuntimeModule object with the specified options
class RuntimeModule(BaseModule):
    def __init__(self):
        super().__init__(module=ModuleTargetModel(target='Runtime',
                                                  type=ModuleEnum.SHARED if bBuildShared else ModuleEnum.STATIC,
                                                  source_files=PyCMakeUtil.glob([PyCMakeUtil.GlobModel(path='./Private/**/*.cpp',
                                                                                           recursive=True)
                                                                                 ]),
                                                  unity_build=True, unity_build_exclusion_patterns = ['*VulkanLoader*']),
                                                  #unity_build=False),
                         fetch_packages=[],
                         find_packages=[glfw3,
                                        glm,
                                        vulkan_header,
                                        vulkan_memory_allocator,
                                        ]
                         )
        self.target_interface.add_compile_options(domain=DomainEnum.PUBLIC, values=[
            '$<$<COMPILE_LANGUAGE:C>: -Wall -Wextra -pedantic -Werror>',
            '$<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -pedantic -Werror -Wunused-variable -Wconversion -Weverything>',
            '$<$<COMPILE_LANGUAGE:CXX>:-Wno-padded -Wno-gnu-zero-variadic-macro-arguments -Wno-reserved-identifier -Wno-exit-time-destructors -Wno-global-constructors -Wno-c++98-compat-pedantic -Wno-float-equal -Wno-covered-switch-default -Wno-c++20-compat>',
            '$<$<COMPILE_LANGUAGE:CXX>:-Wno-error=global-constructors -Wno-error=exit-time-destructors -Wno-error=unsafe-buffer-usage -Wno-error=unused-function -Wno-error=unused-but-set-variable -Wno-error=unused-variable -Wno-error=unused-member-function>'
        ])

        # Do we have to include subdirectory's include paths? probably not
        self.target_interface.add_include_dirs(domain=DomainEnum.PUBLIC,
                                       values=['./../Common/Public'])
        self.target_interface.add_include_dirs(domain=DomainEnum.PUBLIC,
                                               values=['./../Common/Test']) # for testing, we just need Common/Test/Test.h
        self.target_interface.add_include_dirs(domain=DomainEnum.PUBLIC,
                                               values=['./Public'])
        self.target_interface.add_pch_files(domain=DomainEnum.PUBLIC,
                                            values=['./Public/Runtime.shared.pch'])
        self.target_interface.add_link_libs(domain=DomainEnum.PUBLIC, values=['Common'])


        if bThreadSanitizer:
            self.target_interface.add_compile_options(domain=DomainEnum.PUBLIC, values=['${HLVM_CMAKE_CXX_FLAGS_TSAN}'])
            self.target_interface.add_link_libs(domain=DomainEnum.PUBLIC, values=['tsan'])


# Create a TestRuntimeModule object with the specified options
class TestRuntimeModule(BaseModule):
    def __init__(self, cpp_path: str):
        super().__init__(module=ModuleTargetModel(target=os.path.basename(cpp_path).split('.')[0],
                                                  type=ModuleEnum.EXECUTABLE_AND_TEST,
                                                  source_files=[cpp_path],
                                                  unity_build=False),
                         fetch_packages=[],
                         find_packages=[]
                         )
        self.target_interface.add_pch_files(domain=DomainEnum.REUSE_FROM,
                                            values=['Runtime'])
        self.target_interface.add_link_libs(domain=DomainEnum.PRIVATE, values=['Runtime'])

        if bBuildShared:
            # TODO : windows platform compatibility check!
            # https://gitlab.kitware.com/cmake/cmake/-/issues/20289
            self.target_interface.add_compile_options(domain=DomainEnum.PRIVATE, values=['-fPIC'])



# Create a RuntimeProject object with the specified options
class RuntimeProject(BaseProject):
    def __init__(self, **kwargs):
        super().__init__(name='Runtime',
                         version='3.14',
                         vcpkg_context=vcpkg_ctx_runtime, **kwargs)

        # add sub directory
        self.sub_directories.append(SubDirectoryModel(path='./../Common',
                                                    output_dir="Common.output",
                                                    exclude_by_default=True))

        # Proxy
        self.global_interface.add_global_set('ENV{HTTP_PROXY}', ["http://127.0.0.1:8889"])
        self.global_interface.add_global_set('ENV{HTTPS_PROXY}', ["http://127.0.0.1:8889"])
        self.global_interface.add_global_set('ENV{http_proxy}', ["http://127.0.0.1:8889"])
        self.global_interface.add_global_set('ENV{https_proxy}', ["http://127.0.0.1:8889"])

        # Linker
        if bBuildShared:
            self.global_interface.add_global_set('CMAKE_POSITION_INDEPENDENT_CODE', ['ON'])
        else:
            self.global_interface.add_global_set('CMAKE_POLICY_DEFAULT_CMP0069', ['NEW'])
            self.global_interface.add_global_set('CMAKE_INTERPROCEDURAL_OPTIMIZATION', ['ON'])
        self.global_interface.add_global_set('CMAKE_LINKER_TYPE', ['GOLD'])

        # Compiler
        self.global_interface.add_global_set('CMAKE_EXPORT_COMPILE_COMMANDS', ['ON'])
        self.global_interface.add_global_set('CMAKE_C_STANDARD', ['23'])
        self.global_interface.add_global_set('CMAKE_CXX_STANDARD', ['23'])
        self.global_interface.add_global_set('CMAKE_DEBUG_POSTFIX', ['d'])

        # Output
        bin_output_dir = '${PROJECT_SOURCE_DIR}/Binary/${CMAKE_BUILD_TYPE}'
        self.global_interface.add_global_set('CMAKE_RUNTIME_OUTPUT_DIRECTORY', [bin_output_dir])
        self.global_interface.add_global_set('CMAKE_LIBRARY_OUTPUT_DIRECTORY', [bin_output_dir])
        self.global_interface.add_global_set('CMAKE_ARCHIVE_OUTPUT_DIRECTORY', [bin_output_dir])

        # Definitions
        self.global_interface.add_global_set('HLVM_CMAKE_CXX_FLAGS_TSAN', ['-fsanitize=thread'])
        self.global_interface.add_compile_definitions(domain=DomainEnum.GLOBAL,
                                                      values=["$<$<CONFIG:Debug>:HLVM_BUILD_DEBUG=1>",
                                                              "$<$<CONFIG:RelWithDebInfo>:HLVM_BUILD_DEVELOPMENT=1>",
                                                              "$<$<CONFIG:Release>:HLVM_BUILD_RELEASE=1>",
                                                              "$<$<CONFIG:MinSizeRel>:HLVM_BUILD_RELEASE=1>",
                                                              f"HLVM_COMMON_DYNAMIC_LINKED={bBuildShared * 1}"])


        self.modules.append(RuntimeModule())
        self.modules.extend([TestRuntimeModule(path) for path in glob.glob("./Test/*.cpp")])


# Main function
if __name__ == '__main__':
    # cd to script dir
    logging.info(f'Exec {__file__}')
    from pathlib import Path

    _dir = Path(__file__).parent
    logging.debug(f'change dir to {_dir}')
    os.chdir(_dir)

    # write cmake file
    PyCMakeUtil.dump_to_cmake_list([
        RuntimeProject()
    ])
