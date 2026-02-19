/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "Renderer/DeviceManager.h"
#include "Utility/CVar/CVarMacros.h"

#if HLVM_WINDOW_USE_VULKAN && USE_VK_BACKEND

HLVM_STATIC_VAR bool g_VulkanFastSync = true;
AUTO_CVAR_REF_BOOL(VulkanFastSync, g_VulkanFastSync, "Use vulkan fast vsync eMailbox", EConsoleVariableFlag::RequiresRestart)

HLVM_STATIC_VAR bool g_UseValidationLayers = !HLVM_BUILD_RELEASE;
AUTO_CVAR_REF_BOOL(UseValidationLayers, g_UseValidationLayers, "Use vulkan validation layers", EConsoleVariableFlag::RequiresRestart)

HLVM_STATIC_VAR bool g_UseDebugRuntime = HLVM_BUILD_DEBUG;
AUTO_CVAR_REF_BOOL(UseDebugRuntime, g_UseDebugRuntime, "Use vulkan debug runtime", EConsoleVariableFlag::RequiresRestart)

HLVM_STATIC_VAR bool g_vkUsePushConstants = true;
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
	vk::SurfaceCapabilitiesKHR		  capabilities;
	std::vector<vk::SurfaceFormatKHR> formats;
	std::vector<vk::PresentModeKHR>	  presentModes;
};

class FDeviceManagerVk final : public FDeviceManager
{
public:
	virtual ~FDeviceManagerVk() override = default;
	// Window and device lifecycle
	virtual bool CreateWindowDeviceAndSwapChain(const IWindow::Properties& Params) override;
	virtual void Shutdown() override;

	// Window management
	virtual void GetDPIScaleInfo(float& OutScaleX, float& OutScaleY) const override;
	virtual void UpdateWindowSize(const FUInt2& Params) override;

	// Rendering interface
	virtual void BeginFrame() override;
	virtual void EndFrame() override;
	virtual void Present() override;

	// Resource access
	[[nodiscard]] virtual nvrhi::IDevice*	 GetDevice() const override;
	[[nodiscard]] virtual const char*		 GetRendererString() const override;
	[[nodiscard]] virtual nvrhi::GraphicsAPI GetGraphicsAPI() const override;
	virtual nvrhi::ITexture*				 GetCurrentBackBuffer() override;
	virtual nvrhi::ITexture*				 GetBackBuffer(TUINT32 Index) override;
	virtual TUINT32							 GetCurrentBackBufferIndex() override;
	virtual TUINT32							 GetBackBufferCount() override;

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
	std::vector<SwapChainImage> m_SwapChainImages;
	uint32_t					m_SwapChainIndex = INVALID_INDEX_UINT32;

	nvrhi::vulkan::DeviceHandle m_NvrhiDevice;
	nvrhi::DeviceHandle			m_ValidationLayer;

	std::string m_RendererString;

	// Queue family indices
	TUINT32 m_GraphicsQueueFamily = INVALID_INDEX_UINT32;
	TUINT32 m_PresentQueueFamily = INVALID_INDEX_UINT32;
	TUINT32 m_ComputeQueueFamily = INVALID_INDEX_UINT32;
	TUINT32 m_TransferQueueFamily = INVALID_INDEX_UINT32;

	// Synchronization
	std::queue<vk::UniqueSemaphore> m_PresentSemaphoreQueue;
	vk::Semaphore					m_PresentSemaphore;
	nvrhi::EventQueryHandle			m_FrameWaitQuery;

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
			VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
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

	static std::vector<const char*> StringSetToVector(const std::unordered_set<std::string>& set)
	{
		std::vector<const char*> ret;
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
	vk::SurfaceFormatKHR	ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);
	vk::PresentModeKHR		ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes);
	vk::Extent2D			ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities);

	bool CheckDeviceExtensionSupport(vk::PhysicalDevice device);
	bool IsDeviceSuitable(vk::PhysicalDevice device);

	std::vector<const char*> GetRequiredExtensions();
	void					 PopulateDebugMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT& createInfo);

	static VKAPI_ATTR vk::Bool32 VKAPI_CALL DebugCallback(
		vk::DebugUtilsMessageSeverityFlagBitsEXT	  messageSeverity,
		vk::DebugUtilsMessageTypeFlagsEXT			  messageType,
		const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void*										  pUserData);
};

// =============================================================================
// FACTORY IMPLEMENTATION
// =============================================================================

TUniquePtr<FDeviceManager> FDeviceManager::Create(nvrhi::GraphicsAPI api)
{
	switch (api)
	{
		case nvrhi::GraphicsAPI::D3D11:
			return nullptr;
		case nvrhi::GraphicsAPI::D3D12:
			return nullptr;
		case nvrhi::GraphicsAPI::VULKAN:
		default:
			break;
	}
	return TUniquePtr<FDeviceManager>(new FDeviceManagerVk());
}

// =============================================================================
// PUBLIC INTERFACE IMPLEMENTATION
// =============================================================================

nvrhi::IDevice* FDeviceManagerVk::GetDevice() const
{
	if (m_ValidationLayer)
	{
		return m_ValidationLayer;
	}
	return m_NvrhiDevice;
}

const char* FDeviceManagerVk::GetRendererString() const
{
	return m_RendererString.c_str();
}

nvrhi::GraphicsAPI FDeviceManagerVk::GetGraphicsAPI() const
{
	return nvrhi::GraphicsAPI::VULKAN;
}

