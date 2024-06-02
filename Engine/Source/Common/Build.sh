#!/bin/bash

function echo_color() {
    local color_code=$1
    local message=$2
    echo -e "\e[${color_code}m${message}\e[0m"
}

#----------------------------------------------------------
# Step 1: Initialize an empty array
args=("$@")

# Step 2: Use "$@" to capture all input arguments
# This step is already done by the array initialization

# Step 3: Loop through the array to process each argument
for ((i=0; i<${#args[@]}; i++)); do
    # Step 4: Print each argument
    echo "Argument $i: ${args[$i]}"
done

# Step 1: Initialize variables
BuildConfig=
BuildTarget=
RunTest=0
RunClean=0
RunRebuild=0
Verbose=0
BuildGraphViz=0
RunGPerf=0
ParallelThreads=$(nproc)

# Step 2: Loop through each argument
for arg in "$@"; do
    # Step 3: Check for --Config=value
    if [[ $arg == --Config=* ]]; then
        BuildConfig=${arg#*=}
    fi

    # Step 4: Check for --Target=value
    if [[ $arg == --Target=* ]]; then
        BuildTarget=${arg#*=}
    fi

    # Step 5: Check for --Test
    if [[ $arg == --Test ]]; then
        RunTest=1
    fi

    # Step 6: Check for --Clean
    if [[ $arg == --Clean ]]; then
        RunClean=1
    fi

    # Step 7: Check for --Rebuild
    if [[ $arg == --Rebuild ]]; then
        RunRebuild=1
    fi

    # Step 8: Check for --Verbose
    if [[ $arg == --Verbose ]]; then
        Verbose=1
    fi

    # Step 9: Check for --GraphViz
    if [[ $arg == --GraphViz ]]; then
        BuildGraphViz=1
    fi

    # Step 10: Check for --GPerf
    if [[ $arg == --GPerf ]]; then
        RunGPerf=1
    fi

    # Step 11: Check for --ParallelThreads=value
    if [[ $arg == --ParallelThreads=* ]]; then
        ParallelThreads=${arg#*=}
    fi
done

# echo all arguments
echo_color 32 "Receive BuildConfig: ${BuildConfig}"
echo_color 32 "Receive BuildTarget: ${BuildTarget}"
echo_color 32 "Receive RunTest: ${RunTest}"
echo_color 32 "Receive RunClean: ${RunClean}"
echo_color 32 "Receive RunRebuild: ${RunRebuild}"
echo_color 32 "Receive Verbose: ${Verbose}"
echo_color 32 "Receive BuildGraphViz: ${BuildGraphViz}"
echo_color 32 "Receive RunGPerf: ${RunGPerf}"
echo_color 32 "Receive ParallelThreads: ${ParallelThreads}"
#------------------------------------------------------------------
time {

ROOT_DIR=$(pwd)
REPO_DIR=${ROOT_DIR}/../../..

# 获取环境变量
cd ${REPO_DIR}/Binary/GNULinux-x64 || exit 1
source .env
cd ${ROOT_DIR} || exit 1

# Get Gperf directory under vcpkg
if [ ${RunGPerf} -eq 1 ]; then
    # Get Gperf directory under vcpkg
    GPERF_BIN=${ROOT_DIR}/../Dependency/vcpkg/packages/gperftools_x64-linux/tools/gperftools/bin/
    if [ -d "${GPERF_BIN}" ]; then
        echo_color 34 "GPERF_BIN: ${GPERF_BIN}"
        export PATH=${GPERF_BIN}:$PATH
        echo_color 34  "PATH: ${PATH}"
    else
        echo_color 31 "GPERF_BIN: ${GPERF_BIN} not found"
        exit 1
    fi
fi

# 定义要构建的目标目录
buildConfigs=("Debug" "RelWithDebInfo" "Release")
CMAKE_SRC_DIR=${ROOT_DIR}
for config in "${buildConfigs[@]}"; do

    # if BuildConfig exist and config not equal BuildConfig, continue
    if [ -n "${BuildConfig}" ] && [ "${BuildConfig}" != "${config}" ]; then
        echo "Skip ${config} because BuildConfig is ${BuildConfig}"
        continue
    fi

    CMAKE_BUILD_TYPE=${config}
    CMAKE_BUILD_DIR=${ROOT_DIR}/Build/${CMAKE_BUILD_TYPE}

    if [ ${RunClean} -eq 1 ]; then
        rm -rf ${CMAKE_BUILD_DIR}
    fi

    # Gen cmake build directory
    cmake_param="-Wno-dev"
    if [ ${Verbose} -eq 1 ]; then
        cmake_param="-Wdev --debug-output"
    fi
    if [ ${BuildGraphViz} -eq 1 ]; then
        cmake_param="--graphviz=${CMAKE_BUILD_DIR}/build.dot ${cmake_param}"
    fi

    ${CMAKE_BIN} \
      ${cmake_param} \
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

    # 生成依赖图
    if [ ${BuildGraphViz} -eq 1 ]; then
        echo_color 34 "Generate dependency graph png at ${CMAKE_BUILD_DIR}/build.png"
        (dot -Tpng build.dot -o build.png) || exit 1
    fi

    # 构建参数s
    cbuild_param="-j ${ParallelThreads}"
    if [ ${Verbose} -eq 1 ]; then
        cbuild_param="--verbose"
    fi
    if [ ${RunRebuild} -eq 1 ]; then
        cbuild_param="--clean-first ${cbuild_param}"
    fi
    if [ ${RunClean} -eq 1 ]; then
        cbuild_param="--target 'clean' ${cbuild_param}"
    fi
    if [ -n "${BuildTarget}" ]; then
        cbuild_param="--target ${BuildTarget} ${cbuild_param}"
    fi

    # 构建项目
    build_cmd="${CMAKE_BIN} --build . ${cbuild_param}"
    echo_color 34 "Build cmd: ${build_cmd}"
    (${build_cmd}) || exit 1 \
      | tee "${ROOT_DIR}/build_${config}.log"

    # 测试项目
    if [ ${RunTest} -eq 1 ]; then
      ctest_param="-j 4 --output-on-failure --stop-on-failure"
      if [ -n "${BuildTarget}" ]; then
          # ctest run only one target
          # https://stackoverflow.com/questions/54160415/running-only-one-single-test-with-cmake-make
          ctest_param="-R ${BuildTarget} ${ctest_param}"
      fi

      # 测试项目
      test_cmd="${CTEST_BIN} . ${ctest_param}"
      echo_color 34 "Test cmd: ${test_cmd}"
      (${test_cmd}) || exit 1 \
        | tee "${ROOT_DIR}/build_test_${config}.log"
    fi

    # 性能测试
    if [ ${RunGPerf} -eq 1 ]; then
        CMAKE_BIN_DIR=${ROOT_DIR}/Binary/${CMAKE_BUILD_TYPE}
        # 进入目标目录
        cd "${CMAKE_BIN_DIR}" || exit 1

        TEST_LOG="${ROOT_DIR}/test_${config}.log"
        echo_color 32 "Start testing " | tee "${TEST_LOG}"
        for binary in ./Test*; do
            if [ -n "${BuildTarget}" ]; then
                if [[ ${binary} != "./${BuildTarget}" ]]; then
                  continue
                fi
            fi

            if test -x ${binary}; then
              echo_color 32  "Testing ${binary}" | tee -a "${TEST_LOG}"
              if [ -f ${binary}_gperf.prof ]; then
                rm ${binary}_gperf.prof
              fi
              # 测试项目
              (${binary} --gperf=1 || exit 1)
              # 查看性能
              (${GPERF_BIN}/pprof --text ${binary} ${binary}_gperf.prof || exit 1) \
                | tee -a "${TEST_LOG}"
            fi
        done
    fi
done

echo_color 32 "Finished building all targets"
}