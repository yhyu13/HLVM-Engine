/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#include "Test.h"

#include <dylib.hpp>
#ifndef VK_NO_PROTOTYPES
	#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan_core.h>

DECLARE_LOG_CATEGORY(LogTest)

#if defined(WIN32) || defined(_WIN32) || defined(_WIN32_) || defined(WIN64) || defined(_WIN64) || defined(_WIN64_)
	#define VULKAN_LIB "vulkan-1.dll"
#elif defined(ANDROID) || defined(_ANDROID_)
	#define VULKAN_LIB "libvulkan.so"
#else
	#define VULKAN_LIB "libvulkan.so.1"
#endif


#define APPLY_PFN_DEF_VK_FUNCTIONS(PFN_DEF)                 \
	PFN_DEF(vkGetInstanceProcAddr)                          \
	PFN_DEF(vkCreateInstance)                               \
	PFN_DEF(vkEnumerateInstanceExtensionProperties)         \
	PFN_DEF(vkEnumerateInstanceLayerProperties)             \
	PFN_DEF(vkDestroyInstance)                              \
	PFN_DEF(vkEnumeratePhysicalDevices)                     \
	PFN_DEF(vkGetPhysicalDeviceFeatures)                    \
	PFN_DEF(vkGetPhysicalDeviceFormatProperties)            \
	PFN_DEF(vkGetPhysicalDeviceImageFormatProperties)       \
	PFN_DEF(vkGetPhysicalDeviceProperties)                  \
	PFN_DEF(vkGetPhysicalDeviceQueueFamilyProperties)       \
	PFN_DEF(vkGetPhysicalDeviceMemoryProperties)            \
	PFN_DEF(vkGetDeviceProcAddr)                            \
	PFN_DEF(vkCreateDevice)                                 \
	PFN_DEF(vkDestroySurfaceKHR)                            \
	PFN_DEF(vkGetPhysicalDeviceSurfaceSupportKHR)           \
	PFN_DEF(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)      \
	PFN_DEF(vkGetPhysicalDeviceSurfaceFormatsKHR)           \
	PFN_DEF(vkGetPhysicalDeviceSurfacePresentModesKHR)      \
	PFN_DEF(vkCreateSwapchainKHR)                           \
	PFN_DEF(vkDestroySwapchainKHR)                          \
	PFN_DEF(vkGetSwapchainImagesKHR)                        \
	PFN_DEF(vkAcquireNextImageKHR)                          \
	PFN_DEF(vkQueuePresentKHR)                              \
	PFN_DEF(vkDestroyDevice)                                \
	PFN_DEF(vkEnumerateDeviceExtensionProperties)           \
	PFN_DEF(vkEnumerateDeviceLayerProperties)               \
	PFN_DEF(vkGetDeviceQueue)                               \
	PFN_DEF(vkQueueSubmit)                                  \
	PFN_DEF(vkQueueWaitIdle)                                \
	PFN_DEF(vkDeviceWaitIdle)                               \
	PFN_DEF(vkAllocateMemory)                               \
	PFN_DEF(vkFreeMemory)                                   \
	PFN_DEF(vkMapMemory)                                    \
	PFN_DEF(vkUnmapMemory)                                  \
	PFN_DEF(vkFlushMappedMemoryRanges)                      \
	PFN_DEF(vkInvalidateMappedMemoryRanges)                 \
	PFN_DEF(vkGetDeviceMemoryCommitment)                    \
	PFN_DEF(vkBindBufferMemory)                             \
	PFN_DEF(vkBindImageMemory)                              \
	PFN_DEF(vkGetBufferMemoryRequirements)                  \
	PFN_DEF(vkGetImageMemoryRequirements)                   \
	PFN_DEF(vkGetImageSparseMemoryRequirements)             \
	PFN_DEF(vkGetPhysicalDeviceSparseImageFormatProperties) \
	PFN_DEF(vkQueueBindSparse)                              \
	PFN_DEF(vkCreateFence)                                  \
	PFN_DEF(vkDestroyFence)                                 \
	PFN_DEF(vkResetFences)                                  \
	PFN_DEF(vkGetFenceStatus)                               \
	PFN_DEF(vkWaitForFences)                                \
	PFN_DEF(vkCreateSemaphore)                              \
	PFN_DEF(vkDestroySemaphore)                             \
	PFN_DEF(vkCreateEvent)                                  \
	PFN_DEF(vkDestroyEvent)                                 \
	PFN_DEF(vkGetEventStatus)                               \
	PFN_DEF(vkSetEvent)                                     \
	PFN_DEF(vkResetEvent)                                   \
	PFN_DEF(vkCreateQueryPool)                              \
	PFN_DEF(vkDestroyQueryPool)                             \
	PFN_DEF(vkGetQueryPoolResults)                          \
	PFN_DEF(vkCreateBuffer)                                 \
	PFN_DEF(vkDestroyBuffer)                                \
	PFN_DEF(vkCreateBufferView)                             \
	PFN_DEF(vkDestroyBufferView)                            \
	PFN_DEF(vkCreateImage)                                  \
	PFN_DEF(vkDestroyImage)                                 \
	PFN_DEF(vkGetImageSubresourceLayout)                    \
	PFN_DEF(vkCreateImageView)                              \
	PFN_DEF(vkDestroyImageView)                             \
	PFN_DEF(vkCreateShaderModule)                           \
	PFN_DEF(vkDestroyShaderModule)                          \
	PFN_DEF(vkCreatePipelineCache)                          \
	PFN_DEF(vkDestroyPipelineCache)                         \
	PFN_DEF(vkGetPipelineCacheData)                         \
	PFN_DEF(vkMergePipelineCaches)                          \
	PFN_DEF(vkCreateGraphicsPipelines)                      \
	PFN_DEF(vkCreateComputePipelines)                       \
	PFN_DEF(vkDestroyPipeline)                              \
	PFN_DEF(vkCreatePipelineLayout)                         \
	PFN_DEF(vkDestroyPipelineLayout)                        \
	PFN_DEF(vkCreateSampler)                                \
	PFN_DEF(vkDestroySampler)                               \
	PFN_DEF(vkCreateDescriptorSetLayout)                    \
	PFN_DEF(vkDestroyDescriptorSetLayout)                   \
	PFN_DEF(vkCreateDescriptorPool)                         \
	PFN_DEF(vkDestroyDescriptorPool)                        \
	PFN_DEF(vkResetDescriptorPool)                          \
	PFN_DEF(vkAllocateDescriptorSets)                       \
	PFN_DEF(vkFreeDescriptorSets)                           \
	PFN_DEF(vkUpdateDescriptorSets)                         \
	PFN_DEF(vkCreateFramebuffer)                            \
	PFN_DEF(vkDestroyFramebuffer)                           \
	PFN_DEF(vkCreateRenderPass)                             \
	PFN_DEF(vkDestroyRenderPass)                            \
	PFN_DEF(vkGetRenderAreaGranularity)                     \
	PFN_DEF(vkCreateCommandPool)                            \
	PFN_DEF(vkDestroyCommandPool)                           \
	PFN_DEF(vkResetCommandPool)                             \
	PFN_DEF(vkAllocateCommandBuffers)                       \
	PFN_DEF(vkFreeCommandBuffers)                           \
	PFN_DEF(vkBeginCommandBuffer)                           \
	PFN_DEF(vkEndCommandBuffer)                             \
	PFN_DEF(vkResetCommandBuffer)                           \
	PFN_DEF(vkCmdBindPipeline)                              \
	PFN_DEF(vkCmdSetViewport)                               \
	PFN_DEF(vkCmdSetScissor)                                \
	PFN_DEF(vkCmdSetLineWidth)                              \
	PFN_DEF(vkCmdSetDepthBias)                              \
	PFN_DEF(vkCmdSetBlendConstants)                         \
	PFN_DEF(vkCmdSetDepthBounds)                            \
	PFN_DEF(vkCmdSetStencilCompareMask)                     \
	PFN_DEF(vkCmdSetStencilWriteMask)                       \
	PFN_DEF(vkCmdSetStencilReference)                       \
	PFN_DEF(vkCmdBindDescriptorSets)                        \
	PFN_DEF(vkCmdBindIndexBuffer)                           \
	PFN_DEF(vkCmdBindVertexBuffers)                         \
	PFN_DEF(vkCmdDraw)                                      \
	PFN_DEF(vkCmdDrawIndexed)                               \
	PFN_DEF(vkCmdDrawIndirect)                              \
	PFN_DEF(vkCmdDrawIndexedIndirect)                       \
	PFN_DEF(vkCmdDispatch)                                  \
	PFN_DEF(vkCmdDispatchIndirect)                          \
	PFN_DEF(vkCmdCopyBuffer)                                \
	PFN_DEF(vkCmdCopyImage)                                 \
	PFN_DEF(vkCmdBlitImage)                                 \
	PFN_DEF(vkCmdCopyBufferToImage)                         \
	PFN_DEF(vkCmdCopyImageToBuffer)                         \
	PFN_DEF(vkCmdUpdateBuffer)                              \
	PFN_DEF(vkCmdFillBuffer)                                \
	PFN_DEF(vkCmdClearColorImage)                           \
	PFN_DEF(vkCmdClearDepthStencilImage)                    \
	PFN_DEF(vkCmdClearAttachments)                          \
	PFN_DEF(vkCmdResolveImage)                              \
	PFN_DEF(vkCmdSetEvent)                                  \
	PFN_DEF(vkCmdResetEvent)                                \
	PFN_DEF(vkCmdWaitEvents)                                \
	PFN_DEF(vkCmdPipelineBarrier)                           \
	PFN_DEF(vkCmdBeginQuery)                                \
	PFN_DEF(vkCmdEndQuery)                                  \
	PFN_DEF(vkCmdResetQueryPool)                            \
	PFN_DEF(vkCmdWriteTimestamp)                            \
	PFN_DEF(vkCmdCopyQueryPoolResults)                      \
	PFN_DEF(vkCmdPushConstants)                             \
	PFN_DEF(vkCmdBeginRenderPass)                           \
	PFN_DEF(vkCmdNextSubpass)                               \
	PFN_DEF(vkCmdEndRenderPass)                             \
	PFN_DEF(vkCmdExecuteCommands)

