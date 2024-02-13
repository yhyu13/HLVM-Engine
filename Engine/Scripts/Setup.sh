#!/bin/bash

ROOT_DIR=$(pwd)

cd pycmake || exit
./install_wheel.sh
cd "$ROOT_DIR" || exit