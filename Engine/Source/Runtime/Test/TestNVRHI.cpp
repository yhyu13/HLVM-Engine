/**
* Copyright (c) 2026. MIT License. All rights reserved.
*/

#include "Test.h"

DECLARE_LOG_CATEGORY(LogTest)

#if defined(WIN32) || defined(_WIN32) || defined(_WIN32_) || defined(WIN64) || defined(_WIN64) || defined(_WIN64_)
	#define VULKAN_LIB "vulkan-1.dll"
#elif defined(ANDROID) || defined(_ANDROID_)
	#define VULKAN_LIB "libvulkan.so"
#else
	#define VULKAN_LIB "libvulkan.so.1"
#endif

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-function-type-strict"
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wunused-macros"

// First load vulkan hpp with dynamic dispatch (aka VK_NO_PROTOTYPE)
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
static_assert(VULKAN_HPP_DISPATCH_LOADER_DYNAMIC==1, "VULKAN_HPP_DISPATCH_LOADER_DYNAMIC must be defined to 1");
#if ( VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1 )
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#endif

const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation" // 开启可用的校验层
};

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME, // 交换链扩展集合
		VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, // 硬件加速扩展
		VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, // 光线追踪扩展
		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME, // 延迟主机操作扩展
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

#define CHECK_VK_RESULT(result, message)       \
	do                                         \
	{                                          \
		if (result != VK_SUCCESS)              \
		{                                      \
			throw std::runtime_error(message); \
		}                                      \
	}                                          \
	while (0)

// Modern Vulkan-HPP style callback - no casting needed
static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback2(
	vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	vk::DebugUtilsMessageTypeFlagsEXT /* messageType */,
	const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* /* pUserData */)
{
	if (messageSeverity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError) {
		std::cerr << "Error Message: " << pCallbackData->pMessage << std::endl;
	}
	else if (messageSeverity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
		std::cerr << "Warning Message: " << pCallbackData->pMessage << std::endl;
	}
	else if (messageSeverity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo) {
		std::cout << "Info Message: " << pCallbackData->pMessage << std::endl;
	} // ignore verbose logs
	return VK_FALSE;
}

// load nvrhi after vulkan stuff
#include <nvrhi/vulkan.h>
#include <nvrhi/validation.h>

struct DefaultMessageCallback : public nvrhi::IMessageCallback
{
	static DefaultMessageCallback& GetInstance()
	{
		static DefaultMessageCallback instance;
		return instance;
	}

	void message( nvrhi::MessageSeverity severity, const char* messageText ) override
	{
		switch( severity )
		{
			case nvrhi::MessageSeverity::Info:
				HLVM_LOG(LogTest, info, TXT("{0}"), TO_TCHAR_CSTR(messageText));
				break;
			case nvrhi::MessageSeverity::Warning:
				HLVM_LOG(LogTest, warn, TXT("{0}"), TO_TCHAR_CSTR(messageText));
				break;
			case nvrhi::MessageSeverity::Error:
				HLVM_LOG(LogTest, err, TXT("{0}"), TO_TCHAR_CSTR(messageText));
				break;
			case nvrhi::MessageSeverity::Fatal:
				HLVM_LOG(LogTest, critical, TXT("{0}"), TO_TCHAR_CSTR(messageText));
				break;
			default:
				break;
		}
	}
};


