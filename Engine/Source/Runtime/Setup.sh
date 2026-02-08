#!/bin/bash

# vulkan prerequisites
sudo apt-get install libvulkan1 vulkan-sdk vulkan-tools vulkan-validationlayers spriv-tools spirv-cross
# YuHang : Vulkan deps will be handled by manually downlaoded Vulkan SDK tarball installation due to LunarG droped Ubuntu support
# And the fact that Ubuntu 20.04's highest Vulkan SDK version is 1.3.283 (2023/2024) which is to old in 2026
# e.g. VULKAN_SDK=/home/hangyu5/Documents/vulkansdk-linux-x86_64-1.4.328.1/1.4.328.1/x86_64

# glfw 3.4
sudo apt-get install libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev pkg-config