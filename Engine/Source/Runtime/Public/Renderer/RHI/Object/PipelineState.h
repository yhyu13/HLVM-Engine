/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 *  Pipeline State Objects - Fixed for NVRHI API
 *
 *  Graphics and compute pipeline state management using NVRHI.
 */

#pragma once

#include "Renderer/RHI/RHICommon.h"
#include "Template/PointerTemplate.tpp"
#include "Renderer/RHI/Object/ShaderModule.h"

/*-----------------------------------------------------------------------------
	FVertexAttribute - Vertex Attribute Descriptor
-----------------------------------------------------------------------------*/

/**
 * Describes a single vertex attribute for input layout
 */
struct FVertexAttribute
{
	const char*	  SemanticName;
	uint32_t	  SemanticIndex;
	nvrhi::Format Format;
	uint32_t	  BufferSlot;
	uint32_t	  Offset;

	FVertexAttribute()
		: SemanticName(nullptr)
		, SemanticIndex(0)
		, Format(nvrhi::Format::UNKNOWN)
		, BufferSlot(0)
		, Offset(0)
	{
	}

	FVertexAttribute(const char* InSemanticName, uint32_t InSemanticIndex, nvrhi::Format InFormat, uint32_t InBufferSlot, uint32_t InOffset)
		: SemanticName(InSemanticName)
		, SemanticIndex(InSemanticIndex)
		, Format(InFormat)
		, BufferSlot(InBufferSlot)
		, Offset(InOffset)
	{
	}
};

/*-----------------------------------------------------------------------------
	FVertexInputLayout - Vertex Input Layout Descriptor
-----------------------------------------------------------------------------*/

/**
 * Describes the vertex input layout for a graphics pipeline
 */
struct FVertexInputLayout
{
	TArray<FVertexAttribute> Attributes;
	uint32_t				 Stride;

	FVertexInputLayout()
		: Stride(0)
	{
	}

	void AddAttribute(const FVertexAttribute& Attribute)
	{
		Attributes.Add(Attribute);
	}

	void SetStride(uint32_t InStride)
	{
		Stride = InStride;
	}
};

/*-----------------------------------------------------------------------------
	FRasterizerState - Rasterizer State Descriptor
-----------------------------------------------------------------------------*/

/**
 * Rasterizer state configuration
 */
struct FRasterizerState
{
	nvrhi::RasterCullMode CullMode;
	int					  DepthBias;
	float				  DepthBiasClamp;
	bool				  FrontCCW;
	bool				  bEnableDepthBias;
	bool				  bEnableDepthClip;

	FRasterizerState()
		: CullMode(nvrhi::RasterCullMode::Back)
		, DepthBias(0)
		, DepthBiasClamp(0.0f)
		, FrontCCW(false)
		, bEnableDepthBias(false)
		, bEnableDepthClip(true)
	{
	}
};

/*-----------------------------------------------------------------------------
	FDepthStencilState - Depth/Stencil State Descriptor
-----------------------------------------------------------------------------*/

/**
 * Depth/stencil state configuration
 */
struct FDepthStencilState
{
	bool									bEnableDepthTest;
	bool									bEnableDepthWrite;
	bool									bEnableStencilTest;
	nvrhi::ComparisonFunc					DepthFunc;
	nvrhi::DepthStencilState::StencilOpDesc StencilFront;
	nvrhi::DepthStencilState::StencilOpDesc StencilBack;
	uint8_t									StencilReadMask;
	uint8_t									StencilWriteMask;

	FDepthStencilState()
		: bEnableDepthTest(true)
		, bEnableDepthWrite(true)
		, bEnableStencilTest(false)
		, DepthFunc(nvrhi::ComparisonFunc::LessOrEqual)
		, StencilReadMask(0xFF)
		, StencilWriteMask(0xFF)
	{
	}
};

/*-----------------------------------------------------------------------------
	FBlendState - Blend State Descriptor
-----------------------------------------------------------------------------*/

/**
 * Blend state configuration for a single render target
 */
struct FBlendState
{
	bool			   bEnableBlend;
	nvrhi::BlendFactor SrcColorBlendFactor;
	nvrhi::BlendFactor DestColorBlendFactor;
	nvrhi::BlendOp	   ColorBlendOp;
	nvrhi::BlendFactor SrcAlphaBlendFactor;
	nvrhi::BlendFactor DestAlphaBlendFactor;
	nvrhi::BlendOp	   AlphaBlendOp;
	nvrhi::ColorMask   ColorWriteMask;

	FBlendState()
		: bEnableBlend(false)
		, SrcColorBlendFactor(nvrhi::BlendFactor::One)
		, DestColorBlendFactor(nvrhi::BlendFactor::Zero)
		, ColorBlendOp(nvrhi::BlendOp::Add)
		, SrcAlphaBlendFactor(nvrhi::BlendFactor::One)
		, DestAlphaBlendFactor(nvrhi::BlendFactor::Zero)
		, AlphaBlendOp(nvrhi::BlendOp::Add)
		, ColorWriteMask(nvrhi::ColorMask::All)
	{
	}

	static FBlendState DefaultBlend()
	{
		return FBlendState();
	}

	static FBlendState AdditiveBlend()
	{
		FBlendState State;
		State.bEnableBlend = true;
		State.SrcColorBlendFactor = nvrhi::BlendFactor::SrcAlpha;
		State.DestColorBlendFactor = nvrhi::BlendFactor::One;
		State.SrcAlphaBlendFactor = nvrhi::BlendFactor::One;
		State.DestAlphaBlendFactor = nvrhi::BlendFactor::One;
		return State;
	}
};

