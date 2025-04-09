/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "RHIPipelinePre.h"

// Structure for describing graphics pipeline state initialization parameters
struct FGraphicsPipelineStateInitializer
{
	FVertexDeclarationRHIRef VertexDeclaration;							  // Vertex declaration
	FRHIShader*				 VertexShader;								  // Vertex shader
	FRHIShader*				 PixelShader;								  // Pixel shader
	FRHIShader*				 GeometryShader;							  // Geometry shader (optional)
	FRHIShader*				 HullShader;								  // Hull shader (optional)
	FRHIShader*				 DomainShader;								  // Domain shader (optional)
	EPixelFormat			 RenderTargetFormats[RHI::RT_ATTACHMENT_MAX]; // Formats of the render targets
	EPixelFormat			 DepthStencilFormat;						  // Format of the depth-stencil target
	TUINT32					 NumRenderTargets;							  // Number of render targets
	TUINT32					 SampleCount;								  // Number of samples (for MSAA)

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
		std::memset(RenderTargetFormats, HLVM_ENUM_VALUE(EPixelFormat::Unknown), sizeof(RenderTargetFormats));
	}
};

// Structure for describing compute pipeline state initialization parameters
struct FComputePipelineStateInitializer
{
	FRHIShader* ComputeShader; // Compute shader

	// Constructor for easy initialization
	FComputePipelineStateInitializer(FRHIShader* InComputeShader = nullptr)
		: ComputeShader(InComputeShader)
	{
	}
};

