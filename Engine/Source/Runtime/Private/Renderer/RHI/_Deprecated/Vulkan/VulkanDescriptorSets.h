/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "Renderer/RHI/_Deprecated/RHIPipeline.h"
#include "VulkanResource.h"

// Information for the layout of descriptor sets; does not hold runtime objects
class FVulkanDescriptorSetsLayoutInfo
{
public:
	struct FSetLayout
	{
		TVector<VkDescriptorSetLayoutBinding> LayoutBindings;

		bool operator==(const FSetLayout& In) const
		{
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

	const TVector<FSetLayout>& GetLayouts() const
	{
		return SetLayouts;
	}

	bool operator==(const FVulkanDescriptorSetsLayoutInfo& In) const
	{

		if (In.BindPoint != BindPoint)
		{
			return false;
		}

		if (In.SetLayouts.Num() != SetLayouts.Num())
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
		SetLayouts = Info.SetLayouts;
		StageInfos = Info.StageInfos;
	}

	const auto& GetLayoutTypes() const
	{
		return LayoutTypes;
	}

	bool HasInputAttachments() const
	{
		return GetTypesUsed(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) > 0;
	}

	void FinalizeGraphicsBindings(const FVulkanPhysicalDeviceRef& PhysicalDevice, const FGraphicsShaderGatherInfo& ShaderGatherInfo, bool bUseBindless);

public:
	TStaticVector<FStageInfo, RHI::MAX_SHADER_STAGES> StageInfos {RHI::MAX_SHADER_STAGES};

private:
	void AddDescriptor(TUINT32 DescriptorSetIndex, const VkDescriptorSetLayoutBinding& Descriptor);

private:
	TMapSmall<VkDescriptorType, TUINT32> LayoutTypes;
	TVector<FSetLayout>					 SetLayouts;
	VkPipelineBindPoint					 BindPoint = VK_PIPELINE_BIND_POINT_MAX_ENUM;
};

//// Layout for a Pipeline, also includes DescriptorSets layout
//class FVulkanLayout : public FVulkanMinimalContext
//{
//public:
//    FVulkanLayout(bool InGfxLayout, bool InUsesBindless);
//    virtual ~FVulkanLayout();
//
//    bool IsGfxLayout() const
//    {
//       return bIsGfxLayout;
//    }
//
//    inline const FVulkanDescriptorSetsLayout& GetDescriptorSetsLayout() const
//    {
//       return DescriptorSetLayout;
//    }
//
//    inline VkPipelineLayout GetPipelineLayout() const
//    {
//       return bUsesBindless ? Device->GetBindlessDescriptorManager()->GetPipelineLayout() : PipelineLayout;
//    }
//
//    inline bool HasDescriptors() const
//    {
//       return DescriptorSetLayout.GetLayouts().Num() > 0;
//    }
//
//    inline uint32 GetDescriptorSetLayoutHash() const
//    {
//       return DescriptorSetLayout.GetHash();
//    }
//
//protected:
//    const bool bIsGfxLayout;
//    const bool bUsesBindless;
//    FVulkanDescriptorSetsLayout    DescriptorSetLayout;
//    VkPipelineLayout         PipelineLayout;
//
//    template <bool bIsCompute>
//    inline void FinalizeBindings(const FUniformBufferGatherInfo& UBGatherInfo)
//    {
//       // Setting descriptor is only allowed prior to compiling the layout
//       check(DescriptorSetLayout.GetHandles().Num() == 0);
//
//       DescriptorSetLayout.FinalizeBindings<bIsCompute>(UBGatherInfo);
//    }
//
//    inline void ProcessBindingsForStage(VkShaderStageFlagBits StageFlags, ShaderStage::EStage DescSet, const FVulkanShaderHeader& CodeHeader, FUniformBufferGatherInfo& OutUBGatherInfo) const
//    {
//       // Setting descriptor is only allowed prior to compiling the layout
//       check(DescriptorSetLayout.GetHandles().Num() == 0);
//
//       DescriptorSetLayout.ProcessBindingsForStage(StageFlags, DescSet, CodeHeader, OutUBGatherInfo);
//    }
//
//    void Compile(FVulkanDescriptorSetLayoutMap& DSetLayoutMap);
//};

