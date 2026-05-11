/**
 * HLVM-Engine: Common render passes implementation (BlitTexture)
 */

#include "Renderer/Common/FCommonRenderPasses.h"
#include "Renderer/Common/FBindingCache.h"
#include "Renderer/ShaderMake/ShaderBlob.h"
#include "Renderer/Common/FBindingCache.h"

#include <unordered_map>
#include <cstdint>
#include <vector>
#include <fstream>
#include <functional>

#include <Core/Log.h>
#include <Core/String.h>
#include <Platform/FileSystem/Path.h>

DECLARE_LOG_CATEGORY(LogCommon)

// =============================================================================
// BINDING LAYOUT
// =============================================================================

// Create binding layout for Blit shaders
// Uses samplerless texture sampling (texelFetch) with mode control via constant buffer
static nvrhi::BindingLayoutHandle CreateBlitBindingLayout(nvrhi::IDevice* Device)
{
	nvrhi::BindingLayoutDesc Desc;
	Desc.visibility = nvrhi::ShaderType::Pixel;
	Desc.bindings = {
		nvrhi::BindingLayoutItem::ConstantBuffer(0), // BlitParams
		nvrhi::BindingLayoutItem::Texture_SRV(0)     // Texture
	};
	return Device->createBindingLayout(Desc);
}

// =============================================================================
// STATIC HELPERS
// =============================================================================

// Optional override for shader data directory
static FString g_ShaderDataDirOverride;

// STATIC HELPERS
// =============================================================================

// Read binary file helper
static std::vector<char> ReadBinaryFile(const std::string& Filename)
{
	std::ifstream file(Filename, std::ios::ate | std::ios::binary);
	if (!file.is_open())
	{
		return {};
	}

	size_t			  fileSize = static_cast<size_t>(file.tellg());
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
	file.close();

	return buffer;
}
static FString GetShaderDataDir()
{
	if (!g_ShaderDataDirOverride.empty())
	{
		return g_ShaderDataDirOverride;
	}
	// Use GProjectRoot to construct shader directory
	// GProjectRoot = /home/hangyu5/Documents/Gitrepo-My/HLVM-Engine
	// Shader dir = GProjectRoot / Engine/Source/Runtime/Shader
	return FString::Format(
		TXT("{}/Engine/Source/Runtime/Shader"),
		*GProjectRoot);
}
// =============================================================================
// STANDARD TEXTURE HELPERS
// =============================================================================

nvrhi::TextureHandle FCommonRenderPasses::CreateBlackTexture(nvrhi::IDevice* Device)
{
	if (!Device)
		return nullptr;

	nvrhi::TextureDesc Desc;
	Desc.dimension = nvrhi::TextureDimension::Texture1D;
	Desc.width = 1;
	Desc.height = 1;
	Desc.mipLevels = 1;
	Desc.arraySize = 1;
	Desc.format = nvrhi::Format::RGBA8_UNORM;
	Desc.isUAV = false;
	Desc.isRenderTarget = false;
	Desc.initialState = nvrhi::ResourceStates::ShaderResource;
	Desc.keepInitialState = true;
	Desc.debugName = "BlackTexture1D";

	return Device->createTexture(Desc);
}

nvrhi::TextureHandle FCommonRenderPasses::CreateGrayTexture(nvrhi::IDevice* Device)
{
	if (!Device)
		return nullptr;

	nvrhi::TextureDesc Desc;
	Desc.dimension = nvrhi::TextureDimension::Texture1D;
	Desc.width = 1;
	Desc.height = 1;
	Desc.mipLevels = 1;
	Desc.arraySize = 1;
	Desc.format = nvrhi::Format::RGBA8_UNORM;
	Desc.isUAV = false;
	Desc.isRenderTarget = false;
	Desc.initialState = nvrhi::ResourceStates::ShaderResource;
	Desc.keepInitialState = true;
	Desc.debugName = "GrayTexture1D";

	return Device->createTexture(Desc);
}

nvrhi::TextureHandle FCommonRenderPasses::CreateWhiteTexture(nvrhi::IDevice* Device)
{
	if (!Device)
		return nullptr;

	nvrhi::TextureDesc Desc;
	Desc.dimension = nvrhi::TextureDimension::Texture1D;
	Desc.width = 1;
	Desc.height = 1;
	Desc.mipLevels = 1;
	Desc.arraySize = 1;
	Desc.format = nvrhi::Format::RGBA8_UNORM;
	Desc.isUAV = false;
	Desc.isRenderTarget = false;
	Desc.initialState = nvrhi::ResourceStates::ShaderResource;
	Desc.keepInitialState = true;
	Desc.debugName = "WhiteTexture1D";

	return Device->createTexture(Desc);
}

