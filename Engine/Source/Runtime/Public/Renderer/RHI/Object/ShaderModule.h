/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 *  Shader Module Objects
 *
 *  Shader module management using NVRHI.
 */

#pragma once

#include "Renderer/RHI/RHICommon.h"
#include "Template/PointerTemplate.tpp"

/*-----------------------------------------------------------------------------
	FShaderModule - Shader Module Class
-----------------------------------------------------------------------------*/

/**
 * Shader module for loading and managing SPIR-V shaders
 *
 * Manages:
 * - SPIR-V bytecode loading
 * - Shader module creation
 * - Shader stage identification
 *
 * Usage:
 * 1. Create instance
 * 2. Call InitializeFromFile() or InitializeFromMemory()
 * 3. Use GetShaderModuleHandle() for pipeline creation
 */
class FShaderModule
{
public:
	NOCOPYMOVE(FShaderModule);
	FShaderModule();
	virtual ~FShaderModule();

	// Initialize from file (SPIR-V binary)
	bool InitializeFromFile(
		const FPath&	  FilePath,
		nvrhi::ShaderType ShaderType,
		nvrhi::IDevice*	  Device);

	// Initialize from memory buffer
	bool InitializeFromMemory(
		const void*		  Code,
		size_t			  CodeSize,
		nvrhi::ShaderType ShaderType,
		nvrhi::IDevice*	  Device);

	// Access
	[[nodiscard]] nvrhi::ShaderHandle GetShaderHandle() const { return ShaderHandle; }
	[[nodiscard]] nvrhi::ShaderType	  GetShaderType() const { return ShaderType; }
	[[nodiscard]] const char*		  GetEntryPointName() const { return EntryPointName; }

	// Debug name
	void SetDebugName(const TCHAR* Name);

protected:
	nvrhi::ShaderHandle	   ShaderHandle;
	nvrhi::ShaderType	   ShaderType;
	TNNPtr<nvrhi::IDevice> Device;
	TCharArray<128>		   DebugName;
	const char*			   EntryPointName;
};

/*-----------------------------------------------------------------------------
	Inline Implementations
-----------------------------------------------------------------------------*/

HLVM_INLINE_FUNC FShaderModule::FShaderModule()
	: ShaderType(nvrhi::ShaderType::All)
	, Device(nullptr)
	, EntryPointName("main")
{
}

HLVM_INLINE_FUNC FShaderModule::~FShaderModule()
{
	ShaderHandle.Reset();
}
