/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * BLASBuilder - Bottom Level Acceleration Structure Builder
 *
 * Builds BLAS from FStaticMesh geometry for ray tracing.
 */

#pragma once

#include "Renderer/Mesh/StaticMesh.h"
#include <nvrhi/nvrhi.h>
#include <memory>

/**
 * @brief Builder class for creating Bottom Level Acceleration Structures (BLAS)
 *
 * Converts FStaticMesh vertex/index data to NVRHI acceleration structure format.
 * The BLAS contains geometry data used in ray tracing queries.
 */
class BLASBuilder
{
public:
    BLASBuilder() = default;
    ~BLASBuilder() = default;

    NOCOPYMOVE(BLASBuilder);

    /**
     * Build a Bottom Level Acceleration Structure from static mesh geometry
     *
     * @param Device NVRHI device
     * @param CommandList Command list to record BLAS build commands
     * @param Mesh Static mesh to build BLAS from
     * @return Built acceleration structure handle, nullptr on failure
     */
    static nvrhi::rt::AccelStructHandle Build(
        nvrhi::IDevice* Device,
        nvrhi::ICommandList* CommandList,
        const FStaticMesh& Mesh);

    /**
     * Build a Bottom Level Acceleration Structure with multiple meshes
     *
     * @param Device NVRHI device
     * @param CommandList Command list to record BLAS build commands
     * @param Meshes Vector of static meshes to combine in BLAS
     * @return Built acceleration structure handle, nullptr on failure
     */
    static nvrhi::rt::AccelStructHandle Build(
        nvrhi::IDevice* Device,
        nvrhi::ICommandList* CommandList,
        const TVector<FStaticMesh>& Meshes);

private:
    /**
     * Convert FVertex to packed format suitable for BLAS
     * Extracts position and normal as float arrays
     */
    struct FPackedVertex
    {
        float Position[3];
        float Normal[3];
    };

    /**
     * Convert mesh vertices to packed format
     */
    static TVector<FPackedVertex> PackVertices(const FStaticMesh& Mesh);

    /**
     * Convert mesh indices to uint32 array
     */
    static TVector<uint32_t> GetIndices(const FStaticMesh& Mesh);
};
