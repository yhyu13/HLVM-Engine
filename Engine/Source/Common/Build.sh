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
Jobs=$(nproc)
TestRepeatNum=2

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

    # Step 11: Check for --Jobs=value
    if [[ $arg == --Jobs=* ]]; then
        Jobs=${arg#*=}
    fi

    # Step 12: Check for --TestRepeatNum
    if [[ $arg == --TestRepeatNum ]]; then
        TestRepeatNum=${arg#*=}
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
echo_color 32 "Receive Jobs: ${Jobs}"
echo_color 32 "Receive TestRepeatNum: ${TestRepeatNum}"
#------------------------------------------------------------------
time {

# CWD_DIR is the same as script's directory
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
CWD_DIR=$SCRIPT_DIR
REPO_DIR=${CWD_DIR}/../../..

# Change directory to platform .env path 获取环境变量
cd ${REPO_DIR}/Binary/GNULinux-x64 || exit 1
source .env
cd ${CWD_DIR} || exit 1
echo_color 34 "CWD_DIR: ${CWD_DIR}"

# Get Gperf directory under vcpkg
if [ ${RunGPerf} -eq 1 ]; then
    # Get Gperf directory under vcpkg
    GPERF_BIN=${CWD_DIR}/../Dependency/vcpkg/packages/gperftools_x64-linux/tools/gperftools/bin/
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
buildConfigs=(
  "Debug"
  "RelWithDebInfo"
  "Release"
  )
CMAKE_SRC_DIR=${CWD_DIR}
for config in "${buildConfigs[@]}"; do

    # if BuildConfig exist and config not equal BuildConfig, continue
    if [ -n "${BuildConfig}" ] && [ "${BuildConfig}" != "${config}" ]; then
        echo "Skip ${config} because BuildConfig is ${BuildConfig}"
        continue
    fi

    CMAKE_BUILD_TYPE=${config}
    CMAKE_BUILD_DIR=${CWD_DIR}/Build/${CMAKE_BUILD_TYPE}
    CMAKE_BINARY_DIR=${CWD_DIR}/Binary/${CMAKE_BUILD_TYPE}

    if [ ${RunClean} -eq 1 ]; then
        rm -rf ${CMAKE_BINARY_DIR}
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
    cbuild_param="-j ${Jobs}"
    if [ ${Verbose} -eq 1 ]; then
        cbuild_param+="--verbose"
    fi
    if [ -n "${BuildTarget}" ]; then
        cbuild_param+="--target ${BuildTarget} ${cbuild_param}"
    fi
    if [ ${RunRebuild} -eq 1 ]; then
        cbuild_param1="--clean-first ${cbuild_param}"
        # re构建项目
        build_cmd="${CMAKE_BIN} --build . ${cbuild_param1}"
        echo_color 34 "Rebuild cmd: ${build_cmd}"
        output_log="${CWD_DIR}/rebuild_${config}.log"
        (${build_cmd} || exit 1) | tee "${output_log}" 2>&1
        if grep -q "error generated" "${output_log}" || grep -q "errors generated" "${output_log}"; then
            echo_color 31 "Rebuilding Config ${config} failed, checkout ${output_log}, rebuild command ${build_cmd}"
            bash -c "code ${output_log}" &
        fi
    fi
    if [ ${RunClean} -eq 1 ]; then
        cbuild_param1="--target clean ${cbuild_param}"
        # clean构建项目
        build_cmd="${CMAKE_BIN} --build . ${cbuild_param1}"
        echo_color 34 "Clean cmd: ${build_cmd}"
        output_log="${CWD_DIR}/clean_${config}.log"
        (${build_cmd} || exit 1) | tee "${output_log}" 2>&1
        if grep -q "error generated" "${output_log}" || grep -q "errors generated" "${output_log}"; then
            echo_color 31 "Cleaning Config ${config} failed, checkout ${output_log}, clean command ${build_cmd}"
            bash -c "code ${output_log}" &
        fi
    fi

    # 构建项目
    build_cmd="${CMAKE_BIN} --build . ${cbuild_param}"
    echo_color 34 "Build cmd: ${build_cmd}"
    output_log="${CWD_DIR}/build_${config}.log"
    (${build_cmd} || exit 1) | tee "${output_log}" 2>&1
    if grep -q "error generated" "${output_log}" || grep -q "errors generated" "${output_log}"; then
        echo_color 31 "Building Config ${config} failed, checkout ${output_log}, build command ${build_cmd}"
        bash -c "code ${output_log}" &
    fi

    # 测试项目
    if [ ${RunTest} -eq 1 ]; then
      mkdir -p "${CWD_DIR}/Testing/"
      CMAKE_BIN_DIR=${CWD_DIR}/Binary/${CMAKE_BUILD_TYPE}
      echo_color 32 "Testing ${config} at ${CMAKE_BIN_DIR}"

#      !!!Don't use Use ctest -j N (very buggy, generate strange errors for mallocator and parallel test that cannot reproduce)
#      ctest_param="--parallel ${Jobs} --output-on-failure --schedule-random"
#      ctest_param+=" --test_timeout 600  --repeat-until-fail 1"
#      if [ -n "${BuildTarget}" ]; then
#          # ctest run only one target
#          # https://stackoverflow.com/questions/54160415/running-only-one-single-test-with-cmake-make
#          ctest_param="-R ${BuildTarget} ${ctest_param}"
#      fi
#
#      # 测试项目
#      test_cmd="${CTEST_BIN} . ${ctest_param}"
#      echo_color 34 "Test cmd: ${test_cmd}"
#      (${test_cmd} || exit 1) | tee "${CWD_DIR}/build_test_${config}.log" 2>&1

      for ((test_index = 1 ; test_index <= TestRepeatNum ; test_index++ )); do
        echo_color 32 "Run test #${test_index} out of ${TestRepeatNum} repeats"
        # Maually run test in each bg job parallely
        pids=()
        max_jobs=4
        # Function to kill all background jobs
        kill_jobs() {
            for pid in "${pids[@]}"; do
                job=$pid
                echo_color 31 "Killing job $job"
                kill $job > /dev/null 2>&1 || (kill -9 $job > /dev/null 2>&1 &)
            done
            echo "All tests have been terminated."
            exit 0
        }
        # Set a trap to call kill_jobs on termination signals
        trap kill_jobs SIGINT SIGTERM
        test_cmds=()
        test_logs=()

        # scan test dir and parallel execute ctest in each subprocess
        for binary in ${CMAKE_BIN_DIR}/Test*; do
          # Check if file is a binary executable (skip directories and non-executable files)
          if [ ! -f "${binary}" ] || [ ! -x "${binary}" ]; then
            continue
          fi
          test_target=${binary#${CMAKE_BIN_DIR}/}
          ctest_param="--output-on-failure"
          ctest_param+=" --repeat-until-fail 1" # manually repeat test instead of let cmake repeat it
          if [ -n "${BuildTarget}" ]; then
              if [[ ${test_target} != "${BuildTarget}" ]]; then
                continue
              fi
          fi
          # To make ctest run only one target
          # https://stackoverflow.com/questions/54160415/running-only-one-single-test-with-cmake-make
          ctest_param="-R ${test_target} ${ctest_param}"

          # 测试项目
          test_cmd="${CTEST_BIN} . ${ctest_param}"
          echo_color 34 "Test cmd: ${test_cmd}"
          output_log="${CWD_DIR}/Testing/build_test_${config}_${test_target}.log"
          cmd="(${test_cmd} || exit 1) | tee "${output_log}" 2>&1"
          # execute command in background
          timeout 120 bash -c "${cmd}" &
          # Add PID to array
          job=$!
          echo_color 34 "Testing ${test_target} pid: ${job}"
          pids+=(${job})
          test_logs+=("${output_log}")
          test_cmds+=("${cmd}")
          if [ ${#pids[@]} -ge ${max_jobs} ]; then
              # Wait all tests finish
              wait
              # reset pids
              pids=()
          fi
        done
        # Wait all tests finish
        wait

        test_all_success=1
        for i in "${!test_logs[@]}"; do
            test_cmd=${test_cmds[$i]}
            test_log=${test_logs[$i]}
            test_target=${test_log#${CWD_DIR}/Testing/build_test_${config}_}
            test_target=${test_target%.log}
            # If test log does not contain "100% tests passed, 0 tests failed ", output error log
            if ! grep -q "100% tests passed, 0 tests failed" "${test_log}"; then
                echo_color 31 "Testing Config ${config} Target ${test_target} failed, checkout ${test_log}, test command ${test_cmd}"
                bash -c "code ${test_log}" &
                test_all_success=0
            fi
        done
        if [ ${test_all_success} -eq 0 ]; then
            echo_color 31 "Testing Config ${config} failed, log at ${CWD_DIR}/Testing/build_test_${config}_*.log"
            exit 1
        #else
            #echo_color 32 "Testing Config ${config} success, log at ${CWD_DIR}/Testing/build_test_${config}_*.log"
        fi
      done
    fi

    # 性能测试
    if [ ${RunGPerf} -eq 1 ]; then
        mkdir -p "${CWD_DIR}/Testing/"
        CMAKE_BIN_DIR=${CWD_DIR}/Binary/${CMAKE_BUILD_TYPE}
        echo_color 32 "Perf test at ${CMAKE_BIN_DIR}"

        # 进入bin目录
        cd "${CMAKE_BIN_DIR}" || exit 1

        TEST_LOG="${CWD_DIR}/Testing/perf_test_${config}.log"
        echo_color 32 "Start testing " | tee "${TEST_LOG}"
        for binary in ./Test*; do
            # Check if file is a binary executable (skip directories and non-executable files)
            if [ ! -f "${binary}" ] || [ ! -x "${binary}" ]; then
              continue
            fi

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
                | tee -a "${TEST_LOG}" 2>&1
            fi
        done
    fi
done

echo_color 32 "\n\nFinished Build.sh ... exiting"
}