#!/bin/bash

glslangValidator -V --target-env vulkan1.0 -S vert -o Cube.vert.spv Cube.vert
glslangValidator -V --target-env vulkan1.0 -S frag -o Cube.frag.spv Cube.frag
glslangValidator -V --target-env vulkan1.0 -S vert -o Plane.vert.spv Plane.vert
glslangValidator -V --target-env vulkan1.0 -S frag -o Plane.frag.spv Plane.frag