nvrhi::ITexture* FDeviceManagerVk::GetCurrentBackBuffer()
{
	return m_SwapChainImages[m_SwapChainIndex].rhiHandle;
}

nvrhi::ITexture* FDeviceManagerVk::GetBackBuffer(TUINT32 Index)
{
	if (Index < m_SwapChainImages.size())
	{
		return m_SwapChainImages[Index].rhiHandle;
	}
	return nullptr;
}

TUINT32 FDeviceManagerVk::GetCurrentBackBufferIndex()
{
	return m_SwapChainIndex;
}

TUINT32 FDeviceManagerVk::GetBackBufferCount()
{
	return static_cast<TUINT32>(m_SwapChainImages.size());
}

bool FDeviceManagerVk::IsVulkanInstanceExtensionEnabled(const char* ExtensionName) const
{
	return enabledExtensions.instance.find(ExtensionName) != enabledExtensions.instance.end();
}

bool FDeviceManagerVk::IsVulkanDeviceExtensionEnabled(const char* ExtensionName) const
{
	return enabledExtensions.device.find(ExtensionName) != enabledExtensions.device.end();
}

bool FDeviceManagerVk::IsVulkanLayerEnabled(const char* LayerName) const
{
	return enabledExtensions.layers.find(LayerName) != enabledExtensions.layers.end();
}

void FDeviceManagerVk::GetEnabledVulkanInstanceExtensions(TVector<std::string>& OutExtensions) const
{
	for (const auto& ext : enabledExtensions.instance)
	{
		OutExtensions.push_back(ext);
	}
}

void FDeviceManagerVk::GetEnabledVulkanDeviceExtensions(TVector<std::string>& OutExtensions) const
{
	for (const auto& ext : enabledExtensions.device)
	{
		OutExtensions.push_back(ext);
	}
}

void FDeviceManagerVk::GetEnabledVulkanLayers(TVector<std::string>& OutLayers) const
{
	for (const auto& layer : enabledExtensions.layers)
	{
		OutLayers.push_back(layer);
	}
}

void FDeviceManagerVk::ResizeSwapChain()
{
	if (device)
	{
		DestroySwapChain();
		CreateSwapChain();
	}
}

// =============================================================================
// INSTANCE CREATION
// =============================================================================

bool FDeviceManagerVk::CreateInstance()
{
	// Add GLFW required extensions
	auto glfwExtensions = FGLFW3VulkanWindow::GetRequiredExtensions();
	HLVM_ASSERT(glfwExtensions.size() > 0);
	for (uint32_t i = 0; i < glfwExtensions.size(); i++)
	{
		enabledExtensions.instance.insert(glfwExtensions[i]);
	}

	// Add user-requested extensions
	for (const std::string& name : DeviceParams.RequiredVulkanInstanceExtensions)
	{
		enabledExtensions.instance.insert(name);
	}
	for (const std::string& name : DeviceParams.OptionalVulkanInstanceExtensions)
	{
		optionalExtensions.instance.insert(name);
	}

	// Add user-requested layers
	for (const std::string& name : DeviceParams.RequiredVulkanLayers)
	{
		enabledExtensions.layers.insert(name);
	}
	for (const std::string& name : DeviceParams.OptionalVulkanLayers)
	{
		optionalExtensions.layers.insert(name);
	}

	// Check for validation layer support
	if (DeviceParams.bEnableDebugRuntime)
	{
		enabledExtensions.layers.insert("VK_LAYER_KHRONOS_validation");
	}

	std::unordered_set<std::string> requiredExtensions = enabledExtensions.instance;

	// Enumerate available instance extensions
	auto availableExtensions = vk::enumerateInstanceExtensionProperties();
	for (const auto& ext : availableExtensions)
	{
		const std::string name = ext.extensionName;
		if (optionalExtensions.instance.find(name) != optionalExtensions.instance.end())
		{
			enabledExtensions.instance.insert(name);
		}
		requiredExtensions.erase(name);
	}

	if (!requiredExtensions.empty())
	{
		std::stringstream ss;
		ss << "Cannot create Vulkan instance - missing required extensions:";
		for (const auto& ext : requiredExtensions)
		{
			ss << "\n  - " << ext;
		}
		HLVM_LOG(LogRHI, critical, TO_TCHAR_CSTR(ss.str().c_str()));
		return false;
	}

	HLVM_LOG(LogRHI, info, TO_TCHAR_CSTR("Enabled Vulkan instance extensions:\n"));
	for (const auto& ext : enabledExtensions.instance)
	{
		HLVM_LOG(LogRHI, info, TXT("(\"    {}\\n\""), TO_TCHAR_CSTR(ext.c_str()));
	}

	// Check layers
	std::unordered_set<std::string> requiredLayers = enabledExtensions.layers;
	auto							availableLayers = vk::enumerateInstanceLayerProperties();
	for (const auto& layer : availableLayers)
	{
		const std::string name = layer.layerName;
		if (optionalExtensions.layers.find(name) != optionalExtensions.layers.end())
		{
			enabledExtensions.layers.insert(name);
		}
		requiredLayers.erase(name);
	}

	if (!requiredLayers.empty())
	{
		std::stringstream ss;
		ss << "Cannot create Vulkan instance - missing required layers:";
		for (const auto& layer : requiredLayers)
		{
			ss << "\n  - " << layer;
		}
		HLVM_LOG(LogRHI, critical, TO_TCHAR_CSTR(ss.str().c_str()));
		return false;
	}
	HLVM_LOG(LogRHI, info, TO_TCHAR_CSTR("Enabled Vulkan layers:\\n"));
	for (const auto& layer : enabledExtensions.layers)
	{
		HLVM_LOG(LogRHI, info, TXT("(\"    {}\\n\""), TO_TCHAR_CSTR(layer.c_str()));
	}

	// Create instance
	vk::ApplicationInfo appInfo(
		"id Tech 4x",
		VK_MAKE_VERSION(1, 0, 0),
		"id Tech 4x Engine",
		VK_MAKE_VERSION(1, 0, 0),
		VK_API_VERSION_1_2);

	auto extensionsVec = StringSetToVector(enabledExtensions.instance);
	auto layersVec = StringSetToVector(enabledExtensions.layers);

	vk::InstanceCreateInfo createInfo;
	createInfo.setPApplicationInfo(&appInfo)
		.setEnabledExtensionCount(static_cast<uint32_t>(extensionsVec.size()))
		.setPpEnabledExtensionNames(extensionsVec.data())
		.setEnabledLayerCount(static_cast<uint32_t>(layersVec.size()))
		.setPpEnabledLayerNames(layersVec.data());

	// Debug messenger for instance creation/destruction
	vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo;
	if (DeviceParams.bEnableDebugRuntime)
	{
		PopulateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.setPNext(&debugCreateInfo);
	}

	VULKAN_HPP_TRY(
		auto inst = vk::createInstanceUnique(createInfo);
		HLVM_ASSERT(inst);
		instance = std::move(inst););

	VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);

	return true;
}

