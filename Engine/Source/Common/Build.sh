#!/bin/bash

ROOT_DIR=$(pwd)
# 定义要构建的目标目录
directories=("./Build/Debug" "./Build/RelWithDebInfo" "./Build/Release")

for dir in "${directories[@]}"; do
    # 进入目标目录并清理旧的构建
    cd "$dir" || exit 1
    #rm -rf CMakeFiles/

    # 配置CMake（假设项目的顶级目录是上级目录）
    #cmake -DCMAKE_BUILD_TYPE=$(basename "$dir") ../

    # 构建项目
    cmake --build .

    # 回到上级目录
    cd "$ROOT_DIR" || exit
done

echo "Finished building all targets"