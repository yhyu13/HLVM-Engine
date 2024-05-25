from PyCMake.cmakecpp import *

# Create a VcpkgContext object with the specified path for vcpkg root and version
vcpkg_cxt = VcpkgContenxt(vcpkg_root_path='../Dependency/vcpkg',
                          vcpkg_config=VcpkgConfigModel(name='Common',
                                                        version='0.2.0',
                                                        dependencies=[
                                                            "spdlog",
                                                            VcpkgPackage(name="mimalloc", features=["asm", "secure"],
                                                                         default_features=False),
                                                            "magic-enum",
                                                            "boost",
                                                            "libbacktrace",  # used by boost stack trace on linux
                                                            "zstd",
                                                            "botan",
                                                            "rapidjson",
                                                            # VcpkgPackage(name="opentelemetry-cpp",
                                                            #              features=[], default_features=False),
                                                            # "catch2"
                                                            "gperftools",  # linux cpu sampling
                                                            "minitrace",  # chrome format tracing
                                                            VcpkgPackage(name="luajit", features=["buildvm-64"],
                                                                         default_features=True),
                                                            "sol2",
                                                            "pybind11"
                                                        ]))

# Find the spdlog package with the specified options
spdlog = FindPackage(name='spdlog',
                     config=True,
                     required=True,
                     dependant_target_link_libs=[DomainValueModel(domain=DomainEnum.PUBLIC, values=['spdlog::spdlog'])])

# Find the mimalloc package with the specified options
mimalloc = FindPackage(name='mimalloc',
                       config=True,
                       required=True,
                       dependant_target_link_libs=[
                           DomainValueModel(domain=DomainEnum.PUBLIC, values=['mimalloc-static'])])

# Find the magic_enum package with the specified options
magic_enum = FindPackage(name='magic_enum',
                         config=True,
                         required=True,
                         dependant_target_link_libs=[
                             DomainValueModel(domain=DomainEnum.PUBLIC, values=['magic_enum::magic_enum'])])

# Find the Boost package with the specified options
Boost = FindPackage(name='Boost',
                    config=False,
                    required=True,
                    components=['iostreams filesystem system thread fiber date_time program_options'],
                    dependant_target_include_dirs=[
                        DomainValueModel(domain=DomainEnum.PUBLIC, values=['${Boost_INCLUDE_DIRS}'])],
                    dependant_target_link_dirs=[
                        DomainValueModel(domain=DomainEnum.PUBLIC, values=['${Boost_LIBRARY_DIRS}'])],
                    dependant_target_link_libs=[DomainValueModel(domain=DomainEnum.PUBLIC, values=['${Boost_LIBRARIES}',
                                                                                                   'backtrace'])])

# Find the Botan3 package with the specified options
botan3 = FindPackage(name='Botan',
                     config=False,
                     required=True,
                     dependant_target_link_libs=[
                         DomainValueModel(domain=DomainEnum.PUBLIC, values=['Botan::Botan'])])

# Find the zstd package with the specified options
zstd = FindPackage(name='zstd',
                   config=True,
                   required=True,
                   dependant_target_link_libs=[
                       DomainValueModel(domain=DomainEnum.PUBLIC, values=['zstd::libzstd_static'])])

# Find the rapidjson package with the specified options
rapidjson = FindPackage(name='RapidJSON',
                        config=True,
                        required=True,
                        dependant_target_link_libs=[
                            DomainValueModel(domain=DomainEnum.PUBLIC, values=['rapidjson'])])

# Find the opentelemtry package with the specified options
# opentelemetry = FindPackage(name='opentelemetry-cpp',
#                             config=True,
#                             required=True,
#                             dependant_target_include_dirs=[DomainValueModel(domain=DomainEnum.PUBLIC,
#                                                                   values=['${OPENTELEMETRY_CPP_INCLUDE_DIRS}'])],
#                             dependant_target_link_libs=[
#                                 DomainValueModel(domain=DomainEnum.PUBLIC, values=['${OPENTELEMETRY_CPP_LIBRARIES}'])])
# protobuf = FindPackage(name='protobuf',
#                        config=False,
#                        required=True)
# grpc = FindPackage(name='gRPC',
#                    config=False,
#                    required=True)
# curl = FindPackage(name='CURL',
#                    config=False,
#                    required=True)
# nlohmann_json = FindPackage(name='nlohmann_json',
#                             config=False,
#                             required=True)

# # Find the catch2 package with the specified options
#  Linking time is too slowwww, just copy some key ideas e.g. stable seed, section, benchmarking macro)
# catch2 = FindPackage(name='Catch2',
#                         config=True,
#                         required=True,
#                         dependant_target_link_libs=[
#                             DomainValueModel(domain=DomainEnum.PUBLIC, values=['Catch2::Catch2',
#                                                                                'Catch2::Catch2WithMain'])])

