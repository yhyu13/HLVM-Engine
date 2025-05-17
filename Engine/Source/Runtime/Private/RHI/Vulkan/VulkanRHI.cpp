/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "VulkanRHI.h"
#include "Window/Vulkan/GLFW3Vulkan.h"

// TODO : Refactory these static method into each vulkan class
namespace
{
	HLVM_STATIC_VAR bool bUseValidationLayers = VULKAN_ENABLE_VALIDATION_LAYERS;

	HLVM_STATIC_VAR const TVector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation" // 开启可用的校验层, extend if will
	};

	HLVM_STATIC_VAR const TVector<const char*> deviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME, // 交换链扩展集合, extend if will
#if VULKAN_RENDERPASS2
		VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
#endif
	};

	HLVM_STATIC_VAR TVector<std::string> requiredExtensions = {
		/* requested during runtime */
	};

	HLVM_STATIC_FUNC TVector<VkExtensionProperties> EnumerateInstanceExtensionProperties(const char* pLayerName)
	{
		uint32_t extCount = 0;
		VkResult result = VulkanRHI::vkEnumerateInstanceExtensionProperties(pLayerName, &extCount, nullptr);
		if (result != VK_SUCCESS)
		{
			HLVM_LOG(LogVulkanRHI, err, TXT("vkEnumerateInstanceExtensionProperties failed to get extension count. VkResult = {}"), VULKAN_RESULT_TO_TCHAR(result));
			return {};
		}

		TVector<VkExtensionProperties> extensionProperties(extCount);
		result = VulkanRHI::vkEnumerateInstanceExtensionProperties(pLayerName, &extCount, extensionProperties.data());
		if (result != VK_SUCCESS)
		{
			HLVM_LOG(LogVulkanRHI, err, TXT("vkEnumerateInstanceExtensionProperties failed to get extension properties. VkResult = {}"), VULKAN_RESULT_TO_TCHAR(result));
			return {};
		}
		return extensionProperties;
	}

	HLVM_STATIC_FUNC bool IsExtensionSupported(const char* extensionName, const char* pLayerName)
	{
		auto extProps = EnumerateInstanceExtensionProperties(pLayerName);
		auto compare = [&](const VkExtensionProperties& rhs) { return strcmp(extensionName, rhs.extensionName) == 0; };
		return std::find_if(extProps.begin(), extProps.end(), compare) != extProps.end();
	}

	HLVM_STATIC_FUNC TVector<VkLayerProperties> EnumerateInstanceLayerProperties()
	{
		uint32_t layerCount;
		VulkanRHI::vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		TVector<VkLayerProperties> availableLayers(layerCount);
		VulkanRHI::vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
		return availableLayers;
	}

	HLVM_STATIC_FUNC TVector<std::string> ValidateInstanceLayerNames(const TVector<std::string>& names)
	{
		if (names.empty())
		{
			return names;
		}

		auto availableLayers = EnumerateInstanceLayerProperties();

		TSet<std::string> layerNames;
		for (const auto& layer : availableLayers)
		{
			HLVM_LOG(LogVulkanRHI, debug, TXT("Available layer: {}"), TO_TCHAR_CSTR(layer.layerName));
			if (layer.layerName[0] != 0)
			{
				layerNames.insert(layer.layerName);
			}
		}

		TVector<std::string> validatedNames;
		validatedNames.reserve(names.size());
		for (const auto& requestedName : names)
		{
			if (layerNames.count(requestedName) != 0)
			{
				HLVM_LOG(LogVulkanRHI, debug, TXT("Valid requested layer: {}"), TO_TCHAR_CSTR(requestedName.c_str()));
				validatedNames.push_back(requestedName);
			}
			else
			{
				HLVM_LOG(LogVulkanRHI, warn, TXT("Invalid requested layer: {}"), TO_TCHAR_CSTR(requestedName.c_str()));
			}
		}

		return validatedNames;
	}

	HLVM_STATIC_FUNC bool CheckValidationLayerSupport()
	{
		uint32_t layerCount = 0;
		VulkanRHI::vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		TVector<VkLayerProperties> availableLayers(layerCount);
		VulkanRHI::vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
		for (const char* layerName : validationLayers)
		{
			bool layerFound = false;
			for (const auto& layerProperties : availableLayers)
			{
				if (strcmp(layerName, layerProperties.layerName) == 0)
				{
					layerFound = true;
					break;
				}
			}
			if (!layerFound)
			{
				return false;
			}
		}
		return true;
	}

	HLVM_STATIC_FUNC TVector<const char*> GetRequiredExtensions()
	{
		if (bUseValidationLayers)
		{
			requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); // 添加调试扩展
		}
		TVector<const char*> Ret;
		for (const auto& extension : requiredExtensions)
		{
			Ret.push_back(extension.c_str());
		}
		return Ret;
	}

	// Since we are using the debug layer, we need to provide a callback function that will be called when an error occurs
	// We follow vulkan api calling convention as this method is called by vulkan api internally
	HLVM_STATIC_FUNC VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT /*messageType*/,
		const VkDebugUtilsMessengerCallbackDataEXT*														 pCallbackData, void* /*pUserData*/)
	{
		if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		{
			HLVM_LOG(LogVulkanRHI, err, TXT("Error : {}"), TO_TCHAR_CSTR(pCallbackData->pMessage));
		}
		else if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		{
			HLVM_LOG(LogVulkanRHI, warn, TXT("Warning : {}"), TO_TCHAR_CSTR(pCallbackData->pMessage));
		}
		return VK_FALSE;
	}

	HLVM_STATIC_FUNC void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
	{
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = DebugCallback;
	}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-function-type-strict"
	// 使用VulkanRHI::vkGetInstanceProcAddr获取某个api的函数指针
	HLVM_STATIC_FUNC VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
	{
		auto func = R_C(PFN_vkCreateDebugUtilsMessengerEXT, VulkanRHI::vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
		if (func != nullptr)
		{
			return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
		}
		else
		{
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}

	HLVM_STATIC_FUNC void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
	{
		auto func = R_C(PFN_vkDestroyDebugUtilsMessengerEXT, VulkanRHI::vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
		if (func != nullptr)
		{
			func(instance, debugMessenger, pAllocator);
		}
	}
#pragma clang diagnostic pop

	// 查询设备的可用队列族，-1代表无效
	struct QueueFamilyIndices
	{
		// 支持图像绘制的队列
		uint32_t graphicsFamily = std::numeric_limits<uint32_t>::max();

		// 支持图像计算的队列
		uint32_t computeFamily = std::numeric_limits<uint32_t>::max();

		// 支持图像传输的队列
		uint32_t transferFamily = std::numeric_limits<uint32_t>::max();

		// 支持图形呈现的队列
		uint32_t presentFamily = std::numeric_limits<uint32_t>::max();

		bool isComplete()
		{
			return (graphicsFamily < std::numeric_limits<uint32_t>::max())
				&& (presentFamily < std::numeric_limits<uint32_t>::max())
				&& (computeFamily < std::numeric_limits<uint32_t>::max())
				&& (transferFamily < std::numeric_limits<uint32_t>::max());
		}
	};
	HLVM_STATIC_VAR QueueFamilyIndices deviceQueueFamilyIndices;

	// 查询可用的图形队列和呈现队列
	HLVM_STATIC_FUNC QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
	{
		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		VulkanRHI::vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
		TVector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		VulkanRHI::vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		uint32_t index = 0;
		for (const auto& queueFamily : queueFamilies)
		{
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				indices.graphicsFamily = index;
			}
			if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
			{
				indices.computeFamily = index;
			}
			if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
			{
				indices.transferFamily = index;
			}

			VkBool32 presentSupport = false;
			VulkanRHI::vkGetPhysicalDeviceSurfaceSupportKHR(device, index, surface, &presentSupport);
			if (presentSupport)
			{
				indices.presentFamily = index;
			}

			if (indices.isComplete())
			{
				break;
			}
			index++;
		}

		return indices;
	}

	HLVM_STATIC_FUNC bool CheckDeviceExtensionSupport(VkPhysicalDevice device)
	{
		uint32_t extensionCount;
		VulkanRHI::vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		TVector<VkExtensionProperties> availableExtensions(extensionCount);
		VulkanRHI::vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		TSet<std::string> requiredDeviceExtensions(deviceExtensions.begin(), deviceExtensions.end());

		for (const auto& extension : availableExtensions)
		{
			requiredDeviceExtensions.erase(extension.extensionName);
		}

		return requiredDeviceExtensions.empty();
	}

	// 查询并记录交换链支持的细节
	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR	capabilities; // 基础表面特性
		TVector<VkSurfaceFormatKHR> formats;	  // 像素格式、色彩空间
		TVector<VkPresentModeKHR>	presentModes; // 可用的呈现模式
	};

	HLVM_STATIC_FUNC SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
	{
		SwapChainSupportDetails details;

		// 与交换链相关的函数都需要device和surface这两个参数
		// 查询基础表面特性
		VulkanRHI::vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

		// 查询表面支持格式
		uint32_t formatCount;
		VulkanRHI::vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
		if (formatCount != 0)
		{
			details.formats.resize(formatCount);
			VulkanRHI::vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
		}

		// 查询表面支持呈现模式
		uint32_t presentModeCount;
		VulkanRHI::vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

		if (presentModeCount != 0)
		{
			details.presentModes.resize(presentModeCount);
			VulkanRHI::vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
		}

		return details;
	}

	// 为了选择合适的设备，我们需要或许详细的设备信息。包括但不限于：名称、类型和支持Vulkan的版本。
	HLVM_STATIC_FUNC bool IsDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface)
	{
		VkPhysicalDeviceProperties deviceProperties;
		VulkanRHI::vkGetPhysicalDeviceProperties(device, &deviceProperties);

		// 纹理压缩、64为浮点、多窗口渲染是否支持，通过下面函数查询
		VkPhysicalDeviceFeatures deviceFeatures;
		VulkanRHI::vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		// 显卡支持集合着色器的判断条件
		bool isSupportSetShader = (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) && deviceFeatures.geometryShader;

		QueueFamilyIndices indices = FindQueueFamilies(device, surface);
		bool			   extensionsSupported = CheckDeviceExtensionSupport(device);

		bool swapChainAdequate = false;
		if (extensionsSupported)
		{
			SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device, surface);
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}

		return indices.isComplete() && extensionsSupported && swapChainAdequate && isSupportSetShader;
	}

	HLVM_STATIC_FUNC void ResetBeforeInit()
	{
		bUseValidationLayers = VULKAN_ENABLE_VALIDATION_LAYERS;
		requiredExtensions = {};
		deviceQueueFamilyIndices = {};
	}
} // namespace

