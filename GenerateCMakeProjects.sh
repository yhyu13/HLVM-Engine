#!/bin/bash

eval "$('conda' 'shell.bash' 'hook')"
conda activate hlvm

#source .env

pycmake --root_dir ./Engine/Source/