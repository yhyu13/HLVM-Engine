/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "IVulkanDynamicRHI.h"
#include <functional>

class FVulkanRHI final : public IVulkanDynamicRHI
{
public:
	struct FInitializer
	{
		TVector<TVector<FString>>				RequiredExtensions;
		std::function<VkSurfaceKHR(VkInstance)> CreateSurfaceFunc;
	};

public:
	explicit FVulkanRHI(const FInitializer& Params);

	// Initialization and Shutdown
	virtual void Init() override;
	virtual void Shutdown() override;

	// Resource Creation
	virtual FTextureRHIRef			   CreateTexture(const FRHITextureCreateDesc& CreateDesc) override;
	virtual FBufferRHIRef			   CreateBuffer(const FRHIBufferCreateDesc& CreateDesc) override;
	virtual FShaderResourceViewRHIRef  CreateShaderResourceView(FRHITexture* Texture, const FRHIShaderResourceViewCreateInfo& CreateInfo) override;
	virtual FUnorderedAccessViewRHIRef CreateUnorderedAccessView(FRHIBuffer* Buffer, const FRHIUnorderedAccessViewCreateInfo& CreateInfo) override;
	virtual FVertexDeclarationRHIRef   CreateVertexDeclaration(const FVertexDeclarationElementList& Elements) override;

	// Shader Management
	virtual FShaderRHIRef CreateShader(const FShaderCreateInfo& CreateInfo) override;
	virtual void		  ReleaseShader(FShaderRHIRef& Shader) override;

	// Pipeline State Management
	virtual FRHIGraphicsPipelineState* CreateGraphicsPipelineState(const FGraphicsPipelineStateInitializer& Initializer) override;
	virtual FRHIComputePipelineState*  CreateComputePipelineState(const FComputePipelineStateInitializer& Initializer) override;

	// Command List and Context
	virtual FRHICommandListImmediate& GetImmediateCommandList() override;
	virtual FRHIComputeCommandList&	  GetComputeCommandList() override;

	// Synchronization
	virtual void RHISubmitCommandsAndFlushGPU() override;
	virtual void RHIFlushResources() override;

	// Viewport and Swap Chain
	virtual void RHICreateViewport(void* WindowHandle, TUINT32 Width, TUINT32 Height, bool bIsFullscreen, EPixelFormat PreferredPixelFormat, FViewportRHIRef& OutViewport) override;
	virtual void RHIResizeViewport(FViewportRHIRef& Viewport, TUINT32 Width, TUINT32 Height, bool bIsFullscreen, EPixelFormat PreferredPixelFormat) override;
	virtual void RHISwapBuffers(FViewportRHIRef& Viewport) override;

	// Render Pass and Draw Commands
	virtual void RHIBeginRenderPass(const FRHIRenderPassInfo& RenderPassInfo, const TCHAR* Name) override;
	virtual void RHIEndRenderPass() override;
	virtual void RHIDrawPrimitive(TUINT32 BaseVertexIndex, TUINT32 NumPrimitives, TUINT32 NumInstances) override;
	virtual void RHIDrawIndexedPrimitive(FRHIBuffer* IndexBuffer, TUINT32 BaseVertexIndex, TUINT32 FirstInstance, TUINT32 NumVertices, TUINT32 StartIndex, TUINT32 NumPrimitives, TUINT32 NumInstances) override;

	// Compute Dispatch
	virtual void RHIDispatchComputeShader(TUINT32 ThreadGroupCountX, TUINT32 ThreadGroupCountY, TUINT32 ThreadGroupCountZ) override;

	// Query and Timestamp
	virtual FRHIQueryRHIRef CreateQuery(ERHIQueryType QueryType) override;
	virtual void			RHIBeginQuery(FRHIQueryRHIRef& Query) override;
	virtual void			RHIEndQuery(FRHIQueryRHIRef& Query) override;
	virtual void			RHIGetQueryResults(FRHIQueryRHIRef& Query, TUINT64& OutResult, bool bWait) override;

	// Debugging and Profiling
	virtual void RHIPushEvent(const TCHAR* Name) override;
	virtual void RHIPopEvent() override;

	// Memory Management
	virtual void RHIFlushPendingDeletes() override;

	// Misc
	virtual void RHISetGraphicsPipelineState(FRHIGraphicsPipelineState* PipelineState) override;
	virtual void RHISetComputePipelineState(FRHIComputePipelineState* PipelineState) override;
	virtual void RHISetViewport(TUINT32 MinX, TUINT32 MinY, float MinZ, TUINT32 MaxX, TUINT32 MaxY, float MaxZ) override;
	virtual void RHISetScissorRect(bool bEnable, TUINT32 MinX, TUINT32 MinY, TUINT32 MaxX, TUINT32 MaxY) override;

	// Vulkan-specific resource creation
	VkImage		 CreateVulkanImage(const FRHITextureCreateDesc& CreateDesc) override;
	VkBuffer	 CreateVulkanBuffer(const FRHIBufferCreateDesc& CreateDesc) override;
	VkImageView	 CreateVulkanImageView(VkImage Image, const FRHIShaderResourceViewCreateInfo& CreateInfo) override;
	VkBufferView CreateVulkanBufferView(VkBuffer Buffer, const FRHIUnorderedAccessViewCreateInfo& CreateInfo) override;

	// Vulkan-specific command list management
	VkCommandBuffer BeginVulkanCommandBuffer() override;
	void			EndVulkanCommandBuffer(VkCommandBuffer CommandBuffer) override;

