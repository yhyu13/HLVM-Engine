#!/bin/bash

# vcpkg elfutils prerequisites
sudo apt-get install flex bison autoconf autopoint -y

# vcpkg Boost -python3
sudo apt-get install autoconf automake autoconf-archive -y

# vcpkg gperftools - graphviz used to create svg
# for libtool, see answer here https://stackoverflow.com/a/68425747/6658943
sudo apt-get install graphviz libtool -y