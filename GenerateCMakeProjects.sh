#!/bin/bash

eval "$('conda' 'shell.bash' 'hook')"
conda init && conda activate hlvm

#source .env # TODO : use .env to set env variables used by pycmake, currently pycmake use no env variables

pycmake --root_dir ./Engine/Source/