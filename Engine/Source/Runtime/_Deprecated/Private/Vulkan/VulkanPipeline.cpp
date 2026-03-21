/**
 * Copyright (c) 2026. MIT License. All rights reserved.
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
	Key.Hash.Update(this, sizeof(FVulkanGraphicsPSODescription));
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

	Hash.Reset();
	Hash.Update(Bindings, BindingsNum * sizeof(Bindings[0]));
	Hash.Update(Attributes, AttributesNum * sizeof(Attributes[0]));
}

FVulkanVertexInputStateInfo::FVulkanVertexInputStateInfo()
	: BindingsNum(0), BindingsMask(0), AttributesNum(0)
{
	FMemory::Memzero(&Info);
	FMemory::MemzeroArray(&Bindings);
	FMemory::MemzeroArray(&Attributes);
}

bool FVulkanVertexInputStateInfo::operator==(const FVulkanVertexInputStateInfo& Other)
{
	// Ignore bindings
	if (AttributesNum != Other.AttributesNum)
	{
		return false;
	}

	for (TUINT32 i = 0; i < AttributesNum; i++)
	{
		if (FMemory::Memcmp(&Attributes[i], &Other.Attributes[i], sizeof(Attributes[0])) != 0)
		{
			return false;
		}
	}

	return true;
}

void FVulkanGraphicsPSO::GeneratePSOMetadata(const FGraphicsPSOCreateInfo& PSOInitializer, FVulkanDescriptorSetsLayoutInfo& LayoutInfoOut, FVulkanGraphicsPSODescription& DescOut)
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

	DescOut.ColorAttachmentStates.AddDefaulted(NumRenderTargets);
	for (TUINT32 Index = 0; Index < DescOut.ColorAttachmentStates.Num(); ++Index)
	{
		DescOut.ColorAttachmentStates[Index].ReadFrom(BlendState->BlendStates[Index]);
	}

	{
		const VkPipelineVertexInputStateCreateInfo& VBInfo = VertexInputState.GetInfo();
		DescOut.VertexBindings.AddDefaulted(VBInfo.vertexBindingDescriptionCount);
		for (TUINT32 Index = 0; Index < VBInfo.vertexBindingDescriptionCount; ++Index)
		{
			DescOut.VertexBindings[Index].ReadFrom(VBInfo.pVertexBindingDescriptions[Index]);
		}

		DescOut.VertexAttributes.AddDefaulted(VBInfo.vertexAttributeDescriptionCount);
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

	FVulkanRenderTargetLayout RTLayout(PSOInitializer);
	DescOut.RenderTargets.ReadFrom(RTLayout);
}

void FVulkanGraphicsPSODescription::FBlendAttachment::ReadFrom(const VkPipelineColorBlendAttachmentState& InState)
{
	bBlend = InState.blendEnable != VK_FALSE;
	ColorBlendOp = S_C(TUINT32, InState.colorBlendOp);
	SrcColorBlendFactor = S_C(TUINT32, InState.srcColorBlendFactor);
	DstColorBlendFactor = S_C(TUINT32, InState.dstColorBlendFactor);
	AlphaBlendOp = S_C(TUINT32, InState.alphaBlendOp);
	SrcAlphaBlendFactor = S_C(TUINT32, InState.srcAlphaBlendFactor);
	DstAlphaBlendFactor = S_C(TUINT32, InState.dstAlphaBlendFactor);
	ColorWriteMask = S_C(TUINT32, InState.colorWriteMask);
}

void FVulkanGraphicsPSODescription::FBlendAttachment::WriteInto(VkPipelineColorBlendAttachmentState& Out) const
{
	Out.blendEnable = bBlend ? VK_TRUE : VK_FALSE;
	Out.colorBlendOp = S_C(VkBlendOp, ColorBlendOp);
	Out.srcColorBlendFactor = S_C(VkBlendFactor, SrcColorBlendFactor);
	Out.dstColorBlendFactor = S_C(VkBlendFactor, DstColorBlendFactor);
	Out.alphaBlendOp = S_C(VkBlendOp, AlphaBlendOp);
	Out.srcAlphaBlendFactor = S_C(VkBlendFactor, SrcAlphaBlendFactor);
	Out.dstAlphaBlendFactor = S_C(VkBlendFactor, DstAlphaBlendFactor);
	Out.colorWriteMask = S_C(VkColorComponentFlags, ColorWriteMask);
}

void FVulkanGraphicsPSODescription::FVertexBinding::ReadFrom(const VkVertexInputBindingDescription& InState)
{
	Binding = InState.binding;
	InputRate = S_C(TUINT32, InState.inputRate);
	Stride = InState.stride;
}

void FVulkanGraphicsPSODescription::FVertexBinding::WriteInto(VkVertexInputBindingDescription& Out) const
{
	Out.binding = Binding;
	Out.inputRate = S_C(VkVertexInputRate, InputRate);
	Out.stride = Stride;
}

void FVulkanGraphicsPSODescription::FVertexAttribute::ReadFrom(const VkVertexInputAttributeDescription& InState)
{
	Binding = InState.binding;
	Format = S_C(TUINT32, InState.format);
	Location = InState.location;
	Offset = InState.offset;
}

void FVulkanGraphicsPSODescription::FVertexAttribute::WriteInto(VkVertexInputAttributeDescription& Out) const
{
	Out.binding = Binding;
	Out.format = S_C(VkFormat, Format);
	Out.location = Location;
	Out.offset = Offset;
}

void FVulkanGraphicsPSODescription::FRasterizer::ReadFrom(const VkPipelineRasterizationStateCreateInfo& InState)
{
	PolygonMode = S_C(TUINT32, InState.polygonMode);
	CullMode = S_C(TUINT32, InState.cullMode);
	DepthBiasSlopeScale = InState.depthBiasSlopeFactor;
	DepthBiasConstantFactor = InState.depthBiasConstantFactor;
}

void FVulkanGraphicsPSODescription::FRasterizer::WriteInto(VkPipelineRasterizationStateCreateInfo& Out) const
{
	Out.polygonMode = S_C(VkPolygonMode, PolygonMode);
	Out.cullMode = S_C(VkCullModeFlags, CullMode);
	Out.frontFace = VK_FRONT_FACE_CLOCKWISE;
	Out.depthClampEnable = VK_FALSE;
	Out.depthBiasEnable = DepthBiasConstantFactor != 0.0f ? VK_TRUE : VK_FALSE;
	Out.rasterizerDiscardEnable = VK_FALSE;
	Out.depthBiasSlopeFactor = DepthBiasSlopeScale;
	Out.depthBiasConstantFactor = DepthBiasConstantFactor;
}

void FVulkanGraphicsPSODescription::FDepthStencil::ReadFrom(const VkPipelineDepthStencilStateCreateInfo& InState)
{
	DepthCompareOp = S_C(TUINT8, InState.depthCompareOp);
	bDepthTestEnable = InState.depthTestEnable != VK_FALSE;
	bDepthWriteEnable = InState.depthWriteEnable != VK_FALSE;
	bDepthBoundsTestEnable = InState.depthBoundsTestEnable != VK_FALSE;
	bStencilTestEnable = InState.stencilTestEnable != VK_FALSE;
	FrontFailOp = S_C(TUINT8, InState.front.failOp);
	FrontPassOp = S_C(TUINT8, InState.front.passOp);
	FrontDepthFailOp = S_C(TUINT8, InState.front.depthFailOp);
	FrontCompareOp = S_C(TUINT8, InState.front.compareOp);
	FrontCompareMask = S_C(TUINT8, InState.front.compareMask);
	FrontWriteMask = InState.front.writeMask;
	FrontReference = InState.front.reference;
	BackFailOp = S_C(TUINT8, InState.back.failOp);
	BackPassOp = S_C(TUINT8, InState.back.passOp);
	BackDepthFailOp = S_C(TUINT8, InState.back.depthFailOp);
	BackCompareOp = S_C(TUINT8, InState.back.compareOp);
	BackCompareMask = S_C(TUINT8, InState.back.compareMask);
	BackWriteMask = InState.back.writeMask;
	BackReference = InState.back.reference;
}

void FVulkanGraphicsPSODescription::FDepthStencil::WriteInto(VkPipelineDepthStencilStateCreateInfo& Out) const
{
	Out.depthCompareOp = S_C(VkCompareOp, DepthCompareOp);
	Out.depthTestEnable = bDepthTestEnable;
	Out.depthWriteEnable = bDepthWriteEnable;
	Out.depthBoundsTestEnable = bDepthBoundsTestEnable;
	Out.stencilTestEnable = bStencilTestEnable;
	Out.front.failOp = S_C(VkStencilOp, FrontFailOp);
	Out.front.passOp = S_C(VkStencilOp, FrontPassOp);
	Out.front.depthFailOp = S_C(VkStencilOp, FrontDepthFailOp);
	Out.front.compareOp = S_C(VkCompareOp, FrontCompareOp);
	Out.front.compareMask = FrontCompareMask;
	Out.front.writeMask = FrontWriteMask;
	Out.front.reference = FrontReference;
	Out.back.failOp = S_C(VkStencilOp, BackFailOp);
	Out.back.passOp = S_C(VkStencilOp, BackPassOp);
	Out.back.depthFailOp = S_C(VkStencilOp, BackDepthFailOp);
	Out.back.compareOp = S_C(VkCompareOp, BackCompareOp);
	Out.back.writeMask = BackWriteMask;
	Out.back.compareMask = BackCompareMask;
	Out.back.reference = BackReference;
}

void FVulkanGraphicsPSODescription::FRenderTargets::FAttachmentRef::ReadFrom(const VkAttachmentReference& InState)
{
	Attachment = InState.attachment;
	Layout = S_C(TUINT32, InState.layout);
}

void FVulkanGraphicsPSODescription::FRenderTargets::FAttachmentRef::WriteInto(VkAttachmentReference& Out) const
{
	Out.attachment = Attachment;
	Out.layout = S_C(VkImageLayout, Layout);
}

void FVulkanGraphicsPSODescription::FRenderTargets::FStencilAttachmentRef::ReadFrom(const VkAttachmentReferenceStencilLayout& InState)
{
	Layout = S_C(TUINT32, InState.stencilLayout);
}

void FVulkanGraphicsPSODescription::FRenderTargets::FStencilAttachmentRef::WriteInto(VkAttachmentReferenceStencilLayout& Out) const
{
	Out.stencilLayout = S_C(VkImageLayout, Layout);
}

void FVulkanGraphicsPSODescription::FRenderTargets::FAttachmentDesc::ReadFrom(const VkAttachmentDescription& InState)
{
	Format = S_C(TUINT32, InState.format);
	Flags = S_C(TUINT8, InState.flags);
	Samples = S_C(TUINT8, InState.samples);
	LoadOp = S_C(TUINT8, InState.loadOp);
	StoreOp = S_C(TUINT8, InState.storeOp);
	StencilLoadOp = S_C(TUINT8, InState.stencilLoadOp);
	StencilStoreOp = S_C(TUINT8, InState.stencilStoreOp);
	InitialLayout = S_C(TUINT32, InState.initialLayout);
	FinalLayout = S_C(TUINT32, InState.finalLayout);
}

void FVulkanGraphicsPSODescription::FRenderTargets::FAttachmentDesc::WriteInto(VkAttachmentDescription& Out) const
{
	Out.format = S_C(VkFormat, Format);
	Out.flags = Flags;
	Out.samples = S_C(VkSampleCountFlagBits, Samples);
	Out.loadOp = S_C(VkAttachmentLoadOp, LoadOp);
	Out.storeOp = S_C(VkAttachmentStoreOp, StoreOp);
	Out.stencilLoadOp = S_C(VkAttachmentLoadOp, StencilLoadOp);
	Out.stencilStoreOp = S_C(VkAttachmentStoreOp, StencilStoreOp);
	Out.initialLayout = S_C(VkImageLayout, InitialLayout);
	Out.finalLayout = S_C(VkImageLayout, FinalLayout);
}

void FVulkanGraphicsPSODescription::FRenderTargets::FStencilAttachmentDesc::ReadFrom(const VkAttachmentDescriptionStencilLayout& InState)
{
	InitialLayout = S_C(TUINT32, InState.stencilInitialLayout);
	FinalLayout = S_C(TUINT32, InState.stencilFinalLayout);
}

void FVulkanGraphicsPSODescription::FRenderTargets::FStencilAttachmentDesc::WriteInto(VkAttachmentDescriptionStencilLayout& Out) const
{
	Out.stencilInitialLayout = S_C(VkImageLayout, InitialLayout);
	Out.stencilFinalLayout = S_C(VkImageLayout, FinalLayout);
}

void FVulkanGraphicsPSODescription::FRenderTargets::ReadFrom(const FVulkanRenderTargetLayout& RTLayout)
{
	NumAttachments = RTLayout.NumAttachmentDescriptions;
	NumColorAttachments = RTLayout.NumColorAttachments;

	bHasDepthStencil = RTLayout.bHasDepthStencil != 0;
	bHasResolveAttachments = RTLayout.bHasResolveAttachments != 0;
	bHasDepthStencilResolve = RTLayout.bHasDepthStencilResolve != 0;
	bHasFragmentDensityAttachment = RTLayout.bHasFragmentDensityAttachment != 0;
	NumUsedClearValues = RTLayout.NumUsedClearValues;

	RenderPassCompatibleHash = RTLayout.RenderPassCompatibleHash;

	Extent3D.x = RTLayout.Extent.Extent3D.width;
	Extent3D.y = RTLayout.Extent.Extent3D.height;
	Extent3D.z = RTLayout.Extent.Extent3D.depth;

	auto CopyAttachmentRefs = [&](TVector<FVulkanGraphicsPSODescription::FRenderTargets::FAttachmentRef>& Dest, const VkAttachmentReference* Source, TUINT32 Count) {
		for (TUINT32 Index = 0; Index < Count; ++Index)
		{
			FVulkanGraphicsPSODescription::FRenderTargets::FAttachmentRef& New = Dest.AddDefaulted_GetRef();
			New.ReadFrom(Source[Index]);
		}
	};
	CopyAttachmentRefs(ColorAttachments, RTLayout.ColorReferences, HLVM_ARRAY_SIZE(RTLayout.ColorReferences));
	CopyAttachmentRefs(ResolveAttachments, RTLayout.ResolveReferences, HLVM_ARRAY_SIZE(RTLayout.ResolveReferences));
	Depth.ReadFrom(RTLayout.DepthReference);
	Stencil.ReadFrom(RTLayout.StencilReference);
	FragmentDensity.ReadFrom(RTLayout.FragmentDensityReference);

	Descriptions.AddDefaulted(HLVM_ARRAY_SIZE(RTLayout.Desc));
	for (TUINT32 Index = 0; Index < HLVM_ARRAY_SIZE(RTLayout.Desc); ++Index)
	{
		Descriptions[Index].ReadFrom(RTLayout.Desc[Index]);
	}
	StencilDescription.ReadFrom(RTLayout.StencilDesc);
}

void FVulkanGraphicsPSODescription::FRenderTargets::WriteInto(FVulkanRenderTargetLayout& Out) const
{
	Out.NumAttachmentDescriptions = NumAttachments;
	Out.NumColorAttachments = NumColorAttachments;

	Out.bHasDepthStencil = bHasDepthStencil;
	Out.bHasResolveAttachments = bHasResolveAttachments;
	Out.bHasDepthStencilResolve = bHasDepthStencilResolve;
	Out.bHasFragmentDensityAttachment = bHasFragmentDensityAttachment;
	Out.NumUsedClearValues = NumUsedClearValues;

	Out.RenderPassCompatibleHash = RenderPassCompatibleHash;

	Out.Extent.Extent3D.width = Extent3D.x;
	Out.Extent.Extent3D.height = Extent3D.y;
	Out.Extent.Extent3D.depth = Extent3D.z;

	auto CopyAttachmentRefs = [&](const TVector<FVulkanGraphicsPSODescription::FRenderTargets::FAttachmentRef>& Source, VkAttachmentReference* Dest, TUINT32 Count) {
		for (TUINT32 Index = 0; Index < Count; ++Index, ++Dest)
		{
			Source[Index].WriteInto(*Dest);
		}
	};
	CopyAttachmentRefs(ColorAttachments, Out.ColorReferences, HLVM_ARRAY_SIZE(Out.ColorReferences));
	CopyAttachmentRefs(ResolveAttachments, Out.ResolveReferences, HLVM_ARRAY_SIZE(Out.ResolveReferences));
	Depth.WriteInto(Out.DepthReference);
	Stencil.WriteInto(Out.StencilReference);
	FragmentDensity.WriteInto(Out.FragmentDensityReference);

	for (TUINT32 Index = 0; Index < HLVM_ARRAY_SIZE(Out.Desc); ++Index)
	{
		Descriptions[Index].WriteInto(Out.Desc[Index]);
	}
	StencilDescription.WriteInto(Out.StencilDesc);
}