/*-----------------------------------------------------------------------------
	FPipelineState - Base Pipeline State Class
-----------------------------------------------------------------------------*/

/**
 * Base class for graphics and compute pipeline states
 */
class IPipelineState
{
public:
	NOCOPYMOVE(IPipelineState);
	IPipelineState();
	virtual ~IPipelineState() = default;

	[[nodiscard]] virtual bool IsValid() const = 0;

protected:
	TNNPtr<nvrhi::IDevice> Device;
};

/*-----------------------------------------------------------------------------
	FGraphicsPipelineState - Graphics Pipeline State
-----------------------------------------------------------------------------*/

class FGraphicsPipelineBuilder;
/**
 * Graphics pipeline state for rendering
 */
class FGraphicsPipelineState : public IPipelineState
{
public:
	NOCOPYMOVE(FGraphicsPipelineState);
	FGraphicsPipelineState();
	virtual ~FGraphicsPipelineState() override;

	[[nodiscard]] nvrhi::GraphicsPipelineHandle GetGraphicsPipelineHandle() const
	{
		return PipelineHandle;
	}

	[[nodiscard]] virtual bool IsValid() const override
	{
		return PipelineHandle != nullptr;
	}

private:
	friend class FGraphicsPipelineBuilder;
	nvrhi::GraphicsPipelineHandle PipelineHandle;
};

/*-----------------------------------------------------------------------------
	FComputePipelineState - Compute Pipeline State
-----------------------------------------------------------------------------*/
class FComputePipelineBuilder;
/**
 * Compute pipeline state for compute shaders
 */
class FComputePipelineState : public IPipelineState
{
public:
	NOCOPYMOVE(FComputePipelineState);
	FComputePipelineState();
	virtual ~FComputePipelineState() override;

	[[nodiscard]] nvrhi::ComputePipelineHandle GetComputePipelineHandle() const
	{
		return PipelineHandle;
	}

	[[nodiscard]] virtual bool IsValid() const override
	{
		return PipelineHandle != nullptr;
	}

private:
	friend class FComputePipelineBuilder;
	nvrhi::ComputePipelineHandle PipelineHandle;
};

/*-----------------------------------------------------------------------------
	FGraphicsPipelineBuilder - Fluent Builder for Graphics Pipelines
-----------------------------------------------------------------------------*/

/**
 * Fluent builder for constructing graphics pipeline states
 *
 * Usage:
 * ```cpp
 * FGraphicsPipelineBuilder Builder;
 * Builder.SetDevice(Device)
 * 	.AddShader(VertexShader, nvrhi::ShaderType::Vertex)
 * 	.AddShader(FragmentShader, nvrhi::ShaderType::Fragment)
 * 	.SetVertexInputLayout(VertexLayout)
 * 	.SetPrimitiveTopology(nvrhi::PrimitiveType::TriangleList)
 * 	.SetRasterizerState(RasterizerState)
 * 	.SetDepthStencilState(DepthStencilState)
 * 	.AddBlendState(BlendState)
 * 	.SetFramebuffer(Framebuffer);
 *
 * TUniquePtr<FGraphicsPipelineState> Pipeline = Builder.Build();
 * ```
 */
class FGraphicsPipelineBuilder
{
public:
	FGraphicsPipelineBuilder();
	~FGraphicsPipelineBuilder();

	FGraphicsPipelineBuilder& SetDevice(nvrhi::IDevice* InDevice);
	FGraphicsPipelineBuilder& AddShader(nvrhi::ShaderHandle Shader, nvrhi::ShaderType Type);
	FGraphicsPipelineBuilder& SetVertexInputLayout(const FVertexInputLayout& Layout);
	FGraphicsPipelineBuilder& SetPrimitiveTopology(nvrhi::PrimitiveType Topology);
	FGraphicsPipelineBuilder& SetRasterizerState(const FRasterizerState& State);
	FGraphicsPipelineBuilder& SetDepthStencilState(const FDepthStencilState& State);
	FGraphicsPipelineBuilder& AddBlendState(TArrayView<size_t> ColorBuffers, const FBlendState& State);
	FGraphicsPipelineBuilder& SetFramebuffer(nvrhi::IFramebuffer* Framebuffer);

	TUniquePtr<FGraphicsPipelineState> Build();

private:
	nvrhi::GraphicsPipelineDesc Desc;
	nvrhi::RenderState			RenderStateDesc;
	nvrhi::InputLayoutHandle	VertexInputLayout;
	TNNPtr<nvrhi::IDevice>		Device;
	TNNPtr<nvrhi::IFramebuffer> FramebufferHandle;
};

/*-----------------------------------------------------------------------------
	FComputePipelineBuilder - Fluent Builder for Compute Pipelines
-----------------------------------------------------------------------------*/

/**
 * Fluent builder for constructing compute pipeline states
 *
 * Usage:
 * ```cpp
 * FComputePipelineBuilder Builder;
 * Builder.SetDevice(Device)
 * 	.AddShader(ComputeShader);
 *
 * TUniquePtr<FComputePipelineState> Pipeline = Builder.Build();
 * ```
 */
class FComputePipelineBuilder
{
public:
	FComputePipelineBuilder();
	~FComputePipelineBuilder();

	FComputePipelineBuilder& SetDevice(nvrhi::IDevice* InDevice);
	FComputePipelineBuilder& AddShader(nvrhi::ShaderHandle Shader);

	TUniquePtr<FComputePipelineState> Build();

private:
	nvrhi::ComputePipelineDesc Desc;
	TNNPtr<nvrhi::IDevice>	   Device;
	nvrhi::ShaderHandle		   ComputeShader;
};
