/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 *
 *  Buffer Implementation
 */

#include "Renderer/RHI/Object/Buffer.h"

/*-----------------------------------------------------------------------------
	Buffer Implementation - Base Class
-----------------------------------------------------------------------------*/

IBuffer::IBuffer()
	: BufferSize(0), Device(nullptr)
{
}

IBuffer::~IBuffer()
{
	BufferHandle.Reset();
}

void IBuffer::SetDebugName(const TCHAR* Name)
{
	DebugName = Name;
}

/*-----------------------------------------------------------------------------
	FVertexBuffer - Base Class Implementation
-----------------------------------------------------------------------------*/

FVertexBuffer::FVertexBuffer()
{
	Type = EBufferType::Vertex;
}

/*-----------------------------------------------------------------------------
	FStaticVertexBuffer Implementation
-----------------------------------------------------------------------------*/

FStaticVertexBuffer::FStaticVertexBuffer()
{
	Usage = EBufferUsage::Static;
}

FStaticVertexBuffer::~FStaticVertexBuffer()
{
	// Log
	if (!DebugName.IsEmpty())
	{
		HLVM_LOG(LogRHI, debug, TXT("FStaticVertexBuffer::~FStaticVertexBuffer for {}"), *DebugName);
	}
}

bool FStaticVertexBuffer::Initialize(
	nvrhi::ICommandList* CommandList,
	nvrhi::IDevice*		 InDevice,
	const void*			 VertexData,
	size_t				 VertexDataSize)
{
	HLVM_ENSURE_F(!BufferHandle, TXT("Buffer already initialized"));
	HLVM_ENSURE_F(InDevice, TXT("Device is null"));
	HLVM_ENSURE_F(VertexData, TXT("Vertex data is null"));
	HLVM_ENSURE_F(VertexDataSize > 0, TXT("Vertex data size is zero"));
	HLVM_ENSURE_F(CommandList, TXT("CommandList is null for static buffer upload"));

	Device = InDevice;
	BufferSize = VertexDataSize;

	nvrhi::BufferDesc Desc;
	Desc.setByteSize(VertexDataSize);
	Desc.setInitialState(nvrhi::ResourceStates::CopyDest);
	Desc.setCanHaveUAVs(false);
	Desc.setIsVertexBuffer(true);

	BufferHandle = Device->createBuffer(Desc);
	HLVM_ENSURE_F(BufferHandle, TXT("Failed to create static vertex buffer"));

	CommandList->beginTrackingBufferState(BufferHandle, nvrhi::ResourceStates::CopyDest);
	CommandList->writeBuffer(BufferHandle, VertexData, VertexDataSize);
	CommandList->setPermanentBufferState(BufferHandle, nvrhi::ResourceStates::VertexBuffer);

	return true;
}

/*-----------------------------------------------------------------------------
	FDynamicVertexBuffer Implementation
-----------------------------------------------------------------------------*/

FDynamicVertexBuffer::FDynamicVertexBuffer()
	: bIsMapped(false)
{
	Usage = EBufferUsage::Dynamic;
}

FDynamicVertexBuffer::~FDynamicVertexBuffer()
{
	if (bIsMapped)
	{
		Unmap();
	}
	// Log
	if (!DebugName.IsEmpty())
	{
		HLVM_LOG(LogRHI, debug, TXT("FDynamicVertexBuffer::~FDynamicVertexBuffer for {}"), *DebugName);
	}
}

bool FDynamicVertexBuffer::Initialize(
	nvrhi::IDevice* InDevice,
	size_t			InBufferSize)
{
	HLVM_ENSURE_F(!BufferHandle, TXT("Buffer already initialized"));
	HLVM_ENSURE_F(InDevice, TXT("Device is null"));
	HLVM_ENSURE_F(InBufferSize > 0, TXT("Buffer size is zero"));

	Device = InDevice;
	BufferSize = InBufferSize;

	nvrhi::BufferDesc Desc;
	Desc.setByteSize(InBufferSize);
	Desc.setInitialState(nvrhi::ResourceStates::VertexBuffer);
	Desc.setCanHaveUAVs(false);
	Desc.setIsVertexBuffer(true);
	Desc.setCpuAccess(nvrhi::CpuAccessMode::Write);

	BufferHandle = Device->createBuffer(Desc);
	HLVM_ENSURE_F(BufferHandle, TXT("Failed to create dynamic vertex buffer"));

	return true;
}

