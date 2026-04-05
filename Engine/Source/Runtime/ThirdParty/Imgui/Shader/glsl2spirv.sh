#!/bin/bash

# Non-MVP shaders (Donut-style with scale/translate push constants)
glslangValidator -V --target-env vulkan1.0 -S vert -o imgui_vertex.spv imgui.vert
glslangValidator -V --target-env vulkan1.0 -S frag -o imgui_fragment.spv imgui.frag

# MVP shaders (full MVP matrix push constants)
glslangValidator -V --target-env vulkan1.0 -S vert -o imgui_vertex_mvp.spv imgui_vertex_mvp.vert
glslangValidator -V --target-env vulkan1.0 -S frag -o imgui_fragment_mvp.spv imgui_fragment_mvp.frag