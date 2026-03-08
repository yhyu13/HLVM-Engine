/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 *
 *  Pipeline State Implementation - Fixed for NVRHI API
 */

#include "Renderer/RHI/Object/PipelineState.h"
#include "Renderer/RHI/Object/ShaderModule.h"

/*-----------------------------------------------------------------------------
	IPipelineState Implementation
-----------------------------------------------------------------------------*/

IPipelineState::IPipelineState()
	: Device(nullptr)
{
}

/*-----------------------------------------------------------------------------
	FGraphicsPipelineState Implementation
-----------------------------------------------------------------------------*/

FGraphicsPipelineState::FGraphicsPipelineState()
{
}

FGraphicsPipelineState::~FGraphicsPipelineState()
{
}

/*-----------------------------------------------------------------------------
	FComputePipelineState Implementation
-----------------------------------------------------------------------------*/

FComputePipelineState::FComputePipelineState()
{
}

FComputePipelineState::~FComputePipelineState()
{
}

/*-----------------------------------------------------------------------------
	FGraphicsPipelineBuilder Implementation
-----------------------------------------------------------------------------*/

FGraphicsPipelineBuilder::FGraphicsPipelineBuilder()
	: Device(nullptr)
	, FramebufferHandle(nullptr)
{
	Desc.setPrimType(nvrhi::PrimitiveType::TriangleList);

	// Set default render state
	RenderStateDesc.setRasterState(nvrhi::RasterState()
									   .setCullMode(nvrhi::RasterCullMode::Back)
									   .setFrontCounterClockwise(false)
									   .setDepthBias(0)
									   .setDepthBiasClamp(0.0f)
									   .setDepthClipEnable(true));

	RenderStateDesc.setDepthStencilState(nvrhi::DepthStencilState()
											 .setDepthTestEnable(true)
											 .setDepthWriteEnable(true)
											 .setDepthFunc(nvrhi::ComparisonFunc::LessOrEqual));

	RenderStateDesc.setBlendState(nvrhi::BlendState());
}

FGraphicsPipelineBuilder::~FGraphicsPipelineBuilder()
{
}

FGraphicsPipelineBuilder& FGraphicsPipelineBuilder::SetDevice(nvrhi::IDevice* InDevice)
{
	Device = InDevice;
	return *this;
}

FGraphicsPipelineBuilder& FGraphicsPipelineBuilder::AddShader(nvrhi::ShaderHandle Shader, nvrhi::ShaderType Type)
{
	HLVM_ENSURE_F(Shader, TXT("Shader handle is null"));

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch-enum"
	switch (Type)
	{
		case nvrhi::ShaderType::Vertex:
			Desc.VS = Shader;
			break;
		case nvrhi::ShaderType::Pixel:
			Desc.PS = Shader;
			break;
		case nvrhi::ShaderType::Compute:
			HLVM_LOG(LogRHI, err, TXT("Use FComputePipelineBuilder for compute shaders"));
			break;
		case nvrhi::ShaderType::Geometry:
			Desc.GS = Shader;
			break;
		case nvrhi::ShaderType::Hull:
			Desc.HS = Shader;
			break;
		case nvrhi::ShaderType::Domain:
			Desc.DS = Shader;
			break;
		default:
			break;
	}
#pragma clang diagnostic pop

	return *this;
}

FGraphicsPipelineBuilder& FGraphicsPipelineBuilder::SetVertexInputLayout(const FVertexInputLayout& Layout)
{
	// Create NVRHI vertex attribute descriptions
	TArray<nvrhi::VertexAttributeDesc> Attributes;
	Attributes.Reserve(Layout.Attributes.Num());

	for (const FVertexAttribute& Attr : Layout.Attributes)
	{
		nvrhi::VertexAttributeDesc VertexAttr;
		VertexAttr.setName(Attr.SemanticName ? Attr.SemanticName : "");
		VertexAttr.setFormat(Attr.Format);
		VertexAttr.setArraySize(1);
		VertexAttr.setBufferIndex(Attr.BufferSlot);
		VertexAttr.setOffset(Attr.Offset);
		VertexAttr.setElementStride(Layout.Stride);
		VertexAttr.setIsInstanced(false);
		Attributes.Add(VertexAttr);
	}

	// Create input layout
	HLVM_ASSERT(Desc.VS);
	VertexInputLayout = Device->createInputLayout(Attributes.GetData(), Attributes.Num32(), Desc.VS);
	HLVM_ENSURE_F(VertexInputLayout, TXT("Failed to create input layout"));

	Desc.setInputLayout(VertexInputLayout);

	return *this;
}

FGraphicsPipelineBuilder& FGraphicsPipelineBuilder::SetPrimitiveTopology(nvrhi::PrimitiveType Topology)
{
	Desc.setPrimType(Topology);
	return *this;
}

