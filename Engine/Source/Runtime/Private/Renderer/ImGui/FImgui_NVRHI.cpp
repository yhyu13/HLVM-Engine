/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * ImGui NVRHI Renderer Implementation (MVP-based)
 *
 * Provides NVRHI-based rendering for Dear ImGui using MVP matrix approach.
 * Adapted from NVIDIA Donut framework.
 */

#include "Renderer/ImGui/FImgui_NVRHI.h"
#include "Renderer/FShaderFactory.h"

#include <Core/Log.h>
#include <Renderer/RHI/Object/Buffer.h>
#include <Platform/FileSystem/Path.h>

DECLARE_LOG_CATEGORY(LogImguiNew)

bool FImgui_NVRHI::Initialize(nvrhi::IDevice* device, std::shared_ptr<FShaderFactory> shaderFactory)
{
	Device = device;
	ShaderFactory = shaderFactory;

	if (!Device)
	{
		HLVM_LOG(LogImguiNew, err, TXT("FImgui_NVRHI::Initialize: Device is null"));
		return false;
	}

	if (!ShaderFactory)
	{
		HLVM_LOG(LogImguiNew, err, TXT("FImgui_NVRHI::Initialize: ShaderFactory is null"));
		return false;
	}

	nvrhi::CommandListParameters params;
	params.enableImmediateExecution = false;
	CommandList = Device->createCommandList(params);

	if (!CommandList)
	{
		HLVM_LOG(LogImguiNew, err, TXT("FImgui_NVRHI::Initialize: Failed to create command list"));
		return false;
	}

	const FString shaderDir = FString::Format(TXT("{}/Engine/Source/Runtime/ThirdParty/Imgui/Shader"), *GProjectRoot);
	const FPath	  vertPath(shaderDir + TXT("/imgui_vertex_mvp.spv"), EPlatformFileType::Disk);
	const FPath	  fragPath(shaderDir + TXT("/imgui_fragment_mvp.spv"), EPlatformFileType::Disk);

	nvrhi::ShaderDesc vsDesc;
	vsDesc.setShaderType(nvrhi::ShaderType::Vertex);
	VertexShader = ShaderFactory->CreateAutoShader(
		vertPath.string(),
		"main",
		FStaticShader(),
		FStaticShader(),
		FStaticShader(),
		nullptr,
		vsDesc);

	nvrhi::ShaderDesc psDesc;
	psDesc.setShaderType(nvrhi::ShaderType::Pixel);
	PixelShader = ShaderFactory->CreateAutoShader(
		fragPath.string(),
		"main",
		FStaticShader(),
		FStaticShader(),
		FStaticShader(),
		nullptr,
		psDesc);

	if (!VertexShader || !PixelShader)
	{
		HLVM_LOG(LogImguiNew, err, TXT("FImgui_NVRHI::Initialize: Failed to create shaders"));
		return false;
	}

	nvrhi::VertexAttributeDesc vertexAttribLayout[3];
	vertexAttribLayout[0].setName("aPos").setFormat(nvrhi::Format::RG32_FLOAT).setOffset(0).setElementStride(sizeof(ImDrawVert));
	vertexAttribLayout[1].setName("aUV").setFormat(nvrhi::Format::RG32_FLOAT).setOffset(8).setElementStride(sizeof(ImDrawVert));
	vertexAttribLayout[2].setName("aColor").setFormat(nvrhi::Format::RGBA8_UNORM).setOffset(16).setElementStride(sizeof(ImDrawVert));

	ShaderAttribLayout = Device->createInputLayout(vertexAttribLayout, 3, VertexShader);

	if (!ShaderAttribLayout)
	{
		HLVM_LOG(LogImguiNew, err, TXT("FImgui_NVRHI::Initialize: Failed to create input layout"));
		return false;
	}

	nvrhi::SamplerDesc samplerDesc;
	samplerDesc.setAllAddressModes(nvrhi::SamplerAddressMode::Wrap);
	samplerDesc.setAllFilters(true);
	FontSampler = Device->createSampler(samplerDesc);

	if (!FontSampler)
	{
		HLVM_LOG(LogImguiNew, err, TXT("FImgui_NVRHI::Initialize: Failed to create sampler"));
		return false;
	}

	nvrhi::BindingLayoutDesc layoutDesc;
	layoutDesc.visibility = nvrhi::ShaderType::All;
	// Note: Sampler(0) maps to SPIR-V binding 128 via VulkanBindingOffsets.sampler default
	layoutDesc.bindings = {
		nvrhi::BindingLayoutItem::PushConstants(2, sizeof(VERTEX_CONSTANT_BUFFER)),
		nvrhi::BindingLayoutItem::Texture_SRV(0),
		nvrhi::BindingLayoutItem::Sampler(0)
	};
	BindingLayout = Device->createBindingLayout(layoutDesc);

	if (!BindingLayout)
	{
		HLVM_LOG(LogImguiNew, err, TXT("FImgui_NVRHI::Initialize: Failed to create binding layout"));
		return false;
	}

	nvrhi::BlendState blendState;
	blendState.targets[0].setBlendEnable(true).setSrcBlend(nvrhi::BlendFactor::SrcAlpha).setDestBlend(nvrhi::BlendFactor::InvSrcAlpha).setSrcBlendAlpha(nvrhi::BlendFactor::InvSrcAlpha).setDestBlendAlpha(nvrhi::BlendFactor::Zero);

	nvrhi::RasterState rasterState;
	rasterState.setFillSolid().setCullNone().setScissorEnable(true).setDepthClipEnable(true);

	nvrhi::DepthStencilState depthStencilState;
	depthStencilState.disableDepthTest().enableDepthWrite().disableStencil().setDepthFunc(nvrhi::ComparisonFunc::Always);

	nvrhi::GraphicsPipelineDesc pipeDesc;
	pipeDesc.setPrimType(nvrhi::PrimitiveType::TriangleList);
	pipeDesc.setInputLayout(ShaderAttribLayout);
	pipeDesc.setVertexShader(VertexShader);
	pipeDesc.setPixelShader(PixelShader);
	pipeDesc.renderState.blendState = blendState;
	pipeDesc.renderState.depthStencilState = depthStencilState;
	pipeDesc.renderState.rasterState = rasterState;
	pipeDesc.addBindingLayout(BindingLayout);
	PipelineDesc = pipeDesc;

	HLVM_LOG(LogImguiNew, info, TXT("FImgui_NVRHI::Initialize: Successfully initialized"));
	return true;
}

