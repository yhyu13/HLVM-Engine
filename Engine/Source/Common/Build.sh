#!/bin/bash

time {

ROOT_DIR=$(pwd)
REPO_DIR=${ROOT_DIR}/../../..

# 获取环境变量
cd ${REPO_DIR}/Binary/GNULinux-x64 || exit 1
source .env
cd ${ROOT_DIR} || exit 1

# 定义要构建的目标目录
buildConfigs=("Debug" "RelWithDebInfo" "Release")
CMAKE_SRC_DIR=${ROOT_DIR}
for config in "${buildConfigs[@]}"; do

    CMAKE_BUILD_TYPE=${config}
    CMAKE_BUILD_DIR=${ROOT_DIR}/Build/${CMAKE_BUILD_TYPE}

    # Gen cmake build directory
    #-Wno-dev \
    #      -Wdev \
    ${CMAKE_BIN} \
      -Wno-dev \
      --debug-output \
      -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER} \
      -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER} \
      -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} \
      -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}\
      -G Ninja \
      -S ${CMAKE_SRC_DIR} \
      -B ${CMAKE_BUILD_DIR} \
       || exit 1

    # 进入目标目录
    cd "${CMAKE_BUILD_DIR}" || exit 1

    cbuild_param="-j 32 --verbose"
    ctest_param="-j 8 --output-on-failure --stop-on-failure"
    # 构建项目
    (${CMAKE_BIN} --build . ${cbuild_param} && ${CTEST_BIN} . ${ctest_param}) || exit 1 \
      | tee "${ROOT_DIR}/build_${config}.log"
done

echo "Finished building all targets"
}