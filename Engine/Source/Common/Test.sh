#!/bin/bash

time {

ROOT_DIR=$(pwd)
REPO_DIR=${ROOT_DIR}/../../..

# 获取环境变量
cd ${REPO_DIR}/Binary/GNULinux-x64 || exit 1
source .env
cd ${ROOT_DIR} || exit 1

# Get Gperf directory under vcpkg
GPERF_BIN=${ROOT_DIR}/../Dependency/vcpkg/packages/gperftools_x64-linux/tools/gperftools/bin/
echo "GPERF_BIN: ${GPERF_BIN}"
if [ -d "${GPERF_BIN}" ]; then
    echo "GPERF_BIN: ${GPERF_BIN}"
    export PATH=${GPERF_BIN}:$PATH
    echo "PATH: ${PATH}"
else
    echo "GPERF_BIN: ${GPERF_BIN} not found"
    exit 1
fi

# 定义要构建的目标目录
buildConfigs=("Debug" "RelWithDebInfo" "Release")
for config in "${buildConfigs[@]}"; do

    CMAKE_BUILD_TYPE=${config}
    CMAKE_BIN_DIR=${ROOT_DIR}/Binary/${CMAKE_BUILD_TYPE}

    # 进入目标目录
    cd "${CMAKE_BIN_DIR}" || exit 1

    for binary in ./Test*; do
        echo "Testing ${binary}"
        # 测试项目
        (${binary} --gperf=1 || exit 1)
        # 查看性能
        (${GPERF_BIN}/pprof --text ${binary} ${binary}_gperf.prof || exit 1)
    done
done

echo "Finished testing all targets"
}