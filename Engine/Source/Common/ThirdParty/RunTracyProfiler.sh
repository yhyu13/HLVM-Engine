#!/bin/bash

# CWD_DIR is the same as script's directory
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
CWD_DIR=$SCRIPT_DIR
REPO_DIR=${CWD_DIR}/../../../..

cd ${REPO_DIR}/Engine/Source/Common/Build/Release
${REPO_DIR}/Binary/GNULinux-x64/Ninja-1.12.0/ninja -v -j 46 tracy-profiler
cd ${REPO_DIR}/Engine/Source/Common/Binary/Release
./tracy-profiler