# Find the gperftools package with the specified options
gperftools = FindPackage(name='Gperftools',
                         config=False,
                         required=True,
                         components=['profiler'],
                         dependant_target_link_libs=[
                             DomainValueModel(domain=DomainEnum.PUBLIC, values=['${GPERFTOOLS_LIBRARIES}'])])

# Find the minitrace package with the specified options
minitrace = FindPackage(name='minitrace',
                        config=True,
                        required=True,
                        dependant_target_link_libs=[
                            DomainValueModel(domain=DomainEnum.PUBLIC, values=['minitrace::minitrace'])])

# Find the luajit package with the specified options
luajit = FindPackage(name='luajit',
                     config=False,
                     required=True,
                     dependant_target_link_libs=[
                         DomainValueModel(domain=DomainEnum.PUBLIC, values=['luajit::luajit'])])

# Find the sol2 package with the specified options
sol2 = FindPackage(name='sol2',
                   config=True,
                   required=True,
                   dependant_target_link_libs=[
                       DomainValueModel(domain=DomainEnum.PUBLIC, values=['sol2'])])

##########################################################

# Fetch the Yalantinglibs package from GitHub with the specified options
yalantinlibs = FetchContent(name='yalantinglibs',
                            git_repo_url='https://github.com/yhyu13/yalantinglibs.git',
                            git_tag='abf6016a8f7841d29303ef68f118ea85b69a1051',
                            target_compile_options=[TargetDomainValueModel(target='yalantinglibs',
                                                                           domain=DomainEnum.INTERFACE,
                                                                           values=['-DYLT_ENABLE_PMR=ON',
                                                                                   '-DIGUANA_ENABLE_PMR=ON',
                                                                                   '-DENABLE_STRUCT_PACK_OPTIMIZE=ON'])],
                            dependant_target_link_libs=[
                                DomainValueModel(domain=DomainEnum.PUBLIC, values=['yalantinglibs::yalantinglibs'])]
                            )

# Fetch the parallel-hashmap package from GitHub with the specified options
parallel_hashmap = FetchContent(name='parallel-hashmap',
                                git_repo_url='https://github.com/yhyu13/parallel-hashmap.git',
                                git_tag='67c24619e4f5ab2097b74cc397732c17a25d6944',
                                dependant_target_include_dirs=[
                                    DomainValueModel(domain=DomainEnum.PUBLIC,
                                                     values=['${parallel-hashmap_SOURCE_DIR}'])],
                                )

# Fetch the ctre package from GitHub with the specified options
ctre = FetchContent(name='ctre',
                    git_repo_url='https://github.com/yhyu13/compile-time-regular-expressions.git',
                    git_tag='9725886582a928491a086bba1c07909b2e583157',
                    dependant_target_link_libs=[DomainValueModel(domain=DomainEnum.PUBLIC,
                                                                 values=['ctre::ctre'])]
                    )

# Fetch the Tracy package from GitHub with the specified options
tracy = FetchContent(name='Tracy',
                     git_repo_url='https://github.com/yhyu13/tracy.git',
                     git_tag='b48216cc6fbd0f36764c6d36bd71dd8f8e3d3830',
                     target_compile_options=[TargetDomainValueModel(target='TracyClient', domain=DomainEnum.INTERFACE,
                                                                    values=['-DTRACY_ONLY_LOCALHOST=ON',
                                                                            '-DTRACY_NO_FRAME_IMAGE=ON',
                                                                            '-DTRACY_ONLY_IPV4=ON',
                                                                            '-DTRACY_USE_RPMALLOC=ON',
                                                                            '-DTRACY_NO_EXIT=ON',
                                                                            '-DTRACY_LIBBACKTRACE_ELF_DYNLOAD_SUPPORT=ON'])],
                     dependant_target_link_libs=[
                         DomainValueModel(domain=DomainEnum.PUBLIC, values=['Tracy::TracyClient'])]
                     )

"""
Global Config :
"""
bThreadSanitizer = False
bBuildShared = False


