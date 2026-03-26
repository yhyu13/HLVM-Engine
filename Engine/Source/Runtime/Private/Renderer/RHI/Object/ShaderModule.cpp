/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 *  Shader Module Implementation
 */

#include "Renderer/RHI/Object/ShaderModule.h"
#include "Platform/GenericPlatformFile.h"

bool FShaderModule::InitializeFromFile(
	const FPath&	  InFilePath,
	nvrhi::ShaderType InShaderType,
	nvrhi::IDevice*	  InDevice)
{
	HLVM_ENSURE_F(!ShaderHandle, TXT("Shader already initialized"));
	HLVM_ENSURE_F(InDevice, TXT("Device is null"));
	HLVM_ENSURE_F(FGenericPlatformFile::Get(EPlatformFileType::Disk)->Exists(InFilePath), TXT("Shader file not found: {0}"), *FString(InFilePath));

	ShaderType = InShaderType;
	Device = InDevice;

	// Read shader bytecode
	TArray<TBYTE> FileContent;
	if (!FFileSystem::LoadFileToArray(FileContent, InFilePath))
	{
		HLVM_LOG(LogRHI, err, TXT("Failed to load shader file: {0}"), *(InFilePath));
		return false;
	}

	// Create shader from bytecode
	nvrhi::ShaderDesc Desc;
	Desc.setShaderType(ShaderType);
	Desc.entryName = EntryPointName;
	ShaderHandle = Device->createShader(Desc, FileContent.GetData(), FileContent.NumBytes());
	HLVM_ENSURE_F(ShaderHandle, TXT("Failed to create shader"));

	return true;
}

bool FShaderModule::InitializeFromMemory(
	const void*		  Code,
	size_t			  CodeSize,
	nvrhi::ShaderType InShaderType,
	nvrhi::IDevice*	  InDevice)
{
	HLVM_ENSURE_F(!ShaderHandle, TXT("Shader already initialized"));
	HLVM_ENSURE_F(Code, TXT("Code is null"));
	HLVM_ENSURE_F(CodeSize > 0, TXT("Code size is zero"));
	HLVM_ENSURE_F(InDevice, TXT("Device is null"));

	ShaderType = InShaderType;
	Device = InDevice;

	// Create shader from memory
	nvrhi::ShaderDesc Desc;
	Desc.setShaderType(ShaderType);
	Desc.entryName = EntryPointName;
	ShaderHandle = Device->createShader(Desc, Code, CodeSize);
	HLVM_ENSURE_F(ShaderHandle, TXT("Failed to create shader"));

	return true;
}

void FShaderModule::SetDebugName(const TCHAR* Name)
{
	DebugName = Name;
}