FVulkanRHI::FVulkanRHI(const FVulkanRHI::FInitializer& Params)
	: InitializerParam(Params)
{
	ResetBeforeInit();
	for (const auto& extensions : Params.RequiredExtensions)
	{
		for (const auto& extension : extensions)
		{
			requiredExtensions.push_back(extension.ToCharCStr());
		}
	}
	HLVM_LOG(LogVulkanRHI, debug, TXT("VulkanRHI created!"));
}

// Initialization and Shutdown
void FVulkanRHI::Init()
{
	CreateVulkanInstance();
	// Create Debug Messenger right after creating the instance
	if (bUseValidationLayers)
	{
		CreateDebugLayer();
	}
	else
	{
		HLVM_LOG(LogVulkanRHI, warn, TXT("VulkanRHI validation layer disabled!"));
	}
	CreateSurface();
	CreateVulkanPhysicalDevice();
	CreateVulkanLogicalDevice();

	CreateVulkanQueues();
	CreateVulkanViewPort();

	// Lastly, create Vulkan Memory Allocator
	CreateVulkanMemoryAllocator();


	HLVM_LOG(LogVulkanRHI, debug, TXT("VulkanRHI Init!"));
}

void FVulkanRHI::Shutdown()
{
	// TODO : desotry every vulkan handle
	VulkanViewport = nullptr;
	LogicalDevice = nullptr;
	PhysicalDevice = nullptr;

	PendingDestroyRenderPass.Empty();

	VulkanRHI::vkDeviceWaitIdle(VulkanDevice);

	// Cleanup Vulkan resources
	vmaDestroyAllocator(VulkanRHI::VULKAN_VMA_ALLOCATOR);
	VulkanRHI::vkDestroyDevice(VulkanDevice, VulkanRHI::VULKAN_CPU_ALLOCATOR);
	if (bUseValidationLayers)
	{
		DestroyDebugUtilsMessengerEXT(VulkanInstance, DebugMessenger, VulkanRHI::VULKAN_CPU_ALLOCATOR);
	}
	VulkanRHI::vkDestroyInstance(VulkanInstance, VulkanRHI::VULKAN_CPU_ALLOCATOR);

	HLVM_LOG(LogVulkanRHI, debug, TXT("VulkanRHI Shutdown!"));
}

// Resource Creation
FRHITextureRef FVulkanRHI::CreateTexture(const FRHITextureCreateInfo& CreateInfo)
{
	return new FVulkanTexture(CreateInfo);
}

FRHISamplerStateRef FVulkanRHI::CreateSamplerState(const FRHISamplerStateCreateInfo& CreateInfo)
{
	return new FVulkanSamplerState(CreateInfo);
}

FRHIBufferRef FVulkanRHI::CreateBuffer(const FRHIBufferCreateInfo& CreateInfo)
{
	return new FVulkanBuffer(CreateInfo);
}

