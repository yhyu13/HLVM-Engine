from PyCMake.cmakecpp import *

# Create a VcpkgContext object with the specified path for vcpkg root and version
vcpkg_ctx_runtime = VcpkgContenxt(vcpkg_root_path='../Dependency/vcpkg',
                                  vcpkg_config=VcpkgConfigModel(name='Runtime',
                                                                version='0.2.1',
                                                                dependencies=[
                                                                    "glfw3",
                                                                    "glm",
                                                                    "dylib",
                                                                    "vulkan-memory-allocator",
                                                                    "glslang",
                                                                    "assimp",
                                                                    "bullet3",
                                                                    "ktx",
                                                                    VcpkgPackage(name="imgui",
                                                                                 features=["glfw-binding"],
                                                                                 default_features=True),
                                                                ]))

# 导入 Common_cmake.py 中的 vcpkg_ctx_common 变量
import sys
import os

# 添加 Common_cmake.py 到 sys.path
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from Common import Common_cmake
import ShaderMakeBuild

# 合并 vcpkg_ctx_common
vcpkg_ctx_runtime.merge_vckpkg_context(Common_cmake.vcpkg_cxt_common)

# Find the glfw package with the specified options
vulkan = FindPackage(name='Vulkan',
                     required=True,
                     config=False,
                     dependant_target_include_dirs=[
                         DomainValueModel(domain=DomainEnum.PUBLIC, values=['${Vulkan_INCLUDE_DIRS}'])],
                     dependant_target_link_libs=[
                         DomainValueModel(domain=DomainEnum.PUBLIC, values=['${Vulkan_LIBRARIES}'])])

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

# Find the dylib package with the specified options
dylib = FindPackage(name='dylib',
                    config=True,
                    required=True,
                    dependant_target_include_dirs=[
                        DomainValueModel(domain=DomainEnum.PUBLIC, values=['${DYLIB_INCLUDE_DIRS}'])])

# Find the vma package with the specified options, load after vulkan is found
vulkan_memory_allocator = FindPackage(name='VulkanMemoryAllocator',
                                      config=True,
                                      required=True,
                                      dependant_target_link_libs=[
                                          DomainValueModel(domain=DomainEnum.PUBLIC,
                                                           values=['Vulkan::Vulkan',
                                                                   'GPUOpen::VulkanMemoryAllocator'])])

# Find the glslang package with the specified options
glslang = FindPackage(name='glslang',
                      config=True,
                      required=True,
                      dependant_target_link_libs=[
                          DomainValueModel(domain=DomainEnum.PUBLIC, values=[
                              'glslang::OSDependent',
                              'glslang::glslang',
                              'glslang::MachineIndependent',
                              'glslang::GenericCodeGen',
                              'glslang::glslang-default-resource-limits',
                              'glslang::OGLCompiler',
                              'glslang::SPVRemapper',
                              'glslang::SPIRV',
                              'glslang::HLSL'])])

assimp = FindPackage(name='assimp',
                     config=True,
                     required=True,
                     dependant_target_link_libs=[
                         DomainValueModel(domain=DomainEnum.PUBLIC, values=['assimp::assimp'
                                                                            ])
                     ])

bullet3 = FindPackage(name='Bullet',
                      config=True,
                      required=True,
                      dependant_target_link_libs=[
                          DomainValueModel(domain=DomainEnum.PUBLIC, values=['${BULLET_LIBRARIES}'
                                                                             ])
                      ])

imgui = FindPackage(name='imgui',
                    config=True,
                    required=True,
                    dependant_target_link_libs=[
                        DomainValueModel(domain=DomainEnum.PUBLIC, values=['imgui::imgui'])
                    ])

ktx = FindPackage(name='Ktx',
                  config=True,
                  required=True,
                  dependant_target_link_libs=[
                      DomainValueModel(domain=DomainEnum.PUBLIC, values=['KTX::ktx'])
                  ])

##########################################################

