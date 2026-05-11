/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * FShaderFactory Implementation
 *
 * Provides shader loading functionality using NVRHI for SPIR-V shaders.
 */

#include "Renderer/FShaderFactory.h"
#include "Platform/GenericPlatformFile.h"
#include "Core/Log.h"

DECLARE_LOG_CATEGORY(LogShaderFactory)

bool FShaderFactoryImpl::Initialize(nvrhi::IDevice* InDevice)
{
	Device = InDevice;
	if (!Device)
	{
		HLVM_LOG(LogShaderFactory, err, TXT("FShaderFactory::Initialize: Device is null"));
		return false;
	}
	return true;
}

nvrhi::ShaderHandle FShaderFactoryImpl::CreateShader(
	const std::filesystem::path&	 fileName,
	const std::string&				 entryName,
	const std::vector<FShaderMacro>* defines,
	const nvrhi::ShaderDesc&		 desc)
{
	(void)defines;
	if (!Device)
	{
		HLVM_LOG(LogShaderFactory, err, TXT("FShaderFactory::CreateShader: Device not initialized"));
		return nullptr;
	}

	FPath shaderPath(fileName.string().c_str(), EPlatformFileType::Disk);
	if (!FGenericPlatformFile::Get(EPlatformFileType::Disk)->Exists(shaderPath))
	{
		HLVM_LOG(LogShaderFactory, err, TXT("FShaderFactory::CreateShader: Shader file not found: {}"), FString(fileName.string().c_str()));
		return nullptr;
	}

	TArray<TBYTE> fileContent;
	if (!FFileSystem::LoadFileToArray(fileContent, shaderPath))
	{
		HLVM_LOG(LogShaderFactory, err, TXT("FShaderFactory::CreateShader: Failed to load shader: {}"), FString(fileName.string().c_str()));
		return nullptr;
	}

	nvrhi::ShaderDesc shaderDesc = desc;
	shaderDesc.entryName = entryName.c_str();

	nvrhi::ShaderHandle shader = Device->createShader(shaderDesc, fileContent.GetData(), fileContent.NumBytes());
	if (!shader)
	{
		HLVM_LOG(LogShaderFactory, err, TXT("FShaderFactory::CreateShader: Failed to create shader: {}"), FString(fileName.string().c_str()));
		return nullptr;
	}

	return shader;
}

nvrhi::ShaderHandle FShaderFactoryImpl::CreateStaticShader(
	const FStaticShader&			 shader,
	const std::vector<FShaderMacro>* defines,
	const nvrhi::ShaderDesc&		 desc)
{
	(void)defines;
	if (!Device)
	{
		HLVM_LOG(LogShaderFactory, err, TXT("FShaderFactory::CreateStaticShader: Device not initialized"));
		return nullptr;
	}

	if (!shader.Bytecode || shader.Size == 0)
	{
		HLVM_LOG(LogShaderFactory, err, TXT("FShaderFactory::CreateStaticShader: Invalid bytecode"));
		return nullptr;
	}

	nvrhi::ShaderHandle nvrhiShader = Device->createShader(desc, shader.Bytecode, shader.Size);
	if (!nvrhiShader)
	{
		HLVM_LOG(LogShaderFactory, err, TXT("FShaderFactory::CreateStaticShader: Failed to create shader from bytecode"));
		return nullptr;
	}

	return nvrhiShader;
}

nvrhi::ShaderHandle FShaderFactoryImpl::CreateAutoShader(
	const std::filesystem::path&	 fileName,
	const std::string&				 entryName,
	const FStaticShader&			 dxbc,
	const FStaticShader&			 dxil,
	const FStaticShader&			 spirv,
	const std::vector<FShaderMacro>* defines,
	const nvrhi::ShaderDesc&		 desc)
{
	(void)dxbc;
	(void)dxil;
	(void)defines;

	// Try static SPIR-V first (Vulkan)
	if (spirv.Bytecode && spirv.Size > 0)
	{
		nvrhi::ShaderHandle shader = CreateStaticShader(spirv, defines, desc);
		if (shader)
		{
			return shader;
		}
	}

	// Fall back to file-based loading
	return CreateShader(fileName, entryName, defines, desc);
}

nvrhi::ShaderLibraryHandle FShaderFactoryImpl::CreateShaderLibrary(
    const std::filesystem::path&  fileName,
    const std::vector<FShaderMacro>* defines)
{
    (void)defines;
    if (!Device)
    {
        HLVM_LOG(LogShaderFactory, err, TXT("FShaderFactory::CreateShaderLibrary: Device not initialized"));
        return nullptr;
    }

    FPath shaderPath(fileName.string().c_str(), EPlatformFileType::Disk);
    if (!FGenericPlatformFile::Get(EPlatformFileType::Disk)->Exists(shaderPath))
    {
        HLVM_LOG(LogShaderFactory, err, TXT("FShaderFactory::CreateShaderLibrary: Shader library file not found: {}"), FString(fileName.string().c_str()));
        return nullptr;
    }

    TArray<TBYTE> fileContent;
    if (!FFileSystem::LoadFileToArray(fileContent, shaderPath))
    {
        HLVM_LOG(LogShaderFactory, err, TXT("FShaderFactory::CreateShaderLibrary: Failed to load shader library: {}"), FString(fileName.string().c_str()));
        return nullptr;
    }
    nvrhi::ShaderLibraryHandle library = Device->createShaderLibrary(fileContent.GetData(), fileContent.NumBytes());
    if (!library)
    {
        HLVM_LOG(LogShaderFactory, err, TXT("FShaderFactory::CreateShaderLibrary: Failed to create shader library"));
        return nullptr;
    }

    return library;
}
