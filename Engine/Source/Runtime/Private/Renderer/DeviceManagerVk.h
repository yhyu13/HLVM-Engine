/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

// Reference
// https://github.com/RobertBeckebans/RBDOOM-3-BFG/blob/4310fbd200b578014b22dce5fa82a48977eb149a/neo/sys/DeviceManager_VK.cpp
// https://github.com/NVIDIA-RTX/Donut/blob/2c1077673edb0e0d814c202e6ce8e502d245c2da/src/app/vulkan/DeviceManager_VK.cpp
#pragma once

#include "Renderer/DeviceManager.h"

#if HLVM_VULKAN_RENDERER

	#include "Utility/CVar/CVarMacros.h"

	#include <queue>
	#include <deque>

HLVM_INLINE_VAR bool g_VulkanFastSync = true;
AUTO_CVAR_REF_BOOL(VulkanFastSync, g_VulkanFastSync, "Use vulkan fast vsync eMailbox", EConsoleVariableFlag::RequiresRestart)

HLVM_INLINE_VAR bool g_UseValidationLayers = !HLVM_BUILD_RELEASE;
AUTO_CVAR_REF_BOOL(UseValidationLayers, g_UseValidationLayers, "Use vulkan validation layers", EConsoleVariableFlag::RequiresRestart)

HLVM_INLINE_VAR bool g_UseDebugRuntime = HLVM_BUILD_DEBUG;
AUTO_CVAR_REF_BOOL(UseDebugRuntime, g_UseDebugRuntime, "Use vulkan debug runtime", EConsoleVariableFlag::RequiresRestart)

HLVM_INLINE_VAR bool g_vkUsePushConstants = true;
AUTO_CVAR_REF_BOOL(vkUsePushConstants, g_vkUsePushConstants, "Use push constants for Vulkan renderer", EConsoleVariableFlag::RequiresRestart)

// 点击链接查看和 Kimi 的对话 https://www.kimi.com/share/19c6a025-8ba2-8a66-8000-0000e14cda9b
/**
 * QueueFamilyIndices - NVRHI-style queue family discovery
 */
struct QueueFamilyIndices
{
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;
	std::optional<uint32_t> computeFamily;
	std::optional<uint32_t> transferFamily;

	[[nodiscard]] bool IsComplete() const
	{
		return graphicsFamily.has_value() && presentFamily.has_value();
	}

	[[nodiscard]] bool IsCompleteAsync() const
	{
		return graphicsFamily.has_value() && presentFamily.has_value() && computeFamily.has_value() && transferFamily.has_value();
	}
};

/**
 * SwapChainSupportDetails - Surface capabilities query result
 */
struct SwapChainSupportDetails
{
	vk::SurfaceCapabilitiesKHR	  capabilities;
	TVector<vk::SurfaceFormatKHR> formats;
	TVector<vk::PresentModeKHR>	  presentModes;
};

class FDeviceManagerVk final : public FDeviceManager
{
public:
	virtual ~FDeviceManagerVk() override;
	// Window and device lifecycle
	virtual bool CreateWindowDeviceAndSwapChain(const IWindow::Properties& Params) override;
	virtual void Shutdown() override;

	// Window management
	virtual void GetDPIScaleInfo(float& OutScaleX, float& OutScaleY) const override;
	virtual void UpdateWindowSize(const FUInt2& Params) override;

	// Rendering interface
	virtual bool BeginFrame() override;
	virtual bool EndFrame() override;
	virtual bool Present() override;

	// Resource access
	[[nodiscard]] virtual nvrhi::IDevice*	 GetDevice() const override;
	[[nodiscard]] virtual const char*		 GetRendererString() const override;
	[[nodiscard]] virtual nvrhi::GraphicsAPI GetGraphicsAPI() const override;

	// Framebuffer access
	virtual nvrhi::IFramebuffer* GetFramebuffer(TUINT32 Index) override;
	virtual nvrhi::ITexture*	 GetCurrentBackBuffer() override;
	virtual nvrhi::ITexture*	 GetBackBuffer(TUINT32 Index) override;
	virtual TUINT32				 GetCurrentBackBufferIndex() override;
	virtual TUINT32				 GetBackBufferCount() override;
	virtual nvrhi::ITexture* GetDepthTexture(TUINT32 Index) override;

	virtual void SetVSyncMode(TINT32 VSyncMode) override;

	virtual bool IsVulkanInstanceExtensionEnabled(const char* ExtensionName) const override;
	virtual bool IsVulkanDeviceExtensionEnabled(const char* ExtensionName) const override;
	virtual bool IsVulkanLayerEnabled(const char* LayerName) const override;
	virtual void GetEnabledVulkanInstanceExtensions(TVector<std::string>& OutExtensions) const override;
	virtual void GetEnabledVulkanDeviceExtensions(TVector<std::string>& OutExtensions) const override;
	virtual void GetEnabledVulkanLayers(TVector<std::string>& OutLayers) const override;

	// Pure virtual methods for derived classes
	virtual bool CreateDeviceAndSwapChain() override;
	virtual void DestroyDeviceAndSwapChain() override;
	virtual void ResizeSwapChain() override;

private:
	// =============================================================================
	// VULKAN RESOURCES
	// =============================================================================

	vk::UniqueInstance				 instance;
	vk::UniqueDebugUtilsMessengerEXT debugMessenger;
	vk::UniqueSurfaceKHR			 surface;

