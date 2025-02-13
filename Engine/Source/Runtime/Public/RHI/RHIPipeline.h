/**
* Copyright (c) 2025. MIT License. All rights reserved.
*/

#pragma once

#include "RHIPipelineDeclaration.h"

// Structure for describing graphics pipeline state initialization parameters
struct FGraphicsPipelineStateInitializer
{
	FVertexDeclarationRHIRef VertexDeclaration; // Vertex declaration
	FRHIShader* VertexShader; // Vertex shader
	FRHIShader* PixelShader; // Pixel shader
	FRHIShader* GeometryShader; // Geometry shader (optional)
	FRHIShader* HullShader; // Hull shader (optional)
	FRHIShader* DomainShader; // Domain shader (optional)
	EPixelFormat RenderTargetFormats[RHI_MAX_SIMULTANEOUS_RENDER_TARGETS]; // Formats of the render targets
	EPixelFormat DepthStencilFormat; // Format of the depth-stencil target
	TUINT32 NumRenderTargets; // Number of render targets
	TUINT32 SampleCount; // Number of samples (for MSAA)

	// Constructor for easy initialization
	FGraphicsPipelineStateInitializer()
		: VertexDeclaration(nullptr)
		, VertexShader(nullptr)
		, PixelShader(nullptr)
		, GeometryShader(nullptr)
		, HullShader(nullptr)
		, DomainShader(nullptr)
		, DepthStencilFormat(EPixelFormat::Unknown)
		, NumRenderTargets(0)
		, SampleCount(1)
	{
		std::memset(RenderTargetFormats, HLVM_ENUM_V(EPixelFormat, Unknown), sizeof(RenderTargetFormats));
	}
};

// Structure for describing compute pipeline state initialization parameters
struct FComputePipelineStateInitializer
{
	FRHIShader* ComputeShader; // Compute shader

	// Constructor for easy initialization
	FComputePipelineStateInitializer(FRHIShader* InComputeShader = nullptr)
		: ComputeShader(InComputeShader)
	{}
};

// Structure for describing render pass initialization parameters
struct FRHIRenderPassInfo
{
	TVector<FRHITexture*> RenderTargets; // Render targets
	FRHITexture* DepthStencilTarget; // Depth-stencil target
	FClearValueBinding ClearValue; // Clear value for the render targets and depth-stencil target

	// Constructor for easy initialization
	FRHIRenderPassInfo()
		: DepthStencilTarget(nullptr)
	{}
};