	// Vulkan-specific synchronization
	void SubmitVulkanCommandsAndFlushGPU() override;
	void FlushVulkanResources() override;

	// Vulkan-specific viewport and swap chain management
	void CreateVulkanSwapChain(void* WindowHandle, TUINT32 Width, TUINT32 Height, bool bIsFullscreen, EPixelFormat PreferredPixelFormat, FViewportRHIRef& OutViewport) override;
	void ResizeVulkanSwapChain(FViewportRHIRef& Viewport, TUINT32 Width, TUINT32 Height, bool bIsFullscreen, EPixelFormat PreferredPixelFormat) override;
	void PresentVulkanSwapChain(FViewportRHIRef& Viewport) override;

	// Vulkan-specific render pass management
	void BeginVulkanRenderPass(const FRHIRenderPassInfo& RenderPassInfo, const TCHAR* Name) override;
	void EndVulkanRenderPass() override;

	// Vulkan-specific query and timestamp management
	VkQueryPool CreateVulkanQueryPool(ERHIQueryType QueryType) override;
	void		BeginVulkanQuery(VkQueryPool QueryPool, TUINT32 QueryIndex) override;
	void		EndVulkanQuery(VkQueryPool QueryPool, TUINT32 QueryIndex) override;
	void		GetVulkanQueryResults(VkQueryPool QueryPool, TUINT32 QueryIndex, TUINT64& OutResult, bool bWait) override;

	// Vulkan-specific debugging and profiling
	void PushVulkanEvent(const TCHAR* Name) override;
	void PopVulkanEvent() override;

	// Vulkan-specific memory management
	void FlushVulkanPendingDeletes() override;

protected:
	// Vulkan-specific initialization
	void CreateVulkanInstance();
	void CreateDebugLayer();
	void CreateSurface();
	void CreateVulkanPhysicalDevice();
	void CreateVulkanLogicalDevice();
	void CreateVulkanQueues();

	void CreateVulkanMemoryAllocator();

	// Function to generate VkImageCreateInfo from FRHITextureCreateDesc
	VkImageCreateInfo GenerateVkImageCreateInfo(const FRHITextureCreateDesc& CreateDesc);

	// Function to generate VkBufferCreateInfo from FRHIBufferCreateDesc
	VkBufferCreateInfo GenerateVkBufferCreateInfo(const FRHIBufferCreateDesc& CreateDesc);

	// Function to generate VkShaderModuleCreateInfo from FShaderCreateInfo
	VkShaderModuleCreateInfo GenerateVkShaderModuleCreateInfo(const FShaderCreateInfo& CreateDesc);

	// Function to generate VkImageViewCreateInfo from FRHIShaderResourceViewCreateInfo
	VkImageViewCreateInfo GenerateVkImageViewCreateInfo(const FRHIShaderResourceViewCreateInfo& CreateDesc);

	// Function to generate VkBufferViewCreateInfo from FRHIUnorderedAccessViewCreateInfo
	VkBufferViewCreateInfo GenerateVkBufferViewCreateInfo(const FRHIUnorderedAccessViewCreateInfo& CreateDesc);

	// Function to generate VkSamplerCreateInfo from FRHISamplerStateCreateInfo (assuming this struct exists)
	VkSamplerCreateInfo GenerateVkSamplerCreateInfo(const FRHISamplerStateCreateInfo& CreateDesc);

	// Function to generate VkPipelineShaderStageCreateInfo from FShaderCreateInfo
	VkPipelineShaderStageCreateInfo GenerateVkPipelineShaderStageCreateInfo(const FShaderCreateInfo& CreateDesc);

	// Function to generate VkPipelineLayoutCreateInfo from FRHIGraphicsPipelineStateCreateInfo (assuming this struct exists)
	VkPipelineLayoutCreateInfo GenerateVkPipelineLayoutCreateInfo(const FRHIGraphicsPipelineLayoutCreateInfo& CreateDesc);

	// Function to generate VkGraphicsPipelineCreateInfo from FRHIGraphicsPipelineStateCreateInfo (assuming this struct exists)
	VkGraphicsPipelineCreateInfo GenerateVkGraphicsPipelineCreateInfo(const FRHIGraphicsPipelineStateCreateInfo& CreateDesc);

	// Function to generate VkComputePipelineCreateInfo from FRHIComputePipelineStateCreateInfo (assuming this struct exists)
	VkComputePipelineCreateInfo GenerateVkComputePipelineCreateInfo(const FRHIComputePipelineStateCreateInfo& CreateDesc);

	// Function to generate VkQueryPoolCreateInfo from FRHIQueryCreateInfo (assuming this struct exists)
	VkQueryPoolCreateInfo GenerateVkQueryPoolCreateInfo(const FRHIQueryCreateInfo& CreateDesc);

	// Function to allocate Vulkan memory for a given buffer and usage flags
	VkDeviceMemory AllocateVulkanMemory(VkBuffer Buffer, EBufferUsageFlags UsageFlags);

private:
	FInitializer InitializerParam;

	// Vulkan-specific members and methods
	VkInstance				 VulkanInstance;
	VkDebugUtilsMessengerEXT DebugMessenger;
	VkSurfaceKHR			 VulkanSurface; // 用于显示的窗口句柄
	VkDevice				 VulkanDevice;
	VkPhysicalDevice		 VulkanPhysicalDevice;
	VkQueue					 GraphicsQueue;
	VkQueue					 ComputeQueue;
	VkQueue					 TransferQueue;
	VkQueue					 PresentQueue;
	VkSwapchainKHR			 VulkanSwapChain;
};
