/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#include "VulkanDescriptorSets.h"

void FVulkanDescriptorSetsLayoutInfo::FinalizeGraphicsBindings(const FVulkanPhysicalDeviceRef& PhysicalDevice, const FGraphicsShaderGatherInfo& ShaderGatherInfo, bool bUseBindless)
{
	// We'll be reusing this struct
	VkDescriptorSetLayoutBinding Binding;
	FMemory::Memzero(&Binding);
	Binding.descriptorCount = 1;

	const bool	  bConvertAllUBsToDynamic = !bUseBindless && true;
	const bool	  bConvertPackedUBsToDynamic = !bUseBindless && true;
	const TUINT32 MaxDescriptorSetUniformBuffersDynamic = PhysicalDevice->GetProperties().limits.maxDescriptorSetUniformBuffersDynamic;

	TUINT32 CurrentImmutableSampler = 0;
	for (TUINT32 Stage = 0; Stage < RHI::NUM_GFX_SHADER_STAGES; ++Stage)
	{
		HLVM_ENSURE(StageInfos[Stage].IsEmpty());
		if (const FVulkanShaderHeader* ShaderHeader = ShaderGatherInfo.ShaderHeaders[Stage])
		{
			FStageInfo& StageInfo = StageInfos[Stage];

			Binding.stageFlags = VulkanRHI::VulkanShaderStageFromRHIStage(S_C(EShaderStage, Stage));

			StageInfo.PackedGlobalsSize = ShaderHeader->PackedGlobalsSize;
			StageInfo.NumBoundUniformBuffers = ShaderHeader->NumBoundUniformBuffers;

			for (TUINT32 BindingIndex = 0; BindingIndex < ShaderHeader->Bindings.Num(); ++BindingIndex)
			{
				const VkDescriptorType DescriptorType = S_C(VkDescriptorType, ShaderHeader->Bindings[BindingIndex].DescriptorType);

				const bool bIsUniformBuffer = (DescriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
				const bool bIsGlobalPackedConstants = bIsUniformBuffer && ShaderHeader->PackedGlobalsSize && (BindingIndex == 0);

				if (bIsGlobalPackedConstants)
				{
					const VkDescriptorType UBType = bConvertPackedUBsToDynamic ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

					const TUINT32 NewBindingIndex = StageInfo.Types.Add(UBType);
					HLVM_ASSERT_F(NewBindingIndex == 0, TXT("Packed globals should always be the first binding!"));

					Binding.binding = NewBindingIndex;
					Binding.descriptorType = UBType;
					AddDescriptor(Stage, Binding);
				}
				else if (bIsUniformBuffer)
				{
					VkDescriptorType UBType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
					if (bConvertAllUBsToDynamic && LayoutTypes[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC] < MaxDescriptorSetUniformBuffersDynamic)
					{
						UBType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
					}

					// Here we might mess up with the stageFlags, so reset them every loop
					Binding.descriptorType = UBType;
					const bool bUBHasConstantData = (BindingIndex < ShaderHeader->NumBoundUniformBuffers);
					if (bUBHasConstantData)
					{
						const TUINT32 NewBindingIndex = StageInfo.Types.Add(UBType);
						HLVM_ENSURE(NewBindingIndex == BindingIndex);
						Binding.binding = NewBindingIndex;
						AddDescriptor(Stage, Binding);
					}
				}
				else
				{
					const TUINT32 NewTypeIndex = StageInfo.Types.Add(DescriptorType);
					HLVM_ENSURE(NewTypeIndex == BindingIndex);
					Binding.binding = BindingIndex;
					Binding.descriptorType = DescriptorType;
					AddDescriptor(Stage, Binding);
				}
			}
		}
	}
}

void FVulkanDescriptorSetsLayoutInfo::AddDescriptor(TUINT32 DescriptorSetIndex, const VkDescriptorSetLayoutBinding& Descriptor)
{
	// Increment type usage
	if (LayoutTypes.Contains(Descriptor.descriptorType))
	{
		LayoutTypes[Descriptor.descriptorType]++;
	}
	else
	{
		LayoutTypes.Add(Descriptor.descriptorType, 1);
	}

	if (DescriptorSetIndex >= SetLayouts.Num())
	{
		SetLayouts.SetNum(DescriptorSetIndex + 1);
	}
	FSetLayout& DescSetLayout = SetLayouts[DescriptorSetIndex];
	DescSetLayout.LayoutBindings.Add(Descriptor);

	const FStageInfo& SetInfo = StageInfos[DescriptorSetIndex];
	HLVM_ENSURE(StageInfos[DescriptorSetIndex].Types[Descriptor.binding] == Descriptor.descriptorType);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch-enum"
	switch (Descriptor.descriptorType)
	{
		case VK_DESCRIPTOR_TYPE_SAMPLER:
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
			StageInfos[DescriptorSetIndex].NumImageInfos++;
			break;
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
			StageInfos[DescriptorSetIndex].NumBufferInfos++;
			break;
		case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
			StageInfos[DescriptorSetIndex].NumAccelerationStructures++;
			break;
		case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
		case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
			break;
		default:
			HLVM_ASSERT_F(false, TXT("Unsupported descriptor type {}"), S_C(TUINT32, Descriptor.descriptorType));
			break;
#pragma clang diagnostic pop
	}
}