bool FImgui_NVRHI::UpdateFontTexture()
{
	if (!Device)
		return false;

	ImGuiIO& io = ImGui::GetIO();

	if (FontTexture && io.Fonts->TexID != nullptr)
	{
		return true;
	}

	unsigned char* pixels = nullptr;
	int			   width = 0;
	int			   height = 0;

	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	if (!pixels)
	{
		HLVM_LOG(LogImguiNew, err, TXT("FImgui_NVRHI::UpdateFontTexture: Failed to get font data"));
		return false;
	}

	nvrhi::TextureDesc textureDesc;
	textureDesc.width = static_cast<uint32_t>(width);
	textureDesc.height = static_cast<uint32_t>(height);
	textureDesc.format = nvrhi::Format::RGBA8_UNORM;
	textureDesc.debugName = "ImGui Font Texture";
	textureDesc.isShaderResource = true;

	FontTexture = Device->createTexture(textureDesc);

	if (!FontTexture)
	{
		HLVM_LOG(LogImguiNew, err, TXT("FImgui_NVRHI::UpdateFontTexture: Failed to create texture"));
		return false;
	}

	CommandList->open();
	CommandList->beginTrackingTextureState(FontTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::Common);
	CommandList->writeTexture(FontTexture, 0, 0, pixels, static_cast<size_t>(width) * 4);
	CommandList->setPermanentTextureState(FontTexture, nvrhi::ResourceStates::ShaderResource);
	CommandList->commitBarriers();
	CommandList->close();
	Device->executeCommandList(CommandList);

	io.Fonts->TexID = reinterpret_cast<ImTextureID>(FontTexture.Get());

	HLVM_LOG(LogImguiNew, info, TXT("FImgui_NVRHI::UpdateFontTexture: Font texture created {}x{}"), width, height);
	return true;
}

bool FImgui_NVRHI::ReallocateBuffer(nvrhi::BufferHandle& buffer, size_t requiredSize, size_t reallocateSize, bool isIndexBuffer)
{
	if (!Device)
		return false;

	if (buffer == nullptr || buffer->getDesc().byteSize < requiredSize)
	{
		nvrhi::BufferDesc desc;
		desc.byteSize = static_cast<uint32_t>(reallocateSize);
		desc.structStride = 0;
		desc.debugName = isIndexBuffer ? "ImGui Index Buffer" : "ImGui Vertex Buffer";
		desc.canHaveUAVs = false;
		desc.isVertexBuffer = !isIndexBuffer;
		desc.isIndexBuffer = isIndexBuffer;
		desc.isDrawIndirectArgs = false;
		desc.isVolatile = false;
		desc.initialState = isIndexBuffer ? nvrhi::ResourceStates::IndexBuffer : nvrhi::ResourceStates::VertexBuffer;
		desc.keepInitialState = true;

		buffer = Device->createBuffer(desc);

		if (!buffer)
		{
			HLVM_LOG(LogImguiNew, err, TXT("FImgui_NVRHI::ReallocateBuffer: Failed to create buffer"));
			return false;
		}
	}

	return true;
}

