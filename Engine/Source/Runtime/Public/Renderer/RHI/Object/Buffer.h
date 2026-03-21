/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 *  Buffer Objects
 *
 *  Vertex and index buffer management using NVRHI.
 *  Separated into Static (GPU-only, writeBuffer) and Dynamic (CPU-visible, map/unmap) variants.
 */

#pragma once

#include "Renderer/RHI/RHICommon.h"
#include "Template/PointerTemplate.tpp"

HLVM_ENUM(EBufferUsage, TUINT8,
	Static,
	Dynamic);

HLVM_ENUM(EBufferType, TUINT8,
	Vertex,
	Index);

/*-----------------------------------------------------------------------------
	IBuffer - Base Buffer Class
-----------------------------------------------------------------------------*/

/**
 * Base Buffer
 */
class IBuffer
{
public:
	NOCOPYMOVE(IBuffer);
	virtual ~IBuffer();

	// Access
	[[nodiscard]] nvrhi::BufferHandle GetBufferHandle() const { return BufferHandle; }
	[[nodiscard]] size_t			  GetBufferSize() const { return BufferSize; }

	// Debug name
	void					   SetDebugName(const TCHAR* Name);
	[[nodiscard]] const TCHAR* GetDebugName() const { return DebugName.data(); }

	EBufferUsage GetUsage() const { return Usage; }
	EBufferType	 GetType() const { return Type; }

protected:
	IBuffer();

	nvrhi::BufferHandle	   BufferHandle;
	size_t				   BufferSize;
	TNNPtr<nvrhi::IDevice> Device;
	EBufferUsage		   Usage;
	EBufferType			   Type;
	TCharArray<32>		   DebugName;
};

/*-----------------------------------------------------------------------------
	FVertexBuffer - Base Vertex Buffer Class
-----------------------------------------------------------------------------*/

/**
 * Base vertex buffer class providing common functionality
 */
class FVertexBuffer : public IBuffer
{
public:
	NOCOPYMOVE(FVertexBuffer);
protected:
	FVertexBuffer();
};

/*-----------------------------------------------------------------------------
	FStaticVertexBuffer - Static Vertex Buffer (GPU-only, writeBuffer)
-----------------------------------------------------------------------------*/

/**
 * Static vertex buffer for immutable geometry data
 *
 * Characteristics:
 * - GPU-only memory (not CPU accessible)
 * - Data uploaded via CommandList->writeBuffer()
 * - Optimal performance for static meshes
 * - Cannot be efficiently updated after creation
 *
 * Usage:
 * 1. Create instance
 * 2. Call Initialize() with vertex data and command list
 * 3. Bind with CommandList->bindVertexBuffers()
 */
class FStaticVertexBuffer : public FVertexBuffer
{
public:
	NOCOPYMOVE(FStaticVertexBuffer);
	FStaticVertexBuffer();
	virtual ~FStaticVertexBuffer() override;

	bool Initialize(
		nvrhi::ICommandList* CommandList,
		nvrhi::IDevice*		 Device,
		const void*			 VertexData,
		size_t				 VertexDataSize);
};

/*-----------------------------------------------------------------------------
	FDynamicVertexBuffer - Dynamic Vertex Buffer (CPU-visible, map/unmap)
-----------------------------------------------------------------------------*/

/**
 * Dynamic vertex buffer for frequently updated geometry data
 *
 * Characteristics:
 * - CPU-visible memory (can map/unmap)
 * - Data uploaded via Device->mapBuffer()/unmapBuffer()
 * - Supports orphaning (discard previous contents for better performance)
 * - Slower GPU access but flexible for updates
 *
 * Usage:
 * 1. Create instance
 * 2. Call Initialize() with buffer size
 * 3. Update with Map()/Unmap() or Update()
 * 4. Bind with CommandList->bindVertexBuffers()
 */
class FDynamicVertexBuffer : public FVertexBuffer
{
public:
	NOCOPYMOVE(FDynamicVertexBuffer);
	FDynamicVertexBuffer();
	virtual ~FDynamicVertexBuffer() override;

	bool Initialize(
		nvrhi::IDevice* Device,
		size_t			BufferSize);

	void* Map(nvrhi::CpuAccessMode AccessMode = nvrhi::CpuAccessMode::Write);
	void  Unmap();

	void Update(
		nvrhi::ICommandList* CommandList,
		const void*			 Data,
		size_t				 DataSize,
		size_t				 DstOffset = 0);

	[[nodiscard]] bool IsMapped() const { return bIsMapped; }

protected:
	bool bIsMapped;
};

/*-----------------------------------------------------------------------------
	FIndexBuffer - Base Index Buffer Class
-----------------------------------------------------------------------------*/

/**
 * Base index buffer class providing common functionality
 */
class FIndexBuffer : public IBuffer
{
public:
	NOCOPYMOVE(FIndexBuffer);
protected:
	FIndexBuffer();

	nvrhi::Format		   IndexFormat;
};

/*-----------------------------------------------------------------------------
	FStaticIndexBuffer - Static Index Buffer (GPU-only, writeBuffer)
-----------------------------------------------------------------------------*/

/**
 * Static index buffer for immutable index data
 *
 * Characteristics:
 * - GPU-only memory (not CPU accessible)
 * - Data uploaded via CommandList->writeBuffer()
 * - Optimal performance for static meshes
 * - Cannot be efficiently updated after creation
 *
 * Usage:
 * 1. Create instance
 * 2. Call Initialize() with index data and command list
 * 3. Bind with CommandList->bindIndexBuffer()
 */
class FStaticIndexBuffer : public FIndexBuffer
{
public:
	NOCOPYMOVE(FStaticIndexBuffer);
	FStaticIndexBuffer();
	virtual ~FStaticIndexBuffer() override;

	bool Initialize(
		nvrhi::ICommandList* CommandList,
		nvrhi::IDevice*		 Device,
		const void*			 IndexData,
		size_t				 IndexDataSize,
		nvrhi::Format		 IndexFormat);
};

/*-----------------------------------------------------------------------------
	FDynamicIndexBuffer - Dynamic Index Buffer (CPU-visible, map/unmap)
-----------------------------------------------------------------------------*/

/**
 * Dynamic index buffer for frequently updated index data
 *
 * Characteristics:
 * - CPU-visible memory (can map/unmap)
 * - Data uploaded via Device->mapBuffer()/unmapBuffer()
 * - Supports orphaning (discard previous contents for better performance)
 * - Slower GPU access but flexible for updates
 *
 * Usage:
 * 1. Create instance
 * 2. Call Initialize() with buffer size and format
 * 3. Update with Map()/Unmap() or Update()
 * 4. Bind with CommandList->bindIndexBuffer()
 */
class FDynamicIndexBuffer : public FIndexBuffer
{
public:
	NOCOPYMOVE(FDynamicIndexBuffer);
	FDynamicIndexBuffer();
	virtual ~FDynamicIndexBuffer() override;

	bool Initialize(
		nvrhi::IDevice* Device,
		size_t			BufferSize,
		nvrhi::Format	IndexFormat);

	void* Map(nvrhi::CpuAccessMode AccessMode = nvrhi::CpuAccessMode::Write);
	void  Unmap();

	void Update(
		nvrhi::ICommandList* CommandList,
		const void*			 Data,
		size_t				 DataSize,
		size_t				 DstOffset = 0);

	[[nodiscard]] bool IsMapped() const { return bIsMapped; }

protected:
	bool bIsMapped;
};