// =============================================================================
// DEBUG MESSENGER
// =============================================================================

void FDeviceManagerVk::PopulateDebugMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT& createInfo)
{
	createInfo.setMessageSeverity(
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
	createInfo.setMessageType(
		vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance);
	createInfo.setPfnUserCallback(DebugCallback);
	createInfo.setPUserData(this);
}

void FDeviceManagerVk::SetupDebugMessenger()
{
	if (!DeviceParams.bEnableDebugRuntime)
		return;

	vk::DebugUtilsMessengerCreateInfoEXT createInfo;
	PopulateDebugMessengerCreateInfo(createInfo);

	VULKAN_HPP_TRY(
		auto messenger = instance->createDebugUtilsMessengerEXTUnique(createInfo);
		HLVM_ASSERT(messenger);
		debugMessenger = std::move(messenger););
}

// =============================================================================
// SURFACE CREATION
// =============================================================================

bool FDeviceManagerVk::CreateWindowSurface()
{
	FGLFW3VulkanWindow* windowHandle = static_cast<FGLFW3VulkanWindow*>(WindowHandle.get());
	VkSurfaceKHR		rawSurface;
	rawSurface = windowHandle->CreateSurface(*instance);
	if (!rawSurface)
	{
		HLVM_LOG(LogRHI, critical, TXT("Failed to create window surface"));
		return false;
	}

	surface = vk::UniqueSurfaceKHR(
		vk::SurfaceKHR(rawSurface),
		vk::detail::ObjectDestroy<vk::Instance, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>(*instance));

	return true;
}

// =============================================================================
// PHYSICAL DEVICE SELECTION
// =============================================================================

bool FDeviceManagerVk::PickPhysicalDevice()
{
	auto devices = instance->enumeratePhysicalDevices();
	if (devices.empty())
	{
		HLVM_LOG(LogRHI, critical, TXT("No Vulkan-compatible GPUs found"));
		return false;
	}

	std::stringstream errorStream;
	errorStream << "Cannot find suitable Vulkan device:";

	std::vector<vk::PhysicalDevice> discreteGPUs;
	std::vector<vk::PhysicalDevice> otherGPUs;

	for (const auto& dev : devices)
	{
		auto props = dev.getProperties();
		errorStream << "\n"
					<< props.deviceName.data() << ":";

		if (!FindQueueFamilies(dev))
		{
			errorStream << "\n  - missing required queue families";
			continue;
		}

		if (!CheckDeviceExtensionSupport(dev))
		{
			errorStream << "\n  - missing required extensions";
			continue;
		}

		auto features = dev.getFeatures();
		if (!features.samplerAnisotropy)
		{
			errorStream << "\n  - no sampler anisotropy";
			continue;
		}
		if (!features.textureCompressionBC)
		{
			errorStream << "\n  - no BC texture compression";
			continue;
		}

		// Check swapchain support
		auto swapChainSupport = QuerySwapChainSupport(dev);
		if (swapChainSupport.formats.empty() || swapChainSupport.presentModes.empty())
		{
			errorStream << "\n  - inadequate swapchain support";
			continue;
		}

		// Check presentation support
		vk::Bool32 canPresent = dev.getSurfaceSupportKHR(m_GraphicsQueueFamily, *surface);
		if (!canPresent)
		{
			errorStream << "\n  - cannot present to surface";
			continue;
		}

		// Clamp swapchain buffer count
		auto surfaceCaps = dev.getSurfaceCapabilitiesKHR(*surface);
		DeviceParams.SwapChainBufferCount = std::max(surfaceCaps.minImageCount, DeviceParams.SwapChainBufferCount);
		if (surfaceCaps.maxImageCount > 0)
		{
			DeviceParams.SwapChainBufferCount = std::min(DeviceParams.SwapChainBufferCount, surfaceCaps.maxImageCount);
		}
		HLVM_ASSERT(DeviceParams.SwapChainBufferCount <= RHI::MAX_FRAMES_IN_FLIGHT);

		if (props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
		{
			discreteGPUs.push_back(dev);
		}
		else
		{
			otherGPUs.push_back(dev);
		}
	}

	if (!discreteGPUs.empty())
	{
		// TODO : YuHang record vendor and gpu type for debugg info
		// glConfig.vendor = getGPUVendor(discreteGPUs[0].getProperties().vendorID); // Uncomment if glConfig exists
		// glConfig.gpuType = GPU_TYPE_DISCRETE;
		physicalDevice = discreteGPUs[0];
	}
	else if (!otherGPUs.empty())
	{
		// TODO : YuHang record vendor and gpu type for debugg info
		// glConfig.vendor = getGPUVendor(otherGPUs[0].getProperties().vendorID);
		// glConfig.gpuType = GPU_TYPE_OTHER;
		physicalDevice = otherGPUs[0];
	}
	else
	{
		HLVM_LOG(LogRHI, critical, TO_TCHAR_CSTR(errorStream.str().c_str()));
		return false;
	}

	return true;
}

bool FDeviceManagerVk::FindQueueFamilies(vk::PhysicalDevice InDevice)
{
	auto queueFamilies = InDevice.getQueueFamilyProperties();

	m_GraphicsQueueFamily = INVALID_INDEX_UINT32;
	m_PresentQueueFamily = INVALID_INDEX_UINT32;
	m_ComputeQueueFamily = INVALID_INDEX_UINT32;
	m_TransferQueueFamily = INVALID_INDEX_UINT32;

	for (uint32_t i = 0; i < queueFamilies.size(); i++)
	{
		const auto& queueFamily = queueFamilies[i];

		if (m_GraphicsQueueFamily == INVALID_INDEX_UINT32 && queueFamily.queueCount > 0 && (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics))
		{
			m_GraphicsQueueFamily = i;
		}

		if (m_ComputeQueueFamily == INVALID_INDEX_UINT32 && queueFamily.queueCount > 0 && (queueFamily.queueFlags & vk::QueueFlagBits::eCompute) && !(queueFamily.queueFlags & vk::QueueFlagBits::eGraphics))
		{
			m_ComputeQueueFamily = i;
		}

		if (m_TransferQueueFamily == INVALID_INDEX_UINT32 && queueFamily.queueCount > 0 && (queueFamily.queueFlags & vk::QueueFlagBits::eTransfer) && !(queueFamily.queueFlags & vk::QueueFlagBits::eCompute) && !(queueFamily.queueFlags & vk::QueueFlagBits::eGraphics))
		{
			m_TransferQueueFamily = i;
		}

		if (m_PresentQueueFamily == INVALID_INDEX_UINT32 && queueFamily.queueCount > 0)
		{
			vk::Bool32 presentSupported = InDevice.getSurfaceSupportKHR(i, *surface);
			if (presentSupported)
			{
				m_PresentQueueFamily = i;
			}
		}
	}

	return m_GraphicsQueueFamily != INVALID_INDEX_UINT32 && m_PresentQueueFamily != INVALID_INDEX_UINT32;
}

bool FDeviceManagerVk::CheckDeviceExtensionSupport(vk::PhysicalDevice InDevice)
{
	auto availableExtensions = InDevice.enumerateDeviceExtensionProperties();

	std::set<std::string> required(enabledExtensions.device.begin(), enabledExtensions.device.end());

	for (const auto& extension : availableExtensions)
	{
		required.erase(extension.extensionName);
	}

	return required.empty();
}

SwapChainSupportDetails FDeviceManagerVk::QuerySwapChainSupport(vk::PhysicalDevice InDevice)
{
	SwapChainSupportDetails details;
	details.capabilities = InDevice.getSurfaceCapabilitiesKHR(*surface);
	details.formats = InDevice.getSurfaceFormatsKHR(*surface);
	details.presentModes = InDevice.getSurfacePresentModesKHR(*surface);
	return details;
}

// =============================================================================
// LOGICAL DEVICE CREATION
// =============================================================================

bool FDeviceManagerVk::CreateLogicalDevice()
{
	// Enable optional extensions
	auto deviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
	for (const auto& ext : deviceExtensions)
	{
		const std::string name = ext.extensionName;
		if (optionalExtensions.device.find(name) != optionalExtensions.device.end())
		{
			enabledExtensions.device.insert(name);
		}
		if (DeviceParams.bEnableRayTracingExtensions && m_RayTracingExtensions.find(name) != m_RayTracingExtensions.end())
		{
			enabledExtensions.device.insert(name);
		}
	}

	// Log enabled extensions
	HLVM_LOG(LogRHI, info, TXT("Enabled Vulkan device extensions:"));
	for (const auto& ext : enabledExtensions.device)
	{
		HLVM_LOG(LogRHI, info, TXT("    {}"), TO_TCHAR_CSTR(ext.c_str()));
	}

	// Collect unique queue families
	std::set<uint32_t> uniqueQueueFamilies = {
		static_cast<uint32_t>(m_GraphicsQueueFamily),
		static_cast<uint32_t>(m_PresentQueueFamily)
	};

	if (DeviceParams.bEnableComputeQueue && m_ComputeQueueFamily != INVALID_INDEX_UINT32)
	{
		uniqueQueueFamilies.insert(m_ComputeQueueFamily);
	}
	if (DeviceParams.bEnableCopyQueue && m_TransferQueueFamily != INVALID_INDEX_UINT32)
	{
		uniqueQueueFamilies.insert(m_TransferQueueFamily);
	}

	float								   queuePriority = 1.0f;
	std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
	for (uint32_t queueFamily : uniqueQueueFamilies)
	{
		queueCreateInfos.push_back(
			vk::DeviceQueueCreateInfo()
				.setQueueFamilyIndex(queueFamily)
				.setQueueCount(1)
				.setPQueuePriorities(&queuePriority));
	}

	// Device features
	vk::PhysicalDeviceFeatures deviceFeatures;
	deviceFeatures
		.setShaderImageGatherExtended(true)
		.setSamplerAnisotropy(true)
		.setTessellationShader(true)
		.setTextureCompressionBC(true)
		.setGeometryShader(true)
		.setFillModeNonSolid(true)
		.setImageCubeArray(true)
		.setDualSrcBlend(true);

	// Vulkan 1.2 features
	vk::PhysicalDeviceVulkan12Features vulkan12Features;
	vulkan12Features
		.setDescriptorIndexing(true)
		.setRuntimeDescriptorArray(true)
		.setDescriptorBindingPartiallyBound(true)
		.setDescriptorBindingVariableDescriptorCount(true)
		.setTimelineSemaphore(true)
		.setShaderSampledImageArrayNonUniformIndexing(true)
		.setBufferDeviceAddress(IsVulkanDeviceExtensionEnabled(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME));

	auto extensionsVec = StringSetToVector(enabledExtensions.device);

	vk::DeviceCreateInfo createInfo;
	createInfo
		.setQueueCreateInfoCount(static_cast<uint32_t>(queueCreateInfos.size()))
		.setPQueueCreateInfos(queueCreateInfos.data())
		.setPEnabledFeatures(&deviceFeatures)
		.setEnabledExtensionCount(static_cast<uint32_t>(extensionsVec.size()))
		.setPpEnabledExtensionNames(extensionsVec.data())
		.setPNext(&vulkan12Features);

	VULKAN_HPP_TRY(
		auto dev = physicalDevice.createDeviceUnique(createInfo);
		HLVM_ASSERT(dev);
		device = std::move(dev););

	VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);

	// Get queues
	graphicsQueue = device->getQueue(m_GraphicsQueueFamily, 0);
	presentQueue = device->getQueue(m_PresentQueueFamily, 0);

	if (DeviceParams.bEnableComputeQueue && m_ComputeQueueFamily != INVALID_INDEX_UINT32)
	{
		computeQueue = device->getQueue(m_ComputeQueueFamily, 0);
	}
	if (DeviceParams.bEnableCopyQueue && m_TransferQueueFamily != INVALID_INDEX_UINT32)
	{
		transferQueue = device->getQueue(m_TransferQueueFamily, 0);
	}

	// Check D24S8 format support
	vk::ImageFormatProperties imageFormatProperties;
	auto					  formatResult = physicalDevice.getImageFormatProperties(
		 vk::Format::eD24UnormS8Uint,
		 vk::ImageType::e2D,
		 vk::ImageTiling::eOptimal,
		 vk::ImageUsageFlagBits::eDepthStencilAttachment,
		 {},
		 &imageFormatProperties);
	DeviceParams.bEnableImageFormatD24S8 = (formatResult == vk::Result::eSuccess);

	// Query present modes
	auto surfacePModes = physicalDevice.getSurfacePresentModesKHR(*surface);
	enablePModeMailbox = std::find(surfacePModes.begin(), surfacePModes.end(), vk::PresentModeKHR::eMailbox) != surfacePModes.end();
	enablePModeImmediate = std::find(surfacePModes.begin(), surfacePModes.end(), vk::PresentModeKHR::eImmediate) != surfacePModes.end();
	enablePModeFifoRelaxed = std::find(surfacePModes.begin(), surfacePModes.end(), vk::PresentModeKHR::eFifoRelaxed) != surfacePModes.end();

	// Store device info
	auto props = physicalDevice.getProperties();
	m_RendererString = std::string(props.deviceName.data());
	m_DeviceApiVersion = props.apiVersion;

	// Check timestamp query support (Optional: Uncomment if glConfig available)
	/*
	auto queueProps = physicalDevice.getQueueFamilyProperties();
	glConfig.timerQueryAvailable = (props.limits.timestampPeriod > 0.0) &&
								   (props.limits.timestampComputeAndGraphics || queueProps[m_GraphicsQueueFamily].timestampValidBits > 0);
	*/

	HLVM_LOG(LogRHI, info, TXT("Created Vulkan device: {}"), TO_TCHAR_CSTR(m_RendererString.c_str()));

	return true;
}

// =============================================================================
// SWAPCHAIN CREATION
// =============================================================================

bool FDeviceManagerVk::CreateSwapChain()
{
	auto swapChainSupport = QuerySwapChainSupport(physicalDevice);

	vk::SurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
	vk::PresentModeKHR	 presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
	vk::Extent2D		 extent = ChooseSwapExtent(swapChainSupport.capabilities);

	// Clamp buffer count
	uint32_t imageCount = DeviceParams.SwapChainBufferCount;
	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
	{
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}
	imageCount = std::max(imageCount, swapChainSupport.capabilities.minImageCount);

	// Update stored format
	if (DeviceParams.SwapChainFormat == nvrhi::Format::SRGBA8_UNORM)
	{
		DeviceParams.SwapChainFormat = nvrhi::Format::SBGRA8_UNORM;
	}
	else if (DeviceParams.SwapChainFormat == nvrhi::Format::RGBA8_UNORM)
	{
		DeviceParams.SwapChainFormat = nvrhi::Format::BGRA8_UNORM;
	}

	swapChainImageFormat = vk::Format(nvrhi::vulkan::convertFormat(DeviceParams.SwapChainFormat));
	swapChainExtent = extent;

	uint32_t queueFamilyIndices[] = {
		static_cast<uint32_t>(m_GraphicsQueueFamily),
		static_cast<uint32_t>(m_PresentQueueFamily)
	};

	bool concurrentSharing = m_GraphicsQueueFamily != m_PresentQueueFamily;

	vk::SwapchainCreateInfoKHR createInfo;
	createInfo
		.setSurface(*surface)
		.setMinImageCount(imageCount)
		.setImageFormat(swapChainImageFormat)
		.setImageColorSpace(vk::ColorSpaceKHR::eSrgbNonlinear)
		.setImageExtent(extent)
		.setImageArrayLayers(1)
		.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled)
		.setImageSharingMode(concurrentSharing ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive)
		.setQueueFamilyIndexCount(concurrentSharing ? 2u : 0u)
		.setPQueueFamilyIndices(concurrentSharing ? queueFamilyIndices : nullptr)
		.setPreTransform(vk::SurfaceTransformFlagBitsKHR::eIdentity)
		.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
		.setPresentMode(presentMode)
		.setClipped(true)
		.setOldSwapchain(nullptr);

	VULKAN_HPP_TRY(
		auto sc = device->createSwapchainKHRUnique(createInfo);
		HLVM_ASSERT(sc);
		swapChain = std::move(sc););

	// Get swapchain images
	auto images = device->getSwapchainImagesKHR(*swapChain);
	for (auto image : images)
	{
		SwapChainImage sci;
		sci.image = image;

		nvrhi::TextureDesc textureDesc;
		textureDesc.width = extent.width;
		textureDesc.height = extent.height;
		textureDesc.format = DeviceParams.SwapChainFormat;
		textureDesc.debugName = "Swap chain image";
		textureDesc.initialState = nvrhi::ResourceStates::Present;
		textureDesc.keepInitialState = true;
		textureDesc.isRenderTarget = true;

		sci.rhiHandle = m_NvrhiDevice->createHandleForNativeTexture(
			nvrhi::ObjectTypes::VK_Image,
			nvrhi::Object(sci.image),
			textureDesc);
		m_SwapChainImages.push_back(sci);
	}

	m_SwapChainIndex = 0;

	return true;
}