# Fetch the parallel-hashmap package from GitHub with the specified options
nvrhi = FetchContent(name='nvrhi',
                     git_repo_url='https://github.com/yhyu13/NVRHI.git',
                     git_tag='472f99ac68251970dc9e75afa1648c9bc4db7e83',
                     dependant_target_link_libs=[
                         # link nvrhi_vk before nvrhi otherwise link error
                         DomainValueModel(domain=DomainEnum.PUBLIC, values=['nvrhi_vk', 'nvrhi'])]
                     )

"""
Global Config :
"""
bThreadSanitizer = False  # Supers low performance, not even debuggable lol
bBuildShared = False  # Not working on ubuntu/linux, shared lib is PITA
bLinkByGold = False  # Using llvm GOLD linker for link time optimization
bSSE41 = True  # Enable SSE41 for GLM matrix decomposition

bVulkanNoPrototype = True  # True : Dynamic loading vk api on startup from shared lib
# True : Use Vulkan SDK include path instead of system include path
# False : Use system include path, but we may use wrong vulkan sdk version due to Ubuntu apt package management lag behind
bVulkanSDKOVerridePath = True


# Create a RuntimeModule object with the specified options
class RuntimeModule(BaseModule):
    def __init__(self):
        super().__init__(module=ModuleTargetModel(target='Runtime',
                                                  type=ModuleEnum.SHARED if bBuildShared else ModuleEnum.STATIC,
                                                  source_files=PyCMakeUtil.glob(
                                                      [PyCMakeUtil.GlobModel(path='./Private/**/*.cpp',
                                                                             recursive=True)
                                                       ]),
                                                  unity_build=False, unity_build_exclusion_patterns=['*VulkanLoader*']),
                         fetch_packages=[nvrhi,
                                         ],
                         find_packages=[vulkan,
                                        glfw3,
                                        glm,
                                        dylib,
                                        vulkan_memory_allocator,
                                        glslang,
                                        assimp,
                                        bullet3,
                                        imgui,
                                        ktx,
                                        ]
                         )
        compile_options = [
            '$<$<COMPILE_LANGUAGE:C>: -Wall -Wextra -pedantic -Werror>',
            '$<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -pedantic -Werror -Wunused-variable -Wconversion -Weverything>',
            '$<$<COMPILE_LANGUAGE:CXX>:-Wno-unsafe-buffer-usage -Wno-padded -Wno-gnu-zero-variadic-macro-arguments -Wno-reserved-identifier -Wno-exit-time-destructors -Wno-global-constructors -Wno-c++98-compat-pedantic -Wno-float-equal -Wno-covered-switch-default -Wno-c++20-compat>',
            '$<$<COMPILE_LANGUAGE:CXX>:-Wno-error=global-constructors -Wno-error=exit-time-destructors -Wno-error=unused-function -Wno-error=unused-but-set-variable -Wno-error=unused-variable -Wno-error=unused-member-function>',
        ]
        if bSSE41:
            compile_options.append('$<$<COMPILE_LANGUAGE:CXX>:-msse4.1>')

        self.target_interface.add_compile_options(domain=DomainEnum.PUBLIC, values=compile_options)

        # Do we have to include subdirectory's include paths? probably not
        self.target_interface.add_include_dirs(domain=DomainEnum.PUBLIC,
                                               values=['./../Common/Public'])
        self.target_interface.add_include_dirs(domain=DomainEnum.PUBLIC,
                                               values=[
                                                   './../Common/Test'])  # for testing, we just need Common/Test/Test.h
        self.target_interface.add_include_dirs(domain=DomainEnum.PUBLIC,
                                               values=['./Public'])
        # Local stb (before vcpkg to avoid vcpkg's modified stb_image.h)
        self.target_interface.add_include_dirs(domain=DomainEnum.PUBLIC,
                                               values=['./ThirdParty/stb'])
        self.target_interface.add_pch_files(domain=DomainEnum.PUBLIC,
                                            values=['./Public/Runtime.shared.pch'])
        self.target_interface.add_link_libs(domain=DomainEnum.PUBLIC, values=['Common'])

        if bThreadSanitizer:
            self.target_interface.add_compile_options(domain=DomainEnum.PUBLIC, values=['${HLVM_CMAKE_CXX_FLAGS_TSAN}'])
            self.target_interface.add_link_libs(domain=DomainEnum.PUBLIC, values=['tsan'])

        if bVulkanNoPrototype:
            self.target_interface.add_compile_options(domain=DomainEnum.PUBLIC, values=['-DVK_NO_PROTOTYPES'])

    def dump(self, fp):
        # First dump the base module
        super().dump(fp)
        # Disable LTO for STBTextureLoader.cpp to prevent JPEG decoder stripping
        # Use -fno-lto to compile without ThinLTO
        # Use -Wl,-allow-multiple-definition to handle duplicate stb symbols with assimp
        fp.write('\n')
        fp.write('# Disable LTO for STBTextureLoader.cpp to preserve JPEG decoder\n')
        fp.write('set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/Private/Renderer/Texture/STBTextureLoader.cpp\n')
        fp.write('    PROPERTIES COMPILE_FLAGS "-fno-lto")\n')
        fp.write('\n')
        fp.write('# Allow multiple definitions of stb symbols (from assimp and local stb)\n')
        fp.write('if(CMAKE_INTERPROCEDURAL_OPTIMIZATION)\n')
        fp.write('    target_link_options(Runtime PRIVATE -Wl,-allow-multiple-definition)\n')
        fp.write('endif()\n')


