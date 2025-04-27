/**
* Copyright (c) 2025. MIT License. All rights reserved.
*/

#include "VulkanPipeline.h"
#include "VulkanShader.h"

void FVulkanDescSetLayoutBinding::ReadFrom(const VkDescriptorSetLayoutBinding& InState)
{
	Binding = InState.binding;
	DescriptorType = S_C(TUINT32, InState.descriptorType);
	StageFlags = S_C(TUINT32, InState.stageFlags);
}

void FVulkanDescSetLayoutBinding::WriteInto(VkDescriptorSetLayoutBinding& OutState) const
{
	OutState.binding = Binding;
	OutState.descriptorType = S_C(VkDescriptorType, DescriptorType);
	OutState.stageFlags = S_C(VkShaderStageFlags, StageFlags);
}

FVulkanGraphicsPSODescription::PSOKey FVulkanGraphicsPSODescription::GeneratePSOKey() const
{
	// TODO : we should implement UE5 memory writer archive system that fully serailize each element
	// after that, generate the struct hash
	PSOKey Key;
	Key.Hash = FMD5Hash::Hash(this, sizeof(FVulkanGraphicsPSODescription));
	return Key;
}

/*
void FVulkanGraphicsPSO::GeneratePSOMetadata(const FGraphicsPSOInitializer& PSOInitializer, FVulkanDescriptorSetsLayoutInfo& LayoutInfoOut, FVulkanGraphicsPSODescription& DescOut)
{
	FVulkanShaderRef Shaders[RHI::NUM_GFX_SHADER_STAGES];
	GetVulkanGfxShaders(PSOInitializer.BoundShaderState, Shaders);

	FVulkanVertexInputStateInfo VertexInputState;

	{
		const FBoundShaderStateInput& BSI = PSOInitializer.BoundShaderState;

		FUniformBufferGatherInfo UBGatherInfo;
		TUINT32 NumActiveShaders = 0;
		TUINT32 NumBindlessShaders = 0;

		auto ProcessShaderStage = [&LayoutInfoOut, &UBGatherInfo, &NumActiveShaders, &NumBindlessShaders](VkShaderStageFlagBits StageFlag, EShaderStage Stage, FVulkanShaderRef Shader)
		{
			if (Shader)
			{
				const FVulkanShaderHeader& Header = Shader->GetCodeHeader();
				LayoutInfoOut.ProcessBindingsForStage(StageFlag, Stage, Header, UBGatherInfo);
				NumActiveShaders++;
				if (Shader->UsesBindless())
				{
					NumBindlessShaders++;
				}
			}
		};

		if (Shaders[ShaderStage::Vertex])
		{
			const FVulkanShaderHeader& VSHeader = Shaders[ShaderStage::Vertex]->GetCodeHeader();
			VertexInputState.Generate(ResourceCast(PSOInitializer.BoundShaderState.VertexDeclarationRHI), VSHeader.InOutMask);
		}

		if (Shaders[ShaderStage::Pixel] && Shaders[ShaderStage::Pixel]->GetCodeHeader().InputAttachmentInfos.Num())
		{
			// input attachements can't exist in a first sub-pass
			check(PSOInitializer.SubpassHint != ESubpassHint::None);
			check(PSOInitializer.SubpassIndex != 0);
		}

		ProcessShaderStage(VK_SHADER_STAGE_VERTEX_BIT, ShaderStage::Vertex, Shaders[ShaderStage::Vertex]);
		ProcessShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, ShaderStage::Pixel, Shaders[ShaderStage::Pixel]);

#if PLATFORM_SUPPORTS_MESH_SHADERS
		ProcessShaderStage(VK_SHADER_STAGE_MESH_BIT_EXT, ShaderStage::Mesh, Shaders[ShaderStage::Mesh]);
		ProcessShaderStage(VK_SHADER_STAGE_TASK_BIT_EXT, ShaderStage::Task, Shaders[ShaderStage::Task]);
#endif

#if VULKAN_SUPPORTS_GEOMETRY_SHADERS
		ProcessShaderStage(VK_SHADER_STAGE_GEOMETRY_BIT, ShaderStage::Geometry, Shaders[ShaderStage::Geometry]);
#endif

		checkf((NumBindlessShaders == 0) || (NumBindlessShaders == NumActiveShaders), TEXT("All shaders must be bindless or non-bindless."));

		// Second pass
		const TUINT32 NumImmutableSamplers = PSOInitializer.ImmutableSamplerState.ImmutableSamplers.Num();
		TArrayView<FRHISamplerState*> ImmutableSamplers(NumImmutableSamplers > 0 ? &(FRHISamplerState*&)PSOInitializer.ImmutableSamplerState.ImmutableSamplers[0] : nullptr, NumImmutableSamplers);
		LayoutInfoOut.FinalizeBindings<false>(*Device, UBGatherInfo, ImmutableSamplers, (NumBindlessShaders != 0));
	}

	DescOut.SubpassIndex = PSOInitializer.SubpassIndex;

	FVulkanBlendState* BlendState = ResourceCast(PSOInitializer.BlendState);

	DescOut.UseAlphaToCoverage = PSOInitializer.NumSamples > 1 && BlendState->Initializer.bUseAlphaToCoverage ? 1 : 0;

	DescOut.RasterizationSamples = PSOInitializer.NumSamples;
	DescOut.Topology = (TUINT32)UEToVulkanTopologyType(Device, PSOInitializer.PrimitiveType, DescOut.ControlPoints);
	TUINT32 NumRenderTargets = PSOInitializer.ComputeNumValidRenderTargets();

	if (PSOInitializer.SubpassHint == ESubpassHint::DeferredShadingSubpass && PSOInitializer.SubpassIndex >= 2)
	{
		// GBuffer attachements are not used as output in a shading sub-pass
		// Only SceneColor is used as a color attachment
		NumRenderTargets = 1;
	}

	if (PSOInitializer.SubpassHint == ESubpassHint::DepthReadSubpass && PSOInitializer.SubpassIndex >= 1)
	{
		// Only SceneColor is used as a color attachment after the first subpass (not SceneDepthAux)
		NumRenderTargets = 1;
	}

	if (PSOInitializer.SubpassHint == ESubpassHint::CustomResolveSubpass)
	{
		NumRenderTargets = 1; // This applies to base and depth passes as well. One render target for base and depth, another one for custom resolve.
		if (PSOInitializer.SubpassIndex >= 2)
		{
			// the resolve subpass renders to a non MSAA surface
			DescOut.RasterizationSamples = 1;
		}
	}

	DescOut.ColorAttachmentStates.AddUninitialized(NumRenderTargets);
	for (TUINT32 Index = 0; Index < DescOut.ColorAttachmentStates.Num(); ++Index)
	{
		DescOut.ColorAttachmentStates[Index].ReadFrom(BlendState->BlendStates[Index]);
	}

	{
		const VkPipelineVertexInputStateCreateInfo& VBInfo = VertexInputState.GetInfo();
		DescOut.VertexBindings.AddUninitialized(VBInfo.vertexBindingDescriptionCount);
		for (TUINT32 Index = 0; Index < VBInfo.vertexBindingDescriptionCount; ++Index)
		{
			DescOut.VertexBindings[Index].ReadFrom(VBInfo.pVertexBindingDescriptions[Index]);
		}

		DescOut.VertexAttributes.AddUninitialized(VBInfo.vertexAttributeDescriptionCount);
		for (TUINT32 Index = 0; Index < VBInfo.vertexAttributeDescriptionCount; ++Index)
		{
			DescOut.VertexAttributes[Index].ReadFrom(VBInfo.pVertexAttributeDescriptions[Index]);
		}
	}

	const TArray<FVulkanDescriptorSetsLayout::FSetLayout>& Layouts = LayoutInfoOut.GetLayouts();
	DescOut.DescriptorSetLayoutBindings.AddDefaulted(Layouts.Num());
	for (TUINT32 Index = 0; Index < Layouts.Num(); ++Index)
	{
		for (TUINT32 SubIndex = 0; SubIndex < Layouts[Index].LayoutBindings.Num(); ++SubIndex)
		{
			FDescriptorSetLayoutBinding& Binding = DescOut.DescriptorSetLayoutBindings[Index].AddDefaulted_GetRef();
			Binding.ReadFrom(Layouts[Index].LayoutBindings[SubIndex]);
		}
	}

	DescOut.Rasterizer.ReadFrom(ResourceCast(PSOInitializer.RasterizerState)->RasterizerState);
	{
		VkPipelineDepthStencilStateCreateInfo DSInfo;
		ResourceCast(PSOInitializer.DepthStencilState)->SetupCreateInfo(PSOInitializer, DSInfo);
		DescOut.DepthStencil.ReadFrom(DSInfo);
	}

	TUINT32 NumShaders = 0;
#if VULKAN_USE_SHADERKEYS
	TUINT64 SharedKey = 0;
	TUINT64 Primes[] = {
		6843488303525203279llu,
		3095754086865563867llu,
		8242695776924673527llu,
		7556751872809527943llu,
		8278265491465149053llu,
		1263027877466626099llu,
		2698115308251696101llu,
	};
	static_assert(sizeof(Primes) / sizeof(Primes[0]) >= RHI::NUM_GFX_SHADER_STAGES);
	for (TUINT32 Index = 0; Index < RHI::NUM_GFX_SHADER_STAGES; ++Index)
	{
		FVulkanShaderRef Shader = Shaders[Index];
		TUINT64 Key = 0;
		if (Shader)
		{
			Key = Shader->GetShaderKey();
			++NumShaders;
		}
		DescOut.ShaderKeys[Index] = Key;
		SharedKey += Key * Primes[Index];
	}
	DescOut.ShaderKeyShared = SharedKey;
#else
	for (int32 Index = 0; Index < ShaderStage::NumGraphicsStages; ++Index)
	{
		FVulkanShaderRef Shader = Shaders[Index];
		if (Shader)
		{
			check(Shader->Spirv.Num() != 0);

			FSHAHash Hash = GetShaderHashForStage(PSOInitializer, (EShaderStage)Index);
			DescOut.ShaderHashes.Stages[Index] = Hash;

			++NumShaders;
		}
	}
	DescOut.ShaderHashes.Finalize();
#endif
	check(NumShaders > 0);

	FVulkanRenderTargetLayout RTLayout(PSOInitializer);
	DescOut.RenderTargets.ReadFrom(RTLayout);

	// Shading rate:
	DescOut.ShadingRate = PSOInitializer.bAllowVariableRateShading ? PSOInitializer.ShadingRate : EVRSShadingRate::VRSSR_1x1;
	DescOut.Combiner = EVRSRateCombiner::VRSRB_Max;      // @todo: This needs to be specified twice; from pipeline-to-primitive, and from primitive-to-attachment.
														 // We don't have per-primitive VRS so that should just be hard-coded to "passthrough" until this is supported; but we should expose
														 // this setting in the material properies, especially since there's some materials that don't play nicely with
														 // shading rates other than 1x1, in which case we'll want to use VRSRB_Min to override e.g. the attachment shading rate.
														 // For now, just locked to "max".
}
*/