void FDeviceManagerVk::DestroySwapChain()
{
	if (device)
	{
		device->waitIdle();
	}

	while (!m_SwapChainImages.empty())
	{
		auto sci = m_SwapChainImages.back();
		m_SwapChainImages.pop_back();
		sci.rhiHandle = nullptr;
	}

	swapChain.reset();
}

vk::SurfaceFormatKHR FDeviceManagerVk::ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
{
	for (const auto& availableFormat : availableFormats)
	{
		if (availableFormat.format == vk::Format::eB8G8R8A8Srgb && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
		{
			return availableFormat;
		}
	}
	return availableFormats[0];
}

vk::PresentModeKHR FDeviceManagerVk::ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes)
{
	vk::PresentModeKHR presentMode;
	switch (DeviceParams.VSyncMode)
	{
		case 0:
			if (enablePModeMailbox && g_VulkanFastSync)
				presentMode = vk::PresentModeKHR::eMailbox;
			if (enablePModeImmediate)
				presentMode = vk::PresentModeKHR::eImmediate;
			presentMode = vk::PresentModeKHR::eFifo;
			break;
		case 1:
			if (enablePModeFifoRelaxed)
				presentMode = vk::PresentModeKHR::eFifoRelaxed;
			presentMode = vk::PresentModeKHR::eFifo;
			break;
		case 2:
		default:
			presentMode = vk::PresentModeKHR::eFifo;
	}
	if (boost::find(availablePresentModes, presentMode) != availablePresentModes.end())
	{
		return presentMode;
	}
	else
	{
		// Log
		HLVM_LOG(LogRHI, err, TXT("Present mode {} not supported. Using FIFO instead."), *VULKAN_ENUM_TO_FSTRING(presentMode));
		return vk::PresentModeKHR::eFifo;
	}
}

