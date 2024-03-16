#!/bin/bash

eval "$('conda' 'shell.bash' 'hook')"
conda activate hlvm

ROOT_DIR=$(pwd)

cd pycmake || exit
./install_wheel.sh
cd "$ROOT_DIR" || exit