// Either use static definition for PFN functions or use extern & define
//#define DEFINE_VK_FUNCTION_MACRO(function) \
//	static PFN_##function function = nullptr;
//APPLY_PFN_DEF_VK_FUNCTIONS(DEFINE_VK_FUNCTION_MACRO)

#define DEFINE_VK_FUNCTION_MACRO(function) \
	extern PFN_##function function;
APPLY_PFN_DEF_VK_FUNCTIONS(DEFINE_VK_FUNCTION_MACRO)
#define DEFINE_VK_FUNCTION_MACRO2(function) \
	PFN_##function function = nullptr;
APPLY_PFN_DEF_VK_FUNCTIONS(DEFINE_VK_FUNCTION_MACRO2)

static void load_vulkan_functions()
{
	static dylib vulkanlib(VULKAN_LIB, false);
#define GET_VK_FUNCTION_PROCADDR(function) \
	function = reinterpret_cast<PFN_##function>(vulkanlib.get_function<PFN_##function>(#function));
	APPLY_PFN_DEF_VK_FUNCTIONS(GET_VK_FUNCTION_PROCADDR)

	HLVM_ENSURE_F(vulkanlib.native_handle() != nullptr, TXT("Failed to load vulkan library"));
}

const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation" // 开启可用的校验层
};

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME // 交换链扩展集合
};

