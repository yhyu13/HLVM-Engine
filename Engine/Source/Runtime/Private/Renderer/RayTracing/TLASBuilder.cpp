/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * TLASBuilder - Top Level Acceleration Structure Builder
 */

#include "Renderer/RayTracing/TLASBuilder.h"
#include "Core/Log.h"

DECLARE_LOG_CATEGORY(LogTLASBuilder)

bool FTLASBuilder::Initialize(nvrhi::IDevice* InDevice, std::uint32_t InMaxInstances)
{
	if (!InDevice)
	{
		HLVM_LOG(LogTLASBuilder, err, TXT("FTLASBuilder: Cannot initialize with null device"));
		return false;
	}

	if (InMaxInstances == 0)
	{
		HLVM_LOG(LogTLASBuilder, err, TXT("FTLASBuilder: Cannot initialize with zero max instances"));
		return false;
	}

	Device          = InDevice;
	MaxInstances    = InMaxInstances;
	bIsInitialized  = true;

	HLVM_LOG(LogTLASBuilder, trace, TXT("FTLASBuilder: Initialized with max instances {}"), MaxInstances);

	return true;
}

bool FTLASBuilder::AddInstance(const FInstanceDesc& Instance)
{
	if (!bIsInitialized)
	{
		HLVM_LOG(LogTLASBuilder, err, TXT("FTLASBuilder: Cannot add instance - not initialized"));
		return false;
	}

	if (!Instance.BottomLevelAS)
	{
		HLVM_LOG(LogTLASBuilder, err, TXT("FTLASBuilder: Cannot add instance with null BLAS"));
		return false;
	}

	if (Instances.size() >= MaxInstances)
	{
		HLVM_LOG(LogTLASBuilder, err, TXT("FTLASBuilder: Cannot add instance - max instances ({}) reached"), MaxInstances);
		return false;
	}

	Instances.push_back(Instance);
	HLVM_LOG(LogTLASBuilder, trace, TXT("FTLASBuilder: Added instance, total instances now {}"), Instances.size());

	return true;
}

bool FTLASBuilder::Build(nvrhi::ICommandList* CommandList)
{
	if (!bIsInitialized)
	{
		HLVM_LOG(LogTLASBuilder, err, TXT("FTLASBuilder: Cannot build - not initialized"));
		return false;
	}

	if (!CommandList)
	{
		HLVM_LOG(LogTLASBuilder, err, TXT("FTLASBuilder: Cannot build with null command list"));
		return false;
	}

	if (Instances.empty())
	{
		HLVM_LOG(LogTLASBuilder, err, TXT("FTLASBuilder: Cannot build - no instances added"));
		return false;
	}

	// Create TLAS if not already created
	if (!TLas)
	{
		nvrhi::rt::AccelStructDesc TlasDesc;
		TlasDesc.isTopLevel           = true;
		TlasDesc.topLevelMaxInstances = MaxInstances;

		TLas = Device->createAccelStruct(TlasDesc);
		if (!TLas)
		{
			HLVM_LOG(LogTLASBuilder, err, TXT("FTLASBuilder: Failed to create TLAS"));
			return false;
		}

		HLVM_LOG(LogTLASBuilder, trace, TXT("FTLASBuilder: Created TLAS with max {} instances"), MaxInstances);
	}

	// Convert instance descriptors to NVRHI format
	std::vector<nvrhi::rt::InstanceDesc> NvrhiInstances;
	NvrhiInstances.reserve(Instances.size());

	for (const auto& Instance : Instances)
	{
		nvrhi::rt::InstanceDesc Desc;
		Desc.bottomLevelAS = Instance.BottomLevelAS;
		Desc.instanceMask  = Instance.InstanceMask;
		Desc.flags         = Instance.InstanceFlags;
		memcpy(Desc.transform, Instance.Transform, sizeof(Instance.Transform));
		NvrhiInstances.push_back(Desc);
	}

	// Build TLAS
	CommandList->buildTopLevelAccelStruct(TLas, NvrhiInstances.data(), static_cast<std::uint32_t>(NvrhiInstances.size()));

	HLVM_LOG(LogTLASBuilder, info, TXT("FTLASBuilder: Built TLAS with {} instances"), Instances.size());

	return true;
}

void FTLASBuilder::Reset()
{
	TLas.Reset();
	Instances.clear();
	HLVM_LOG(LogTLASBuilder, trace, TXT("FTLASBuilder: Reset"));
}