nvrhi::IGraphicsPipeline* FImgui_NVRHI::GetPipeline(nvrhi::FramebufferInfo const& framebufferInfo)
{
	if (!Device)
		return nullptr;

	if (Pipeline)
	{
		return Pipeline.Get();
	}

	Pipeline = Device->createGraphicsPipeline(PipelineDesc, framebufferInfo);

	if (!Pipeline)
	{
		HLVM_LOG(LogImguiNew, err, TXT("FImgui_NVRHI::GetPipeline: Failed to create graphics pipeline"));
		return nullptr;
	}

	return Pipeline.Get();
}

nvrhi::IBindingSet* FImgui_NVRHI::GetBindingSet(nvrhi::ITexture* texture, nvrhi::IGraphicsPipeline* pipeline)
{
	if (!Device)
		return nullptr;

	auto key = std::make_pair(texture, pipeline);
	auto iter = BindingsCache.find(key);
	if (iter != BindingsCache.end())
	{
		return iter->second.Get();
	}

	nvrhi::BindingSetDesc desc;
	desc.bindings = {
		nvrhi::BindingSetItem::PushConstants(2, sizeof(VERTEX_CONSTANT_BUFFER)),
		nvrhi::BindingSetItem::Texture_SRV(0, texture),
		nvrhi::BindingSetItem::Sampler(0, FontSampler)
	};

	nvrhi::BindingSetHandle binding = Device->createBindingSet(desc, BindingLayout);

	if (!binding)
	{
		HLVM_LOG(LogImguiNew, err, TXT("FImgui_NVRHI::GetBindingSet: Failed to create binding set"));
		return nullptr;
	}

	BindingsCache[key] = binding;
	return binding.Get();
}

bool FImgui_NVRHI::UpdateGeometry(nvrhi::ICommandList* cmdList)
{
	if (!Device)
		return false;

	ImDrawData* drawData = ImGui::GetDrawData();
	if (!drawData || drawData->CmdListsCount == 0)
	{
		return true;
	}

	size_t vtxRequiredSize = static_cast<size_t>(drawData->TotalVtxCount) * sizeof(ImDrawVert);
	size_t vtxReallocSize = (static_cast<size_t>(drawData->TotalVtxCount) + 5000) * sizeof(ImDrawVert);
	if (!ReallocateBuffer(VertexBuffer, vtxRequiredSize, vtxReallocSize, false))
	{
		return false;
	}

	size_t idxRequiredSize = static_cast<size_t>(drawData->TotalIdxCount) * sizeof(ImDrawIdx);
	size_t idxReallocSize = (static_cast<size_t>(drawData->TotalIdxCount) + 5000) * sizeof(ImDrawIdx);
	if (!ReallocateBuffer(IndexBuffer, idxRequiredSize, idxReallocSize, true))
	{
		return false;
	}

	VertexBufferData.resize(VertexBuffer->getDesc().byteSize / sizeof(ImDrawVert));
	IndexBufferData.resize(IndexBuffer->getDesc().byteSize / sizeof(ImDrawIdx));

	ImDrawVert* vtxDst = &VertexBufferData[0];
	ImDrawIdx*	idxDst = &IndexBufferData[0];

	for (int n = 0; n < drawData->CmdListsCount; n++)
	{
		const ImDrawList* cmdListPtr = drawData->CmdLists[n];

		memcpy(vtxDst, cmdListPtr->VtxBuffer.Data, static_cast<size_t>(cmdListPtr->VtxBuffer.Size) * sizeof(ImDrawVert));
		memcpy(idxDst, cmdListPtr->IdxBuffer.Data, static_cast<size_t>(cmdListPtr->IdxBuffer.Size) * sizeof(ImDrawIdx));

		vtxDst += cmdListPtr->VtxBuffer.Size;
		idxDst += cmdListPtr->IdxBuffer.Size;
	}

	cmdList->writeBuffer(VertexBuffer, &VertexBufferData[0], VertexBuffer->getDesc().byteSize);
	cmdList->writeBuffer(IndexBuffer, &IndexBufferData[0], IndexBuffer->getDesc().byteSize);

	return true;
}

