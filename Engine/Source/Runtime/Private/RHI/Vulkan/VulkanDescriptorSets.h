/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "RHI/RHIPipeline.h"
#include "VulkanRHIResourcePre.h"

// Information for the layout of descriptor sets; does not hold runtime objects
class FVulkanDescriptorSetsLayoutInfo
{
public:
	FVulkanDescriptorSetsLayoutInfo()
	{
		// Add expected descriptor types
		for (TUINT32 i = VK_DESCRIPTOR_TYPE_BEGIN_RANGE; i <= VK_DESCRIPTOR_TYPE_END_RANGE; ++i)
		{
			LayoutTypes.Add(static_cast<VkDescriptorType>(i), 0);
		}

		LayoutTypes.Add(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 0);
	}

	TUINT32 GetTypesUsed(VkDescriptorType Type) const
	{
		if (auto Value = LayoutTypes.Find(Type); Value != nullptr)
		{
			return *Value;
		}
		else
		{
			return 0;
		}
	}

	struct FSetLayout
	{
		TVector<VkDescriptorSetLayoutBinding> LayoutBindings;
		FMD5Digest							  Hash;

		void GenerateHash()
		{
			Hash = FMD5Hash::Hash(LayoutBindings.GetData(), sizeof(VkDescriptorSetLayoutBinding) * LayoutBindings.Num());
		}

		friend FMD5Digest GetTypeHash(const FSetLayout& In)
		{
			return In.Hash;
		}

		bool operator==(const FSetLayout& In) const
		{
			if (In.Hash != Hash)
			{
				return false;
			}

			const TUINT32 NumBindings = LayoutBindings.Num();
			if (In.LayoutBindings.Num() != NumBindings)
			{
				return false;
			}

			if (NumBindings != 0 && FMemory::Memcmp(In.LayoutBindings.GetData(), LayoutBindings.GetData(), NumBindings * sizeof(VkDescriptorSetLayoutBinding)) != 0)
			{
				return false;
			}

			return true;
		}

		bool operator!=(const FSetLayout& In) const
		{
			return !(*this == In);
		}
	};

	const TVector<FSetLayout>& GetLayouts() const
	{
		return SetLayouts;
	}

	friend TUINT32 GetTypeHash(const FVulkanDescriptorSetsLayoutInfo& In)
	{
		return In.Hash;
	}

	bool operator==(const FVulkanDescriptorSetsLayoutInfo& In) const
	{
		if (In.Hash != Hash)
		{
			return false;
		}

		if (In.BindPoint != BindPoint)
		{
			return false;
		}

		if (In.SetLayouts.Num() != SetLayouts.Num())
		{
			return false;
		}

		if (In.TypesUsageID != TypesUsageID)
		{
			return false;
		}

		for (TUINT32 Index = 0; Index < In.SetLayouts.Num(); ++Index)
		{
			if (In.SetLayouts[Index] != SetLayouts[Index])
			{
				return false;
			}
		}

		if (StageInfos != In.StageInfos)
		{
			return false;
		}

		return true;
	}

	void CopyFrom(const FVulkanDescriptorSetsLayoutInfo& Info)
	{
		LayoutTypes = Info.LayoutTypes;
		Hash = Info.Hash;
		TypesUsageID = Info.TypesUsageID;
		SetLayouts = Info.SetLayouts;
		StageInfos = Info.StageInfos;
	}

	const auto& GetLayoutTypes() const
	{
		return LayoutTypes;
	}

	TUINT32 GetTypesUsageID() const
	{
		return TypesUsageID;
	}

	bool HasInputAttachments() const
	{
		return GetTypesUsed(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) > 0;
	}

	struct FStageInfo
	{
		TVector<VkDescriptorType> Types;
		TUINT32					  PackedGlobalsSize = 0;
		TUINT32					  NumBoundUniformBuffers = 0;
		TUINT32					  NumImageInfos = 0;
		TUINT32					  NumBufferInfos = 0;
		TUINT32					  NumAccelerationStructures = 0;

		bool IsEmpty() const
		{
			if ((Types.Num() != 0) || (PackedGlobalsSize != 0) || (NumBoundUniformBuffers != 0))
			{
				return false;
			}

			return true;
		}

		bool operator==(const FStageInfo& In) const
		{
			if (PackedGlobalsSize != In.PackedGlobalsSize || NumBoundUniformBuffers != In.NumBoundUniformBuffers || NumBufferInfos != In.NumBufferInfos || NumImageInfos != In.NumImageInfos || NumAccelerationStructures != In.NumAccelerationStructures || Types.Num() != In.Types.Num() || FMemory::Memcmp(Types.GetData(), In.Types.GetData(), Types.NumBytes()))
			{
				return false;
			}

			return true;
		}
	};
	TStaticVector<FStageInfo, RHI::MAX_SHADER_STAGES> StageInfos;

protected:
	TMapSmall<VkDescriptorType, TUINT32> LayoutTypes;
	TVector<FSetLayout>					 SetLayouts;

	TUINT32 Hash = 0;

	TUINT32 TypesUsageID = TUINT32_MAX;

	VkPipelineBindPoint BindPoint = VK_PIPELINE_BIND_POINT_MAX_ENUM;

	void CompileTypesUsageID();

	void AddDescriptor(TINT32 DescriptorSetIndex, const VkDescriptorSetLayoutBinding& Descriptor);
};
