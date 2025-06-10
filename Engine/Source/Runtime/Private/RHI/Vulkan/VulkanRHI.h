/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "RHI/Vulkan/IVulkanDynamicRHI.h"
#include "VulkanResourcePost.h"

struct FVulkanRHIInitializer
{
	TVector<TVector<FString>>				RequiredExtensions;
	std::function<VkSurfaceKHR(VkInstance)> CreateSurfaceFunc;
	TSharedPtr<IWindow>						NativeWindowHandle;
};

class FVulkanRHI final : public IVulkanDynamicRHI
{
public:
	HLVM_STATIC_FUNC FVulkanRHI* Get()
	{
		return RHI::GetDynamicRHI<FVulkanRHI>();
	}

	FVulkanRHI() = delete;
	explicit FVulkanRHI(const FVulkanRHIInitializer& Params);

	// Initialization and Shutdown
	virtual void Init() override;
	virtual void Shutdown() override;

	// Resource Creation
	virtual FRHITextureRef			   CreateTexture(const FRHITextureCreateInfo& CreateInfo) override;
	virtual FRHISamplerStateRef		   CreateSamplerState(const FRHISamplerStateCreateInfo& CreateInfo) override;
	virtual FRHIBufferRef			   CreateBuffer(const FRHIBufferCreateInfo& CreateInfo) override;
	virtual FShaderResourceViewRHIRef  CreateShaderResourceView(FRHITexture* Texture, const FRHIShaderResourceViewCreateInfo& CreateInfo) override;
	virtual FUnorderedAccessViewRHIRef CreateUnorderedAccessView(FRHIBuffer* Buffer, const FRHIUnorderedAccessViewCreateInfo& CreateInfo) override;
	virtual FVertexDeclarationRHIRef   CreateVertexDeclaration(const FVertexDeclarationElementList& Elements) override;

	// Shader Management
	virtual FRHIShaderRef CreateShader(const FShaderCreateInfo& CreateInfo) override;

	// Pipeline State Management
	virtual FRHIGraphicsPSO* CreateGraphicsPSO(const FGraphicsPSOCreateInfo& Initializer) override;
	virtual FRHIComputePSO*	 CreateComputePSO(const FComputePSOInitializer& Initializer) override;

	// Command List and Context
	virtual FRHICommandListImmediate& GetImmediateCommandList() override;
	virtual FRHIComputeCommandList&	  GetComputeCommandList() override;

	// Synchronization
	virtual void RHISubmitCommandsAndFlushGPU() override;
	virtual void RHIFlushResources() override;

	// Viewport and Swap Chain
	virtual void			RHICreateViewport(void* WindowHandle, TUINT32 Width, TUINT32 Height, bool bIsFullscreen, EPixelFormat PreferredPixelFormat, FRHIViewportRef& OutViewport) override;
	virtual void			RHIResizeViewport(FRHIViewportRef& Viewport, TUINT32 Width, TUINT32 Height, bool bIsFullscreen, EPixelFormat PreferredPixelFormat) override;
	virtual void			RHISwapBuffers(FRHIViewportRef& Viewport) override;
	virtual FRHITextureRef	GetRHIBackBuffer() override;
	virtual FRHIViewportRef GetRHIViewport() override;

	// Render Pass and Draw Commands
	virtual void RHIBeginRenderPass(const FRHIRenderPassInfo& RenderPassInfo) override;
	virtual void RHIEndRenderPass() override;
	virtual void RHIDrawPrimitive(TUINT32 BaseVertexIndex, TUINT32 NumPrimitives, TUINT32 NumInstances) override;
	virtual void RHIDrawIndexedPrimitive(FRHIBuffer* IndexBuffer, TUINT32 BaseVertexIndex, TUINT32 FirstInstance, TUINT32 NumVertices, TUINT32 StartIndex, TUINT32 NumPrimitives, TUINT32 NumInstances) override;

	// Compute Dispatch
	virtual void RHIDispatchComputeShader(TUINT32 ThreadGroupCountX, TUINT32 ThreadGroupCountY, TUINT32 ThreadGroupCountZ) override;

	// Query and Timestamp
	virtual FQueryRHIRef CreateQuery(ERHIQueryType QueryType) override;
	virtual void		 RHIBeginQuery(FQueryRHIRef& Query) override;
	virtual void		 RHIEndQuery(FQueryRHIRef& Query) override;
	virtual void		 RHIGetQueryResults(FQueryRHIRef& Query, TUINT64& OutResult, bool bWait) override;

	// Debugging and Profiling
	virtual void RHIPushEvent(const TCHAR* Name) override;
	virtual void RHIPopEvent() override;

	// Memory Management
	virtual void RHIFlushPendingDeletes() override;

	// Misc
	virtual void RHISetGraphicsPSO(FRHIGraphicsPSO* PipelineState) override;
	virtual void RHISetComputePSO(FRHIComputePSO* PipelineState) override;
	virtual void RHISetViewport(TUINT32 MinX, TUINT32 MinY, float MinZ, TUINT32 MaxX, TUINT32 MaxY, float MaxZ) override;
	virtual void RHISetScissorRect(bool bEnable, TUINT32 MinX, TUINT32 MinY, TUINT32 MaxX, TUINT32 MaxY) override;

	// Vulkan-specific resource creation
	VkImage		 CreateVulkanImage(const FRHITextureCreateInfo& CreateInfo) override;
	void		 DestroyVulkanImage(VkImage Image) override;
	VkSampler	 CreateVulkanSampler(const FRHISamplerStateCreateInfo& CreateInfo) override;
	VkBuffer	 CreateVulkanBuffer(const FRHIBufferCreateInfo& CreateInfo, void** OutAllocation) override;
	void		 DestroyVulkanBuffer(VkBuffer Buffer, void** InAllocation) override;
	VkImageView	 CreateVulkanImageView(VkImage Image, const FRHIShaderResourceViewCreateInfo& CreateInfo) override;
	VkBufferView CreateVulkanBufferView(VkBuffer Buffer, const FRHIUnorderedAccessViewCreateInfo& CreateInfo) override;