void FImgui_NVRHI::Render(nvrhi::IFramebuffer* framebuffer)
{
	if (!Device)
		return;

	ImDrawData* drawData = ImGui::GetDrawData();
	if (!drawData || drawData->CmdListsCount == 0)
	{
		return;
	}

	const ImGuiIO& io = ImGui::GetIO();

	CommandList->open();

	if (!UpdateGeometry(CommandList))
	{
		HLVM_LOG(LogImguiNew, err, TXT("FImgui_NVRHI::Render: Failed to update geometry"));
		CommandList->close();
		return;
	}

	drawData->ScaleClipRects(io.DisplayFramebufferScale);

	nvrhi::GraphicsState drawState;
	drawState.framebuffer = framebuffer;
	drawState.pipeline = GetPipeline(framebuffer->getFramebufferInfo());

	if (!drawState.pipeline)
	{
		HLVM_LOG(LogImguiNew, err, TXT("FImgui_NVRHI::Render: No pipeline available"));
		CommandList->close();
		return;
	}

	drawState.viewport.viewports.push_back(nvrhi::Viewport(
		io.DisplaySize.x * io.DisplayFramebufferScale.x,
		io.DisplaySize.y * io.DisplayFramebufferScale.y));
	drawState.viewport.scissorRects.resize(1);

	nvrhi::VertexBufferBinding vbufBinding;
	vbufBinding.buffer = VertexBuffer;
	vbufBinding.slot = 0;
	vbufBinding.offset = 0;
	drawState.vertexBuffers.push_back(vbufBinding);

	drawState.indexBuffer.buffer = IndexBuffer;
	drawState.indexBuffer.format = (sizeof(ImDrawIdx) == 2 ? nvrhi::Format::R16_UINT : nvrhi::Format::R32_UINT);
	drawState.indexBuffer.offset = 0;

	int vtxOffset = 0;
	int idxOffset = 0;
	for (int n = 0; n < drawData->CmdListsCount; n++)
	{
		const ImDrawList* cmdListPtr = drawData->CmdLists[n];
		for (int i = 0; i < cmdListPtr->CmdBuffer.Size; i++)
		{
			const ImDrawCmd* cmd = &cmdListPtr->CmdBuffer[i];

			if (cmd->UserCallback)
			{
				cmd->UserCallback(cmdListPtr, cmd);
			}
			else
			{
				drawState.bindings = { GetBindingSet(static_cast<nvrhi::ITexture*>(cmd->TextureId), drawState.pipeline) };

				drawState.viewport.scissorRects[0] = nvrhi::Rect(
					static_cast<int>(cmd->ClipRect.x),
					static_cast<int>(cmd->ClipRect.z),
					static_cast<int>(cmd->ClipRect.y),
					static_cast<int>(cmd->ClipRect.w));

				nvrhi::DrawArguments drawArgs;
				drawArgs.vertexCount = cmd->ElemCount;
				drawArgs.startIndexLocation = static_cast<uint32_t>(idxOffset);
				drawArgs.startVertexLocation = static_cast<uint32_t>(vtxOffset);

				CommandList->setGraphicsState(drawState);

				VERTEX_CONSTANT_BUFFER mvp;
				float				   scaleX = 2.0f / io.DisplaySize.x;
				float				   scaleY = 2.0f / io.DisplaySize.y;
				float				   translateX = -1.0f;
				float				   translateY = 1.0f;

				mvp.mvp[0][0] = scaleX;
				mvp.mvp[0][1] = 0.0f;
				mvp.mvp[0][2] = 0.0f;
				mvp.mvp[0][3] = 0.0f;
				mvp.mvp[1][0] = 0.0f;
				mvp.mvp[1][1] = -scaleY;
				mvp.mvp[1][2] = 0.0f;
				mvp.mvp[1][3] = 0.0f;
				mvp.mvp[2][0] = 0.0f;
				mvp.mvp[2][1] = 0.0f;
				mvp.mvp[2][2] = 1.0f;
				mvp.mvp[2][3] = 0.0f;
				mvp.mvp[3][0] = translateX;
				mvp.mvp[3][1] = translateY;
				mvp.mvp[3][2] = 0.0f;
				mvp.mvp[3][3] = 1.0f;

				CommandList->setPushConstants(&mvp, sizeof(mvp));

				CommandList->drawIndexed(drawArgs);
			}

			idxOffset += cmd->ElemCount;
		}

		vtxOffset += cmdListPtr->VtxBuffer.Size;
	}

	CommandList->close();
	Device->executeCommandList(CommandList);
}

void FImgui_NVRHI::BackBufferResizing()
{
	Pipeline = nullptr;
	BindingsCache.clear();
}

void FImgui_NVRHI::Shutdown()
{
	// Clear all NVRHI resources first
	BindingsCache.clear();
	Pipeline = nullptr;
	BindingLayout = nullptr;
	VertexBuffer = nullptr;
	IndexBuffer = nullptr;
	VertexBufferData.clear();
	IndexBufferData.clear();
	FontTexture = nullptr;
	FontSampler = nullptr;
	VertexShader = nullptr;
	PixelShader = nullptr;
	ShaderAttribLayout = nullptr;
	CommandList = nullptr;

	// Reset device last to ensure all operations complete before device is potentially destroyed
	Device = nullptr;

	HLVM_LOG(LogImguiNew, info, TXT("FImgui_NVRHI::Shutdown: Shutdown complete"));
}
