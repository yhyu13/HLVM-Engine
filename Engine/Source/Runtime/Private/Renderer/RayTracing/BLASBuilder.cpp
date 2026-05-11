/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * BLASBuilder - Bottom Level Acceleration Structure Builder Implementation
 */

#include "BLASBuilder.h"
#include "Core/Log.h"
#include "Definition/TypeDefinition.h"
#include <nvrhi/utils.h>

DECLARE_LOG_CATEGORY(LogBLASBuilder);

nvrhi::rt::AccelStructHandle BLASBuilder::Build(
    nvrhi::IDevice* Device,
    nvrhi::ICommandList* CommandList,
    const FStaticMesh& Mesh)
{
    if (!Device || !CommandList)
    {
        HLVM_LOG(LogBLASBuilder, err, TXT("BLASBuilder::Build: Invalid device or command list"));
        return nullptr;
    }

    if (!Mesh.IsValid())
    {
        HLVM_LOG(LogBLASBuilder, err, TXT("BLASBuilder::Build: Invalid mesh data"));
        return nullptr;
    }

    // Pack vertices (only position and normal needed for BLAS)
    TVector<FPackedVertex> PackedVertices = PackVertices(Mesh);
    TVector<uint32_t> Indices = GetIndices(Mesh);

    const uint64_t VertexBufferSize = PackedVertices.size() * sizeof(FPackedVertex);
    const uint64_t IndexBufferSize = Indices.size() * sizeof(uint32_t);

    // Create vertex buffer for BLAS
    nvrhi::BufferDesc VBDesc;
    VBDesc.byteSize = VertexBufferSize;
    VBDesc.isAccelStructBuildInput = true;
    VBDesc.initialState = nvrhi::ResourceStates::ShaderResource;
    VBDesc.keepInitialState = true;
    VBDesc.debugName = "BLAS_VertexBuffer";

    nvrhi::BufferHandle VertexBuffer = Device->createBuffer(VBDesc);
    if (!VertexBuffer)
    {
        HLVM_LOG(LogBLASBuilder, err, TXT("BLASBuilder::Build: Failed to create vertex buffer"));
        return nullptr;
    }

    // Create index buffer for BLAS
    nvrhi::BufferDesc IBDesc;
    IBDesc.byteSize = IndexBufferSize;
    IBDesc.isAccelStructBuildInput = true;
    IBDesc.initialState = nvrhi::ResourceStates::ShaderResource;
    IBDesc.keepInitialState = true;
    IBDesc.debugName = "BLAS_IndexBuffer";

    nvrhi::BufferHandle IndexBuffer = Device->createBuffer(IBDesc);
    if (!IndexBuffer)
    {
        HLVM_LOG(LogBLASBuilder, err, TXT("BLASBuilder::Build: Failed to create index buffer"));
        return nullptr;
    }

    // Write data to buffers
    CommandList->writeBuffer(VertexBuffer, PackedVertices.data(), VertexBufferSize);
    CommandList->writeBuffer(IndexBuffer, Indices.data(), IndexBufferSize);

    // Create BLAS description
    nvrhi::rt::AccelStructDesc BlasDesc;
    BlasDesc.isTopLevel = false;

    nvrhi::rt::GeometryDesc GeometryDesc;
    auto& Triangles = GeometryDesc.geometryData.triangles;

    Triangles.indexBuffer = IndexBuffer;
    Triangles.vertexBuffer = VertexBuffer;
    Triangles.indexFormat = nvrhi::Format::R32_UINT;
    Triangles.indexCount = static_cast<uint32_t>(Indices.size());
    Triangles.vertexFormat = nvrhi::Format::RGB32_FLOAT;
    Triangles.vertexStride = sizeof(FPackedVertex);
    Triangles.vertexCount = static_cast<uint32_t>(PackedVertices.size());

    GeometryDesc.geometryType = nvrhi::rt::GeometryType::Triangles;
    GeometryDesc.flags = nvrhi::rt::GeometryFlags::Opaque;

    BlasDesc.bottomLevelGeometries.push_back(GeometryDesc);

    // Create and build acceleration structure
    nvrhi::rt::AccelStructHandle BottomLevelAS = Device->createAccelStruct(BlasDesc);
    if (!BottomLevelAS)
    {
        HLVM_LOG(LogBLASBuilder, err, TXT("BLASBuilder::Build: Failed to create acceleration structure"));
        return nullptr;
    }

    nvrhi::utils::BuildBottomLevelAccelStruct(CommandList, BottomLevelAS, BlasDesc);

    HLVM_LOG(LogBLASBuilder, info,
        TXT("BLASBuilder::Build: Built BLAS with {} vertices, {} indices"),
        PackedVertices.size(), Indices.size());

    return BottomLevelAS;
}