void* FDynamicVertexBuffer::Map(nvrhi::CpuAccessMode AccessMode)
{
	HLVM_ENSURE_F(BufferHandle, TXT("Buffer not initialized"));
	HLVM_ENSURE_F(!bIsMapped, TXT("Buffer already mapped"));

	void* MappedData = Device->mapBuffer(BufferHandle, AccessMode);
	if (MappedData)
	{
		bIsMapped = true;
	}
	return MappedData;
}

void FDynamicVertexBuffer::Unmap()
{
	HLVM_ENSURE_F(BufferHandle, TXT("Buffer not initialized"));
	HLVM_ENSURE_F(bIsMapped, TXT("Buffer not mapped"));

	Device->unmapBuffer(BufferHandle);
	bIsMapped = false;
}

void FDynamicVertexBuffer::Update(
	nvrhi::ICommandList* CommandList,
	const void*			 Data,
	size_t				 DataSize,
	size_t				 DstOffset)
{
	HLVM_ENSURE_F(BufferHandle, TXT("Buffer not initialized"));
	HLVM_ENSURE_F(Data, TXT("Data is null"));
	HLVM_ENSURE_F(DstOffset + DataSize <= BufferSize, TXT("Update exceeds buffer size"));
	HLVM_ENSURE_F(!IsMapped(), TXT("Buffer already mapped"));

	if (CommandList)
	{
		CommandList->writeBuffer(BufferHandle, Data, DataSize, DstOffset);
	}
	else
	{
		void* MappedData = Map(nvrhi::CpuAccessMode::Write);
		if (MappedData)
		{
			memcpy(static_cast<char*>(MappedData) + DstOffset, Data, DataSize);
			Unmap();
		}
	}
}

/*-----------------------------------------------------------------------------
	FIndexBuffer - Base Class Implementation
-----------------------------------------------------------------------------*/

FIndexBuffer::FIndexBuffer()
	: IndexFormat(nvrhi::Format::R32_UINT)
{
	Type = EBufferType::Index;
}

/*-----------------------------------------------------------------------------
	FStaticIndexBuffer Implementation
-----------------------------------------------------------------------------*/

FStaticIndexBuffer::FStaticIndexBuffer()
{
	Usage = EBufferUsage::Static;
}

FStaticIndexBuffer::~FStaticIndexBuffer()
{
	// Log
	if (!DebugName.IsEmpty())
	{
		HLVM_LOG(LogRHI, debug, TXT("FStaticIndexBuffer::~FStaticIndexBuffer for {}"), *DebugName);
	}
}

bool FStaticIndexBuffer::Initialize(
	nvrhi::ICommandList* CommandList,
	nvrhi::IDevice*		 InDevice,
	const void*			 IndexData,
	size_t				 IndexDataSize,
	nvrhi::Format		 InIndexFormat)
{
	HLVM_ENSURE_F(!BufferHandle, TXT("Buffer already initialized"));
	HLVM_ENSURE_F(InDevice, TXT("Device is null"));
	HLVM_ENSURE_F(IndexData, TXT("Index data is null"));
	HLVM_ENSURE_F(IndexDataSize > 0, TXT("Index data size is zero"));
	HLVM_ENSURE_F(InIndexFormat == nvrhi::Format::R16_UINT || InIndexFormat == nvrhi::Format::R32_UINT,
		TXT("Invalid index format, must be R16_UINT or R32_UINT"));
	HLVM_ENSURE_F(CommandList, TXT("CommandList is null for static buffer upload"));

	Device = InDevice;
	BufferSize = IndexDataSize;
	IndexFormat = InIndexFormat;

	nvrhi::BufferDesc Desc;
	Desc.setByteSize(IndexDataSize);
	Desc.setInitialState(nvrhi::ResourceStates::CopyDest);
	Desc.setCanHaveUAVs(false);
	Desc.setIsIndexBuffer(true);
	Desc.setFormat(InIndexFormat);

	BufferHandle = Device->createBuffer(Desc);
	HLVM_ENSURE_F(BufferHandle, TXT("Failed to create static index buffer"));

	CommandList->beginTrackingBufferState(BufferHandle, nvrhi::ResourceStates::CopyDest);
	CommandList->writeBuffer(BufferHandle, IndexData, IndexDataSize);
	CommandList->setPermanentBufferState(BufferHandle, nvrhi::ResourceStates::IndexBuffer);

	return true;
}

