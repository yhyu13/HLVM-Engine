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

static void GetVulkanGfxShaders(const FBoundShaderStateInput& BSI, FVulkanShaderRef Shaders[RHI::NUM_GFX_SHADER_STAGES])
{
	Shaders[HLVM_ENUM_VALUE(EShaderStage::Vertex)] = BSI.GetVertexShader();
	Shaders[HLVM_ENUM_VALUE(EShaderStage::Pixel)] = BSI.GetPixelShader();
	Shaders[HLVM_ENUM_VALUE(EShaderStage::Geometry)] = BSI.GetMeshShader();
	Shaders[HLVM_ENUM_VALUE(EShaderStage::Mesh)] = BSI.GetMeshShader();
	Shaders[HLVM_ENUM_VALUE(EShaderStage::Task)] = BSI.GetTaskShader();
}

void FVulkanVertexInputStateInfo::Generate(FVulkanVertexDeclarationRef VertexDeclaration, TUINT32 VertexHeaderInOutAttributeMask)
{
	// GenerateVertexInputState is expected to be called only once!
	HLVM_ASSERT(Info.sType == 0);

	// Generate vertex attribute Layout
	const FVertexDeclarationElementList& VertexInput = VertexDeclaration->Elements;

	// Generate Bindings
	for (const FVertexElement& Element : VertexInput)
	{
		if ((1 << Element.AttributeIndex) & VertexHeaderInOutAttributeMask)
		{
			HLVM_ASSERT(Element.StreamIndex < RHI::MAX_VERTEX_ELEMENTS);

			VkVertexInputBindingDescription& CurrBinding = Bindings[Element.StreamIndex];
			if ((BindingsMask & (1 << Element.StreamIndex)) != 0)
			{
				// If exists, validate.
				// Info must be the same
				HLVM_ASSERT(CurrBinding.binding == Element.StreamIndex);
				HLVM_ASSERT(CurrBinding.inputRate == Element.bUseInstance ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX);
				HLVM_ASSERT(CurrBinding.stride == Element.Stride);
			}
			else
			{
				// Zeroed outside
				HLVM_ASSERT(CurrBinding.binding == 0 && CurrBinding.inputRate == 0 && CurrBinding.stride == 0);
				CurrBinding.binding = Element.StreamIndex;
				CurrBinding.inputRate = Element.bUseInstance ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
				CurrBinding.stride = Element.Stride;

				// Add mask flag and increment number of bindings
				BindingsMask |= 1 << Element.StreamIndex;
			}
		}
	}

	// Remove gaps between bindings
	BindingsNum = 0;
	BindingToStream.Reset();
	StreamToBinding.Reset();
	for (TUINT32 i = 0; i < HLVM_ARRAY_SIZE(Bindings); i++)
	{
		if (!((1 << i) & BindingsMask))
		{
			continue;
		}

		BindingToStream.Add(BindingsNum, i);
		StreamToBinding.Add(i, BindingsNum);
		VkVertexInputBindingDescription& CurrBinding = Bindings[BindingsNum];
		CurrBinding = Bindings[i];
		CurrBinding.binding = BindingsNum;
		BindingsNum++;
	}

	// Clean originally placed bindings
	FMemory::Memset(Bindings + BindingsNum, 0, sizeof(Bindings[0]) * (HLVM_ARRAY_SIZE(Bindings) - BindingsNum));

	// Attributes are expected to be uninitialized/empty
	HLVM_ASSERT(AttributesNum == 0);
	for (const FVertexElement& CurrElement : VertexInput)
	{
		// Mask-out unused vertex input
		if ((!((1 << CurrElement.AttributeIndex) & VertexHeaderInOutAttributeMask))
			|| !StreamToBinding.Contains(CurrElement.StreamIndex))
		{
			continue;
		}

		VkVertexInputAttributeDescription& CurrAttribute = Attributes[AttributesNum++]; // Zeroed at the begin of the function
		HLVM_ASSERT(CurrAttribute.location == 0 && CurrAttribute.binding == 0 && CurrAttribute.format == 0 && CurrAttribute.offset == 0);

		CurrAttribute.binding = StreamToBinding.at(CurrElement.StreamIndex);
		CurrAttribute.location = CurrElement.AttributeIndex;
		CurrAttribute.format = VulkanRHI::RHIVertexElementTypeToVulkanFormat(CurrElement.Type);
		CurrAttribute.offset = CurrElement.Offset;
	}
	HLVM_ASSERT(AttributesNum > 0);

	// Vertex Input
	Info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	// Its possible to have no vertex buffers
	if (BindingsNum == 0)
	{
		HLVM_ASSERT(Hash.Valid());
		return;
	}

	Info.vertexBindingDescriptionCount = BindingsNum;
	Info.pVertexBindingDescriptions = Bindings;

	Info.vertexAttributeDescriptionCount = AttributesNum;
	Info.pVertexAttributeDescriptions = Attributes;

	Hash = FMD5Hash::Hash(Bindings, BindingsNum * sizeof(Bindings[0]));
	Hash = FMD5Hash::Hash(Attributes, AttributesNum * sizeof(Attributes[0]), &Hash);
}