nvrhi::rt::AccelStructHandle BLASBuilder::Build(
    nvrhi::IDevice* Device,
    nvrhi::ICommandList* CommandList,
    const TVector<FStaticMesh>& Meshes)
{
    if (!Device || !CommandList)
    {
        HLVM_LOG(LogBLASBuilder, err, TXT("BLASBuilder::Build: Invalid device or command list"));
        return nullptr;
    }

    if (Meshes.empty())
    {
        HLVM_LOG(LogBLASBuilder, err, TXT("BLASBuilder::Build: Empty mesh vector"));
        return nullptr;
    }

    // Build BLAS with multiple geometries
    nvrhi::rt::AccelStructDesc BlasDesc;
    BlasDesc.isTopLevel = false;

    // Process each mesh
    for (const FStaticMesh& Mesh : Meshes)
    {
        if (!Mesh.IsValid())
        {
            continue;
        }

        nvrhi::rt::GeometryDesc GeometryDesc;
        auto& Triangles = GeometryDesc.geometryData.triangles;

        // For simplicity with multi-mesh BLAS, we create separate buffers per mesh
        // In production, you might want to pre-allocate and batch the data

        TVector<FPackedVertex> PackedVertices = PackVertices(Mesh);
        TVector<uint32_t> Indices = GetIndices(Mesh);

        const uint64_t VertexBufferSize = PackedVertices.size() * sizeof(FPackedVertex);
        const uint64_t IndexBufferSize = Indices.size() * sizeof(uint32_t);

        nvrhi::BufferDesc VBDesc;
        VBDesc.byteSize = VertexBufferSize;
        VBDesc.isAccelStructBuildInput = true;
        VBDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        VBDesc.keepInitialState = true;

        nvrhi::BufferHandle VertexBuffer = Device->createBuffer(VBDesc);

        nvrhi::BufferDesc IBDesc;
        IBDesc.byteSize = IndexBufferSize;
        IBDesc.isAccelStructBuildInput = true;
        IBDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        IBDesc.keepInitialState = true;

        nvrhi::BufferHandle IndexBuffer = Device->createBuffer(IBDesc);

        CommandList->writeBuffer(VertexBuffer, PackedVertices.data(), VertexBufferSize);
        CommandList->writeBuffer(IndexBuffer, Indices.data(), IndexBufferSize);

        Triangles.indexBuffer = IndexBuffer;
        Triangles.vertexBuffer = VertexBuffer;
        Triangles.indexFormat = nvrhi::Format::R32_UINT;
        Triangles.indexCount = static_cast<uint32_t>(Indices.size());
        Triangles.vertexFormat = nvrhi::Format::RGB32_FLOAT;
        Triangles.vertexStride = sizeof(FPackedVertex);
        Triangles.vertexCount = static_cast<uint32_t>(PackedVertices.size());

        GeometryDesc.geometryType = nvrhi::rt::GeometryType::Triangles;
        GeometryDesc.flags = nvrhi::rt::GeometryFlags::Opaque;

        BlasDesc.bottomLevelGeometries.push_back(GeometryDesc);
    }

    if (BlasDesc.bottomLevelGeometries.empty())
    {
        HLVM_LOG(LogBLASBuilder, err, TXT("BLASBuilder::Build: No valid geometries"));
        return nullptr;
    }

    nvrhi::rt::AccelStructHandle BottomLevelAS = Device->createAccelStruct(BlasDesc);
    if (!BottomLevelAS)
    {
        HLVM_LOG(LogBLASBuilder, err, TXT("BLASBuilder::Build: Failed to create acceleration structure"));
        return nullptr;
    }

    nvrhi::utils::BuildBottomLevelAccelStruct(CommandList, BottomLevelAS, BlasDesc);

    HLVM_LOG(LogBLASBuilder, info,
        TXT("BLASBuilder::Build: Built multi-mesh BLAS with {} geometries"),
        BlasDesc.bottomLevelGeometries.size());

    return BottomLevelAS;
}

TVector<BLASBuilder::FPackedVertex> BLASBuilder::PackVertices(const FStaticMesh& Mesh)
{
    TVector<FPackedVertex> PackedVertices;
    PackedVertices.reserve(Mesh.NumVertices());

    const TVector<FVertex>& Vertices = Mesh.GetVertices();
    for (const FVertex& Vertex : Vertices)
    {
        FPackedVertex Packed;
        Packed.Position[0] = Vertex.Position.x;
        Packed.Position[1] = Vertex.Position.y;
        Packed.Position[2] = Vertex.Position.z;
        Packed.Normal[0] = Vertex.Normal.x;
        Packed.Normal[1] = Vertex.Normal.y;
        Packed.Normal[2] = Vertex.Normal.z;
        PackedVertices.push_back(Packed);
    }

    return PackedVertices;
}

TVector<uint32_t> BLASBuilder::GetIndices(const FStaticMesh& Mesh)
{
    return Mesh.GetIndices();
}