# Create a RuntimeTestModule object with the specified options
class RuntimeTestModule(BaseModule):
    def __init__(self, cpp_path: str, shader_data_dir: str = None):
        test_name = os.path.basename(cpp_path).split('.')[0]
        super().__init__(module=ModuleTargetModel(target=test_name,
                                                  type=ModuleEnum.EXECUTABLE_AND_TEST,
                                                  source_files=[cpp_path],
                                                  unity_build=False),
                         fetch_packages=[],
                         find_packages=[]
                         )
        self.target_interface.add_pch_files(domain=DomainEnum.REUSE_FROM,
                                            values=['Runtime'])
        self.target_interface.add_link_libs(domain=DomainEnum.PRIVATE, values=['Runtime'])
        self.target_name = test_name  # Store for shader integration
        self.shader_data_dir = shader_data_dir  # Store for shader integration

        if bBuildShared:
            # TODO : windows platform compatibility check!
            # https://gitlab.kitware.com/cmake/cmake/-/issues/20289
            self.target_interface.add_compile_options(domain=DomainEnum.PRIVATE, values=['-fPIC'])

    def dump(self, fp):
        # First dump the base module info (test executable)
        super().dump(fp)
        # Add linker flag to allow multiple definitions (for stb symbols from assimp and local stb)
        if not bBuildShared:
            fp.write('\n')
            fp.write('# Allow multiple definitions when linking test with LTO\n')
            fp.write('if(CMAKE_INTERPROCEDURAL_OPTIMIZATION)\n')
            fp.write(f'    target_link_options({self.target_name} PRIVATE -Wl,-allow-multiple-definition)\n')
            fp.write('endif()\n')

        # Then dump shader build CMake code if this test has shader data
        if self.shader_data_dir:
            # Choose the right ShaderMake function based on target name
            if "FullDeferredShading2" in self.target_name:
                shader_cmake = ShaderMakeBuild.create_full_deferred_shading2_shadermake(self.target_name)
            elif "RTShadowsGBuffer" in self.target_name:
                shader_cmake = ShaderMakeBuild.create_rt_shadows_gbuffer_shadermake(self.target_name)
            elif "RTReflections" in self.target_name:
                shader_cmake = ShaderMakeBuild.create_rt_reflections_shadermake(self.target_name)
            elif "CornellBoxGI" in self.target_name:
                shader_cmake = ShaderMakeBuild.create_cornell_box_gi_shadermake(self.target_name)
            elif "SponzaDeferred" in self.target_name:
                shader_cmake = ShaderMakeBuild.create_sponza_deferred_shadermake(self.target_name)
            elif "RenderSponza" in self.target_name:
                shader_cmake = ShaderMakeBuild.create_render_sponza_shadermake(self.target_name)
            elif "PBRLighting" in self.target_name:
                shader_cmake = ShaderMakeBuild.create_pbr_lighting_shadermake(self.target_name)
            elif "ToneMapping" in self.target_name:
                shader_cmake = ShaderMakeBuild.create_tone_mapping_shadermake(self.target_name)
            elif "GPUInstancing" in self.target_name:
                shader_cmake = ShaderMakeBuild.create_gpu_instancing_shadermake(self.target_name)
            elif "PathTraceGI" in self.target_name:
                shader_cmake = ShaderMakeBuild.create_path_trace_gi_shadermake(self.target_name)
            elif "PathTraceTriangle" in self.target_name:
                shader_cmake = ShaderMakeBuild.create_path_trace_triangle_shadermake(self.target_name)
            else:
                shader_cmake = ShaderMakeBuild.create_deferred_shading_shadermake(self.target_name)
            shader_cmake.dump(fp)
            fp.write(f"add_dependencies({self.target_name} {self.target_name}_ShaderMake)\n")

            # Create symlink for shader data directory in binary output directory
            # This is needed because tests load shader data relative to executable path
            shader_data_name = self.target_name + "_Data"
            fp.write(f"\n")
            fp.write(f"# Create symlink for shader data directory\n")
            fp.write(f"add_custom_command(TARGET {self.target_name} POST_BUILD\n")
            fp.write(f"    COMMAND ${{CMAKE_COMMAND}} -E create_symlink\n")
            fp.write(f"        ${{CMAKE_SOURCE_DIR}}/Test/{shader_data_name}\n")
            fp.write(f"        \"${{CMAKE_RUNTIME_OUTPUT_DIRECTORY}}/{shader_data_name}\"\n")
            fp.write(f"    COMMENT \"Creating symlink for {shader_data_name}\"\n")
            fp.write(f")\n")


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

        # CMP0077 set to new in order for Cmake to respect our setted value for CMAKE >=3.14
        self.global_interface.add_global_set('CMAKE_POLICY_DEFAULT_CMP0077', ['NEW'])

        # Linker
        if bBuildShared:
            self.global_interface.add_global_set('CMAKE_POSITION_INDEPENDENT_CODE', ['ON'])
        else:
            self.global_interface.add_global_set('CMAKE_POLICY_DEFAULT_CMP0069', ['NEW'])
            self.global_interface.add_global_set('CMAKE_INTERPROCEDURAL_OPTIMIZATION', ['ON'])
        if bLinkByGold:
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
                                                              f"HLVM_COMMON_DYNAMIC_LINKED={bBuildShared * 1}",
                                                              f"HLVM_ROOT=$ENV{{HLVM_ROOT}}"])

        if bVulkanSDKOVerridePath:
            # env get $Vulkan_SDK
            vulkan_sdk_path = os.getenv('VULKAN_SDK')
            # check 1.4.328.1 in path
            if "1.4.328.1" not in vulkan_sdk_path:
                # throw exception
                raise RuntimeError("Vulkan SDK 1.4.328.1 not found")
            self.global_interface.add_global_set('ENV{VULKAN_SDK}', [vulkan_sdk_path])

        hlvm_root = os.getenv('HLVM_ROOT')
        self.global_interface.add_global_set('ENV{HLVM_ROOT}', [hlvm_root])

        self.modules.append(RuntimeModule())

        # Add common shader compilation for shared Blit shaders
        common_shaders = ShaderMakeBuild.create_common_shadermake()
        self.modules.append(common_shaders)

        # Add shared GI path-tracing shaders (FGIPass)
        gi_shaders = ShaderMakeBuild.create_gi_shadermake()
        self.modules.append(gi_shaders)

        # Create test modules - detect tests with shader data directories
        for test_cpp in glob.glob("./Test/*.cpp"):
            test_name = os.path.basename(test_cpp).split('.')[0]
            # Check if this test has a shader data directory with ShaderMake.cfg
            shader_data_dir = None
            possible_data_dir = f"./Test/{test_name}_Data"
            if os.path.isdir(possible_data_dir) and os.path.exists(os.path.join(possible_data_dir, "ShaderMake.cfg")):
                shader_data_dir = possible_data_dir
            self.modules.append(RuntimeTestModule(test_cpp, shader_data_dir))


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