#if !HLVM_BUILD_RELEASE
const bool enableValidationLayers = true;
#else
const bool enableValidationLayers = false;
#endif // NDEBUG

static std::vector<const char*> getRequiredExtensions() {
	std::vector<const char*> extentions;
		if (enableValidationLayers) {
		extentions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); // 添加调试扩展
	}
	return extentions;
}

static bool checkValidationLayerSupport() {
	uint32_t layerCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
	for (const char* layerName : validationLayers) {
		bool layerFound = false;
		for (const auto& layerProperties : availableLayers) {
			if (strcmp(layerName, layerProperties.layerName) == 0) {
				layerFound = true;
				break;
			}
		}
		if (!layerFound) {
			return false;
		}
	}
	return true;
}

// 接受调试信息的回调函数
// 参数1：诊断信息
// 参数2：资源创建之类的信息
// 参数3：警告信息
// 参数4：不合法和可能造成崩溃的操作信息
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT /* messageType */,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* /* pUserData */) {
	if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
		std::cerr << "Error Message: " << pCallbackData->pMessage << std::endl;
	}
	else if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		std::cerr << "Warning Message: " << pCallbackData->pMessage << std::endl;
	}
	else if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
		std::cout << "Info Message: " << pCallbackData->pMessage << std::endl;
	} // ignore verbose logs
	return VK_FALSE;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-function-type-strict"
#pragma clang diagnostic ignored "-Wold-style-cast"
// 使用vkGetInstanceProcAddr获取某个api的函数指针
static VkResult createDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
	auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
		vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	} else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

static void destroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
	auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
		vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}

#define CHECK_VK_RESULT(result, message)       \
	do                                         \
	{                                          \
		if (result != VK_SUCCESS)              \
		{                                      \
			throw std::runtime_error(message); \
		}                                      \
	}                                          \
	while (0)