/*-----------------------------------------------------------------------------
	FDynamicIndexBuffer Implementation
-----------------------------------------------------------------------------*/

FDynamicIndexBuffer::FDynamicIndexBuffer()
	: bIsMapped(false)
{
	Usage = EBufferUsage::Dynamic;
}

FDynamicIndexBuffer::~FDynamicIndexBuffer()
{
	if (bIsMapped)
	{
		Unmap();
	}
	// Log
	if (!DebugName.IsEmpty())
	{
		HLVM_LOG(LogRHI, debug, TXT("FDynamicIndexBuffer::~FDynamicIndexBuffer for {}"), *DebugName);
	}
}

bool FDynamicIndexBuffer::Initialize(
	nvrhi::IDevice* InDevice,
	size_t			InBufferSize,
	nvrhi::Format	InIndexFormat)
{
	HLVM_ENSURE_F(!BufferHandle, TXT("Buffer already initialized"));
	HLVM_ENSURE_F(InDevice, TXT("Device is null"));
	HLVM_ENSURE_F(InBufferSize > 0, TXT("Buffer size is zero"));
	HLVM_ENSURE_F(InIndexFormat == nvrhi::Format::R16_UINT || InIndexFormat == nvrhi::Format::R32_UINT,
		TXT("Invalid index format, must be R16_UINT or R32_UINT"));

	Device = InDevice;
	BufferSize = InBufferSize;
	IndexFormat = InIndexFormat;

	nvrhi::BufferDesc Desc;
	Desc.setByteSize(InBufferSize);
	Desc.setInitialState(nvrhi::ResourceStates::IndexBuffer);
	Desc.setCanHaveUAVs(false);
	Desc.setIsIndexBuffer(true);
	Desc.setFormat(InIndexFormat);
	Desc.setCpuAccess(nvrhi::CpuAccessMode::Write);

	BufferHandle = Device->createBuffer(Desc);
	HLVM_ENSURE_F(BufferHandle, TXT("Failed to create dynamic index buffer"));

	return true;
}

void* FDynamicIndexBuffer::Map(nvrhi::CpuAccessMode AccessMode)
{
	HLVM_ENSURE_F(BufferHandle, TXT("Buffer not initialized"));
	HLVM_ENSURE_F(!bIsMapped, TXT("Buffer already mapped"));

	void* MappedData = Device->mapBuffer(BufferHandle, AccessMode);
	if (MappedData)
	{
		bIsMapped = true;
	}
	return MappedData;
}

void FDynamicIndexBuffer::Unmap()
{
	HLVM_ENSURE_F(BufferHandle, TXT("Buffer not initialized"));
	HLVM_ENSURE_F(bIsMapped, TXT("Buffer not mapped"));

	Device->unmapBuffer(BufferHandle);
	bIsMapped = false;
}

void FDynamicIndexBuffer::Update(
	nvrhi::ICommandList* CommandList,
	const void*			 Data,
	size_t				 DataSize,
	size_t				 DstOffset)
{
	HLVM_ENSURE_F(BufferHandle, TXT("Buffer not initialized"));
	HLVM_ENSURE_F(Data, TXT("Data is null"));
	HLVM_ENSURE_F(DstOffset + DataSize <= BufferSize, TXT("Update exceeds buffer size"));
	HLVM_ENSURE_F(!IsMapped(), TXT("Buffer already mapped"));

	if (CommandList)
	{
		CommandList->writeBuffer(BufferHandle, Data, DataSize, DstOffset);
	}
	else
	{
		void* MappedData = Map(nvrhi::CpuAccessMode::Write);
		if (MappedData)
		{
			memcpy(static_cast<char*>(MappedData) + DstOffset, Data, DataSize);
			Unmap();
		}
	}
}