vk::Extent2D FDeviceManagerVk::ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities)
{
	if (capabilities.currentExtent.width != UINT32_MAX)
	{
		return capabilities.currentExtent;
	}

	vk::Extent2D actualExtent = {
		DeviceParams.BackBufferWidth,
		DeviceParams.BackBufferHeight
	};

	actualExtent.width = std::clamp(
		actualExtent.width,
		capabilities.minImageExtent.width,
		capabilities.maxImageExtent.width);
	actualExtent.height = std::clamp(
		actualExtent.height,
		capabilities.minImageExtent.height,
		capabilities.maxImageExtent.height);

	return actualExtent;
}

// =============================================================================
// SYNCHRONIZATION
// =============================================================================

void FDeviceManagerVk::CreateSyncObjects()
{
	// Create semaphores for each swapchain image
	for (size_t i = 0; i < m_SwapChainImages.size(); i++)
	{
		VULKAN_HPP_TRY(
			auto sem = device->createSemaphoreUnique(vk::SemaphoreCreateInfo());
			HLVM_ASSERT(sem);
			m_PresentSemaphoreQueue.push(std::move(sem)););
	}

	// Get front semaphore for use
	m_PresentSemaphore = *m_PresentSemaphoreQueue.front();

	m_FrameWaitQuery = m_NvrhiDevice->createEventQuery();
	m_NvrhiDevice->setEventQuery(m_FrameWaitQuery, nvrhi::CommandQueue::Graphics);
}