RECORD_BOOL(vulkan_test1)
{
	load_vulkan_functions();

	VkInstance		 instance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice		 device = VK_NULL_HANDLE;
	VkQueue			 queue = VK_NULL_HANDLE;
	VkCommandPool	 commandPool = VK_NULL_HANDLE;
	VkCommandBuffer	 commandBuffer = VK_NULL_HANDLE;
	VkSemaphore		 semaphore = VK_NULL_HANDLE;
	VkFence			 fence = VK_NULL_HANDLE;
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory bufferMemory = VK_NULL_HANDLE;
	VkBufferView bufferView = VK_NULL_HANDLE;
	VkImage image = VK_NULL_HANDLE;
	VkDeviceMemory imageMemory = VK_NULL_HANDLE;
	VkImageView imageView = VK_NULL_HANDLE;
	VkFramebuffer framebuffer = VK_NULL_HANDLE;
	VkRenderPass renderPass = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	VkSampler sampler = VK_NULL_HANDLE;
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;


	try
	{
		// Initialize Vulkan instance
		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Vulkan Test";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		auto extensions = getRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
		if (enableValidationLayers) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();

			debugCreateInfo = {};
			debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			debugCreateInfo.pfnUserCallback = debugCallback;

			createInfo.pNext = reinterpret_cast<VkDebugUtilsMessengerCreateInfoEXT*>(
				&debugCreateInfo);
		} else {
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}

		VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
		CHECK_VK_RESULT(result, "Failed to create Vulkan instance!");

		if (enableValidationLayers) {
			if (createDebugUtilsMessengerEXT(instance, &debugCreateInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
				throw std::runtime_error("failed to set up debug messenger!");
			}
		}

		// Enumerate physical devices
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
		if (deviceCount == 0)
		{
			throw std::runtime_error("No Vulkan-supported GPUs found!");
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
		physicalDevice = devices[0]; // Use the first GPU

		// Create a logical device
		float					queuePriority = 1.0f;
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = 0; // Assume first queue family
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;

		VkDeviceCreateInfo deviceCreateInfo = {};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
		deviceCreateInfo.queueCreateInfoCount = 1;

		VkPhysicalDeviceFeatures deviceFeatures{};
		deviceFeatures.geometryShader = true;
		deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
		deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
		deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

		result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);
		CHECK_VK_RESULT(result, "Failed to create logical device!");

		// Get a queue
		vkGetDeviceQueue(device, 0, 0, &queue);

		// Create a command pool
		VkCommandPoolCreateInfo commandPoolCreateInfo = {};
		commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		commandPoolCreateInfo.queueFamilyIndex = 0; // Assume first queue family

		result = vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool);
		CHECK_VK_RESULT(result, "Failed to create command pool!");

		// Allocate a command buffer
		VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
		commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferAllocateInfo.commandPool = commandPool;
		commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		commandBufferAllocateInfo.commandBufferCount = 1;

		result = vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer);
		CHECK_VK_RESULT(result, "Failed to allocate command buffer!");

		// Create a semaphore
		VkSemaphoreCreateInfo semaphoreCreateInfo = {};
		semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		result = vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphore);
		CHECK_VK_RESULT(result, "Failed to create semaphore!");

		// Create a fence
		VkFenceCreateInfo fenceCreateInfo = {};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

		result = vkCreateFence(device, &fenceCreateInfo, nullptr, &fence);
		CHECK_VK_RESULT(result, "Failed to create fence!");

		// Create a buffer
		VkBufferCreateInfo bufferCreateInfo = {};
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCreateInfo.size = 1024; // 1 KB buffer
		bufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		result = vkCreateBuffer(device, &bufferCreateInfo, nullptr, &buffer);
		CHECK_VK_RESULT(result, "Failed to create buffer!");

		// Allocate memory for the buffer
		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = 0; // Assume first memory type is suitable

		result = vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory);
		CHECK_VK_RESULT(result, "Failed to allocate buffer memory!");

		vkBindBufferMemory(device, buffer, bufferMemory, 0);

		// Create a buffer view
		VkBufferViewCreateInfo bufferViewCreateInfo = {};
		bufferViewCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
		bufferViewCreateInfo.buffer = buffer;
		bufferViewCreateInfo.format = VK_FORMAT_R32_SFLOAT;
		bufferViewCreateInfo.offset = 0;
		bufferViewCreateInfo.range = VK_WHOLE_SIZE;

		result = vkCreateBufferView(device, &bufferViewCreateInfo, nullptr, &bufferView);
		CHECK_VK_RESULT(result, "Failed to create buffer view!");

		// Create an image
		VkImageCreateInfo imageCreateInfo = {};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		imageCreateInfo.extent = {512, 512, 1};
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;

		result = vkCreateImage(device, &imageCreateInfo, nullptr, &image);
		CHECK_VK_RESULT(result, "Failed to create image!");

		// Allocate memory for the image
		vkGetImageMemoryRequirements(device, image, &memRequirements);

		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = 0; // Assume first memory type is suitable

		result = vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory);
		CHECK_VK_RESULT(result, "Failed to allocate image memory!");

		vkBindImageMemory(device, image, imageMemory, 0);

		// Create an image view
		VkImageViewCreateInfo imageViewCreateInfo = {};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.image = image;
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.layerCount = 1;

		result = vkCreateImageView(device, &imageViewCreateInfo, nullptr, &imageView);
		CHECK_VK_RESULT(result, "Failed to create image view!");

		// Create a render pass
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = VK_FORMAT_R8G8B8A8_UNORM;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentRef = {};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;

		VkRenderPassCreateInfo renderPassCreateInfo = {};
		renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassCreateInfo.attachmentCount = 1;
		renderPassCreateInfo.pAttachments = &colorAttachment;
		renderPassCreateInfo.subpassCount = 1;
		renderPassCreateInfo.pSubpasses = &subpass;

		result = vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass);
		CHECK_VK_RESULT(result, "Failed to create render pass!");

		// Create a framebuffer
		VkFramebufferCreateInfo framebufferCreateInfo = {};
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.renderPass = renderPass;
		framebufferCreateInfo.attachmentCount = 1;
		framebufferCreateInfo.pAttachments = &imageView;
		framebufferCreateInfo.width = 512;
		framebufferCreateInfo.height = 512;
		framebufferCreateInfo.layers = 1;

		result = vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &framebuffer);
		CHECK_VK_RESULT(result, "Failed to create framebuffer!");

		// Create a descriptor set layout
		VkDescriptorSetLayoutBinding layoutBinding = {};
		layoutBinding.binding = 0;
		layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		layoutBinding.descriptorCount = 1;
		layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
		layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutCreateInfo.bindingCount = 1;
		layoutCreateInfo.pBindings = &layoutBinding;

		result = vkCreateDescriptorSetLayout(device, &layoutCreateInfo, nullptr, &descriptorSetLayout);
		CHECK_VK_RESULT(result, "Failed to create descriptor set layout!");

		// Create a descriptor pool
		VkDescriptorPoolSize poolSize = {};
		poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSize.descriptorCount = 1;

		VkDescriptorPoolCreateInfo poolCreateInfo = {};
		poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolCreateInfo.poolSizeCount = 1;
		poolCreateInfo.pPoolSizes = &poolSize;
		poolCreateInfo.maxSets = 1;

		result = vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &descriptorPool);
		CHECK_VK_RESULT(result, "Failed to create descriptor pool!");

		// Allocate a descriptor set
		VkDescriptorSetAllocateInfo allocSetInfo = {};
		allocSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocSetInfo.descriptorPool = descriptorPool;
		allocSetInfo.descriptorSetCount = 1;
		allocSetInfo.pSetLayouts = &descriptorSetLayout;

		result = vkAllocateDescriptorSets(device, &allocSetInfo, &descriptorSet);
		CHECK_VK_RESULT(result, "Failed to allocate descriptor set!");

		// Create a sampler
		VkSamplerCreateInfo samplerCreateInfo = {};
		samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

		result = vkCreateSampler(device, &samplerCreateInfo, nullptr, &sampler);
		CHECK_VK_RESULT(result, "Failed to create sampler!");

		// Cleanup
		vkDestroySampler(device, sampler, nullptr);
		vkDestroyDescriptorPool(device, descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		vkDestroyFramebuffer(device, framebuffer, nullptr);
		vkDestroyRenderPass(device, renderPass, nullptr);
		vkDestroyImageView(device, imageView, nullptr);
		vkDestroyImage(device, image, nullptr);
		vkFreeMemory(device, imageMemory, nullptr);
		vkDestroyBufferView(device, bufferView, nullptr);
		vkDestroyBuffer(device, buffer, nullptr);
		vkFreeMemory(device, bufferMemory, nullptr);
		vkDestroyFence(device, fence, nullptr);
		vkDestroySemaphore(device, semaphore, nullptr);
		vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
		vkDestroyCommandPool(device, commandPool, nullptr);
		vkDestroyDevice(device, nullptr);
		if (enableValidationLayers) {
			destroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
		}
		vkDestroyInstance(instance, nullptr);

		std::cout << "Vulkan initialization and cleanup completed successfully!" << std::endl;
	}
	catch (const std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return false;
	}

	return true;
}
#pragma clang diagnostic pop
