#!/bin/bash

eval "$('conda' 'shell.bash' 'hook')"

ROOT_DIR=$(pwd)

# install conda env
conda env create -f ./env.yaml
conda activate hlvm

# Setup
git submodule update --init --recursive

# vcpkg
cd ./Engine/Source/Dependency/vcpkg || exit
./bootstrap-vcpkg.sh
cd "$ROOT_DIR" || exit

# Execute all setup scripts
find ./Engine/ -name "Dependency" -type d -prune -o -name "Setup.sh" -print | while IFS= read -r file; do
    # Get the directory of the current file
    dir=$(dirname "$file")

    # Navigate to that directory
    cd "$dir" || { echo "Failed to cd into $dir"; continue; }

    # Execute the script with bash (assuming it is a bash script)
    if ! eval "./$(basename "$file")"; then
        echo "Script at '$file' failed with an error."
    fi

    # Return to the original working directory after execution
    cd "$ROOT_DIR" || exit
done