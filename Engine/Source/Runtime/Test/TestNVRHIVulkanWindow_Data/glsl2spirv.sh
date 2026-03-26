#!/bin/bash

glslangValidator -V --target-env vulkan1.0 -S vert -o vert.spv shader.vert
glslangValidator -V --target-env vulkan1.0 -S frag -o frag.spv shader.frag
glslangValidator -V --target-env vulkan1.0 -S geom -o geom.spv shader.geom