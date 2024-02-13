from PyCMake.cmakecpp import *

vcpkg_cxt = VcpkgContenxt(vcpkg_root_path='../Dependency/vcpkg',
                          vcpkg_config=VcpkgConfigModel(name='Common',
                                                        version='0.1.0',
                                                        dependencies=[
                                                            "spdlog",
                                                            "mimalloc",
                                                            "magic-enum",
                                                            "boost",
                                                            "elfutils"
                                                        ],
                                                        builtin_baseline='53bef8994c541b6561884a8395ea35715ece75db'))

yalantinlibs = FetchContent(name='yalantinglibs',
                            git_repo_url='https://github.com/yhyu13/yalantinglibs.git',
                            git_tag='679cbac8f3c5566a842c91b9d332632d3076f6ac',
                            compile_definitions=[DomainValueModel(domain=DomainEnum.INTERFACE,
                                                                  values=['YLT_ENABLE_PMR',
                                                                          'IGUANA_ENABLE_PMR',
                                                                          'ENABLE_STRUCT_PACK_OPTIMIZE'])],
                            target_link_libs=[
                                DomainValueModel(domain=DomainEnum.PUBLIC, values=['yalantinglibs::yalantinglibs'])]
                            )
backward = FetchContent(name='backward',
                        git_repo_url='https://github.com/yhyu13/backward-cpp.git',
                        git_tag='51f0700452cf71c57d43c2d028277b24cde32502',
                        target_link_libs=[DomainValueModel(domain=DomainEnum.PUBLIC,
                                                           values=['Backward::Interface', '${CMAKE_DL_LIBS}'])]
                        )
parallel_hashmap = FetchContent(name='parallel-hashmap',
                                git_repo_url='https://github.com/yhyu13/parallel-hashmap.git',
                                git_tag='67c24619e4f5ab2097b74cc397732c17a25d6944',
                                target_include_dirs=[
                                    DomainValueModel(domain=DomainEnum.PUBLIC,
                                                     values=['${parallel-hashmap_SOURCE_DIR}'])],
                                )
ctre = FetchContent(name='ctre',
                    git_repo_url='https://github.com/yhyu13/compile-time-regular-expressions.git',
                    git_tag='9725886582a928491a086bba1c07909b2e583157',
                    target_link_libs=[DomainValueModel(domain=DomainEnum.PUBLIC,
                                                       values=['ctre::ctre'])]
                    )

spdlog = FindPackage(name='spdlog',
                     config=True,
                     required=True,
                     target_link_libs=[DomainValueModel(domain=DomainEnum.PUBLIC, values=['spdlog::spdlog'])])

mimalloc = FindPackage(name='mimalloc',
                       config=True,
                       required=True,
                       target_link_libs=[DomainValueModel(domain=DomainEnum.PUBLIC, values=['mimalloc'])])

magic_enum = FindPackage(name='magic_enum',
                         config=True,
                         required=True,
                         target_link_libs=[
                             DomainValueModel(domain=DomainEnum.PUBLIC, values=['yalantinglibs::yalantinglibs'])])

Boost = FindPackage(name='Boost',
                    config=False,
                    required=True,
                    components=['filesystem system thread date_time regex timer chrono'],
                    target_include_dirs=[DomainValueModel(domain=DomainEnum.PUBLIC, values=['${Boost_INCLUDE_DIRS}'])],
                    target_link_dirs=[DomainValueModel(domain=DomainEnum.PUBLIC, values=['${Boost_LIBRARY_DIRS}'])],
                    target_link_libs=[DomainValueModel(domain=DomainEnum.PUBLIC, values=['${Boost_LIBRARIES}'])])


class CommonModule(BaseModule):
    def __init__(self):
        super().__init__(module=ModuleTargetModel(target='Common',
                                                  type=ModuleEnum.STATIC,
                                                  source_files=glob_cmake_paths([GlobModel(path='./Private/**/*.cpp',
                                                                                           recursive=True)
                                                                                 ]),
                                                  unity_build=True),
                         fetch_packages=[yalantinlibs,
                                         backward,
                                         parallel_hashmap,
                                         ctre
                                         ],
                         find_packages=[spdlog,
                                        mimalloc,
                                        magic_enum,
                                        Boost,
                                        ]
                         )

        self.target_interface.add_include_dirs(domain=DomainEnum.PUBLIC,
                                               values=['./Public'])
        self.target_interface.add_pch_files(domain=DomainEnum.PUBLIC,
                                            values=['./Public/Common.shared.pch'])


class TestCommonModule(BaseModule):
    def __init__(self, cpp_path: str):
        super().__init__(module=ModuleTargetModel(target=os.path.basename(cpp_path).split('.')[0],
                                                  type=ModuleEnum.EXECUTABLE,
                                                  source_files=[ToCMakePath(cpp_path)],
                                                  unity_build=False)
                         )
        self.target_interface.add_pch_files(domain=DomainEnum.REUSE_FROM,
                                            values=['Common'])
        self.target_interface.add_link_libs(domain=DomainEnum.PRIVATE, values=['Common'])


class CommonProject(BaseProject):
    def __init__(self, **kwargs):
        super().__init__(name='Common',
                         version='3.14',
                         vcpkg_context=vcpkg_cxt, **kwargs)
        vcpkg_cxt.dump('./vcpkg.json')

        self.global_interface.add_compile_definitions(domain=DomainEnum.GLOBAL,
                                                      values=["$<$<CONFIG:Debug>:HLVM_BUILD_DEBUG=1>",
                                                              "$<$<CONFIG:RelWithDebInfo>:HLVM_BUILD_DEVELOPMENT=1>",
                                                              "$<$<CONFIG:Release>:HLVM_BUILD_RELEASE=1>",
                                                              "$<$<CONFIG:MinSizeRel>:HLVM_BUILD_RELEASE=1>"])
        self.global_interface.add_compile_options(domain=DomainEnum.GLOBAL,
                                                  values=[])

        self.modules.append(CommonModule())
        self.modules.extend([TestCommonModule(path) for path in glob.glob("./Test/*.cpp")])


if __name__ == '__main__':
    # cd to script dir
    logging.info(f'Exec {__file__}')
    from pathlib import Path

    _dir = Path(__file__).parent
    logging.debug(f'change dir to {_dir}')
    os.chdir(_dir)

    # write cmake file
    write_cmake_file_to_current_dir([
        CommonProject()
    ])


    # # Enable all warnings
    # set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
    #
    # # Enable even more pedantic warnings
    # set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pedantic")
    #
    # # Treat warnings as errors
    # set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
    #
    # # Same for C++
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Werror")
    #
    # # Optionally, enable specific warnings like unused variables or implicit conversion
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wunused-variable -Wconversion")
    #
    # # For a stricter check, include warnings that are not enabled by -Wall and -Wextra
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Weverything")
    #
    # # But if you want to disable certain warnings (for example, some third-party libraries might trigger them), you can exclude:
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-c++98-compat-pedantic")  # Disable C++98 compatibility warnings