nvrhi::TextureHandle FCommonRenderPasses::CreateBlackTexture2DArray(nvrhi::IDevice* Device)
{
	if (!Device)
		return nullptr;

	nvrhi::TextureDesc Desc;
	Desc.dimension = nvrhi::TextureDimension::Texture2D;
	Desc.width = 2;
	Desc.height = 2;
	Desc.mipLevels = 1;
	Desc.arraySize = 1;
	Desc.format = nvrhi::Format::RGBA8_UNORM;
	Desc.isUAV = false;
	Desc.isRenderTarget = false;
	Desc.initialState = nvrhi::ResourceStates::ShaderResource;
	Desc.keepInitialState = true;
	Desc.debugName = "BlackTexture2D";

	return Device->createTexture(Desc);
}

nvrhi::TextureHandle FCommonRenderPasses::CreateWhiteTexture2DArray(nvrhi::IDevice* Device)
{
	if (!Device)
		return nullptr;

	nvrhi::TextureDesc Desc;
	Desc.dimension = nvrhi::TextureDimension::Texture2D;
	Desc.width = 2;
	Desc.height = 2;
	Desc.mipLevels = 1;
	Desc.arraySize = 1;
	Desc.format = nvrhi::Format::RGBA8_UNORM;
	Desc.isUAV = false;
	Desc.isRenderTarget = false;
	Desc.initialState = nvrhi::ResourceStates::ShaderResource;
	Desc.keepInitialState = true;
	Desc.debugName = "WhiteTexture2D";

	return Device->createTexture(Desc);
}

// =============================================================================
// BLIT TEXTURE IMPLEMENTATION
// =============================================================================

// Static Blit resources (cached for reuse)
static nvrhi::ShaderHandle		  g_BlitVS;
static nvrhi::ShaderHandle		  g_BlitPS;
static nvrhi::BindingLayoutHandle g_BlitBindingLayout;
static bool						  g_BlitInitialized = false;
static FString					  g_LastShaderDataDir;
// Initialize Blit resources
static bool InitBlitResources(nvrhi::IDevice* Device, const FString& ShaderDataDir)
{
	if (g_BlitInitialized && g_LastShaderDataDir == ShaderDataDir)
		return true;

	if (!Device)
		return false;

	// Load Blit vertex shader from .sblob
	auto BlitVertCode = ReadBinaryFile(
		FPath::Combine(ShaderDataDir, TXT("BlitVS.sblob")).string());
	if (BlitVertCode.empty())
	{
		return false;
	}

	// Extract SPIR-V binary from .sblob
	const void* VSBinary = nullptr;
	size_t VSBinarySize = 0;
	if (!ShaderMake::FindPermutationInBlob(BlitVertCode.data(), BlitVertCode.size(),
		nullptr, 0, &VSBinary, &VSBinarySize))
	{
		return false;
	}

	nvrhi::ShaderDesc VSDesc;
	VSDesc.setShaderType(nvrhi::ShaderType::Vertex);
	VSDesc.setEntryName("main");
	g_BlitVS = Device->createShader(VSDesc, VSBinary, VSBinarySize);
	if (!g_BlitVS)
	{
		return false;
	}

	// Load Blit fragment shader from .sblob
	auto BlitFragCode = ReadBinaryFile(
		FPath::Combine(ShaderDataDir, TXT("BlitPS.sblob")).string());
	if (BlitFragCode.empty())
	{
		return false;
	}

	// Extract SPIR-V binary from .sblob
	const void* PSBinary = nullptr;
	size_t PSBinarySize = 0;
	if (!ShaderMake::FindPermutationInBlob(BlitFragCode.data(), BlitFragCode.size(),
		nullptr, 0, &PSBinary, &PSBinarySize))
	{
		return false;
	}

	nvrhi::ShaderDesc FSDesc;
	FSDesc.setShaderType(nvrhi::ShaderType::Pixel);
	FSDesc.setEntryName("main");
	g_BlitPS = Device->createShader(FSDesc, PSBinary, PSBinarySize);
	if (!g_BlitPS)
	{
		return false;
	}

	// Create Blit binding layout
	g_BlitBindingLayout = CreateBlitBindingLayout(Device);
	if (!g_BlitBindingLayout)
	{
		return false;
	}

	g_LastShaderDataDir = ShaderDataDir;
	g_BlitInitialized = true;
	return true;
}

// Deinitialize Blit resources (reset static variables)
static void DeinitBlitResources()
{
	g_BlitVS = nullptr;
	g_BlitPS = nullptr;
	g_BlitBindingLayout = nullptr;
	g_BlitInitialized = false;
	g_LastShaderDataDir = FString();
}