FShaderResourceViewRHIRef FVulkanRHI::CreateShaderResourceView(FRHITexture* Texture, const FRHIShaderResourceViewCreateInfo& CreateInfo)
{
	FVulkanTexture* VulkanTexture = static_cast<FVulkanTexture*>(Texture);
	VkImageView		ImageView = CreateVulkanImageView(VulkanTexture->GetImage(), CreateInfo);
	return new FVulkanShaderResourceView(ImageView, CreateInfo);
}

FUnorderedAccessViewRHIRef FVulkanRHI::CreateUnorderedAccessView(FRHIBuffer* Buffer, const FRHIUnorderedAccessViewCreateInfo& CreateInfo)
{
	FVulkanBuffer* VulkanBuffer = static_cast<FVulkanBuffer*>(Buffer);
	VkBufferView   BufferView = CreateVulkanBufferView(VulkanBuffer->GetBuffer(), CreateInfo);
	return new FVulkanUnorderedAccessView(BufferView, CreateInfo);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"

FVertexDeclarationRHIRef FVulkanRHI::CreateVertexDeclaration(const FVertexDeclarationElementList& Elements)
{
	// Implement vertex declaration creation
	return FVertexDeclarationRHIRef();
}

// Shader Management
FRHIShaderRef FVulkanRHI::CreateShader(const FShaderCreateInfo& CreateInfo)
{
	return new FVulkanShader(CreateInfo);
}

// Pipeline State Management
FRHIGraphicsPSO* FVulkanRHI::CreateGraphicsPSO(const FGraphicsPSOCreateInfo& Initializer)
{
	return nullptr;
	//	VkPipeline		 Pipeline = CreateVulkanGraphicsPipeline(Initializer);
	//	VkPipelineLayout PipelineLayout = CreateVulkanPipelineLayout(Initializer);
	//	return new FVulkanGraphicsPSO(Pipeline, PipelineLayout);
}

FRHIComputePSO* FVulkanRHI::CreateComputePSO(const FComputePSOInitializer& Initializer)
{
	return nullptr;
	//	VkPipeline		 Pipeline = CreateVulkanComputePipeline(Initializer);
	//	VkPipelineLayout PipelineLayout = CreateVulkanPipelineLayout(Initializer);
	//	return new FVulkanComputePSO(Pipeline, PipelineLayout);
}

// Command List and Context
FRHICommandListImmediate& FVulkanRHI::GetImmediateCommandList()
{
	// Return the immediate command list
	static FRHICommandListImmediate ImmediateCommandList;
	return ImmediateCommandList;
}

FRHIComputeCommandList& FVulkanRHI::GetComputeCommandList()
{
	// Return the compute command list
	static FRHIComputeCommandList ComputeCommandList;
	return ComputeCommandList;
}

// Synchronization
void FVulkanRHI::RHISubmitCommandsAndFlushGPU()
{
	SubmitVulkanCommandsAndFlushGPU();
}

void FVulkanRHI::RHIFlushResources()
{
	FlushVulkanResources();
}

// Viewport and Swap Chain
void FVulkanRHI::RHICreateViewport(void* WindowHandle, TUINT32 Width, TUINT32 Height, bool bIsFullscreen, EPixelFormat PreferredPixelFormat, FRHIViewportRef& OutViewport)
{
	CreateVulkanSwapChain(WindowHandle, Width, Height, bIsFullscreen, PreferredPixelFormat, OutViewport);
}

void FVulkanRHI::RHIResizeViewport(FRHIViewportRef& Viewport, TUINT32 Width, TUINT32 Height, bool bIsFullscreen, EPixelFormat PreferredPixelFormat)
{
	ResizeVulkanSwapChain(Viewport, Width, Height, bIsFullscreen, PreferredPixelFormat);
}

void FVulkanRHI::RHISwapBuffers(FRHIViewportRef& Viewport)
{
	PresentVulkanSwapChain(Viewport);
}

FRHITextureRef FVulkanRHI::GetRHIBackBuffer()
{
	return VulkanViewport->GetBackBuffer();
}

FRHIViewportRef FVulkanRHI::GetRHIViewport()
{
	return VulkanViewport;
}

// Render Pass and Draw Commands
void FVulkanRHI::RHIBeginRenderPass(const FRHIRenderPassInfo& RenderPassInfo)
{
	RenderPassInfo.Validate();
	BeginVulkanRenderPass(RenderPassInfo);
}

void FVulkanRHI::RHIEndRenderPass()
{
	EndVulkanRenderPass();
}

void FVulkanRHI::RHIDrawPrimitive(TUINT32 BaseVertexIndex, TUINT32 NumPrimitives, TUINT32 NumInstances)
{
	// Implement draw primitive
}

void FVulkanRHI::RHIDrawIndexedPrimitive(FRHIBuffer* IndexBuffer, TUINT32 BaseVertexIndex, TUINT32 FirstInstance, TUINT32 NumVertices, TUINT32 StartIndex, TUINT32 NumPrimitives, TUINT32 NumInstances)
{
	// Implement draw indexed primitive
}

// Compute Dispatch
void FVulkanRHI::RHIDispatchComputeShader(TUINT32 ThreadGroupCountX, TUINT32 ThreadGroupCountY, TUINT32 ThreadGroupCountZ)
{
	// Implement compute dispatch
}

// Query and Timestamp
FQueryRHIRef FVulkanRHI::CreateQuery(ERHIQueryType QueryType)
{
	return FQueryRHIRef{};
	//	VkQueryPool QueryPool = CreateVulkanQueryPool(QueryType);
	//	return new FVulkanQuery(QueryPool, 0, QueryType);
}

void FVulkanRHI::RHIBeginQuery(FQueryRHIRef& Query)
{
	//	FVulkanQuery* VulkanQuery = static_cast<FVulkanQuery*>(Query.GetReference());
	//	BeginVulkanQuery(VulkanQuery->GetQueryPool(), VulkanQuery->GetQueryIndex());
}

void FVulkanRHI::RHIEndQuery(FQueryRHIRef& Query)
{
	//	FVulkanQuery* VulkanQuery = static_cast<FVulkanQuery*>(Query.GetReference());
	//	EndVulkanQuery(VulkanQuery->GetQueryPool(), VulkanQuery->GetQueryIndex());
}

void FVulkanRHI::RHIGetQueryResults(FQueryRHIRef& Query, TUINT64& OutResult, bool bWait)
{
	//	FVulkanQuery* VulkanQuery = static_cast<FVulkanQuery*>(Query.GetReference());
	//	GetVulkanQueryResults(VulkanQuery->GetQueryPool(), VulkanQuery->GetQueryIndex(), OutResult, bWait);
}

// Debugging and Profiling
void FVulkanRHI::RHIPushEvent(const TCHAR* Name)
{
	PushVulkanEvent(Name);
}

void FVulkanRHI::RHIPopEvent()
{
	PopVulkanEvent();
}

// Memory Management
void FVulkanRHI::RHIFlushPendingDeletes()
{
	FlushVulkanPendingDeletes();
}

// Misc
void FVulkanRHI::RHISetGraphicsPSO(FRHIGraphicsPSO* PipelineState)
{
	//	FVulkanGraphicsPSO* VulkanPipelineState = static_cast<FVulkanGraphicsPSO*>(PipelineState);
	//	VulkanRHI::vkCmdBindPipeline(GetCurrentCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, VulkanPipelineState->GetPipeline());
}

void FVulkanRHI::RHISetComputePSO(FRHIComputePSO* PipelineState)
{
	//	FVulkanComputePSO* VulkanPipelineState = static_cast<FVulkanComputePSO*>(PipelineState);
	//	VulkanRHI::vkCmdBindPipeline(GetCurrentCommandBuffer(), VK_PIPELINE_BIND_POINT_COMPUTE, VulkanPipelineState->GetPipeline());
}

void FVulkanRHI::RHISetViewport(TUINT32 MinX, TUINT32 MinY, float MinZ, TUINT32 MaxX, TUINT32 MaxY, float MaxZ)
{
	//	VkViewport Viewport = { static_cast<float>(MinX), static_cast<float>(MinY), static_cast<float>(MaxX - MinX), static_cast<float>(MaxY - MinY), MinZ, MaxZ };
	//	VulkanRHI::vkCmdSetViewport(GetCurrentCommandBuffer(), 0, 1, &Viewport);
}

void FVulkanRHI::RHISetScissorRect(bool bEnable, TUINT32 MinX, TUINT32 MinY, TUINT32 MaxX, TUINT32 MaxY)
{
	//	VkRect2D Scissor = { { static_cast<int32_t>(MinX), static_cast<int32_t>(MinY) }, { MaxX - MinX, MaxY - MinY } };
	//	VulkanRHI::vkCmdSetScissor(GetCurrentCommandBuffer(), 0, 1, &Scissor);
}

// Vulkan-specific initialization
void FVulkanRHI::CreateVulkanInstance()
{
	VkApplicationInfo AppInfo = {};
	AppInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	AppInfo.pApplicationName = "HLVM";
	AppInfo.applicationVersion = VK_MAKE_VERSION(HLVM_MAJOR_VERSION, HLVM_MINOR_VERSION, HLVM_PATCH_VERSION);
	AppInfo.pEngineName = "HLVM";
	AppInfo.engineVersion = VK_MAKE_VERSION(HLVM_MAJOR_VERSION, HLVM_MINOR_VERSION, HLVM_PATCH_VERSION);
	AppInfo.apiVersion = VULKAN_API_VERSION;

	VkInstanceCreateInfo CreateInfo = {};
	CreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	CreateInfo.pApplicationInfo = &AppInfo;

	auto extensions = GetRequiredExtensions();
	CreateInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	CreateInfo.ppEnabledExtensionNames = extensions.data();

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
	if (bUseValidationLayers)
	{
		if (CheckValidationLayerSupport())
		{
			CreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			CreateInfo.ppEnabledLayerNames = validationLayers.data();

			PopulateDebugMessengerCreateInfo(debugCreateInfo);
			CreateInfo.pNext = S_C(VkDebugUtilsMessengerCreateInfoEXT*, &debugCreateInfo);
		}
		else
		{
			HLVM_LOG(LogVulkanRHI, warn, TXT("Validation layers requested, but not available!"));
			bUseValidationLayers = false;
		}
	}
	else
	{
		CreateInfo.enabledLayerCount = 0;
		CreateInfo.pNext = nullptr;
	}

	VkResult Result = VulkanRHI::vkCreateInstance(&CreateInfo, nullptr, &VulkanInstance);
	HLVM_ENSURE(Result == VK_SUCCESS);
}

void FVulkanRHI::CreateDebugLayer()
{
	VkDebugUtilsMessengerCreateInfoEXT createInfo;
	PopulateDebugMessengerCreateInfo(createInfo);
	VkResult Result = CreateDebugUtilsMessengerEXT(VulkanInstance, &createInfo, nullptr, &DebugMessenger);
	HLVM_ENSURE(Result == VK_SUCCESS);
}

void FVulkanRHI::CreateSurface()
{
	VulkanSurface = InitializerParam.CreateSurfaceFunc(VulkanInstance);
	HLVM_ENSURE(VulkanSurface != VK_NULL_HANDLE);
}

void FVulkanRHI::CreateVulkanPhysicalDevice()
{
	// 1 Find suitable device
	uint32_t DeviceCount = 0;
	VulkanRHI::vkEnumeratePhysicalDevices(VulkanInstance, &DeviceCount, nullptr);
	HLVM_ENSURE(DeviceCount > 0);

	TVector<VkPhysicalDevice> PhysicalDevices(DeviceCount);
	VulkanRHI::vkEnumeratePhysicalDevices(VulkanInstance, &DeviceCount, PhysicalDevices.data());

	for (const auto& device : PhysicalDevices)
	{
		if (IsDeviceSuitable(device, VulkanSurface))
		{
			VulkanPhysicalDevice = device;
			break;
		}
	}
	HLVM_ENSURE(VulkanPhysicalDevice != VK_NULL_HANDLE);
	PhysicalDevice = new FVulkanPhysicalDevice(VulkanPhysicalDevice);
}

void FVulkanRHI::CreateVulkanLogicalDevice()
{
	// 2 Create logical device
	deviceQueueFamilyIndices = FindQueueFamilies(PhysicalDevice->GetHandle(), VulkanSurface);

	TVector<VkDeviceQueueCreateInfo> queueCreateInfos;
	TSet<uint32_t>					 uniqueQueueFamilies = {
		  deviceQueueFamilyIndices.graphicsFamily,
		  deviceQueueFamilyIndices.computeFamily,
		  deviceQueueFamilyIndices.transferFamily,
		  deviceQueueFamilyIndices.presentFamily
	};

	float queuePriority = 1.0f;
	for (uint32_t queueFamily : uniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures deviceFeatures{};
	deviceFeatures.geometryShader = true;
	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();

	if (bUseValidationLayers)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else
	{
		createInfo.enabledLayerCount = 0;
	}

	VULKAN_ENSURE(VulkanRHI::vkCreateDevice(VulkanPhysicalDevice, &createInfo, nullptr, &VulkanDevice));
	LogicalDevice = new FVulkanLogicalDevice(VulkanDevice);
}

void FVulkanRHI::CreateVulkanQueues()
{
	// Queues are created during device creation
	VulkanRHI::vkGetDeviceQueue(VulkanDevice, deviceQueueFamilyIndices.graphicsFamily, 0, &GraphicsQueue);
	HLVM_ENSURE(GraphicsQueue != VK_NULL_HANDLE);

	VulkanRHI::vkGetDeviceQueue(VulkanDevice, deviceQueueFamilyIndices.computeFamily, 0, &ComputeQueue);
	HLVM_ENSURE(ComputeQueue != VK_NULL_HANDLE);

	VulkanRHI::vkGetDeviceQueue(VulkanDevice, deviceQueueFamilyIndices.transferFamily, 0, &TransferQueue);
	HLVM_ENSURE(TransferQueue != VK_NULL_HANDLE);

	VulkanRHI::vkGetDeviceQueue(VulkanDevice, deviceQueueFamilyIndices.presentFamily, 0, &PresentQueue);
	HLVM_ENSURE(PresentQueue != VK_NULL_HANDLE);
}

void FVulkanRHI::CreateVulkanViewPort()
{
	SharedRefPtr<FGLFW3Vulkan> glfwWindow = SP_C(FGLFW3Vulkan, InitializerParam.NativeWindowHandle);
	const IWindow::Properties& Property = glfwWindow->GetProperties();

	FRHIViewportCreateInfo ViewportDesc;
	ViewportDesc.DebugName = Property.Title;
	ViewportDesc.Extent = Property.Extent;
	ViewportDesc.ViewportType = ERHIViewportType::Fullscreen;
	ViewportDesc.Format = EPixelFormat::R8G8B8A8_UNorm;
	ViewportDesc.NativeWindowHandle = glfwWindow.get();
	ViewportDesc.bHeadlessRendering = false;
	VulkanViewport = new FVulkanViewport(ViewportDesc);

	FVulkanSwapChain::FRecreateInfo RecreateInfo;
	RecreateInfo.OldSwapChain = nullptr;
	RecreateInfo.Surface = VulkanSurface;
	VulkanViewport->CreateSwapChain(RecreateInfo);
}

void FVulkanRHI::CreateVulkanMemoryAllocator()
{
	VmaAllocatorCreateInfo allocatorCreateInfo = {};
	allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
	allocatorCreateInfo.vulkanApiVersion = VULKAN_API_VERSION;
	allocatorCreateInfo.physicalDevice = VulkanPhysicalDevice;
	allocatorCreateInfo.device = VulkanDevice;
	allocatorCreateInfo.instance = VulkanInstance;
	allocatorCreateInfo.pVulkanFunctions = &VulkanRHI::VULKAN_VMA_FUNCTIONS;
	vmaCreateAllocator(&allocatorCreateInfo, &VulkanRHI::VULKAN_VMA_ALLOCATOR);
}

// Vulkan-specific resource creation
VkImage FVulkanRHI::CreateVulkanImage(const FRHITextureCreateInfo& CreateInfo)
{
	VkImageCreateInfo ImageInfo = {};
	ImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ImageInfo.imageType = VK_IMAGE_TYPE_2D;
	ImageInfo.extent.width = CreateInfo.Extent.x;
	ImageInfo.extent.height = CreateInfo.Extent.y;
	ImageInfo.extent.depth = CreateInfo.Extent.z;
	ImageInfo.mipLevels = CreateInfo.NumMips;
	ImageInfo.arrayLayers = 1;
	ImageInfo.format = VulkanRHI::VulkanFormatFromRHIFormat(CreateInfo.Format);
	ImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	ImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	ImageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	ImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

	VkImage	 Image;
	VkResult Result = VulkanRHI::vkCreateImage(VulkanDevice, &ImageInfo, VulkanRHI::VULKAN_CPU_ALLOCATOR, &Image);
	HLVM_ENSURE(Result == VK_SUCCESS);

	return Image;
}

void FVulkanRHI::DestroyVulkanImage(VkImage Image)
{
	HLVM_ENSURE(Image != VK_NULL_HANDLE);
	VulkanRHI::vkDestroyImage(VulkanDevice, Image, VulkanRHI::VULKAN_CPU_ALLOCATOR);
}

VkSampler FVulkanRHI::CreateVulkanSampler(const FRHISamplerStateCreateInfo& CreateInfo)
{
	VkSamplerCreateInfo SamplerInfo;
	VulkanRHI::ZeroVulkanStruct(&SamplerInfo, VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);

	SamplerInfo.magFilter = VulkanRHI::VulkanFilterFromRHIFilter(CreateInfo.Filter);
	SamplerInfo.minFilter = VulkanRHI::VulkanFilterFromRHIFilter(CreateInfo.Filter);
	SamplerInfo.mipmapMode = VulkanRHI::VulkanMipFilterFromRHIFilter(CreateInfo.Filter);
	SamplerInfo.addressModeU = VulkanRHI::VulkanAddressModeFromRHIAddressMode(CreateInfo.AddressModeU);
	SamplerInfo.addressModeV = VulkanRHI::VulkanAddressModeFromRHIAddressMode(CreateInfo.AddressModeV);
	SamplerInfo.addressModeW = VulkanRHI::VulkanAddressModeFromRHIAddressMode(CreateInfo.AddressModeW);

	SamplerInfo.mipLodBias = CreateInfo.MipBias;

	SamplerInfo.maxAnisotropy = 1.0f;
	if (CreateInfo.Filter == ETextureFilter::Anisotropic)
	{
		SamplerInfo.maxAnisotropy = FMath::Clamp(CreateInfo.MaxAnisotropy, 1.0f, PhysicalDevice->GetProperties().limits.maxSamplerAnisotropy);
	}
	SamplerInfo.anisotropyEnable = SamplerInfo.maxAnisotropy > 1.0f;

	SamplerInfo.compareEnable = CreateInfo.ComparisonFunction != ECompareFunction::Never ? VK_TRUE : VK_FALSE;
	SamplerInfo.compareOp = VulkanRHI::VulkanCompareOpFromRHI(CreateInfo.ComparisonFunction);
	SamplerInfo.minLod = CreateInfo.MinMipLevel;
	SamplerInfo.maxLod = CreateInfo.MaxMipLevel;
	// Only support opaque white (all 1) and transparent black (all 0) for the time being
	SamplerInfo.borderColor = CreateInfo.BorderColor == FVec4{ 0.0f } ? VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK : VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

	// call vk api
	VkSampler Sampler;
	VULKAN_ENSURE(VulkanRHI::vkCreateSampler(VulkanDevice, &SamplerInfo, VulkanRHI::VULKAN_CPU_ALLOCATOR, &Sampler));
	return Sampler;
}

VkBuffer FVulkanRHI::CreateVulkanBuffer(const FRHIBufferCreateInfo& CreateInfo, void** OutAllocation)
{
	VkBufferUsageFlags	  UsageFlags = VulkanRHI::VulkanBufferUsageFlagsFromRHIUsageFlags(CreateInfo.UsageFlags);
	VkMemoryPropertyFlags MemoryPropertyFlags = VulkanRHI::VulkanMemoryPropertyFlagsFromRHIMemoryPropertyFlags(CreateInfo.MemoryPropertyFlags);
	VkDeviceSize		  Size = CreateInfo.Size;

	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = Size;
	bufferCreateInfo.usage = UsageFlags;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocCreateInfo = {};
	allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
	allocCreateInfo.requiredFlags = MemoryPropertyFlags;

	VkBuffer	  Buffer;
	VmaAllocation Allocation;
	VULKAN_ENSURE(vmaCreateBuffer(VulkanRHI::VULKAN_VMA_ALLOCATOR, &bufferCreateInfo, &allocCreateInfo, &Buffer, &Allocation, nullptr));
	*OutAllocation = Allocation;

	return Buffer;
}

void FVulkanRHI::DestroyVulkanBuffer(VkBuffer Buffer, void** InAllocation)
{
	HLVM_ENSURE(*InAllocation != nullptr);
	HLVM_ENSURE(Buffer != VK_NULL_HANDLE);
	vmaDestroyBuffer(VulkanRHI::VULKAN_VMA_ALLOCATOR, Buffer, R_C(VmaAllocation, *InAllocation));
}

VkImageView FVulkanRHI::CreateVulkanImageView(VkImage Image, const FRHIShaderResourceViewCreateInfo& CreateInfo)
{
	VkImageViewCreateInfo ViewInfo = {};
	ViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	ViewInfo.image = Image;
	ViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	ViewInfo.format = VulkanRHI::VulkanFormatFromRHIFormat(CreateInfo.Format);
	ViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	ViewInfo.subresourceRange.baseMipLevel = 0;
	ViewInfo.subresourceRange.levelCount = 1;
	ViewInfo.subresourceRange.baseArrayLayer = 0;
	ViewInfo.subresourceRange.layerCount = 1;

	VkImageView ImageView;
	VkResult	Result = VulkanRHI::vkCreateImageView(VulkanDevice, &ViewInfo, nullptr, &ImageView);
	HLVM_ENSURE(Result == VK_SUCCESS);

	return ImageView;
}

VkBufferView FVulkanRHI::CreateVulkanBufferView(VkBuffer Buffer, const FRHIUnorderedAccessViewCreateInfo& CreateInfo)
{
	VkBufferViewCreateInfo ViewInfo = {};
	ViewInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
	ViewInfo.buffer = Buffer;
	ViewInfo.format = VulkanRHI::VulkanFormatFromRHIFormat(CreateInfo.Format);
	ViewInfo.offset = CreateInfo.Offset;
	ViewInfo.range = CreateInfo.Size;

	VkBufferView BufferView;
	VkResult	 Result = VulkanRHI::vkCreateBufferView(VulkanDevice, &ViewInfo, nullptr, &BufferView);
	HLVM_ENSURE(Result == VK_SUCCESS);

	return BufferView;
}

VkShaderModule FVulkanRHI::CreateVulkanShaderModule(const FShaderCreateInfo& CreateInfo)
{
	VkShaderModuleCreateInfo createInfo;
	VulkanRHI::ZeroVulkanStruct(&createInfo, VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
	const TVector<TBYTE>& code = CreateInfo.Code;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shaderModule = VK_NULL_HANDLE;
	VULKAN_ENSURE(VulkanRHI::vkCreateShaderModule(VulkanDevice, &createInfo, VulkanRHI::VULKAN_CPU_ALLOCATOR, &shaderModule));
	return shaderModule;
}

void FVulkanRHI::DestroyVulkanShaderModule(VkShaderModule ShaderModule)
{
	HLVM_ASSERT(ShaderModule != VK_NULL_HANDLE);
	VulkanRHI::vkDestroyShaderModule(VulkanDevice, ShaderModule, VulkanRHI::VULKAN_CPU_ALLOCATOR);
}

// Vulkan-specific command list management
VkCommandBuffer FVulkanRHI::BeginVulkanCommandBuffer()
{
	VkCommandBufferAllocateInfo AllocInfo = {};
	AllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	// AllocInfo.commandPool = GetCommandPool();
	AllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	AllocInfo.commandBufferCount = 1;

	VkCommandBuffer CommandBuffer;
	VkResult		Result = VulkanRHI::vkAllocateCommandBuffers(VulkanDevice, &AllocInfo, &CommandBuffer);
	HLVM_ENSURE(Result == VK_SUCCESS);

	VkCommandBufferBeginInfo BeginInfo = {};
	BeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	BeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	Result = VulkanRHI::vkBeginCommandBuffer(CommandBuffer, &BeginInfo);
	HLVM_ENSURE(Result == VK_SUCCESS);

	return CommandBuffer;
}

void FVulkanRHI::EndVulkanCommandBuffer(VkCommandBuffer CommandBuffer)
{
	VkResult Result = VulkanRHI::vkEndCommandBuffer(CommandBuffer);
	HLVM_ENSURE(Result == VK_SUCCESS);
}

// Vulkan-specific synchronization
void FVulkanRHI::SubmitVulkanCommandsAndFlushGPU()
{
	VkSubmitInfo SubmitInfo = {};
	SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	SubmitInfo.commandBufferCount = 1;
	// SubmitInfo.pCommandBuffers = &GetCurrentCommandBuffer();

	VkResult Result = VulkanRHI::vkQueueSubmit(GraphicsQueue, 1, &SubmitInfo, VK_NULL_HANDLE);
	HLVM_ENSURE(Result == VK_SUCCESS);

	VulkanRHI::vkQueueWaitIdle(GraphicsQueue);
}

void FVulkanRHI::FlushVulkanResources()
{
	VulkanRHI::vkDeviceWaitIdle(VulkanDevice);
}

// Vulkan-specific viewport and swap chain management
void FVulkanRHI::CreateVulkanSwapChain(void* WindowHandle, TUINT32 Width, TUINT32 Height, bool bIsFullscreen, EPixelFormat PreferredPixelFormat, FRHIViewportRef& OutViewport)
{
	// Implement swap chain creation
}

void FVulkanRHI::ResizeVulkanSwapChain(FRHIViewportRef& Viewport, TUINT32 Width, TUINT32 Height, bool bIsFullscreen, EPixelFormat PreferredPixelFormat)
{
	// Implement swap chain resizing
}

void FVulkanRHI::PresentVulkanSwapChain(FRHIViewportRef& Viewport)
{
	// Implement swap chain presentation
}

// Vulkan-specific render pass management
void FVulkanRHI::BeginVulkanRenderPass(const FRHIRenderPassInfo& RenderPassInfo)
{
	HLVM_ASSERT(ActiveRenderPass == nullptr);
	// TODO Implement RenderPassAdditionalInfo based on renderpass info
	ActiveRenderPass = new FVulkanRenderPass(LogicalDevice, { RenderPassInfo, FVulkanRenderTargetLayout::RenderPassAdditionalInfo{} });
}

void FVulkanRHI::EndVulkanRenderPass()
{
	HLVM_ASSERT(ActiveRenderPass != nullptr);
	PendingDestroyRenderPass.Add(ActiveRenderPass);
	ActiveRenderPass = nullptr;
}

// Vulkan-specific query and timestamp management
VkQueryPool FVulkanRHI::CreateVulkanQueryPool(ERHIQueryType QueryType)
{
	VkQueryPoolCreateInfo PoolInfo = {};
	PoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	// PoolInfo.queryType = ConvertQueryTypeToVulkan(QueryType);
	PoolInfo.queryCount = 1;

	VkQueryPool QueryPool;
	VkResult	Result = VulkanRHI::vkCreateQueryPool(VulkanDevice, &PoolInfo, nullptr, &QueryPool);
	HLVM_ENSURE(Result == VK_SUCCESS);

	return QueryPool;
}

void FVulkanRHI::BeginVulkanQuery(VkQueryPool QueryPool, TUINT32 QueryIndex)
{
	// VulkanRHI::vkCmdBeginQuery(GetCurrentCommandBuffer(), QueryPool, QueryIndex, 0);
}

void FVulkanRHI::EndVulkanQuery(VkQueryPool QueryPool, TUINT32 QueryIndex)
{
	// VulkanRHI::vkCmdEndQuery(GetCurrentCommandBuffer(), QueryPool, QueryIndex);
}

void FVulkanRHI::GetVulkanQueryResults(VkQueryPool QueryPool, TUINT32 QueryIndex, TUINT64& OutResult, bool bWait)
{
	VkResult Result = VulkanRHI::vkGetQueryPoolResults(VulkanDevice, QueryPool, QueryIndex, 1, sizeof(OutResult), &OutResult, sizeof(OutResult), bWait ? VK_QUERY_RESULT_WAIT_BIT : 0);
	HLVM_ENSURE(Result == VK_SUCCESS);
}

// Vulkan-specific debugging and profiling
void FVulkanRHI::PushVulkanEvent(const TCHAR* Name)
{
	// Implement debug event push
}

void FVulkanRHI::PopVulkanEvent()
{
	// Implement debug event pop
}

// Vulkan-specific memory management
void FVulkanRHI::FlushVulkanPendingDeletes()
{
	// Implement pending resource deletion
}

// Generate VkImageCreateInfo from FRHITextureCreateInfo
VkImageCreateInfo FVulkanRHI::GenerateVkImageCreateInfo(const FRHITextureCreateInfo& CreateInfo)
{
	VkImageCreateInfo ImageCreateInfo = {};
	ImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ImageCreateInfo.imageType = VK_IMAGE_TYPE_2D; // Assuming 2D texture for simplicity
	ImageCreateInfo.extent.width = CreateInfo.Extent.x;
	ImageCreateInfo.extent.height = CreateInfo.Extent.y;
	ImageCreateInfo.extent.depth = CreateInfo.Extent.z;
	ImageCreateInfo.mipLevels = CreateInfo.NumMips;
	ImageCreateInfo.samples = S_C(VkSampleCountFlagBits, CreateInfo.NumSamples);
	ImageCreateInfo.format = VulkanRHI::VulkanFormatFromRHIFormat(CreateInfo.Format); // Helper function to convert RHI format to Vulkan format
	ImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	ImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	ImageCreateInfo.usage = VulkanRHI::VulkanTextureUsageFlagsFromRHIUsageFlags(CreateInfo.Flags); // Helper function to convert RHI usage flags to Vulkan usage flags
	ImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	return ImageCreateInfo;
}

// Generate VkBufferCreateInfo from FRHIBufferCreateInfo
VkBufferCreateInfo FVulkanRHI::GenerateVkBufferCreateInfo(const FRHIBufferCreateInfo& CreateInfo)
{
	VkBufferCreateInfo BufferCreateInfo = {};
	BufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	BufferCreateInfo.size = CreateInfo.Size;
	BufferCreateInfo.usage = VulkanRHI::VulkanBufferUsageFlagsFromRHIUsageFlags(CreateInfo.UsageFlags); // Helper function to convert RHI usage flags to Vulkan usage flags
	BufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	return BufferCreateInfo;
}

// Generate VkShaderModuleCreateInfo from FShaderCreateInfo
VkShaderModuleCreateInfo FVulkanRHI::GenerateVkShaderModuleCreateInfo(const FShaderCreateInfo& CreateInfo)
{
	VkShaderModuleCreateInfo ShaderModuleCreateInfo = {};
	ShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ShaderModuleCreateInfo.codeSize = CreateInfo.Code.size();
	ShaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(CreateInfo.Code.data());

	return ShaderModuleCreateInfo;
}

// Generate VkImageViewCreateInfo from FRHIShaderResourceViewCreateInfo
VkImageViewCreateInfo FVulkanRHI::GenerateVkImageViewCreateInfo(const FRHIShaderResourceViewCreateInfo& CreateInfo)
{
	VkImageViewCreateInfo ImageViewCreateInfo = {};
	ImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	ImageViewCreateInfo.image = VK_NULL_HANDLE;											  // To be set later
	ImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;								  // Assuming 2D image view for simplicity
	ImageViewCreateInfo.format = VulkanRHI::VulkanFormatFromRHIFormat(CreateInfo.Format); // Helper function to convert RHI format to Vulkan format
	ImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	ImageViewCreateInfo.subresourceRange.baseMipLevel = CreateInfo.MipLevel;
	ImageViewCreateInfo.subresourceRange.levelCount = CreateInfo.NumMipLevels;
	ImageViewCreateInfo.subresourceRange.baseArrayLayer = CreateInfo.FirstArraySlice;
	ImageViewCreateInfo.subresourceRange.layerCount = CreateInfo.NumArraySlices;

	return ImageViewCreateInfo;
}

// Generate VkBufferViewCreateInfo from FRHIUnorderedAccessViewCreateInfo
VkBufferViewCreateInfo FVulkanRHI::GenerateVkBufferViewCreateInfo(const FRHIUnorderedAccessViewCreateInfo& CreateInfo)
{
	VkBufferViewCreateInfo BufferViewCreateInfo = {};
	BufferViewCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
	BufferViewCreateInfo.buffer = VK_NULL_HANDLE;										   // To be set later
	BufferViewCreateInfo.format = VulkanRHI::VulkanFormatFromRHIFormat(CreateInfo.Format); // Helper function to convert RHI format to Vulkan format
	BufferViewCreateInfo.offset = 0;
	BufferViewCreateInfo.range = VK_WHOLE_SIZE;

	return BufferViewCreateInfo;
}

// Generate VkSamplerCreateInfo from FRHISamplerStateCreateInfo (assuming this struct exists)
VkSamplerCreateInfo FVulkanRHI::GenerateVkSamplerCreateInfo(const FRHISamplerStateCreateInfo& CreateInfo)
{
	VkSamplerCreateInfo SamplerCreateInfo = {};
	SamplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

	// Set filter modes
	SamplerCreateInfo.magFilter = VulkanRHI::VulkanFilterFromRHIFilter(CreateInfo.Filter);
	SamplerCreateInfo.minFilter = VulkanRHI::VulkanFilterFromRHIFilter(CreateInfo.Filter);

	// Set address modes
	SamplerCreateInfo.addressModeU = VulkanRHI::VulkanAddressModeFromRHIAddressMode(CreateInfo.AddressModeU);
	SamplerCreateInfo.addressModeV = VulkanRHI::VulkanAddressModeFromRHIAddressMode(CreateInfo.AddressModeV);
	SamplerCreateInfo.addressModeW = VulkanRHI::VulkanAddressModeFromRHIAddressMode(CreateInfo.AddressModeW);

	// Set mip map level of detail bias
	SamplerCreateInfo.mipLodBias = CreateInfo.MipBias;

	// Set maximum anisotropy
	SamplerCreateInfo.anisotropyEnable = VK_TRUE;
	SamplerCreateInfo.maxAnisotropy = CreateInfo.MaxAnisotropy;

	// Set comparison function
	SamplerCreateInfo.compareEnable = VK_TRUE;
	SamplerCreateInfo.compareOp = VulkanRHI::VulkanCompareOpFromRHI(CreateInfo.ComparisonFunction);

	// Set border color
	SamplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	SamplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

	return SamplerCreateInfo;
}

// Generate VkPipelineShaderStageCreateInfo from FShaderCreateInfo
VkPipelineShaderStageCreateInfo FVulkanRHI::GenerateVkPipelineShaderStageCreateInfo(const FShaderCreateInfo& CreateInfo, VkShaderModule ShaderModule)
{
	VkPipelineShaderStageCreateInfo ShaderStageCreateInfo;
	VulkanRHI::ZeroVulkanStruct(&ShaderStageCreateInfo, VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
	ShaderStageCreateInfo.stage = VulkanRHI::VulkanShaderStageFromRHIStage(CreateInfo.Stage); // Convert RHI shader stage to Vulkan shader stage
	ShaderStageCreateInfo.module = ShaderModule;
	ShaderStageCreateInfo.pName = TO_CHAR_CSTR(CreateInfo.EntryPoints[0].c_str());
	return ShaderStageCreateInfo;
}

// Generate VkPipelineLayoutCreateInfo from FRHIGraphicsPSOCreateInfo
VkPipelineLayoutCreateInfo FVulkanRHI::GenerateVkPipelineLayoutCreateInfo(const FRHIGraphicsPipelineLayoutCreateInfo& CreateInfo)
{
	VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo = {};
	PipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	//	// Add descriptor set layouts
	//	TVector<VkDescriptorSetLayout> descriptorSetLayouts;
	//	for (const auto& layout : CreateInfo.DescSetLayouts)
	//	{
	//		descriptorSetLayouts.push_back(layout);
	//	}
	//	PipelineLayoutCreateInfo.descriptorSetLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
	//	PipelineLayoutCreateInfo.pDescSetLayouts = descriptorSetLayouts.data();
	//
	//	// Add push constant ranges
	//	TVector<VkPushConstantRange> pushConstantRanges;
	//	for (const auto& range : CreateInfo.PushConstantRanges)
	//	{
	//		pushConstantRanges.push_back(range);
	//	}
	//	PipelineLayoutCreateInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
	//	PipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();

	return PipelineLayoutCreateInfo;
}

// Generate VkGraphicsPipelineCreateInfo from FRHIGraphicsPSOCreateInfo
VkGraphicsPipelineCreateInfo FVulkanRHI::GenerateVkGraphicsPipelineCreateInfo(const FRHIGraphicsPSOCreateInfo& CreateInfo)
{
	VkGraphicsPipelineCreateInfo PipelineCreateInfo = {};
	PipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

	// Add shader stages
	TVector<VkPipelineShaderStageCreateInfo> shaderStages;
	for (const auto& shader : CreateInfo.Shaders)
	{
		VkPipelineShaderStageCreateInfo stageInfo = {};
		stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stageInfo.stage = VulkanRHI::VulkanShaderStageFromRHIStage(shader.Stage);
		stageInfo.module = VK_NULL_HANDLE; // To be set later
		stageInfo.pName = TO_CHAR_CSTR(shader.EntryPoints[0].c_str());
		shaderStages.push_back(stageInfo);
	}
	PipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	PipelineCreateInfo.pStages = shaderStages.data();

	//	// Add vertex input state
	//	PipelineCreateInfo.pVertexInputState = &CreateInfo.VertexInputState;
	//
	//	// Add input assembly state
	//	PipelineCreateInfo.pInputAssemblyState = &CreateInfo.InputAssemblyState;
	//
	//	// Add viewport state
	//	PipelineCreateInfo.pViewportState = &CreateInfo.ViewportState;
	//
	//	// Add rasterization state
	//	PipelineCreateInfo.pRasterizationState = &CreateInfo.RasterizationState;
	//
	//	// Add multisample state
	//	PipelineCreateInfo.pMultisampleState = &CreateInfo.MultisampleState;
	//
	//	// Add depth stencil state
	//	PipelineCreateInfo.pDepthStencilState = &CreateInfo.DepthStencilState;
	//
	//	// Add color blend state
	//	PipelineCreateInfo.pColorBlendState = &CreateInfo.ColorBlendState;
	//
	//	// Add dynamic state
	//	PipelineCreateInfo.pDynamicState = &CreateInfo.DynamicState;

	// Add pipeline layout
	PipelineCreateInfo.layout = VK_NULL_HANDLE; // To be set later

	// Add render pass and subpass
	PipelineCreateInfo.renderPass = VK_NULL_HANDLE; // To be set later
	PipelineCreateInfo.subpass = 0;

	return PipelineCreateInfo;
}

// Generate VkComputePipelineCreateInfo from FRHIComputePSOCreateInfo (assuming this struct exists)
VkComputePipelineCreateInfo FVulkanRHI::GenerateVkComputePipelineCreateInfo(const FRHIComputePSOCreateInfo& CreateInfo)
{
	VkComputePipelineCreateInfo PipelineCreateInfo = {};
	PipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;

	return PipelineCreateInfo;
}

// Generate VkQueryPoolCreateInfo from FRHIQueryCreateInfo (assuming this struct exists)
VkQueryPoolCreateInfo FVulkanRHI::GenerateVkQueryPoolCreateInfo(const FRHIQueryCreateInfo& CreateInfo)
{
	VkQueryPoolCreateInfo QueryPoolCreateInfo = {};
	QueryPoolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	// QueryPoolCreateInfo.queryType = VulkanQueryTypeFromRHIQueryType(CreateInfo.QueryType); // Helper function to convert RHI query type to Vulkan query type
	QueryPoolCreateInfo.queryCount = CreateInfo.NumQueries;

	return QueryPoolCreateInfo;
}

void FVulkanRHI::SetVulkanMinimalContext(void* InContext) const
{
	FVulkanMinimalContext* MinimalContext = S_C(FVulkanMinimalContext*, InContext);
	*MinimalContext = FVulkanMinimalContext(VulkanInstance, PhysicalDevice, LogicalDevice);
}

#pragma clang diagnostic pop