FGraphicsPipelineBuilder& FGraphicsPipelineBuilder::SetRasterizerState(const FRasterizerState& State)
{
	RenderStateDesc.setRasterState(nvrhi::RasterState()
									   .setCullMode(State.CullMode)
									   .setFrontCounterClockwise(State.FrontCCW)
									   .setDepthBias(State.bEnableDepthBias ? State.DepthBias : 0)
									   .setDepthBiasClamp(State.DepthBiasClamp)
									   .setDepthClipEnable(State.bEnableDepthClip));

	return *this;
}

FGraphicsPipelineBuilder& FGraphicsPipelineBuilder::SetDepthStencilState(const FDepthStencilState& State)
{
	nvrhi::DepthStencilState DepthDesc;
	DepthDesc.setDepthTestEnable(State.bEnableDepthTest);
	DepthDesc.setDepthWriteEnable(State.bEnableDepthWrite);
	DepthDesc.setDepthFunc(State.DepthFunc);
	DepthDesc.setStencilEnable(State.bEnableStencilTest);
	DepthDesc.setStencilReadMask(State.StencilReadMask);
	DepthDesc.setStencilWriteMask(State.StencilWriteMask);
	DepthDesc.setFrontFaceStencil(State.StencilFront);
	DepthDesc.setBackFaceStencil(State.StencilBack);

	RenderStateDesc.setDepthStencilState(DepthDesc);

	return *this;
}

FGraphicsPipelineBuilder& FGraphicsPipelineBuilder::AddBlendState(TArrayView<size_t> ColorBuffers, const FBlendState& State)
{
	HLVM_ASSERT(nvrhi::c_MaxRenderTargets >= ColorBuffers.Num32());
	nvrhi::BlendState BlendDesc;
	for (size_t i = 0; i < ColorBuffers.Num(); ++i)
	{
		BlendDesc.targets[i].setBlendEnable(State.bEnableBlend);
		BlendDesc.targets[i].setSrcBlend(State.SrcColorBlendFactor);
		BlendDesc.targets[i].setDestBlend(State.DestColorBlendFactor);
		BlendDesc.targets[i].setBlendOp(State.ColorBlendOp);
		BlendDesc.targets[i].setSrcBlendAlpha(State.SrcAlphaBlendFactor);
		BlendDesc.targets[i].setDestBlendAlpha(State.DestAlphaBlendFactor);
		BlendDesc.targets[i].setBlendOpAlpha(State.AlphaBlendOp);
		BlendDesc.targets[i].setColorWriteMask(State.ColorWriteMask);
	}

	RenderStateDesc.setBlendState(BlendDesc);

	return *this;
}

FGraphicsPipelineBuilder& FGraphicsPipelineBuilder::SetFramebuffer(nvrhi::IFramebuffer* Framebuffer)
{
	FramebufferHandle = Framebuffer;
	return *this;
}

TUniquePtr<FGraphicsPipelineState> FGraphicsPipelineBuilder::Build()
{
	HLVM_ENSURE_F(Device, TXT("Device not set"));
	HLVM_ENSURE_F(Desc.VS, TXT("Vertex shader not set"));
	HLVM_ENSURE_F(Desc.PS, TXT("Pixel shader not set"));
	HLVM_ENSURE_F(FramebufferHandle, TXT("Framebuffer not set"));

	// Set render state
	Desc.setRenderState(RenderStateDesc);

	TUniquePtr<FGraphicsPipelineState> Pipeline = MAKE_UNIQUE(FGraphicsPipelineState);
	Pipeline->Device = Device;

	// Get framebuffer info and create pipeline
	nvrhi::FramebufferInfo FBInfo = FramebufferHandle->getDesc();
	Pipeline->PipelineHandle = Device->createGraphicsPipeline(Desc, FBInfo);

	HLVM_ENSURE_F(Pipeline->PipelineHandle, TXT("Failed to create graphics pipeline"));

	return Pipeline;
}

/*-----------------------------------------------------------------------------
	FComputePipelineBuilder Implementation
-----------------------------------------------------------------------------*/

FComputePipelineBuilder::FComputePipelineBuilder()
	: Device(nullptr)
{
}

FComputePipelineBuilder::~FComputePipelineBuilder()
{
}

FComputePipelineBuilder& FComputePipelineBuilder::SetDevice(nvrhi::IDevice* InDevice)
{
	Device = InDevice;
	return *this;
}

FComputePipelineBuilder& FComputePipelineBuilder::AddShader(nvrhi::ShaderHandle Shader)
{
	HLVM_ENSURE_F(Shader, TXT("Shader handle is null"));
	ComputeShader = Shader;
	return *this;
}

TUniquePtr<FComputePipelineState> FComputePipelineBuilder::Build()
{
	HLVM_ENSURE_F(Device, TXT("Device not set"));
	HLVM_ENSURE_F(ComputeShader, TXT("Compute shader not set"));

	Desc.setComputeShader(ComputeShader);

	TUniquePtr<FComputePipelineState> Pipeline = MAKE_UNIQUE(FComputePipelineState);
	Pipeline->Device = Device;
	Pipeline->PipelineHandle = Device->createComputePipeline(Desc);

	HLVM_ENSURE_F(Pipeline->PipelineHandle, TXT("Failed to create compute pipeline"));

	return Pipeline;
}
