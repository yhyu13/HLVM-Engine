#!/bin/bash

ROOT_DIR=$(pwd)

# Setup
git submodule update --init --recursive

# vcpkg
cd Engine/Source/Dependency/vcpkg
./bootstrap-vcpkg.sh
cd $ROOT_DIR

# install conda env
conda env create -f env.yaml