	VkShaderModule CreateVulkanShaderModule(const FShaderCreateInfo& CreateInfo) override;
	void		   DestroyVulkanShaderModule(VkShaderModule ShaderModule) override;

	// Vulkan-specific command list management
	VkCommandBuffer BeginVulkanCommandBuffer() override;
	void			EndVulkanCommandBuffer(VkCommandBuffer CommandBuffer) override;

	// Vulkan-specific synchronization
	void SubmitVulkanCommandsAndFlushGPU() override;
	void FlushVulkanResources() override;

	// Vulkan-specific viewport and swap chain management
	void CreateVulkanSwapChain(void* WindowHandle, TUINT32 Width, TUINT32 Height, bool bIsFullscreen, EPixelFormat PreferredPixelFormat, FRHIViewportRef& OutViewport) override;
	void ResizeVulkanSwapChain(FRHIViewportRef& Viewport, TUINT32 Width, TUINT32 Height, bool bIsFullscreen, EPixelFormat PreferredPixelFormat) override;
	void PresentVulkanSwapChain(FRHIViewportRef& Viewport) override;

	// Vulkan-specific render pass management
	void BeginVulkanRenderPass(const FRHIRenderPassInfo& RenderPassInfo) override;
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

	void SetVulkanMinimalContext(void* InContext) const override;

private:
	// Vulkan-specific initialization
	void CreateVulkanInstance();
	void CreateDebugLayer();
	void CreateSurface();
	void CreateVulkanPhysicalDevice();
	void CreateVulkanLogicalDevice();
	void CreateGlobals();

	void CreateVulkanQueues();
	void CreateVulkanViewPort();

	void CreateVulkanMemoryAllocator();

	// Function to generate VkImageCreateInfo from FRHITextureCreateInfo
	VkImageCreateInfo GenerateVkImageCreateInfo(const FRHITextureCreateInfo& CreateInfo);

	// Function to generate VkBufferCreateInfo from FRHIBufferCreateInfo
	VkBufferCreateInfo GenerateVkBufferCreateInfo(const FRHIBufferCreateInfo& CreateInfo);

	// Function to generate VkShaderModuleCreateInfo from FShaderCreateInfo
	VkShaderModuleCreateInfo GenerateVkShaderModuleCreateInfo(const FShaderCreateInfo& CreateInfo);

	// Function to generate VkImageViewCreateInfo from FRHIShaderResourceViewCreateInfo
	VkImageViewCreateInfo GenerateVkImageViewCreateInfo(const FRHIShaderResourceViewCreateInfo& CreateInfo);

	// Function to generate VkBufferViewCreateInfo from FRHIUnorderedAccessViewCreateInfo
	VkBufferViewCreateInfo GenerateVkBufferViewCreateInfo(const FRHIUnorderedAccessViewCreateInfo& CreateInfo);

	// Function to generate VkSamplerCreateInfo from FRHISamplerStateCreateInfo (assuming this struct exists)
	VkSamplerCreateInfo GenerateVkSamplerCreateInfo(const FRHISamplerStateCreateInfo& CreateInfo);

	// Function to generate VkPipelineShaderStageCreateInfo from FShaderCreateInfo
	VkPipelineShaderStageCreateInfo GenerateVkPipelineShaderStageCreateInfo(const FShaderCreateInfo& CreateInfo, VkShaderModule ShaderModule) override;

	// Function to generate VkPipelineLayoutCreateInfo from FRHIGraphicsPSOCreateInfo (assuming this struct exists)
	VkPipelineLayoutCreateInfo GenerateVkPipelineLayoutCreateInfo(const FRHIGraphicsPipelineLayoutCreateInfo& CreateInfo);

	// Function to generate VkGraphicsPipelineCreateInfo from FRHIGraphicsPSOCreateInfo (assuming this struct exists)
	VkGraphicsPipelineCreateInfo GenerateVkGraphicsPipelineCreateInfo(const FRHIGraphicsPSOCreateInfo& CreateInfo);

	// Function to generate VkComputePipelineCreateInfo from FRHIComputePSOCreateInfo (assuming this struct exists)
	VkComputePipelineCreateInfo GenerateVkComputePipelineCreateInfo(const FRHIComputePSOCreateInfo& CreateInfo);

	// Function to generate VkQueryPoolCreateInfo from FRHIQueryCreateInfo (assuming this struct exists)
	VkQueryPoolCreateInfo GenerateVkQueryPoolCreateInfo(const FRHIQueryCreateInfo& CreateInfo);

private:
	FVulkanRHIInitializer InitializerParam;

	// Vulkan-specific members and methods
	VkInstance				 Instance;
	VkDebugUtilsMessengerEXT DebugMessenger;
	VkSurfaceKHR			 VulkanSurface; // 用于显示的窗口句柄 // TODO, we should only let swapchain manage surface
	VkQueue					 GraphicsQueue;
	VkQueue					 ComputeQueue;
	VkQueue					 TransferQueue;
	VkQueue					 PresentQueue;

	FVulkanPhysicalDeviceRef PhysicalDevice;
	FVulkanLogicalDeviceRef	 LogicalDevice;
	FVulkanViewportRef		 VulkanViewport;

	TSharedPtr<FVulkanRenderPassManager> RenderPassManager;
	FVulkanRenderPassRef	 CurrentRenderPass;
	FVulkanFrameBufferRef	 CurrentFrameBuffer;
};
