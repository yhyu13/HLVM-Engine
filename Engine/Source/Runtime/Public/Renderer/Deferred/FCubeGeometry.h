/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * FCubeGeometry - Cube geometry for deferred shading test.
 * Mirrors Donut's CubeGeometry.h exactly for 1:1 compatibility.
 * 
 * Note: Normals and tangents are packed as 4 SNORM8 values per uint32_t,
 * matching Donut's vectorToSnorm8() packing.
 */

#pragma once

#include "Math/MathGLM.h"
#include <cstdint>

// Vertex positions - 24 vertices for cube (4 per face, 6 faces)
static const FVec3 g_Positions[] = {
    // Front face
    { -0.5f,  0.5f, -0.5f },
    {  0.5f, -0.5f, -0.5f },
    { -0.5f, -0.5f, -0.5f },
    {  0.5f,  0.5f, -0.5f },

    // Right side face
    {  0.5f, -0.5f, -0.5f },
    {  0.5f,  0.5f,  0.5f },
    {  0.5f, -0.5f,  0.5f },
    {  0.5f,  0.5f, -0.5f },

    // Left side face
    { -0.5f,  0.5f,  0.5f },
    { -0.5f, -0.5f, -0.5f },
    { -0.5f, -0.5f,  0.5f },
    { -0.5f,  0.5f, -0.5f },

    // Back face
    {  0.5f,  0.5f,  0.5f },
    { -0.5f, -0.5f,  0.5f },
    {  0.5f, -0.5f,  0.5f },
    { -0.5f,  0.5f,  0.5f },

    // Top face
    { -0.5f,  0.5f, -0.5f },
    {  0.5f,  0.5f,  0.5f },
    {  0.5f,  0.5f, -0.5f },
    { -0.5f,  0.5f,  0.5f },

    // Bottom face
    {  0.5f, -0.5f,  0.5f },
    { -0.5f, -0.5f, -0.5f },
    {  0.5f, -0.5f, -0.5f },
    { -0.5f, -0.5f,  0.5f },
};

// UV coordinates - 24 vertices for cube
static const FVec2 g_TexCoords[] = {
    // Front face
    { 0.0f, 0.0f },
    { 1.0f, 1.0f },
    { 0.0f, 1.0f },
    { 1.0f, 0.0f },

    // Right side face
    { 0.0f, 1.0f },
    { 1.0f, 0.0f },
    { 1.0f, 1.0f },
    { 0.0f, 0.0f },

    // Left side face
    { 0.0f, 0.0f },
    { 1.0f, 1.0f },
    { 0.0f, 1.0f },
    { 1.0f, 0.0f },

    // Back face
    { 0.0f, 0.0f },
    { 1.0f, 1.0f },
    { 0.0f, 1.0f },
    { 1.0f, 0.0f },

    // Top face
    { 0.0f, 1.0f },
    { 1.0f, 0.0f },
    { 1.0f, 1.0f },
    { 0.0f, 0.0f },

    // Bottom face
    { 1.0f, 1.0f },
    { 0.0f, 0.0f },
    { 1.0f, 0.0f },
    { 0.0f, 1.0f },
};

// Normals packed as 4 SNORM8 values per uint32_t (matches Donut's vectorToSnorm8)
// Format: byte[0]=x, byte[1]=y, byte[2]=z, byte[3]=w
// Front face: normal (0, 0, -1) → z=-1.0 maps to snorm8 value -128 = 0x80
static const uint32_t g_Normals[] = {
    // Front face (0, 0, -1)
    0x00008000, 0x00008000, 0x00008000, 0x00008000,
    // Right side face (1, 0, 0)
    0x00007F7F, 0x00007F7F, 0x00007F7F, 0x00007F7F,
    // Left side face (-1, 0, 0)
    0x00008080, 0x00008080, 0x00008080, 0x00008080,
    // Back face (0, 0, 1)
    0x00007F00, 0x00007F00, 0x00007F00, 0x00007F00,
    // Top face (0, 1, 0) → (0, 127, 0, 0) = 0x00007F00
    0x00007F00, 0x00007F00, 0x00007F00, 0x00007F00,
    // Bottom face (0, -1, 0) → (0, -128, 0, 0) = 0x00008000
    0x00008000, 0x00008000, 0x00008000, 0x00008000,
};

// Tangents packed as 4 SNORM8 values per uint32_t (matches Donut's vectorToSnorm8)
// Front face: tangent (1, 0, 0, 1) → x=1.0 → 127, w=1.0 → 1
static const uint32_t g_Tangents[] = {
    // Front face (1, 0, 0, 1)
    0x01007F7F, 0x01007F7F, 0x01007F7F, 0x01007F7F,
    // Right side face (0, 0, 1, 1) → (0, 0, 127, 1) = 0x01007F00
    0x01007F00, 0x01007F00, 0x01007F00, 0x01007F00,
    // Left side face (0, 0, -1, 1) → (0, 0, -128, 1) = 0x01008000
    0x01008000, 0x01008000, 0x01008000, 0x01008000,
    // Back face (-1, 0, 0, 1) → (-128, 0, 0, 1) = 0x01008080
    0x01008080, 0x01008080, 0x01008080, 0x01008080,
    // Top face (1, 0, 0, 1)
    0x01007F7F, 0x01007F7F, 0x01007F7F, 0x01007F7F,
    // Bottom face (1, 0, 0, 1)
    0x01007F7F, 0x01007F7F, 0x01007F7F, 0x01007F7F,
};

// Index buffer - 36 indices for 12 triangles (2 per face)
static const uint32_t g_Indices[] = {
     0,  1,  2,   0,  3,  1,  // front face
     4,  5,  6,   4,  7,  5,  // right face
     8,  9, 10,   8, 11,  9,  // left face
    12, 13, 14,  12, 15, 13,  // back face
    16, 17, 18,  16, 19, 17,  // top face
    20, 21, 22,  20, 23, 21,  // bottom face
};

constexpr size_t g_NumVertices = sizeof(g_Positions) / sizeof(FVec3);
constexpr size_t g_NumIndices = sizeof(g_Indices) / sizeof(uint32_t);

// Compute byte sizes for buffer creation
constexpr size_t g_PositionsSize = sizeof(g_Positions);
constexpr size_t g_TexCoordsSize = sizeof(g_TexCoords);
constexpr size_t g_NormalsSize = sizeof(g_Normals);
constexpr size_t g_TangentsSize = sizeof(g_Tangents);
constexpr size_t g_IndicesSize = sizeof(g_Indices);