void FVulkanGraphicsPSO::GeneratePSOMetadata(const FGraphicsPSOInitializer& PSOInitializer, FVulkanDescriptorSetsLayoutInfo& LayoutInfoOut, FVulkanGraphicsPSODescription& DescOut)
{
	FVulkanShaderRef Shaders[RHI::NUM_GFX_SHADER_STAGES];
	GetVulkanGfxShaders(PSOInitializer.BoundShaderState, Shaders);

	FVulkanVertexInputStateInfo VertexInputState;
	{
		FGraphicsShaderGatherInfo GfxGatherInfo;
		TUINT32					  NumActiveShaders = 0;
		TUINT32					  NumBindlessShaders = 0;

		auto ProcessShaderStage = [&GfxGatherInfo, &NumActiveShaders, &NumBindlessShaders](EShaderStage Stage, FVulkanShaderRef Shader) {
			if (Shader)
			{
				const FVulkanShaderHeader* Header = Shader->GetCodeHeader();
				GfxGatherInfo.ShaderHeaders[HLVM_ENUM_VALUE(Stage)] = Header;
				NumActiveShaders++;
				if (Shader->UsesBindless())
				{
					NumBindlessShaders++;
				}
			}
		};

		if (Shaders[HLVM_ENUM_VALUE(EShaderStage::Vertex)])
		{
			const FVulkanShaderHeader* VSHeader = Shaders[HLVM_ENUM_VALUE(EShaderStage::Vertex)]->GetCodeHeader();
			VertexInputState.Generate(PSOInitializer.BoundShaderState.VertexDeclarationRHI, VSHeader->InOutMask);
		}

		if (Shaders[HLVM_ENUM_VALUE(EShaderStage::Pixel)] && Shaders[HLVM_ENUM_VALUE(EShaderStage::Pixel)]->GetCodeHeader()->InputAttachmentInfos.Num())
		{
			// input attachements can't exist in a first sub-pass
			HLVM_ENSURE(PSOInitializer.SubpassHint != ESubpassHint::Default);
			HLVM_ENSURE(PSOInitializer.SubpassIndex != 0);
		}

		ProcessShaderStage(EShaderStage::Vertex, Shaders[HLVM_ENUM_VALUE(EShaderStage::Vertex)]);
		ProcessShaderStage(EShaderStage::Pixel, Shaders[HLVM_ENUM_VALUE(EShaderStage::Pixel)]);

#if PLATFORM_SUPPORTS_MESH_SHADERS
		ProcessShaderStage(EShaderStage::Mesh, Shaders[HLVM_ENUM_VALUE(EShaderStage::Mesh)]);
		ProcessShaderStage(EShaderStage::Task, Shaders[HLVM_ENUM_VALUE(EShaderStage::Task)]);
#endif

#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
		ProcessShaderStage(EShaderStage::Geometry, Shaders[HLVM_ENUM_VALUE(EShaderStage::Geometry)]);
#endif

		HLVM_ASSERT_F((NumBindlessShaders == 0) || (NumBindlessShaders == NumActiveShaders), TXT("All shaders must be bindless or non-bindless."));

		LayoutInfoOut.FinalizeGraphicsBindings(PhysicalDevice, GfxGatherInfo, (NumBindlessShaders != 0));
	}

	DescOut.SubpassIndex = PSOInitializer.SubpassIndex;

	FVulkanBlendStateRef BlendState = S_C(FVulkanBlendStateRef, PSOInitializer.BlendState);
	DescOut.UseAlphaToCoverage = PSOInitializer.NumSamples > 1 && BlendState->GetCreateInfo().bUseAlphaToCoverage ? 1 : 0;

	DescOut.RasterizationSamples = PSOInitializer.NumSamples;
	DescOut.Topology = S_C(TUINT32, VulkanRHI::VulkanPrimitiveTopologyFromRHIPrimitiveType(PSOInitializer.PrimitiveType));
	TUINT32 NumRenderTargets = PSOInitializer.ComputeNumValidRenderTargets();

	if (PSOInitializer.SubpassHint == ESubpassHint::DeferredShading && PSOInitializer.SubpassIndex >= 2)
	{
		// GBuffer attachements are not used as output in a shading sub-pass
		// Only SceneColor is used as a color attachment
		NumRenderTargets = 1;
	}

	if (PSOInitializer.SubpassHint == ESubpassHint::DepthReading && PSOInitializer.SubpassIndex >= 1)
	{
		// Only SceneColor is used as a color attachment after the first subpass (not SceneDepthAux)
		NumRenderTargets = 1;
	}

	if (PSOInitializer.SubpassHint == ESubpassHint::CustomResolve)
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

	{
		const TVector<FVulkanDescriptorSetsLayoutInfo::FSetLayout>& Layouts = LayoutInfoOut.GetLayouts();
		HLVM_ASSERT(DescOut.DescSetLayoutBindings.Num() == 0);
		DescOut.DescSetLayoutBindings.AddDefaulted(Layouts.Num());
		for (TUINT32 Index = 0; Index < Layouts.Num(); ++Index)
		{
			for (TUINT32 SubIndex = 0; SubIndex < Layouts[Index].LayoutBindings.Num(); ++SubIndex)
			{
				FVulkanDescSetLayoutBinding& Binding = DescOut.DescSetLayoutBindings[Index].AddDefaulted_GetRef();
				Binding.ReadFrom(Layouts[Index].LayoutBindings[SubIndex]);
			}
		}
	}

	{
		FVulkanRasterizerStateRef RasterizerStateRef = S_C(FVulkanRasterizerStateRef, PSOInitializer.RasterizerState);
		DescOut.Rasterizer.ReadFrom(RasterizerStateRef->RasterizerState);
	}

	{
		VkPipelineDepthStencilStateCreateInfo DSInfo;
		FVulkanDepthStencilStateRef			  DepthStencilStateRef = S_C(FVulkanDepthStencilStateRef, PSOInitializer.DepthStencilState);
		DepthStencilStateRef->SetupCreateInfo(PSOInitializer, DSInfo);
		DescOut.DepthStencil.ReadFrom(DSInfo);
	}

//	TUINT32 NumShaders = 0;
//#if VULKAN_USE_SHADERKEYS
//	TUINT64 SharedKey = 0;
//	TUINT64 Primes[] = {
//		6843488303525203279llu,
//		3095754086865563867llu,
//		8242695776924673527llu,
//		7556751872809527943llu,
//		8278265491465149053llu,
//		1263027877466626099llu,
//		2698115308251696101llu,
//	};
//	static_assert(sizeof(Primes) / sizeof(Primes[0]) >= RHI::NUM_GFX_SHADER_STAGES);
//	for (TUINT32 Index = 0; Index < RHI::NUM_GFX_SHADER_STAGES; ++Index)
//	{
//		FVulkanShaderRef Shader = Shaders[Index];
//		TUINT64			 Key = 0;
//		if (Shader)
//		{
//			Key = Shader->GetShaderKey();
//			++NumShaders;
//		}
//		DescOut.ShaderKeys[Index] = Key;
//		SharedKey += Key * Primes[Index];
//	}
//	DescOut.ShaderKeyShared = SharedKey;
//#else
//	for (int32 Index = 0; Index < EShaderStage::NumGraphicsStages; ++Index)
//	{
//		FVulkanShaderRef Shader = Shaders[Index];
//		if (Shader)
//		{
//			HLVM_ASSERT(Shader->Spirv.Num() != 0);
//
//			FSHAHash Hash = GetShaderHashForStage(PSOInitializer, (EShaderStage)Index);
//			DescOut.ShaderHashes.Stages[Index] = Hash;
//
//			++NumShaders;
//		}
//	}
//	DescOut.ShaderHashes.Finalize();
//#endif
	//HLVM_ASSERT(NumShaders > 0);

	//FVulkanRenderTargetLayout RTLayout(PSOInitializer);
	//DescOut.RenderTargets.ReadFrom(RTLayout);
}