// =============================================================================
// MAIN CREATION / DESTRUCTION
// =============================================================================

bool FDeviceManagerVk::CreateDeviceAndSwapChain()
{
	DeviceParams.bEnableNVRHIValidationLayer = g_UseValidationLayers;
	DeviceParams.bEnableDebugRuntime = g_UseDebugRuntime;

	// Initialize dynamic loader
	#if VK_HEADER_VERSION >= 301
	using VulkanDynamicLoader = vk::detail::DynamicLoader;
	#else
	using VulkanDynamicLoader = vk::DynamicLoader;
	#endif

	static const VulkanDynamicLoader dl;
	auto							 vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
	VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

	HLVM_ENSURE(CreateInstance());

	if (DeviceParams.bEnableDebugRuntime)
	{
		SetupDebugMessenger();
	}

	// Add user-requested device extensions
	for (const std::string& name : DeviceParams.RequiredVulkanDeviceExtensions)
	{
		enabledExtensions.device.insert(name);
	}
	for (const std::string& name : DeviceParams.OptionalVulkanDeviceExtensions)
	{
		optionalExtensions.device.insert(name);
	}

	HLVM_ENSURE(CreateWindowSurface());
	HLVM_ENSURE(PickPhysicalDevice());
	HLVM_ENSURE(FindQueueFamilies(physicalDevice));
	HLVM_ENSURE(CreateLogicalDevice());

	// Create NVRHI device
	auto vecInstanceExt = StringSetToVector(enabledExtensions.instance);
	auto vecLayers = StringSetToVector(enabledExtensions.layers);
	auto vecDeviceExt = StringSetToVector(enabledExtensions.device);

	nvrhi::vulkan::DeviceDesc deviceDesc;
	deviceDesc.errorCB = &FNVRHIMessageCallback::GetInstance();
	deviceDesc.instance = *instance;
	deviceDesc.physicalDevice = physicalDevice;
	deviceDesc.device = *device;
	deviceDesc.graphicsQueue = graphicsQueue;
	deviceDesc.graphicsQueueIndex = static_cast<int>(m_GraphicsQueueFamily);

	if (DeviceParams.bEnableComputeQueue && m_ComputeQueueFamily != INVALID_INDEX_UINT32)
	{
		deviceDesc.computeQueue = computeQueue;
		deviceDesc.computeQueueIndex = static_cast<int>(m_ComputeQueueFamily);
	}
	if (DeviceParams.bEnableCopyQueue && m_TransferQueueFamily != INVALID_INDEX_UINT32)
	{
		deviceDesc.transferQueue = transferQueue;
		deviceDesc.transferQueueIndex = static_cast<int>(m_TransferQueueFamily);
	}

	deviceDesc.instanceExtensions = vecInstanceExt.data();
	deviceDesc.numInstanceExtensions = vecInstanceExt.size();
	deviceDesc.deviceExtensions = vecDeviceExt.data();
	deviceDesc.numDeviceExtensions = vecDeviceExt.size();

	m_NvrhiDevice = nvrhi::vulkan::createDevice(deviceDesc);

	if (DeviceParams.bEnableNVRHIValidationLayer)
	{
		m_ValidationLayer = nvrhi::validation::createValidationLayer(m_NvrhiDevice);
	}

	// Determine max push constant size
	if (g_vkUsePushConstants)
	{
		auto deviceProperties = physicalDevice.getProperties();
		DeviceParams.MaxPushConstantSize = std::min(
			static_cast<uint32_t>(deviceProperties.limits.maxPushConstantsSize),
			nvrhi::c_MaxPushConstantSize);
	}

	HLVM_ENSURE(CreateSwapChain());
	CreateSyncObjects();

	return true;
}