// Refernce https://github.com/KhronosGroup/Vulkan-Samples/blob/main/samples/api/hpp_hello_triangle_1_3/hpp_hello_triangle_1_3.cpp
RECORD_BOOL(nvrhi_vulkan_test1)
{
#if ( VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1 )
	static vk::detail::DynamicLoader dl(VULKAN_LIB);
	PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
	VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
#endif

	vk::Instance instance;
	vk::DebugUtilsMessengerEXT debugMessenger;
	vk::PhysicalDevice physicalDevice;
	vk::Device device;
	vk::Queue queue;
	vk::CommandPool commandPool;
	vk::CommandBuffer commandBuffer;
	vk::Semaphore semaphore;
	vk::Fence fence;
	vk::Buffer buffer;
	vk::DeviceMemory bufferMemory;
	vk::BufferView bufferView;
	vk::Image image;
	vk::DeviceMemory imageMemory;
	vk::ImageView imageView;
	vk::Framebuffer framebuffer;
	vk::RenderPass renderPass;
	vk::PipelineLayout pipelineLayout;
	vk::Pipeline pipeline;
	vk::DescriptorSetLayout descriptorSetLayout;
	vk::DescriptorSet descriptorSet;
	vk::DescriptorPool descriptorPool;
	vk::Sampler sampler;
	vk::SwapchainKHR swapchain;

	try
	{
		// Initialize Vulkan instance
		vk::ApplicationInfo appInfo(
			"Vulkan Test", VK_MAKE_VERSION(1, 0, 0),
			"No Engine", VK_MAKE_VERSION(1, 0, 0),
			VK_API_VERSION_1_0
		);

		auto extensions = getRequiredExtensions();

		vk::InstanceCreateInfo createInfo;
		createInfo.setPApplicationInfo(&appInfo)
			.setEnabledExtensionCount(static_cast<uint32_t>(extensions.size()))
			.setPpEnabledExtensionNames(extensions.data());

		vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo;
		if (enableValidationLayers) {
			createInfo.setEnabledLayerCount(static_cast<uint32_t>(validationLayers.size()))
				.setPpEnabledLayerNames(validationLayers.data());

			debugCreateInfo.setMessageSeverity(
				vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
				vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
				vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
			);
			debugCreateInfo.setMessageType(
				vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
				vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
				vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
			);
			debugCreateInfo.setPfnUserCallback(debugCallback2);

			createInfo.setPNext(&debugCreateInfo);
		} else {
			createInfo.setEnabledLayerCount(0)
				.setPNext(nullptr);
		}

		vk::Result result = vk::createInstance(&createInfo, nullptr, &instance);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to create Vulkan instance!");
#if ( VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1 )
		// initialize function pointers for instance
		VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);
#endif

		if (enableValidationLayers) {
			auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
				instance.getProcAddr("vkCreateDebugUtilsMessengerEXT"));
			if (func) {
				VkDebugUtilsMessengerCreateInfoEXT rawCreateInfo = static_cast<VkDebugUtilsMessengerCreateInfoEXT>(debugCreateInfo);
				VkDebugUtilsMessengerEXT rawMessenger;
				if (func(instance, &rawCreateInfo, nullptr, &rawMessenger) != VK_SUCCESS) {
					throw std::runtime_error("failed to set up debug messenger!");
				}
				debugMessenger = rawMessenger;
			}
		}

		// Enumerate physical devices
		std::vector<vk::PhysicalDevice> devices = instance.enumeratePhysicalDevices();
		if (devices.empty())
		{
			throw std::runtime_error("No Vulkan-supported GPUs found!");
		}
		physicalDevice = devices[0]; // Use the first GPU

		// Create a logical device
		float queuePriority = 1.0f;
		vk::DeviceQueueCreateInfo queueCreateInfo(
			vk::DeviceQueueCreateFlags(),
			0, // Assume first queue family
			1,
			&queuePriority
		);

		vk::PhysicalDeviceFeatures deviceFeatures;
		deviceFeatures.setGeometryShader(true);

		vk::DeviceCreateInfo deviceCreateInfo;
		deviceCreateInfo.setQueueCreateInfoCount(1)
			.setPQueueCreateInfos(&queueCreateInfo)
			.setPEnabledFeatures(&deviceFeatures)
			.setEnabledExtensionCount(static_cast<uint32_t>(deviceExtensions.size()))
			.setPpEnabledExtensionNames(deviceExtensions.data());

		result = physicalDevice.createDevice(&deviceCreateInfo, nullptr, &device);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to create logical device!");
#if ( VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1 )
		// initialize function pointers for device
		VULKAN_HPP_DEFAULT_DISPATCHER.init(device);