	vk::PhysicalDevice physicalDevice;
	vk::UniqueDevice   device;

	vk::Queue graphicsQueue;
	vk::Queue presentQueue;
	vk::Queue computeQueue;
	vk::Queue transferQueue;

	vk::UniqueSwapchainKHR swapChain;
	vk::Format			   swapChainImageFormat;
	vk::Extent2D		   swapChainExtent;

	struct SwapChainImage
	{
		vk::Image			 image;
		nvrhi::TextureHandle rhiHandle;
	};
	TVector<SwapChainImage> m_SwapChainImages;
	uint32_t				m_SwapChainIndex = INVALID_INDEX_UINT32;

	nvrhi::vulkan::DeviceHandle m_NvrhiDevice;
	nvrhi::DeviceHandle			m_ValidationLayer;

	std::string m_RendererString;

	// Queue family indices
	TUINT32 m_GraphicsQueueFamily = INVALID_INDEX_UINT32;
	TUINT32 m_PresentQueueFamily = INVALID_INDEX_UINT32;
	TUINT32 m_ComputeQueueFamily = INVALID_INDEX_UINT32;
	TUINT32 m_TransferQueueFamily = INVALID_INDEX_UINT32;

	// Synchronization - following Donuts pattern
	bool								bCanPresent = false;
	TVector<vk::Semaphore>				m_PresentSemaphores;
	TVector<vk::Semaphore>				m_AcquireSemaphores;
	uint32_t							m_AcquireSemaphoreIndex = 0;
	std::deque<nvrhi::EventQueryHandle> m_FramesInFlight;
	TVector<nvrhi::EventQueryHandle>	m_QueryPool;
	// Framebuffers (one per swapchain image)
	TVector<nvrhi::FramebufferHandle> m_Framebuffers;
	// Depth textures (one per swapchain image)
	TVector<nvrhi::TextureHandle> m_DepthTextures;

	// Surface present mode support
	bool enablePModeMailbox = false;
	bool enablePModeImmediate = false;
	bool enablePModeFifoRelaxed = false;

	// Device API version
	uint32_t m_DeviceApiVersion = VK_HEADER_VERSION_COMPLETE;

	// =============================================================================
	// EXTENSION MANAGEMENT
	// =============================================================================

	struct VulkanExtensionSet
	{
		std::unordered_set<std::string> instance;
		std::unordered_set<std::string> layers;
		std::unordered_set<std::string> device;
	};

	VulkanExtensionSet enabledExtensions = {
		// instance
		{
			VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME },
		// layers
		{},
		// device
		{
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			VK_KHR_MAINTENANCE1_EXTENSION_NAME,
			VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, // Caveat : YuHang NVRHI requires dynamic rendering
			VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME, // Caveat : YuHang NVRHI requires synchronization2
		}
	};

	VulkanExtensionSet optionalExtensions = {
		// instance
		{
			VK_EXT_SAMPLER_FILTER_MINMAX_EXTENSION_NAME,
			VK_EXT_DEBUG_UTILS_EXTENSION_NAME },
		// layers
		{},
		// device
		{
			VK_EXT_DEBUG_MARKER_EXTENSION_NAME,
			VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
			VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
			VK_NV_MESH_SHADER_EXTENSION_NAME,
			VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME,
			VK_EXT_MEMORY_BUDGET_EXTENSION_NAME }
	};

	std::unordered_set<std::string> m_RayTracingExtensions = {
		VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
		VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
		VK_KHR_RAY_QUERY_EXTENSION_NAME,
		VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME
	};

	// =============================================================================
	// HELPER METHODS
	// =============================================================================

	static TVector<const char*> StringSetToVector(const std::unordered_set<std::string>& set)
	{
		TVector<const char*> ret;
		for (const auto& s : set)
		{
			ret.push_back(s.c_str());
		}
		return ret;
	}

	// =============================================================================
	// INITIALIZATION PHASES
	// =============================================================================

	bool CreateInstance();
	void SetupDebugMessenger();
	bool CreateWindowSurface();
	bool PickPhysicalDevice();
	bool FindQueueFamilies(vk::PhysicalDevice device);
	bool CreateLogicalDevice();
	bool CreateSwapChain();
	void DestroySwapChain();
	void CreateSyncObjects();

	// =============================================================================
	// UTILITY METHODS
	// =============================================================================

	SwapChainSupportDetails QuerySwapChainSupport(vk::PhysicalDevice device);
	vk::SurfaceFormatKHR	ChooseSwapSurfaceFormat(const TVector<vk::SurfaceFormatKHR>& availableFormats);
	vk::PresentModeKHR		ChooseSwapPresentMode(const TVector<vk::PresentModeKHR>& availablePresentModes);
	vk::Extent2D			ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities);

	bool CheckDeviceExtensionSupport(vk::PhysicalDevice device);
	bool IsDeviceSuitable(vk::PhysicalDevice device);

	TVector<const char*> GetRequiredExtensions();
	void				 PopulateDebugMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT& createInfo);

	static VKAPI_ATTR vk::Bool32 VKAPI_CALL DebugCallback(
		vk::DebugUtilsMessageSeverityFlagBitsEXT	  messageSeverity,
		vk::DebugUtilsMessageTypeFlagsEXT			  messageType,
		const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void*										  pUserData);
};

#endif
