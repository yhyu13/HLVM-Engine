#include "RHI/Vulkan/VulkanRHI.h"
// #include "VulkanRHIResource.h"
// #include "VulkanRHIDefinition.h"
// #include "VulkanUtils.h" // Helper functions for Vulkan operations

DECLARE_LOG_CATEGORY(LogVulkanRHI)

namespace
{
	HLVM_STATIC_VAR bool bUseValidationLayers = VK_ENABLE_VALIDATION_LAYERS;

	HLVM_STATIC_VAR const TVector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation" // 开启可用的校验层, extend if will
	};

	HLVM_STATIC_VAR const TVector<const char*> deviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME // 交换链扩展集合, extend if will
	};

	HLVM_STATIC_VAR TVector<std::string> requiredExtensions = {
		/* requested during runtime */
	};

	HLVM_STATIC_FUNC std::vector<VkExtensionProperties> EnumerateInstanceExtensionProperties(const char* pLayerName)
	{
		uint32_t extCount = 0;
		VkResult result = vkEnumerateInstanceExtensionProperties(pLayerName, &extCount, nullptr);
		if (result != VK_SUCCESS)
		{
			HLVM_LOG(LogVulkanRHI, err, TXT("vkEnumerateInstanceExtensionProperties failed to get extension count. VkResult = {}"), VK_RESULT_TO_TCHAR(result));
			return {};
		}

		std::vector<VkExtensionProperties> extensionProperties(extCount);
		result = vkEnumerateInstanceExtensionProperties(pLayerName, &extCount, extensionProperties.data());
		if (result != VK_SUCCESS)
		{
			HLVM_LOG(LogVulkanRHI, err, TXT("vkEnumerateInstanceExtensionProperties failed to get extension properties. VkResult = {}"), VK_RESULT_TO_TCHAR(result));
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

	HLVM_STATIC_FUNC std::vector<VkLayerProperties> EnumerateInstanceLayerProperties()
	{
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
		return availableLayers;
	}

	HLVM_STATIC_FUNC std::vector<std::string> ValidateInstanceLayerNames(const std::vector<std::string>& names)
	{
		if (names.empty())
		{
			return names;
		}

		auto availableLayers = EnumerateInstanceLayerProperties();

		std::set<std::string> layerNames;
		for (const auto& layer : availableLayers)
		{
			HLVM_LOG(LogVulkanRHI, debug, TXT("Available layer: {}"), TO_TCHAR_CSTR(layer.layerName));
			if (layer.layerName[0] != 0)
				layerNames.insert(layer.layerName);
		}

		std::vector<std::string> validatedNames;
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
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		TVector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
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
		else if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
		{
			HLVM_LOG(LogVulkanRHI, info, TXT("Info : {}"), TO_TCHAR_CSTR(pCallbackData->pMessage));
		} // ignore verbose logs
		return VK_FALSE;
	}

	HLVM_STATIC_FUNC void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
	{
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = DebugCallback;
	}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-function-type-strict"
	// 使用vkGetInstanceProcAddr获取某个api的函数指针
	HLVM_STATIC_FUNC VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
	{
		auto func = R_C(PFN_vkCreateDebugUtilsMessengerEXT, vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
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
		auto func = R_C(PFN_vkDestroyDebugUtilsMessengerEXT, vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
		if (func != nullptr)
		{
			func(instance, debugMessenger, pAllocator);
		}
	}
#pragma clang diagnostic pop
} // namespace

FVulkanRHI::FVulkanRHI(const FVulkanRHI::FInitializer& Params)
	: InitializerParam(Params)
{
	for (const auto& extensions : Params.RequiredExtensions)
	{
		for (const auto& extension : extensions)
		{
			requiredExtensions.push_back(extension.ToCharCStr());
		}
	}
}

// Initialization and Shutdown
void FVulkanRHI::Init()
{
	CreateVulkanInstance();
	/**
	 * Create Debug Messenger right after creating the instance
	 */
	if (bUseValidationLayers)
	{
		CreateDebugLayer();
	}
	CreateSurface();
	CreateVulkanDevice();
	CreateVulkanQueues();
	// TODO : Create Vulkan SwapChain and so on
	CreateVulkanMemoryAllocator();
}

void FVulkanRHI::Shutdown()
{
	vkDeviceWaitIdle(VulkanDevice);

	// Cleanup Vulkan resources
	if (VulkanDevice)
	{
		vkDestroyDevice(VulkanDevice, nullptr);
		VulkanDevice = VK_NULL_HANDLE;
	}

	if (VulkanInstance)
	{
		vkDestroyInstance(VulkanInstance, nullptr);
		VulkanInstance = VK_NULL_HANDLE;
	}
}

// Resource Creation
FTextureRHIRef FVulkanRHI::CreateTexture(const FRHITextureCreateDesc& CreateDesc)
{
	VkImage Image = CreateVulkanImage(CreateDesc);
	return new FVulkanTexture(Image, CreateDesc);
}

FBufferRHIRef FVulkanRHI::CreateBuffer(const FRHIBufferCreateDesc& CreateDesc)
{
	VkBuffer	   Buffer = CreateVulkanBuffer(CreateDesc);
	VkDeviceMemory Memory = AllocateVulkanMemory(Buffer, CreateDesc.UsageFlags);
	return new FVulkanBuffer(Buffer, Memory, CreateDesc);
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
FShaderRHIRef FVulkanRHI::CreateShader(const FShaderCreateInfo& CreateInfo)
{
	return nullptr;
	//	VkShaderModule ShaderModule = CreateVulkanShaderModule(CreateInfo);
	//	return new FVulkanShader(ShaderModule, CreateInfo.Stage);
}

void FVulkanRHI::ReleaseShader(FShaderRHIRef& Shader)
{
	//	FVulkanShader* VulkanShader = static_cast<FVulkanShader*>(Shader.GetReference());
	//	vkDestroyShaderModule(VulkanDevice, VulkanShader->GetShaderModule(), nullptr);
	//	Shader.SafeRelease();
}

// Pipeline State Management
FRHIGraphicsPipelineState* FVulkanRHI::CreateGraphicsPipelineState(const FGraphicsPipelineStateInitializer& Initializer)
{
	return nullptr;
	//	VkPipeline		 Pipeline = CreateVulkanGraphicsPipeline(Initializer);
	//	VkPipelineLayout PipelineLayout = CreateVulkanPipelineLayout(Initializer);
	//	return new FVulkanGraphicsPipelineState(Pipeline, PipelineLayout);
}

FRHIComputePipelineState* FVulkanRHI::CreateComputePipelineState(const FComputePipelineStateInitializer& Initializer)
{
	return nullptr;
	//	VkPipeline		 Pipeline = CreateVulkanComputePipeline(Initializer);
	//	VkPipelineLayout PipelineLayout = CreateVulkanPipelineLayout(Initializer);
	//	return new FVulkanComputePipelineState(Pipeline, PipelineLayout);
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
void FVulkanRHI::RHICreateViewport(void* WindowHandle, TUINT32 Width, TUINT32 Height, bool bIsFullscreen, EPixelFormat PreferredPixelFormat, FViewportRHIRef& OutViewport)
{
	CreateVulkanSwapChain(WindowHandle, Width, Height, bIsFullscreen, PreferredPixelFormat, OutViewport);
}

void FVulkanRHI::RHIResizeViewport(FViewportRHIRef& Viewport, TUINT32 Width, TUINT32 Height, bool bIsFullscreen, EPixelFormat PreferredPixelFormat)
{
	ResizeVulkanSwapChain(Viewport, Width, Height, bIsFullscreen, PreferredPixelFormat);
}

void FVulkanRHI::RHISwapBuffers(FViewportRHIRef& Viewport)
{
	PresentVulkanSwapChain(Viewport);
}

// Render Pass and Draw Commands
void FVulkanRHI::RHIBeginRenderPass(const FRHIRenderPassInfo& RenderPassInfo, const TCHAR* Name)
{
	BeginVulkanRenderPass(RenderPassInfo, Name);
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
FRHIQueryRHIRef FVulkanRHI::CreateQuery(ERHIQueryType QueryType)
{
	return FRHIQueryRHIRef{};
	//	VkQueryPool QueryPool = CreateVulkanQueryPool(QueryType);
	//	return new FVulkanQuery(QueryPool, 0, QueryType);
}

void FVulkanRHI::RHIBeginQuery(FRHIQueryRHIRef& Query)
{
	//	FVulkanQuery* VulkanQuery = static_cast<FVulkanQuery*>(Query.GetReference());
	//	BeginVulkanQuery(VulkanQuery->GetQueryPool(), VulkanQuery->GetQueryIndex());
}

void FVulkanRHI::RHIEndQuery(FRHIQueryRHIRef& Query)
{
	//	FVulkanQuery* VulkanQuery = static_cast<FVulkanQuery*>(Query.GetReference());
	//	EndVulkanQuery(VulkanQuery->GetQueryPool(), VulkanQuery->GetQueryIndex());
}

void FVulkanRHI::RHIGetQueryResults(FRHIQueryRHIRef& Query, TUINT64& OutResult, bool bWait)
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
void FVulkanRHI::RHISetGraphicsPipelineState(FRHIGraphicsPipelineState* PipelineState)
{
	//	FVulkanGraphicsPipelineState* VulkanPipelineState = static_cast<FVulkanGraphicsPipelineState*>(PipelineState);
	//	vkCmdBindPipeline(GetCurrentCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, VulkanPipelineState->GetPipeline());
}

void FVulkanRHI::RHISetComputePipelineState(FRHIComputePipelineState* PipelineState)
{
	//	FVulkanComputePipelineState* VulkanPipelineState = static_cast<FVulkanComputePipelineState*>(PipelineState);
	//	vkCmdBindPipeline(GetCurrentCommandBuffer(), VK_PIPELINE_BIND_POINT_COMPUTE, VulkanPipelineState->GetPipeline());
}

void FVulkanRHI::RHISetViewport(TUINT32 MinX, TUINT32 MinY, float MinZ, TUINT32 MaxX, TUINT32 MaxY, float MaxZ)
{
	//	VkViewport Viewport = { static_cast<float>(MinX), static_cast<float>(MinY), static_cast<float>(MaxX - MinX), static_cast<float>(MaxY - MinY), MinZ, MaxZ };
	//	vkCmdSetViewport(GetCurrentCommandBuffer(), 0, 1, &Viewport);
}

void FVulkanRHI::RHISetScissorRect(bool bEnable, TUINT32 MinX, TUINT32 MinY, TUINT32 MaxX, TUINT32 MaxY)
{
	//	VkRect2D Scissor = { { static_cast<int32_t>(MinX), static_cast<int32_t>(MinY) }, { MaxX - MinX, MaxY - MinY } };
	//	vkCmdSetScissor(GetCurrentCommandBuffer(), 0, 1, &Scissor);
}

// Vulkan-specific initialization
void FVulkanRHI::CreateVulkanInstance()
{
	VkApplicationInfo AppInfo = {};
	AppInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	AppInfo.pApplicationName = "HLVM";
	AppInfo.applicationVersion = VK_MAKE_VERSION(0, 2, 1);
	AppInfo.pEngineName = "HLVM";
	AppInfo.engineVersion = VK_MAKE_VERSION(0, 2, 1);
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

	VkResult Result = vkCreateInstance(&CreateInfo, nullptr, &VulkanInstance);
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

void FVulkanRHI::CreateVulkanDevice()
{
	uint32_t DeviceCount = 0;
	vkEnumeratePhysicalDevices(VulkanInstance, &DeviceCount, nullptr);
	HLVM_ENSURE(DeviceCount > 0);

	std::vector<VkPhysicalDevice> PhysicalDevices(DeviceCount);
	vkEnumeratePhysicalDevices(VulkanInstance, &DeviceCount, PhysicalDevices.data());

	VulkanPhysicalDevice = PhysicalDevices[0]; // Select the first device for simplicity

	float					QueuePriority = 1.0f;
	VkDeviceQueueCreateInfo QueueCreateInfo = {};
	QueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	QueueCreateInfo.queueFamilyIndex = 0; // Assume graphics and compute are in the same queue family
	QueueCreateInfo.queueCount = 1;
	QueueCreateInfo.pQueuePriorities = &QueuePriority;

	VkDeviceCreateInfo DeviceCreateInfo = {};
	DeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	DeviceCreateInfo.queueCreateInfoCount = 1;
	DeviceCreateInfo.pQueueCreateInfos = &QueueCreateInfo;

	VkResult Result = vkCreateDevice(VulkanPhysicalDevice, &DeviceCreateInfo, nullptr, &VulkanDevice);
	HLVM_ENSURE(Result == VK_SUCCESS);

	vkGetDeviceQueue(VulkanDevice, 0, 0, &GraphicsQueue);
	vkGetDeviceQueue(VulkanDevice, 0, 0, &ComputeQueue);
}

void FVulkanRHI::CreateVulkanQueues()
{
	// Queues are created during device creation
}

void FVulkanRHI::CreateVulkanMemoryAllocator()
{
	VmaAllocatorCreateInfo allocatorCreateInfo = {};
	allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
	allocatorCreateInfo.vulkanApiVersion = VULKAN_API_VERSION;
	allocatorCreateInfo.physicalDevice = VulkanPhysicalDevice;
	allocatorCreateInfo.device = VulkanDevice;
	allocatorCreateInfo.instance = VulkanInstance;
	allocatorCreateInfo.pVulkanFunctions = &VMAVulkanFunctions;
	vmaCreateAllocator(&allocatorCreateInfo, &VMAAllocator);
}

// Vulkan-specific resource creation
VkImage FVulkanRHI::CreateVulkanImage(const FRHITextureCreateDesc& CreateDesc)
{
	VkImageCreateInfo ImageInfo = {};
	ImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ImageInfo.imageType = VK_IMAGE_TYPE_2D;
	ImageInfo.extent.width = CreateDesc.Dimensions.x;
	ImageInfo.extent.height = CreateDesc.Dimensions.y;
	ImageInfo.extent.depth = CreateDesc.Dimensions.z;
	ImageInfo.mipLevels = 1;
	ImageInfo.arrayLayers = 1;
	ImageInfo.format = VulkanFormatFromRHIFormat(CreateDesc.Format);
	ImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	ImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	ImageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	ImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

	VkImage	 Image;
	VkResult Result = vkCreateImage(VulkanDevice, &ImageInfo, nullptr, &Image);
	HLVM_ENSURE(Result == VK_SUCCESS);

	return Image;
}

VkBuffer FVulkanRHI::CreateVulkanBuffer(const FRHIBufferCreateDesc& CreateDesc)
{
	VkBufferCreateInfo BufferInfo = {};
	BufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	BufferInfo.size = CreateDesc.SizeInBytes;
	BufferInfo.usage = VulkanBufferUsageFlagsFromRHIUsageFlags(CreateDesc.UsageFlags);
	BufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkBuffer Buffer;
	VkResult Result = vkCreateBuffer(VulkanDevice, &BufferInfo, nullptr, &Buffer);
	HLVM_ENSURE(Result == VK_SUCCESS);

	return Buffer;
}

VkImageView FVulkanRHI::CreateVulkanImageView(VkImage Image, const FRHIShaderResourceViewCreateInfo& CreateInfo)
{
	VkImageViewCreateInfo ViewInfo = {};
	ViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	ViewInfo.image = Image;
	ViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	ViewInfo.format = VulkanFormatFromRHIFormat(CreateInfo.Format);
	ViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	ViewInfo.subresourceRange.baseMipLevel = 0;
	ViewInfo.subresourceRange.levelCount = 1;
	ViewInfo.subresourceRange.baseArrayLayer = 0;
	ViewInfo.subresourceRange.layerCount = 1;

	VkImageView ImageView;
	VkResult	Result = vkCreateImageView(VulkanDevice, &ViewInfo, nullptr, &ImageView);
	HLVM_ENSURE(Result == VK_SUCCESS);

	return ImageView;
}

VkBufferView FVulkanRHI::CreateVulkanBufferView(VkBuffer Buffer, const FRHIUnorderedAccessViewCreateInfo& CreateInfo)
{
	VkBufferViewCreateInfo ViewInfo = {};
	ViewInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
	ViewInfo.buffer = Buffer;
	ViewInfo.format = VulkanFormatFromRHIFormat(CreateInfo.Format);
	ViewInfo.offset = CreateInfo.Offset;
	ViewInfo.range = CreateInfo.Size;

	VkBufferView BufferView;
	VkResult	 Result = vkCreateBufferView(VulkanDevice, &ViewInfo, nullptr, &BufferView);
	HLVM_ENSURE(Result == VK_SUCCESS);

	return BufferView;
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
	VkResult		Result = vkAllocateCommandBuffers(VulkanDevice, &AllocInfo, &CommandBuffer);
	HLVM_ENSURE(Result == VK_SUCCESS);

	VkCommandBufferBeginInfo BeginInfo = {};
	BeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	BeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	Result = vkBeginCommandBuffer(CommandBuffer, &BeginInfo);
	HLVM_ENSURE(Result == VK_SUCCESS);

	return CommandBuffer;
}

void FVulkanRHI::EndVulkanCommandBuffer(VkCommandBuffer CommandBuffer)
{
	VkResult Result = vkEndCommandBuffer(CommandBuffer);
	HLVM_ENSURE(Result == VK_SUCCESS);
}

// Vulkan-specific synchronization
void FVulkanRHI::SubmitVulkanCommandsAndFlushGPU()
{
	VkSubmitInfo SubmitInfo = {};
	SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	SubmitInfo.commandBufferCount = 1;
	// SubmitInfo.pCommandBuffers = &GetCurrentCommandBuffer();

	VkResult Result = vkQueueSubmit(GraphicsQueue, 1, &SubmitInfo, VK_NULL_HANDLE);
	HLVM_ENSURE(Result == VK_SUCCESS);

	vkQueueWaitIdle(GraphicsQueue);
}

void FVulkanRHI::FlushVulkanResources()
{
	vkDeviceWaitIdle(VulkanDevice);
}

// Vulkan-specific viewport and swap chain management
void FVulkanRHI::CreateVulkanSwapChain(void* WindowHandle, TUINT32 Width, TUINT32 Height, bool bIsFullscreen, EPixelFormat PreferredPixelFormat, FViewportRHIRef& OutViewport)
{
	// Implement swap chain creation
}

void FVulkanRHI::ResizeVulkanSwapChain(FViewportRHIRef& Viewport, TUINT32 Width, TUINT32 Height, bool bIsFullscreen, EPixelFormat PreferredPixelFormat)
{
	// Implement swap chain resizing
}

void FVulkanRHI::PresentVulkanSwapChain(FViewportRHIRef& Viewport)
{
	// Implement swap chain presentation
}

// Vulkan-specific render pass management
void FVulkanRHI::BeginVulkanRenderPass(const FRHIRenderPassInfo& RenderPassInfo, const TCHAR* Name)
{
	// Implement render pass begin
}

void FVulkanRHI::EndVulkanRenderPass()
{
	// Implement render pass end
}

// Vulkan-specific query and timestamp management
VkQueryPool FVulkanRHI::CreateVulkanQueryPool(ERHIQueryType QueryType)
{
	VkQueryPoolCreateInfo PoolInfo = {};
	PoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	// PoolInfo.queryType = ConvertQueryTypeToVulkan(QueryType);
	PoolInfo.queryCount = 1;

	VkQueryPool QueryPool;
	VkResult	Result = vkCreateQueryPool(VulkanDevice, &PoolInfo, nullptr, &QueryPool);
	HLVM_ENSURE(Result == VK_SUCCESS);

	return QueryPool;
}

void FVulkanRHI::BeginVulkanQuery(VkQueryPool QueryPool, TUINT32 QueryIndex)
{
	// vkCmdBeginQuery(GetCurrentCommandBuffer(), QueryPool, QueryIndex, 0);
}

void FVulkanRHI::EndVulkanQuery(VkQueryPool QueryPool, TUINT32 QueryIndex)
{
	// vkCmdEndQuery(GetCurrentCommandBuffer(), QueryPool, QueryIndex);
}

void FVulkanRHI::GetVulkanQueryResults(VkQueryPool QueryPool, TUINT32 QueryIndex, TUINT64& OutResult, bool bWait)
{
	VkResult Result = vkGetQueryPoolResults(VulkanDevice, QueryPool, QueryIndex, 1, sizeof(OutResult), &OutResult, sizeof(OutResult), bWait ? VK_QUERY_RESULT_WAIT_BIT : 0);
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

// Generate VkImageCreateInfo from FRHITextureCreateDesc
VkImageCreateInfo FVulkanRHI::GenerateVkImageCreateInfo(const FRHITextureCreateDesc& CreateDesc)
{
	VkImageCreateInfo ImageCreateInfo = {};
	ImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ImageCreateInfo.imageType = VK_IMAGE_TYPE_2D; // Assuming 2D texture for simplicity
	ImageCreateInfo.extent.width = CreateDesc.Dimensions.x;
	ImageCreateInfo.extent.height = CreateDesc.Dimensions.y;
	ImageCreateInfo.extent.depth = CreateDesc.Dimensions.z;
	ImageCreateInfo.mipLevels = CreateDesc.NumMips;
	ImageCreateInfo.arrayLayers = CreateDesc.NumSamples;
	ImageCreateInfo.format = VulkanFormatFromRHIFormat(CreateDesc.Format); // Helper function to convert RHI format to Vulkan format
	ImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	ImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	ImageCreateInfo.usage = VulkanTextureUsageFlagsFromRHIUsageFlags(CreateDesc.Flags); // Helper function to convert RHI usage flags to Vulkan usage flags
	ImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;									// Assuming single sample for simplicity
	ImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	return ImageCreateInfo;
}

// Generate VkBufferCreateInfo from FRHIBufferCreateDesc
VkBufferCreateInfo FVulkanRHI::GenerateVkBufferCreateInfo(const FRHIBufferCreateDesc& CreateDesc)
{
	VkBufferCreateInfo BufferCreateInfo = {};
	BufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	BufferCreateInfo.size = CreateDesc.SizeInBytes;
	BufferCreateInfo.usage = VulkanBufferUsageFlagsFromRHIUsageFlags(CreateDesc.UsageFlags); // Helper function to convert RHI usage flags to Vulkan usage flags
	BufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	return BufferCreateInfo;
}

// Generate VkShaderModuleCreateInfo from FShaderCreateInfo
VkShaderModuleCreateInfo FVulkanRHI::GenerateVkShaderModuleCreateInfo(const FShaderCreateInfo& CreateDesc)
{
	VkShaderModuleCreateInfo ShaderModuleCreateInfo = {};
	ShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ShaderModuleCreateInfo.codeSize = CreateDesc.Code.size();
	ShaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(CreateDesc.Code.data());

	return ShaderModuleCreateInfo;
}

// Generate VkImageViewCreateInfo from FRHIShaderResourceViewCreateInfo
VkImageViewCreateInfo FVulkanRHI::GenerateVkImageViewCreateInfo(const FRHIShaderResourceViewCreateInfo& CreateDesc)
{
	VkImageViewCreateInfo ImageViewCreateInfo = {};
	ImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	ImageViewCreateInfo.image = VK_NULL_HANDLE;								   // To be set later
	ImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;					   // Assuming 2D image view for simplicity
	ImageViewCreateInfo.format = VulkanFormatFromRHIFormat(CreateDesc.Format); // Helper function to convert RHI format to Vulkan format
	ImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	ImageViewCreateInfo.subresourceRange.baseMipLevel = CreateDesc.MipLevel;
	ImageViewCreateInfo.subresourceRange.levelCount = CreateDesc.NumMipLevels;
	ImageViewCreateInfo.subresourceRange.baseArrayLayer = CreateDesc.FirstArraySlice;
	ImageViewCreateInfo.subresourceRange.layerCount = CreateDesc.NumArraySlices;

	return ImageViewCreateInfo;
}

// Generate VkBufferViewCreateInfo from FRHIUnorderedAccessViewCreateInfo
VkBufferViewCreateInfo FVulkanRHI::GenerateVkBufferViewCreateInfo(const FRHIUnorderedAccessViewCreateInfo& CreateDesc)
{
	VkBufferViewCreateInfo BufferViewCreateInfo = {};
	BufferViewCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
	BufferViewCreateInfo.buffer = VK_NULL_HANDLE;								// To be set later
	BufferViewCreateInfo.format = VulkanFormatFromRHIFormat(CreateDesc.Format); // Helper function to convert RHI format to Vulkan format
	BufferViewCreateInfo.offset = 0;
	BufferViewCreateInfo.range = VK_WHOLE_SIZE;

	return BufferViewCreateInfo;
}

// Generate VkSamplerCreateInfo from FRHISamplerStateCreateInfo (assuming this struct exists)
VkSamplerCreateInfo FVulkanRHI::GenerateVkSamplerCreateInfo(const FRHISamplerStateCreateInfo& CreateDesc)
{
	VkSamplerCreateInfo SamplerCreateInfo = {};
	SamplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

	// Set filter modes
	SamplerCreateInfo.magFilter = VulkanFilterFromRHIFilter(CreateDesc.Filter);
	SamplerCreateInfo.minFilter = VulkanFilterFromRHIFilter(CreateDesc.Filter);

	// Set address modes
	SamplerCreateInfo.addressModeU = VulkanAddressModeFromRHIAddressMode(CreateDesc.AddressModeU);
	SamplerCreateInfo.addressModeV = VulkanAddressModeFromRHIAddressMode(CreateDesc.AddressModeV);
	SamplerCreateInfo.addressModeW = VulkanAddressModeFromRHIAddressMode(CreateDesc.AddressModeW);

	// Set mip map level of detail bias
	SamplerCreateInfo.mipLodBias = static_cast<float>(CreateDesc.MipMapLevelOfDetailBias);

	// Set maximum anisotropy
	SamplerCreateInfo.anisotropyEnable = VK_TRUE;
	SamplerCreateInfo.maxAnisotropy = static_cast<float>(CreateDesc.MaxAnisotropy);

	// Set comparison function
	SamplerCreateInfo.compareEnable = VK_TRUE;
	SamplerCreateInfo.compareOp = VulkanCompareOpFromRHICompareFunction(CreateDesc.ComparisonFunction);

	// Set border color
	SamplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	SamplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

	return SamplerCreateInfo;
}

// Generate VkPipelineShaderStageCreateInfo from FShaderCreateInfo
VkPipelineShaderStageCreateInfo FVulkanRHI::GenerateVkPipelineShaderStageCreateInfo(const FShaderCreateInfo& CreateDesc)
{
	VkPipelineShaderStageCreateInfo ShaderStageCreateInfo = {};
	ShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	ShaderStageCreateInfo.stage = VulkanShaderStageFromRHIStage(CreateDesc.Stage); // Convert RHI shader stage to Vulkan shader stage
	ShaderStageCreateInfo.module = VK_NULL_HANDLE;								   // To be set later
	ShaderStageCreateInfo.pName = TO_CHAR_CSTR(CreateDesc.EntryPoints[0].c_str());

	return ShaderStageCreateInfo;
}

// Generate VkPipelineLayoutCreateInfo from FRHIGraphicsPipelineStateCreateInfo
VkPipelineLayoutCreateInfo FVulkanRHI::GenerateVkPipelineLayoutCreateInfo(const FRHIGraphicsPipelineLayoutCreateInfo& CreateDesc)
{
	VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo = {};
	PipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	//	// Add descriptor set layouts
	//	std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
	//	for (const auto& layout : CreateDesc.DescriptorSetLayouts)
	//	{
	//		descriptorSetLayouts.push_back(layout);
	//	}
	//	PipelineLayoutCreateInfo.descriptorSetLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
	//	PipelineLayoutCreateInfo.pDescriptorSetLayouts = descriptorSetLayouts.data();
	//
	//	// Add push constant ranges
	//	std::vector<VkPushConstantRange> pushConstantRanges;
	//	for (const auto& range : CreateDesc.PushConstantRanges)
	//	{
	//		pushConstantRanges.push_back(range);
	//	}
	//	PipelineLayoutCreateInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
	//	PipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();

	return PipelineLayoutCreateInfo;
}

// Generate VkGraphicsPipelineCreateInfo from FRHIGraphicsPipelineStateCreateInfo
VkGraphicsPipelineCreateInfo FVulkanRHI::GenerateVkGraphicsPipelineCreateInfo(const FRHIGraphicsPipelineStateCreateInfo& CreateDesc)
{
	VkGraphicsPipelineCreateInfo PipelineCreateInfo = {};
	PipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

	// Add shader stages
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
	for (const auto& shader : CreateDesc.Shaders)
	{
		VkPipelineShaderStageCreateInfo stageInfo = {};
		stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stageInfo.stage = VulkanShaderStageFromRHIStage(shader.Stage);
		stageInfo.module = VK_NULL_HANDLE; // To be set later
		stageInfo.pName = TO_CHAR_CSTR(shader.EntryPoints[0].c_str());
		shaderStages.push_back(stageInfo);
	}
	PipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	PipelineCreateInfo.pStages = shaderStages.data();

	//	// Add vertex input state
	//	PipelineCreateInfo.pVertexInputState = &CreateDesc.VertexInputState;
	//
	//	// Add input assembly state
	//	PipelineCreateInfo.pInputAssemblyState = &CreateDesc.InputAssemblyState;
	//
	//	// Add viewport state
	//	PipelineCreateInfo.pViewportState = &CreateDesc.ViewportState;
	//
	//	// Add rasterization state
	//	PipelineCreateInfo.pRasterizationState = &CreateDesc.RasterizationState;
	//
	//	// Add multisample state
	//	PipelineCreateInfo.pMultisampleState = &CreateDesc.MultisampleState;
	//
	//	// Add depth stencil state
	//	PipelineCreateInfo.pDepthStencilState = &CreateDesc.DepthStencilState;
	//
	//	// Add color blend state
	//	PipelineCreateInfo.pColorBlendState = &CreateDesc.ColorBlendState;
	//
	//	// Add dynamic state
	//	PipelineCreateInfo.pDynamicState = &CreateDesc.DynamicState;

	// Add pipeline layout
	PipelineCreateInfo.layout = VK_NULL_HANDLE; // To be set later

	// Add render pass and subpass
	PipelineCreateInfo.renderPass = VK_NULL_HANDLE; // To be set later
	PipelineCreateInfo.subpass = 0;

	return PipelineCreateInfo;
}

// Generate VkComputePipelineCreateInfo from FRHIComputePipelineStateCreateInfo (assuming this struct exists)
VkComputePipelineCreateInfo FVulkanRHI::GenerateVkComputePipelineCreateInfo(const FRHIComputePipelineStateCreateInfo& CreateDesc)
{
	VkComputePipelineCreateInfo PipelineCreateInfo = {};
	PipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;

	return PipelineCreateInfo;
}

// Generate VkQueryPoolCreateInfo from FRHIQueryCreateInfo (assuming this struct exists)
VkQueryPoolCreateInfo FVulkanRHI::GenerateVkQueryPoolCreateInfo(const FRHIQueryCreateInfo& CreateDesc)
{
	VkQueryPoolCreateInfo QueryPoolCreateInfo = {};
	QueryPoolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	// QueryPoolCreateInfo.queryType = VulkanQueryTypeFromRHIQueryType(CreateDesc.QueryType); // Helper function to convert RHI query type to Vulkan query type
	QueryPoolCreateInfo.queryCount = CreateDesc.NumQueries;

	return QueryPoolCreateInfo;
}

VkDeviceMemory FVulkanRHI::AllocateVulkanMemory(VkBuffer Buffer, EBufferUsageFlags UsageFlags)
{
	VkBufferCreateInfo bufferInfo;
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = 65536;
	// bufferInfo.usage = UsageFlags;

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

	VkBuffer	  buffer;
	VmaAllocation allocation;
	// vmaAllocateMemoryForBuffer(VMAAllocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);

	VkDeviceMemory dm = nullptr;
	return dm;
}
#pragma clang diagnostic pop