#endif
		// Get a queue
		queue = device.getQueue(0, 0);

		{
			nvrhi::vulkan::DeviceDesc deviceDesc;
			deviceDesc.errorCB = &(DefaultMessageCallback::GetInstance());
			deviceDesc.physicalDevice = physicalDevice;
			deviceDesc.device = device;
			deviceDesc.graphicsQueue = queue;
			deviceDesc.graphicsQueueIndex = 0;
			deviceDesc.deviceExtensions = C_C(const char**, deviceExtensions.data());
			deviceDesc.numDeviceExtensions = std::size(deviceExtensions);

			nvrhi::DeviceHandle m_NvrhiDevice = nvrhi::vulkan::createDevice(deviceDesc);
		}

		// Create a command pool
		vk::CommandPoolCreateInfo commandPoolCreateInfo;
		commandPoolCreateInfo.setQueueFamilyIndex(0); // Assume first queue family

		result = device.createCommandPool(&commandPoolCreateInfo, nullptr, &commandPool);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to create command pool!");

		// Allocate a command buffer
		vk::CommandBufferAllocateInfo commandBufferAllocateInfo;
		commandBufferAllocateInfo.setCommandPool(commandPool)
			.setLevel(vk::CommandBufferLevel::ePrimary)
			.setCommandBufferCount(1);

		result = device.allocateCommandBuffers(&commandBufferAllocateInfo, &commandBuffer);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to allocate command buffer!");

		// Create a semaphore
		vk::SemaphoreCreateInfo semaphoreCreateInfo;

		result = device.createSemaphore(&semaphoreCreateInfo, nullptr, &semaphore);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to create semaphore!");

		// Create a fence
		vk::FenceCreateInfo fenceCreateInfo;

		result = device.createFence(&fenceCreateInfo, nullptr, &fence);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to create fence!");

		// Create a buffer
		vk::BufferCreateInfo bufferCreateInfo;
		bufferCreateInfo.setSize(1024) // 1 KB buffer
			.setUsage(vk::BufferUsageFlagBits::eVertexBuffer)
			.setSharingMode(vk::SharingMode::eExclusive);

		result = device.createBuffer(&bufferCreateInfo, nullptr, &buffer);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to create buffer!");

		// Allocate memory for the buffer
		vk::MemoryRequirements memRequirements = device.getBufferMemoryRequirements(buffer);

		vk::MemoryAllocateInfo allocInfo;
		allocInfo.setAllocationSize(memRequirements.size)
			.setMemoryTypeIndex(0); // Assume first memory type is suitable

		result = device.allocateMemory(&allocInfo, nullptr, &bufferMemory);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to allocate buffer memory!");

		device.bindBufferMemory(buffer, bufferMemory, 0);

		// Create a buffer view
		vk::BufferViewCreateInfo bufferViewCreateInfo;
		bufferViewCreateInfo.setBuffer(buffer)
			.setFormat(vk::Format::eR32Sfloat)
			.setOffset(0)
			.setRange(VK_WHOLE_SIZE);

		result = device.createBufferView(&bufferViewCreateInfo, nullptr, &bufferView);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to create buffer view!");

		// Create an image
		vk::ImageCreateInfo imageCreateInfo;
		imageCreateInfo.setImageType(vk::ImageType::e2D)
			.setFormat(vk::Format::eR8G8B8A8Unorm)
			.setExtent({512, 512, 1})
			.setMipLevels(1)
			.setArrayLayers(1)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setTiling(vk::ImageTiling::eOptimal)
			.setUsage(vk::ImageUsageFlagBits::eSampled);

		result = device.createImage(&imageCreateInfo, nullptr, &image);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to create image!");

		// Allocate memory for the image
		memRequirements = device.getImageMemoryRequirements(image);

		allocInfo.setAllocationSize(memRequirements.size)
			.setMemoryTypeIndex(0); // Assume first memory type is suitable

		result = device.allocateMemory(&allocInfo, nullptr, &imageMemory);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to allocate image memory!");

		device.bindImageMemory(image, imageMemory, 0);

		// Create an image view
		vk::ImageViewCreateInfo imageViewCreateInfo;
		imageViewCreateInfo.setImage(image)
			.setViewType(vk::ImageViewType::e2D)
			.setFormat(vk::Format::eR8G8B8A8Unorm)
			.setSubresourceRange(vk::ImageSubresourceRange(
				vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1
				));

		result = device.createImageView(&imageViewCreateInfo, nullptr, &imageView);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to create image view!");

		// Create a render pass
		vk::AttachmentDescription colorAttachment;
		colorAttachment.setFormat(vk::Format::eR8G8B8A8Unorm)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(vk::AttachmentLoadOp::eClear)
			.setStoreOp(vk::AttachmentStoreOp::eStore)
			.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
			.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

		vk::AttachmentReference colorAttachmentRef(
			0, vk::ImageLayout::eColorAttachmentOptimal
		);

		vk::SubpassDescription subpass;
		subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
			.setColorAttachmentCount(1)
			.setPColorAttachments(&colorAttachmentRef);

		vk::RenderPassCreateInfo renderPassCreateInfo;
		renderPassCreateInfo.setAttachmentCount(1)
			.setPAttachments(&colorAttachment)
			.setSubpassCount(1)
			.setPSubpasses(&subpass);

		result = device.createRenderPass(&renderPassCreateInfo, nullptr, &renderPass);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to create render pass!");

		// Create a framebuffer
		vk::FramebufferCreateInfo framebufferCreateInfo;
		framebufferCreateInfo.setRenderPass(renderPass)
			.setAttachmentCount(1)
			.setPAttachments(&imageView)
			.setWidth(512)
			.setHeight(512)
			.setLayers(1);

		result = device.createFramebuffer(&framebufferCreateInfo, nullptr, &framebuffer);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to create framebuffer!");

		// Create a descriptor set layout
		vk::DescriptorSetLayoutBinding layoutBinding(
			0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex
		);

		vk::DescriptorSetLayoutCreateInfo layoutCreateInfo;
		layoutCreateInfo.setBindingCount(1)
			.setPBindings(&layoutBinding);

		result = device.createDescriptorSetLayout(&layoutCreateInfo, nullptr, &descriptorSetLayout);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to create descriptor set layout!");

		// Create a descriptor pool
		vk::DescriptorPoolSize poolSize(
			vk::DescriptorType::eUniformBuffer, 1
		);

		vk::DescriptorPoolCreateInfo poolCreateInfo;
		poolCreateInfo.setPoolSizeCount(1)
			.setPPoolSizes(&poolSize)
			.setMaxSets(1);

		result = device.createDescriptorPool(&poolCreateInfo, nullptr, &descriptorPool);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to create descriptor pool!");

		// Allocate a descriptor set
		vk::DescriptorSetAllocateInfo allocSetInfo;
		allocSetInfo.setDescriptorPool(descriptorPool)
			.setDescriptorSetCount(1)
			.setPSetLayouts(&descriptorSetLayout);

		result = device.allocateDescriptorSets(&allocSetInfo, &descriptorSet);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to allocate descriptor set!");

		// Create a sampler
		vk::SamplerCreateInfo samplerCreateInfo;
		samplerCreateInfo.setMagFilter(vk::Filter::eLinear)
			.setMinFilter(vk::Filter::eLinear)
			.setAddressModeU(vk::SamplerAddressMode::eRepeat)
			.setAddressModeV(vk::SamplerAddressMode::eRepeat)
			.setAddressModeW(vk::SamplerAddressMode::eRepeat);

		result = device.createSampler(&samplerCreateInfo, nullptr, &sampler);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to create sampler!");

		// Cleanup
		device.destroySampler(sampler, nullptr);
		device.destroyDescriptorPool(descriptorPool, nullptr);
		device.destroyDescriptorSetLayout(descriptorSetLayout, nullptr);
		device.destroyFramebuffer(framebuffer, nullptr);
		device.destroyRenderPass(renderPass, nullptr);
		device.destroyImageView(imageView, nullptr);
		device.destroyImage(image, nullptr);
		device.freeMemory(imageMemory, nullptr);
		device.destroyBufferView(bufferView, nullptr);
		device.destroyBuffer(buffer, nullptr);
		device.freeMemory(bufferMemory, nullptr);
		device.destroyFence(fence, nullptr);
		device.destroySemaphore(semaphore, nullptr);
		device.freeCommandBuffers(commandPool, 1, &commandBuffer);
		device.destroyCommandPool(commandPool, nullptr);
		device.destroy(nullptr);
		if (enableValidationLayers) {
			auto destroyFunc = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
				instance.getProcAddr("vkDestroyDebugUtilsMessengerEXT"));
			if (destroyFunc) {
				destroyFunc(instance, debugMessenger, nullptr);
			}
		}
		instance.destroy(nullptr);

		std::cout << "Vulkan initialization and cleanup completed successfully!" << std::endl;
	}
	catch (const std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return false;
	}

	return true;
}