void FDeviceManagerVk::DestroyDeviceAndSwapChain()
{
	if (device)
	{
		device->waitIdle();
	}

	m_FrameWaitQuery = nullptr;

	while (!m_PresentSemaphoreQueue.empty())
	{
		m_PresentSemaphoreQueue.pop();
	}
	m_PresentSemaphore = nullptr;

	DestroySwapChain();

	m_NvrhiDevice = nullptr;
	m_ValidationLayer = nullptr;
	m_RendererString.clear();

	debugMessenger.reset();
	device.reset();
	surface.reset();
	instance.reset();
}

// =============================================================================
// FRAME RENDERING
// =============================================================================

void FDeviceManagerVk::BeginFrame()
{
	// Get GPU memory stats (Optional: requires extension support)
	/*
	vk::PhysicalDeviceMemoryProperties2 memoryProperties2;
	vk::PhysicalDeviceMemoryBudgetPropertiesEXT memoryBudget;
	memoryProperties2.pNext = &memoryBudget;
	physicalDevice.getMemoryProperties2(&memoryProperties2);

	VkDeviceSize gpuMemoryAllocated = 0;
	for (uint32_t i = 0; i < memoryProperties2.memoryProperties.memoryHeapCount; i++)
	{
		gpuMemoryAllocated += memoryBudget.heapUsage[i];
	}
	// Update memory stats...
	*/

	// Acquire next image
	VULKAN_HPP_TRY(
		auto [result, imageIndex] = device->acquireNextImageKHR(
			*swapChain,
			std::numeric_limits<uint64_t>::max(),
			m_PresentSemaphore,
			nullptr);

		if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
			HLVM_LOG(LogRHI, warn, TXT("Failed to acquire swapchain image"));
			return;
		}

		m_SwapChainIndex = imageIndex;
		m_NvrhiDevice->queueWaitForSemaphore(nvrhi::CommandQueue::Graphics, m_PresentSemaphore, 0););
}