void FCommonRenderPasses::Shutdown()
{
	if (g_BlitInitialized)
	{
		DeinitBlitResources();
	}
}

void FCommonRenderPasses::SetShaderDataDir(const FString& Directory)
{
	g_ShaderDataDirOverride = Directory;
	// Reset initialization so next BlitTexture will reinitialize with new directory
	if (g_BlitInitialized)
	{
		DeinitBlitResources();
	}
}

void FCommonRenderPasses::BlitTexture(
	nvrhi::ICommandList*  CommandList,
	nvrhi::IFramebuffer*  Framebuffer,
	nvrhi::TextureHandle  SrcTexture,
	FBindingCache*		  BindingCache,
	uint32_t			  Width,
	uint32_t			  Height,
	const BlitParameters& Params)
{
	if (!CommandList || !Framebuffer || !SrcTexture || !BindingCache)
		return;

	nvrhi::IDevice* Device = BindingCache->GetDevice();
	if (!Device)
		return;

	(void)Params; // Sampler type not used for samplerless sampling

	// Get shader data directory
	const auto ShaderDataDir = GetShaderDataDir();

	// Initialize Blit resources if not already done
	if (!InitBlitResources(Device, ShaderDataDir))
	{
		HLVM_LOG(LogCommon, err, TXT("Failed to initialize Blit resources"));
		return;
	}

	// Get the actual framebuffer info (width, height, format) for PSO creation
	nvrhi::FramebufferInfo ActualFBInfo = Framebuffer->getFramebufferInfo();

	// Create PSO for this frame (no caching - avoids framebuffer state issues)
	// NOTE: Use full ActualFBInfo to avoid framebuffer mismatch validation errors
	nvrhi::GraphicsPipelineDesc PSODesc;
	PSODesc.VS = g_BlitVS;
	PSODesc.PS = g_BlitPS;
	PSODesc.primType = nvrhi::PrimitiveType::TriangleStrip;
	PSODesc.renderState.rasterState.setCullNone();
	PSODesc.renderState.depthStencilState.depthTestEnable = false;
	PSODesc.renderState.depthStencilState.stencilEnable = false;
	PSODesc.bindingLayouts = { g_BlitBindingLayout };
	nvrhi::GraphicsPipelineHandle BlitPipeline = Device->createGraphicsPipeline(PSODesc, ActualFBInfo);

	if (!BlitPipeline)
	{
		HLVM_LOG(LogCommon, err, TXT("Failed to create Blit PSO"));
		return;
	}

	// Create BlitParams constant buffer with the mode
	struct FBlitParams
	{
		float Mode;
		float Pad1;
		float Pad2;
		float Pad3;
	};
	FBlitParams BlitParamsData = {};
	BlitParamsData.Mode = static_cast<float>(Params.Mode);

	nvrhi::BufferHandle BlitParamsCB = Device->createBuffer(
		nvrhi::BufferDesc()
			.setByteSize(sizeof(FBlitParams))
			.setIsConstantBuffer(true)
			.setInitialState(nvrhi::ResourceStates::ConstantBuffer)
			.setKeepInitialState(true));
	CommandList->writeBuffer(BlitParamsCB, &BlitParamsData, sizeof(FBlitParams), 0);

	// Create binding set for this texture (samplerless) with BlitParams
	nvrhi::BindingSetDesc BlitSetDesc;
	BlitSetDesc.bindings = {
		nvrhi::BindingSetItem::ConstantBuffer(0, BlitParamsCB),
		nvrhi::BindingSetItem::Texture_SRV(0, SrcTexture)
	};
	nvrhi::BindingSetHandle BlitBindingSet = BindingCache->GetOrCreateBindingSet(BlitSetDesc, g_BlitBindingLayout);

	if (!BlitBindingSet)
	{
		HLVM_LOG(LogCommon, err, TXT("Failed to create Blit binding set"));
		return;
	}

	// Execute blit
	nvrhi::GraphicsState State;
	State.setPipeline(BlitPipeline);
	State.setFramebuffer(Framebuffer);
	State.addBindingSet(BlitBindingSet);

	// Match Donut's pattern: separate viewport and scissor calls
	nvrhi::Viewport BlitViewport(0.0f, float(Width), 0.0f, float(Height), 0.0f, 1.0f);
	State.viewport.addViewport(BlitViewport);
	State.viewport.addScissorRect(nvrhi::Rect(BlitViewport));

	CommandList->setGraphicsState(State);

	nvrhi::DrawArguments Args;
	Args.setVertexCount(4);
	Args.setInstanceCount(1);
	CommandList->draw(Args);
}
