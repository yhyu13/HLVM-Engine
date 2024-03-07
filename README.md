# HLVM-Engine

## Brief

是工作中受UE启发的个人游戏引擎练习项目，在搭建游戏引擎关键基础设施的同时，额外目标是创造比UE5更友好更便捷的pak文件查读和内存管理、遥测系统，同时磨刀linux开发工具链和c++20特性熟练度，不涉及渲染动画特效等一系列游戏内容开发的工作流程，欢迎交流学习

## Build

### Linux-x64

#### Prerequisites:

配置暂时放在zhihu上，有时间再放到项目bundle在一起
https://zhuanlan.zhihu.com/p/677704467

- Anaconda3
- git
- clang-16
- cmake 3.28

然后
```
./Setup.sh
./GenerateCMakeProjects.sh
```

## Features

 - Build system using CMake
 - Unit testing with CTest
 - Platform-specific code for Linux and Windows
 - File system handling with Boost and custom implementations
 - Compression using Zstd
 - Encryption using RSA
 - Logging system
 - Custom memory management with Mimalloc allocators
 - Templated utility functions for common tasks
 - Obfuscation techniques for strings and expressions
 - Performance testing with ScopedTimer and Timer
 - Custom build and clean scripts
 - Custom preprocessor macros for platform-specific code
 - Custom string handling
 - Custom file system handling with packing support
 - Custom exception handling
 - Custom parallel processing with locking mechanisms
 - Boost hashing functions
 - Custom template meta-programming for common tasks
 - Debugging utilities for Linux and Windows
 - Work Steal thread pool