#!/bin/bash

time {

ROOT_DIR=$(pwd)
REPO_DIR=${ROOT_DIR}/../../..

# 获取环境变量
cd ${REPO_DIR}/Binary/GNULinux-x64 || exit 1
source .env
cd ${ROOT_DIR} || exit 1

VCPKG_ROOT=${ROOT_DIR}/../Dependency/vcpkg
gperftools_path=(find ${VCPKG_ROOT} -type d -name 'gperftools' -print -quit)
# Check if the file was found
if [ -z "gperftools_path" ]; then
  echo "pprof file not found"
  exit 1
else
  echo "gperftools_path: gperftools_path"
fi
GPERF_BIN=${gperftools_path}/bin/

# 定义要构建的目标目录
buildConfigs=("Debug" "RelWithDebInfo" "Release")
for config in "${buildConfigs[@]}"; do

    CMAKE_BUILD_TYPE=${config}
    CMAKE_BUILD_DIR=${ROOT_DIR}/Build/${CMAKE_BUILD_TYPE}

    # 进入目标目录
    cd "${CMAKE_BUILD_DIR}" || exit 1

#    # TODO execute each test with --gperf=1, since ctest's binary output goes no where (I cannot find output .prof file)?
#    ctest_cmd='''--test-command "--gperf=1"'''
#    ctest_param="-j 4 --output-on-failure --stop-on-failure ${ctest_cmd} "
#    echo ${ctest_param}
#    # 测试项目
#    (${CTEST_BIN} . ${ctest_param} || exit 1)
done

echo "Finished testing all targets"
}