void FDeviceManagerVk::EndFrame()
{
	m_NvrhiDevice->queueSignalSemaphore(nvrhi::CommandQueue::Graphics, m_PresentSemaphore, 0);
}

void FDeviceManagerVk::Present()
{
	vk::PresentInfoKHR presentInfo;
	presentInfo
		.setWaitSemaphoreCount(1)
		.setPWaitSemaphores(&m_PresentSemaphore)
		.setSwapchainCount(1)
		.setPSwapchains(&*swapChain)
		.setPImageIndices(&m_SwapChainIndex);

	VULKAN_HPP_TRY(
		auto result = presentQueue.presentKHR(presentInfo);
		if (result != vk::Result::eSuccess && result != vk::Result::eErrorOutOfDateKHR && result != vk::Result::eSuboptimalKHR) {
			HLVM_LOG(LogRHI, warn, TXT("Failed to present swapchain image"));
		});

	// Cycle semaphore queue
	auto front = std::move(const_cast<vk::UniqueSemaphore&>(m_PresentSemaphoreQueue.front()));
	m_PresentSemaphoreQueue.pop();
	m_PresentSemaphoreQueue.push(std::move(front));
	m_PresentSemaphore = *m_PresentSemaphoreQueue.front();

	// Frame synchronization
	if constexpr (RHI::MAX_FRAMES_IN_FLIGHT > 2)
	{
		m_NvrhiDevice->waitEventQuery(m_FrameWaitQuery);
	}

	m_NvrhiDevice->resetEventQuery(m_FrameWaitQuery);
	m_NvrhiDevice->setEventQuery(m_FrameWaitQuery, nvrhi::CommandQueue::Graphics);

	if constexpr (RHI::MAX_FRAMES_IN_FLIGHT < 3)
	{
		m_NvrhiDevice->waitEventQuery(m_FrameWaitQuery);
	}
}

// =============================================================================
// DEBUG CALLBACK
// =============================================================================

VKAPI_ATTR vk::Bool32 VKAPI_CALL FDeviceManagerVk::DebugCallback(
	vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	vk::DebugUtilsMessageTypeFlagsEXT /*messageType*/,
	const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void*										  pUserData)
{
	const FDeviceManagerVk* manager = static_cast<const FDeviceManagerVk*>(pUserData);

	if (manager)
	{
		const auto& ignored = manager->DeviceParams.IgnoredVulkanValidationMessageLocations;
		// Note: location not available in DebugUtils, would need to parse message or use DebugReport
	}

	if (messageSeverity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
	{
		HLVM_LOG(LogRHI, err, TXT("[Vulkan] ERROR: {}"), TO_TCHAR_CSTR(pCallbackData->pMessage));
	}
	else if (messageSeverity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
	{
		HLVM_LOG(LogRHI, warn, TXT("[Vulkan] WARNING: {}"), TO_TCHAR_CSTR(pCallbackData->pMessage));
	}
	else if (messageSeverity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo)
	{
		HLVM_LOG(LogRHI, info, TXT("[Vulkan] INFO: {}"), TO_TCHAR_CSTR(pCallbackData->pMessage));
	}

	return VK_FALSE;
}

// =============================================================================
// STUBS / EMPTY IMPLEMENTATIONS FOR INTERFACE
// =============================================================================

bool FDeviceManagerVk::CreateWindowDeviceAndSwapChain(const IWindow::Properties& Params)
{
	// Typically handled by CreateDeviceAndSwapChain in this implementation

	// 1 . Create window
	TSharedPtr<FGLFW3VulkanWindow> gWindow;
	// Log
	HLVM_LOG(LogRHI, debug, TXT("Creating window with properties:\n{}"), Params.ToString());
	gWindow = MAKE_SHARED(FGLFW3VulkanWindow, Params);
	if (gWindow)
	{
		HLVM_LOG(LogRHI, debug, TXT("FGLFW3VulkanWindow created!"));
		WindowHandle = MoveTemp(gWindow);
	}
	else
	{
		HLVM_LOG(LogRHI, err, TXT("Failed to create window with properties:\n{}"), Params.ToString());
		return false;
	}
	return CreateDeviceAndSwapChain();
}

void FDeviceManagerVk::Shutdown()
{
	DestroyDeviceAndSwapChain();
}

void FDeviceManagerVk::GetDPIScaleInfo(float& OutScaleX, float& OutScaleY) const
{
	// TODO: Implement DPI scaling query
	OutScaleX = 1.0f;
	OutScaleY = 1.0f;
}

void FDeviceManagerVk::UpdateWindowSize(const FUInt2& Params)
{
	DeviceParams.BackBufferWidth = Params.x;
	DeviceParams.BackBufferHeight = Params.y;
	ResizeSwapChain();
}

void FDeviceManagerVk::SetVSyncMode(TINT32 VSyncMode)
{
	DeviceParams.VSyncMode = VSyncMode;
	// Requires swapchain recreationation to apply new present mode
	ResizeSwapChain();
}
#endif
