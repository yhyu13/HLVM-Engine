#!/bin/bash

# vcpkg elfutils prerequisites
sudo apt-get install flex bison autoconf autopoint -y

# Boost
sudo apt-get install autoconf automake autoconf-archive -y

# graphviz used by gperftools to visualize svg
sudo apt-get install graphviz -y