# Create a CommonModule object with the specified options
class CommonModule(BaseModule):
    def __init__(self):
        super().__init__(module=ModuleTargetModel(target='Common',
                                                  type=ModuleEnum.SHARED if bBuildShared else ModuleEnum.STATIC,
                                                  source_files=glob_cmake_paths([GlobModel(path='./Private/**/*.cpp',
                                                                                           recursive=True)
                                                                                 ]),
                                                  unity_build=True),
                         fetch_packages=[yalantinlibs,
                                         parallel_hashmap,
                                         ctre,
                                         tracy,
                                         ],
                         find_packages=[spdlog,
                                        mimalloc,
                                        magic_enum,
                                        Boost,
                                        botan3,
                                        zstd,
                                        rapidjson,
                                        # opentelemetry,
                                        # protobuf,
                                        # grpc,
                                        # curl,
                                        # nlohmann_json,
                                        # catch2
                                        gperftools,
                                        minitrace,
                                        luajit,
                                        sol2,
                                        ]
                         )
        self.target_interface.add_compile_options(domain=DomainEnum.PUBLIC, values=[
            '$<$<COMPILE_LANGUAGE:C>: -Wall -Wextra -pedantic -Werror>',
            '$<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -pedantic -Werror -Wunused-variable -Wconversion -Weverything>',
            '$<$<COMPILE_LANGUAGE:CXX>:-Wno-padded -Wno-gnu-zero-variadic-macro-arguments -Wno-reserved-identifier -Wno-exit-time-destructors -Wno-global-constructors -Wno-c++98-compat-pedantic -Wno-float-equal>',
            '$<$<COMPILE_LANGUAGE:CXX>:-Wno-error=global-constructors -Wno-error=exit-time-destructors -Wno-error=unsafe-buffer-usage -Wno-error=unused-function -Wno-error=unused-but-set-variable -Wno-error=unused-variable -Wno-error=unused-member-function>'
        ])

        self.target_interface.add_include_dirs(domain=DomainEnum.PUBLIC,
                                               values=['./Public'])
        self.target_interface.add_pch_files(domain=DomainEnum.PUBLIC,
                                            values=['./Public/Common.shared.pch'])

        if bThreadSanitizer:
            self.target_interface.add_compile_options(domain=DomainEnum.PUBLIC, values=['${HLVM_CMAKE_CXX_FLAGS_TSAN}'])
            self.target_interface.add_link_libs(domain=DomainEnum.PUBLIC, values=['tsan'])


# Create a TestCommonModule object with the specified options
class TestCommonModule(BaseModule):
    def __init__(self, cpp_path: str):
        super().__init__(module=ModuleTargetModel(target=os.path.basename(cpp_path).split('.')[0],
                                                  type=ModuleEnum.EXECUTABLE_AND_TEST,
                                                  source_files=[ToCMakePath(cpp_path)],
                                                  unity_build=False),
                         fetch_packages=[],
                         find_packages=[]
                         )
        self.target_interface.add_pch_files(domain=DomainEnum.REUSE_FROM,
                                            values=['Common'])
        self.target_interface.add_link_libs(domain=DomainEnum.PRIVATE, values=['Common'])
        if bBuildShared:
            # TODO : windows platform compatibility check!
            # https://gitlab.kitware.com/cmake/cmake/-/issues/20289
            self.target_interface.add_compile_options(domain=DomainEnum.PRIVATE, values=['-fPIC'])


# Create a CommonProject object with the specified options
class CommonProject(BaseProject):
    def __init__(self, **kwargs):
        super().__init__(name='Common',
                         version='3.14',
                         vcpkg_context=vcpkg_cxt, **kwargs)

        # Vcpkg Dependencies
        vcpkg_cxt.dump('./vcpkg.json')

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

        # Output
        self.global_interface.add_global_set('CMAKE_DEBUG_POSTFIX', ['d'])
        bin_output_dir = '${PROJECT_SOURCE_DIR}/Binary/${CMAKE_BUILD_TYPE}'
        self.global_interface.add_global_set('CMAKE_RUNTIME_OUTPUT_DIRECTORY', [bin_output_dir])
        self.global_interface.add_global_set('CMAKE_LIBRARY_OUTPUT_DIRECTORY', [bin_output_dir])
        self.global_interface.add_global_set('CMAKE_ARCHIVE_OUTPUT_DIRECTORY', [bin_output_dir])
        self.global_interface.add_global_set('HLVM_CMAKE_CXX_FLAGS_TSAN', ['-fsanitize=thread'])

        # Definitions
        self.global_interface.add_compile_definitions(domain=DomainEnum.GLOBAL,
                                                      values=["$<$<CONFIG:Debug>:HLVM_BUILD_DEBUG=1>",
                                                              "$<$<CONFIG:RelWithDebInfo>:HLVM_BUILD_DEVELOPMENT=1>",
                                                              "$<$<CONFIG:Release>:HLVM_BUILD_RELEASE=1>",
                                                              "$<$<CONFIG:MinSizeRel>:HLVM_BUILD_RELEASE=1>",
                                                              f"HLVM_COMMON_DYNAMIC_LINKED={bBuildShared * 1}"])

        # Create a CommonModule object for the main project
        self.modules.append(CommonModule())
        # Create TestCommonModule objects for all tests
        self.modules.extend([TestCommonModule(path) for path in glob.glob("./Test/*.cpp")])


# Main function
if __name__ == '__main__':
    # cd to script dir
    logging.info(f'Exec {__file__}')
    from pathlib import Path

    _dir = Path(__file__).parent
    logging.debug(f'change dir to {_dir}')
    os.chdir(_dir)

    # write cmake file
    dump_to_cmake_list([
        CommonProject()
    ], './')
