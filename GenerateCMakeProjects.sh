#!/bin/bash

eval "$('conda' 'shell.bash' 'hook')"
conda init && conda activate hlvm

#source .env

pycmake --root_dir ./Engine/Source/