RECORD_BOOL(nvrhi_vulkan_test2)
{
	// Already initialized
//#if ( VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1 )
//	static vk::detail::DynamicLoader dl(VULKAN_LIB);
//	PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
//	VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
//#endif

	vk::Instance instance;
	vk::DebugUtilsMessengerEXT debugMessenger;
	vk::PhysicalDevice physicalDevice;
	vk::Device device;
	vk::Queue queue;
	vk::CommandPool commandPool;
	vk::CommandBuffer commandBuffer;
	vk::Semaphore semaphore;
	vk::Fence fence;
	vk::Buffer buffer;
	vk::DeviceMemory bufferMemory;
	vk::BufferView bufferView;
	vk::Image image;
	vk::DeviceMemory imageMemory;
	vk::ImageView imageView;
	vk::Framebuffer framebuffer;
	vk::RenderPass renderPass;
	vk::PipelineLayout pipelineLayout;
	vk::Pipeline pipeline;
	vk::DescriptorSetLayout descriptorSetLayout;
	vk::DescriptorSet descriptorSet;
	vk::DescriptorPool descriptorPool;
	vk::Sampler sampler;
	vk::SwapchainKHR swapchain;

	try
	{
		// Fixed: Separate instance and device extensions properly
		std::vector<const char*> instanceExtensions = getRequiredExtensions();
		// VK_KHR_surface is an INSTANCE extension, add it here
		instanceExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME); // Required by swapchain
		instanceExtensions.push_back("VK_KHR_xcb_surface"); // Required by linux: vulkaninfo | grep '^GPU id'

		// Initialize Vulkan instance - use Vulkan 1.1 for vkGetPhysicalDeviceProperties2
		vk::ApplicationInfo appInfo(
			"Vulkan Test", VK_MAKE_VERSION(1, 0, 0),
			"No Engine", VK_MAKE_VERSION(1, 0, 0),
			VK_API_VERSION_1_1  // Fixed: was VK_API_VERSION_1_0
		);

		auto extensions = getRequiredExtensions();

		vk::InstanceCreateInfo createInfo;
		createInfo.setPApplicationInfo(&appInfo)
			.setEnabledExtensionCount(static_cast<uint32_t>(extensions.size()))
			.setPpEnabledExtensionNames(extensions.data());

		vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo;
		if (enableValidationLayers) {
			createInfo.setEnabledLayerCount(static_cast<uint32_t>(validationLayers.size()))
				.setPpEnabledLayerNames(validationLayers.data());

			debugCreateInfo.setMessageSeverity(
				vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
				vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
				vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
			);
			debugCreateInfo.setMessageType(
				vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
				vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
				vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
			);
			debugCreateInfo.setPfnUserCallback(debugCallback2);

			createInfo.setPNext(&debugCreateInfo);
		} else {
			createInfo.setEnabledLayerCount(0)
				.setPNext(nullptr);
		}

		vk::Result result = vk::createInstance(&createInfo, nullptr, &instance);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to create Vulkan instance!");

		if (enableValidationLayers) {
			auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
				instance.getProcAddr("vkCreateDebugUtilsMessengerEXT"));
			if (func) {
				VkDebugUtilsMessengerCreateInfoEXT rawCreateInfo = static_cast<VkDebugUtilsMessengerCreateInfoEXT>(debugCreateInfo);
				VkDebugUtilsMessengerEXT rawMessenger;
				if (func(instance, &rawCreateInfo, nullptr, &rawMessenger) != VK_SUCCESS) {
					throw std::runtime_error("failed to set up debug messenger!");
				}
				debugMessenger = rawMessenger;
			}
		}

		// Enumerate physical devices
		std::vector<vk::PhysicalDevice> devices = instance.enumeratePhysicalDevices();
		if (devices.empty())
		{
			throw std::runtime_error("No Vulkan-supported GPUs found!");
		}
		physicalDevice = devices[0];

		// Fixed: Proper device extensions with all dependencies
		std::vector<const char*> deviceExtensions2 = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
			VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
			VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,   // Required by acceleration structure
			VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, // Required by acceleration structure
			VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
			VK_KHR_SPIRV_1_4_EXTENSION_NAME,         // Required by ray tracing pipeline
			VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
			VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME // Required for timeline semaphores
		};

		// Create a logical device with timeline semaphore feature
		float queuePriority = 1.0f;
		vk::DeviceQueueCreateInfo queueCreateInfo(
			vk::DeviceQueueCreateFlags(),
			0,
			1,
			&queuePriority
		);

		// Fixed: Enable timeline semaphore feature
		vk::PhysicalDeviceTimelineSemaphoreFeatures timelineFeatures;
		timelineFeatures.setTimelineSemaphore(true);

		vk::PhysicalDeviceFeatures deviceFeatures;
		deviceFeatures.setGeometryShader(true);

		vk::PhysicalDeviceFeatures2 deviceFeatures2;
		deviceFeatures2.setFeatures(deviceFeatures)
			.setPNext(&timelineFeatures);

		vk::DeviceCreateInfo deviceCreateInfo;
		deviceCreateInfo.setQueueCreateInfoCount(1)
			.setPQueueCreateInfos(&queueCreateInfo)
			.setPEnabledFeatures(nullptr) // Using features2 instead
			.setPNext(&deviceFeatures2)
			.setEnabledExtensionCount(static_cast<uint32_t>(deviceExtensions2.size()))
			.setPpEnabledExtensionNames(deviceExtensions2.data());

		result = physicalDevice.createDevice(&deviceCreateInfo, nullptr, &device);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to create logical device!");

		// Get a queue
		queue = device.getQueue(0, 0);

		{
			nvrhi::vulkan::DeviceDesc deviceDesc;
			deviceDesc.errorCB = &(DefaultMessageCallback::GetInstance());
			deviceDesc.physicalDevice = physicalDevice;
			deviceDesc.device = device;
			deviceDesc.graphicsQueue = queue;
			deviceDesc.graphicsQueueIndex = 0;
			deviceDesc.deviceExtensions = C_C(const char**, deviceExtensions2.data());
			deviceDesc.numDeviceExtensions = std::size(deviceExtensions2);

			nvrhi::DeviceHandle m_NvrhiDevice = nvrhi::vulkan::createDevice(deviceDesc);
		}

		// Create a command pool
		vk::CommandPoolCreateInfo commandPoolCreateInfo;
		commandPoolCreateInfo.setQueueFamilyIndex(0);

		result = device.createCommandPool(&commandPoolCreateInfo, nullptr, &commandPool);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to create command pool!");

		// Allocate a command buffer
		vk::CommandBufferAllocateInfo commandBufferAllocateInfo;
		commandBufferAllocateInfo.setCommandPool(commandPool)
			.setLevel(vk::CommandBufferLevel::ePrimary)
			.setCommandBufferCount(1);

		result = device.allocateCommandBuffers(&commandBufferAllocateInfo, &commandBuffer);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to allocate command buffer!");

		// Create a semaphore
		vk::SemaphoreCreateInfo semaphoreCreateInfo;

		result = device.createSemaphore(&semaphoreCreateInfo, nullptr, &semaphore);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to create semaphore!");

		// Create a fence
		vk::FenceCreateInfo fenceCreateInfo;

		result = device.createFence(&fenceCreateInfo, nullptr, &fence);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to create fence!");

		// Fixed: Buffer usage must include texel buffer bit for buffer views
		vk::BufferCreateInfo bufferCreateInfo;
		bufferCreateInfo.setSize(1024)
			.setUsage(vk::BufferUsageFlagBits::eVertexBuffer |
				vk::BufferUsageFlagBits::eUniformTexelBuffer) // Fixed: added texel buffer
			.setSharingMode(vk::SharingMode::eExclusive);

		result = device.createBuffer(&bufferCreateInfo, nullptr, &buffer);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to create buffer!");

		// Allocate memory for the buffer
		vk::MemoryRequirements memRequirements = device.getBufferMemoryRequirements(buffer);

		vk::MemoryAllocateInfo allocInfo;
		allocInfo.setAllocationSize(memRequirements.size)
			.setMemoryTypeIndex(0);

		result = device.allocateMemory(&allocInfo, nullptr, &bufferMemory);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to allocate buffer memory!");

		device.bindBufferMemory(buffer, bufferMemory, 0);

		// Create a buffer view
		vk::BufferViewCreateInfo bufferViewCreateInfo;
		bufferViewCreateInfo.setBuffer(buffer)
			.setFormat(vk::Format::eR32Sfloat)
			.setOffset(0)
			.setRange(VK_WHOLE_SIZE);

		result = device.createBufferView(&bufferViewCreateInfo, nullptr, &bufferView);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to create buffer view!");

		// Fixed: Image usage must include color attachment for framebuffer
		vk::ImageCreateInfo imageCreateInfo;
		imageCreateInfo.setImageType(vk::ImageType::e2D)
			.setFormat(vk::Format::eR8G8B8A8Unorm)
			.setExtent({512, 512, 1})
			.setMipLevels(1)
			.setArrayLayers(1)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setTiling(vk::ImageTiling::eOptimal)
			.setUsage(vk::ImageUsageFlagBits::eSampled |
				vk::ImageUsageFlagBits::eColorAttachment); // Fixed: added color attachment

		result = device.createImage(&imageCreateInfo, nullptr, &image);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to create image!");

		// Allocate memory for the image
		memRequirements = device.getImageMemoryRequirements(image);

		allocInfo.setAllocationSize(memRequirements.size)
			.setMemoryTypeIndex(0);

		result = device.allocateMemory(&allocInfo, nullptr, &imageMemory);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to allocate image memory!");

		device.bindImageMemory(image, imageMemory, 0);

		// Create an image view
		vk::ImageViewCreateInfo imageViewCreateInfo;
		imageViewCreateInfo.setImage(image)
			.setViewType(vk::ImageViewType::e2D)
			.setFormat(vk::Format::eR8G8B8A8Unorm)
			.setSubresourceRange(vk::ImageSubresourceRange(
				vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1
				));

		result = device.createImageView(&imageViewCreateInfo, nullptr, &imageView);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to create image view!");

		// Create a render pass
		vk::AttachmentDescription colorAttachment;
		colorAttachment.setFormat(vk::Format::eR8G8B8A8Unorm)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(vk::AttachmentLoadOp::eClear)
			.setStoreOp(vk::AttachmentStoreOp::eStore)
			.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
			.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

		vk::AttachmentReference colorAttachmentRef(
			0, vk::ImageLayout::eColorAttachmentOptimal
		);

		vk::SubpassDescription subpass;
		subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
			.setColorAttachmentCount(1)
			.setPColorAttachments(&colorAttachmentRef);

		vk::RenderPassCreateInfo renderPassCreateInfo;
		renderPassCreateInfo.setAttachmentCount(1)
			.setPAttachments(&colorAttachment)
			.setSubpassCount(1)
			.setPSubpasses(&subpass);

		result = device.createRenderPass(&renderPassCreateInfo, nullptr, &renderPass);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to create render pass!");

		// Create a framebuffer
		vk::FramebufferCreateInfo framebufferCreateInfo;
		framebufferCreateInfo.setRenderPass(renderPass)
			.setAttachmentCount(1)
			.setPAttachments(&imageView)
			.setWidth(512)
			.setHeight(512)
			.setLayers(1);

		result = device.createFramebuffer(&framebufferCreateInfo, nullptr, &framebuffer);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to create framebuffer!");

		// Create a descriptor set layout
		vk::DescriptorSetLayoutBinding layoutBinding(
			0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex
		);

		vk::DescriptorSetLayoutCreateInfo layoutCreateInfo;
		layoutCreateInfo.setBindingCount(1)
			.setPBindings(&layoutBinding);

		result = device.createDescriptorSetLayout(&layoutCreateInfo, nullptr, &descriptorSetLayout);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to create descriptor set layout!");

		// Create a descriptor pool
		vk::DescriptorPoolSize poolSize(
			vk::DescriptorType::eUniformBuffer, 1
		);

		vk::DescriptorPoolCreateInfo poolCreateInfo;
		poolCreateInfo.setPoolSizeCount(1)
			.setPPoolSizes(&poolSize)
			.setMaxSets(1);

		result = device.createDescriptorPool(&poolCreateInfo, nullptr, &descriptorPool);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to create descriptor pool!");

		// Allocate a descriptor set
		vk::DescriptorSetAllocateInfo allocSetInfo;
		allocSetInfo.setDescriptorPool(descriptorPool)
			.setDescriptorSetCount(1)
			.setPSetLayouts(&descriptorSetLayout);

		result = device.allocateDescriptorSets(&allocSetInfo, &descriptorSet);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to allocate descriptor set!");

		// Create a sampler
		vk::SamplerCreateInfo samplerCreateInfo;
		samplerCreateInfo.setMagFilter(vk::Filter::eLinear)
			.setMinFilter(vk::Filter::eLinear)
			.setAddressModeU(vk::SamplerAddressMode::eRepeat)
			.setAddressModeV(vk::SamplerAddressMode::eRepeat)
			.setAddressModeW(vk::SamplerAddressMode::eRepeat);

		result = device.createSampler(&samplerCreateInfo, nullptr, &sampler);
		CHECK_VK_RESULT(static_cast<VkResult>(result), "Failed to create sampler!");

		// Cleanup
		device.destroySampler(sampler, nullptr);
		device.destroyDescriptorPool(descriptorPool, nullptr);
		device.destroyDescriptorSetLayout(descriptorSetLayout, nullptr);
		device.destroyFramebuffer(framebuffer, nullptr);
		device.destroyRenderPass(renderPass, nullptr);
		device.destroyImageView(imageView, nullptr);
		device.destroyImage(image, nullptr);
		device.freeMemory(imageMemory, nullptr);
		device.destroyBufferView(bufferView, nullptr);
		device.destroyBuffer(buffer, nullptr);
		device.freeMemory(bufferMemory, nullptr);
		device.destroyFence(fence, nullptr);
		device.destroySemaphore(semaphore, nullptr);
		device.freeCommandBuffers(commandPool, 1, &commandBuffer);
		device.destroyCommandPool(commandPool, nullptr);
		device.destroy(nullptr);
		if (enableValidationLayers) {
			auto destroyFunc = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
				instance.getProcAddr("vkDestroyDebugUtilsMessengerEXT"));
			if (destroyFunc) {
				destroyFunc(instance, debugMessenger, nullptr);
			}
		}
		instance.destroy(nullptr);

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
