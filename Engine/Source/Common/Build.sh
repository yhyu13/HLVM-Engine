#!/bin/bash

time {

export ROOT_DIR=$(pwd)
export REPO_DIR=${ROOT_DIR}/../../..

# 获取环境变量
source ${REPO_DIR}/Binary/GNULinux-x64/.env

# 定义要构建的目录
export CMAKE_SRC_DIR=${ROOT_DIR}

# 定义要构建的目标目录
buildConfigs=("Debug" "RelWithDebInfo" "Release")

for config in "${buildConfigs[@]}"; do

    CMAKE_BUILD_TYPE=${config}
    CMAKE_BUILD_DIR=${ROOT_DIR}/Build/${CMAKE_BUILD_TYPE}

    # Gen cmake build directory
    ${CMAKE_BIN} \
      -Wno-dev \
      -G Ninja \
      -S ${CMAKE_SRC_DIR} \
      -B ${CMAKE_BUILD_DIR} \
       || exit 1

    # 进入目标目录
    cd "${CMAKE_BUILD_DIR}" || exit 1

    # 构建项目
    (${CMAKE_BIN} --build . && ${CTEST_BIN} . || exit 1) | tee "${ROOT_DIR}/build_${config}.log"

    # 回到上级目录
    cd "${ROOT_DIR}" || exit 1
done

echo "Finished building all targets"
}