/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "Test.h"

DECLARE_LOG_CATEGORY(LogTest)

#include "Window/WindowDefinition.h"
#if HLVM_WINDOW_USE_VULKAN
	#include "Window/Vulkan/GLFW3VulkanWindow.h"

using namespace VulkanRHI;

	#if 1 // Test Vulkan triangle program with direct vulkan api calls
		#pragma clang diagnostic push
		#pragma clang diagnostic ignored "-Wdocumentation"
		#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
		#pragma clang diagnostic ignored "-Wold-style-cast"
		#pragma clang diagnostic ignored "-Wextra-semi-stmt"
		#pragma clang diagnostic ignored "-Wmissing-noreturn"
		#pragma clang diagnostic ignored "-Wcast-function-type-strict"
		#pragma clang diagnostic ignored "-Wunused-parameter"
		#pragma clang diagnostic ignored "-Wshadow"
		#pragma clang diagnostic ignored "-Wmissing-braces"
		#pragma clang diagnostic ignored "-Wsign-conversion"

using namespace std;

const uint32_t	   WIDTH = 800;
const uint32_t	   HEIGHT = 600;
static const char* WINDOW_TITLE = "Vulkan Test";

const int MAX_FRAMES_IN_FLIGHT = 2; // 定义GPU同时渲染帧数

const vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation" // 开启可用的校验层
};

const vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME // 交换链扩展集合
};

		#if VULKAN_ENABLE_VALIDATION_LAYERS
const bool enableValidationLayers = true;
		#else
const bool enableValidationLayers = false;
		#endif // NDEBUG

// 使用vkGetInstanceProcAddr获取某个api的函数指针
static VkResult createDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else
	{
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

static void destroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		func(instance, debugMessenger, pAllocator);
	}
}

// 查询设备的可用队列族，-1代表无效
struct QueueFamilyIndices
{
	// 支持图像绘制的队列
	uint32_t graphicsFamily = std::numeric_limits<uint32_t>::max();

	// 支持图形呈现的队列
	uint32_t presentFamily = std::numeric_limits<uint32_t>::max();

	bool isComplete()
	{
		return (graphicsFamily < std::numeric_limits<uint32_t>::max())
			&& (presentFamily < std::numeric_limits<uint32_t>::max());
	}
};

// 查询并记录交换链支持的细节
struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR   capabilities; // 基础表面特性
	vector<VkSurfaceFormatKHR> formats;		 // 像素格式、色彩空间
	vector<VkPresentModeKHR>   presentModes; // 可用的呈现模式
};

struct Vertex
{
	float position[3];
	float color[3];
};

class HelloTriangleApplication
{
public:
	void run()
	{
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

private:
	GLFWwindow* window;

	VkInstance				 instance; // 用以调用Vulkan的Api
	VkDebugUtilsMessengerEXT debugMessenger;
	VkSurfaceKHR			 surface; // 用于显示的窗口句柄

	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE; // 显卡设备
	VkDevice		 device;						  // 逻辑设备，用于与物理设备交互的接口，而且逻辑设备并不Vulkan实例交互

	VkQueue graphicsQueue; // 用来存储逻辑设备的图形绘制队列句柄
	VkQueue presentQueue;  // 用来存储逻辑设备的图形呈现队列句柄

	VkSwapchainKHR		  swapChain;
	vector<VkImage>		  swapChainImages; // 交换链图像句柄
	VkFormat			  swapChainImageFormat;
	VkExtent2D			  swapChainExtent;
	vector<VkImageView>	  swapChainImageViews;	 // Vulkan对象，包括处于交换链，或者管线，都需要绑定一个VkImageView对象来访问它
	vector<VkFramebuffer> swapChainFramebuffers; // 添加一个集合存储帧缓冲对象

	VkBuffer	   vertexBuffer; // 顶点数据Buffer
	VkBuffer	   indexBuffer;	 // 顶点index数据buffer
	VkDeviceMemory vertexMemory;
	VkDeviceMemory indexMemory;

	VkRenderPass			renderPass;
	VkPipelineLayout		pipelineLayout; // 管线布局
	VkPipeline				graphicsPipeline;
	VkCommandPool			commandPool; // 指令池
	vector<VkCommandBuffer> commandBuffers;

	vector<VkSemaphore> imageAvailableSemaphores; // 用于绘制的同步变量
	vector<VkSemaphore> renderFinishedSemaphores;
	vector<VkFence>		inFlightFences;
	vector<VkFence>		imagesInFlight;
	size_t				currentFrame = 0;

	void initWindow()
	{
		cout << "initWindow IN" << endl;
		glfwInit(); // 初始化图形库框架：Graphics Libraries Framework

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // GLFW会默认创建OpenGL的Context，但是，我们使用的是Vulkan，所以需要取消
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);	  // 禁止窗口拉伸，涉及SwapChain重建

		window = glfwCreateWindow(WIDTH, HEIGHT, WINDOW_TITLE, nullptr, nullptr); // 创建窗口
	}

	void initVulkan()
	{
		cout << "initVulkan IN" << endl;
		createInstance();
		setupDebugMessenger();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
		createImageViews();
		PrepareVertexAndIndexBuffer();
		createRenderPass();
		createGraphicsPipeline();
		createFramebuffers();
		createCommandPool();
		createCommandBuffers();
		createSyncObjects();
	}

	void mainLoop()
	{
		cout << "mainLoop IN" << endl;
		// 窗口需要长期保持，设计游戏循环
		FTimer timer;
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents(); // 函数检查是否由事件输入
			drawFrame();

			if (timer.MarkSec() > 2.0)
			{
				glfwSetWindowShouldClose(window, GLFW_TRUE);
			}
		}
		vkDeviceWaitIdle(device);
	}

	void cleanup()
	{
		cout << "cleanUp IN" << endl;
		vkDestroyBuffer(device, vertexBuffer, nullptr);
		vkFreeMemory(device, vertexMemory, nullptr);
		vkDestroyBuffer(device, indexBuffer, nullptr);
		vkFreeMemory(device, indexMemory, nullptr);
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
			vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
			vkDestroyFence(device, inFlightFences[i], nullptr);
		}
		vkDestroyCommandPool(device, commandPool, nullptr);
		for (auto framebuffer : swapChainFramebuffers)
		{
			vkDestroyFramebuffer(device, framebuffer, nullptr);
		}
		vkDestroyPipeline(device, graphicsPipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyRenderPass(device, renderPass, nullptr);
		for (auto imageView : swapChainImageViews)
		{
			vkDestroyImageView(device, imageView, nullptr);
		}
		vkDestroySwapchainKHR(device, swapChain, nullptr);
		vkDestroyDevice(device, nullptr);
		if (enableValidationLayers)
		{
			destroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
		}
		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);

		glfwDestroyWindow(window); // 销毁窗口
		glfwTerminate();		   // 释放GLFW分配的内存
	}

	void createInstance()
	{
		cout << "createInstance IN" << endl;
		if (enableValidationLayers && !checkValidationLayerSupport())
		{
			throw std::runtime_error("un-available layer");
		}
		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; // 显示指定结构体的类型
		appInfo.pApplicationName = "Hello Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 2, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 2, 0);
		appInfo.apiVersion = VK_API_VERSION_1_2;

		// 将GLFW的窗口和Vulkan关联
		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		auto extensions = getRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
		if (enableValidationLayers)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();

			populateDebugMessengerCreateInfo(debugCreateInfo);
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
		}
		else
		{
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}

		if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create instance!");
		}
	}

	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
	{
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = debugCallback;
	}

	void setupDebugMessenger()
	{
		if (!enableValidationLayers)
		{
			return;
		}

		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		populateDebugMessengerCreateInfo(createInfo);

		if (createDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to set up debug messenger!");
		}
	}

	// 创建显示窗口
	void createSurface()
	{
		if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create window surface.");
		}
	}

	void pickPhysicalDevice()
	{
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr); // 第三个参数为空，则表示该函数为读取功能
		if (deviceCount == 0)
		{
			throw std::runtime_error("Failed to find GPU with Vulkan Supported.");
		}
		vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

		for (const auto& device : devices)
		{
			if (isDeviceSuitable(device))
			{
				physicalDevice = device;
				break;
			}
		}

		if (physicalDevice == VK_NULL_HANDLE)
		{
			throw std::runtime_error("Failed to find a suitable a GPU.");
		}
	}

	// 选择物理设备之后，并不能直接与之交互，还需要一个逻辑设备作为接口交互
	void createLogicalDevice()
	{
		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

		vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		set<uint32_t>					uniqueQueueFamilies = { indices.graphicsFamily, indices.presentFamily };

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

		if (enableValidationLayers)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}

		if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create logical device!");
		}

		vkGetDeviceQueue(device, indices.graphicsFamily, 0, &graphicsQueue);
		vkGetDeviceQueue(device, indices.presentFamily, 0, &presentQueue);
	}

	// 创建交换链
	void createSwapChain()
	{
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);
		VkSurfaceFormatKHR		surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
		VkPresentModeKHR		presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
		VkExtent2D				extent = chooseSwapExtent(swapChainSupport.capabilities);

		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1; // 交换链支持的最小图像个数+1数量类实现三倍缓存
		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
		{
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = surface;

		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;							 // 用于指定每个图像所包含的层次。除了VR场景外，一般都是1.
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // 指定我们在图像上的操作，此处我们将图像作为颜色来使用

		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
		uint32_t		   queueFamilyIndices[] = { indices.graphicsFamily, indices.presentFamily };

		// 判断图形绘制队列和呈现队列是不是同一个队列
		if (indices.graphicsFamily != indices.presentFamily)
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT; // 图像在同一时间可以被多个队列使用，协同模式
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // 如果相同，图像在同一时间只能被一个队列拥有，性能最佳
		}

		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // 忽略alpha通道
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;

		createInfo.oldSwapchain = VK_NULL_HANDLE;

		if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create swap chain!");
		}

		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
		swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

		swapChainImageFormat = surfaceFormat.format;
		swapChainExtent = extent;
	}

	void createImageViews()
	{
		swapChainImageViews.resize(swapChainImages.size());

		// 遍历创建ImageView---图像视图，该图像可以作为纹理使用，但是作为渲染目标，还需要帧缓冲对象
		for (size_t i = 0; i < swapChainImages.size(); i++)
		{
			VkImageViewCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = swapChainImages[i];
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;			 // viewType和fromat成员变量用于指定图像数据的解释方式
			createInfo.format = swapChainImageFormat;				 // 一维纹理、二维纹理、三维纹理或者立方体贴图
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY; // 用于图像颜色通道映射，保持默认即可
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // 用于指定图像的用途和图像哪一部分可以被访问
			createInfo.subresourceRange.baseMipLevel = 0;						// 此处图像用作渲染，没有细分级别，只存在一个图层
			createInfo.subresourceRange.levelCount = 1;							// 不是VR应用，可以保持默认
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;

			if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create image views!");
			}
		}
	}

	uint32_t getMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties)
	{
		VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);
		for (uint32_t i = 0; i < deviceMemoryProperties.memoryTypeCount; i++)
		{
			if ((typeBits & 1) == 1)
			{
				if ((deviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
				{
					return i;
				}
			}
			typeBits >>= 1;
		}
		return 0;
	}

	VkResult createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkBuffer* buffer, VkDeviceMemory* memory, VkDeviceSize size, void* data = nullptr)
	{
		// Create the buffer handle
		VkBufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCreateInfo.usage = usageFlags;
		bufferCreateInfo.size = size;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		vkCreateBuffer(device, &bufferCreateInfo, nullptr, buffer);

		// Create the memory backing up the buffer handle
		VkMemoryRequirements memReqs;
		VkMemoryAllocateInfo memAlloc{};
		memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		vkGetBufferMemoryRequirements(device, *buffer, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, memoryPropertyFlags);
		vkAllocateMemory(device, &memAlloc, nullptr, memory);

		if (data != nullptr)
		{
			void* mapped;
			vkMapMemory(device, *memory, 0, size, 0, &mapped);
			memcpy(mapped, data, size);
			vkUnmapMemory(device, *memory);
		}

		vkBindBufferMemory(device, *buffer, *memory, 0);
		return VK_SUCCESS;
	}

	void PrepareVertexAndIndexBuffer()
	{
		/*
				Prepare vertex and index buffers
		*/
		{
			std::vector<Vertex> vertices = {
				{ { 0.0f, 0.8f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
				{ { -0.8f, -0.8f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
				{ { 0.8f, -0.8f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
			};
			std::vector<uint32_t> indices = { 0, 1, 2 };

			const VkDeviceSize vertexBufferSize = vertices.size() * sizeof(Vertex);
			const VkDeviceSize indexBufferSize = indices.size() * sizeof(uint32_t);

			// Vertices
			createBuffer(
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				&vertexBuffer,
				&vertexMemory,
				vertexBufferSize,
				vertices.data());

			// Indices
			createBuffer(
				VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				&indexBuffer,
				&indexMemory,
				indexBufferSize,
				indices.data());
		}
	}

	void createRenderPass()
	{
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = swapChainImageFormat;					 // 颜色缓冲附着(VkImageView)的格式
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;				 // 指定采样数，没有使用多重采样，设置为1
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;			 // 指定渲染前对附着数据的操作
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;			 // 指定渲染后对附着数据的操作
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // 模板缓冲设置，不用关心
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;	   // 指定渲染开始时图像布局的方式
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // 指定渲染结束时图像布局的方式

		// 一个渲染流程包含多个子流程，子流程依赖上一流程处理后的帧缓冲内容
		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;

		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		// 创建渲染流程
		if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create render pass!");
		}
	}

	void createGraphicsPipeline()
	{
		const auto DataDir = FString::Format(TXT("{}/../../Test/{}_Data"), *GExecutablePath, *GExecutableName);
		const bool bDataDirExist = FGenericPlatformFile::Get(EPlatformFileType::Disk)->Exists(DataDir);
		HLVM_ENSURE_F(bDataDirExist, TXT("Data directory not exist {}"), *DataDir);

		auto vertShaderCode = readFile(FPath::Combine(DataDir, TXT("vert.spv")).string());
		auto fragShaderCode = readFile(FPath::Combine(DataDir, TXT("frag.spv")).string());
		auto geomShaderCode = readFile(FPath::Combine(DataDir, TXT("geom.spv")).string());

		VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
		VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);
		VkShaderModule geomShaderModule = createShaderModule(geomShaderCode);

		VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo geomShaderStageInfo{};
		geomShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		geomShaderStageInfo.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
		geomShaderStageInfo.module = geomShaderModule;
		geomShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

		// 第一步：顶点输入环节
		// Vertex bindings an attributes
		// Binding description
		VkVertexInputBindingDescription vInputBindDescription{};
		vInputBindDescription.binding = 0;
		vInputBindDescription.stride = sizeof(Vertex);
		vInputBindDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		std::vector<VkVertexInputBindingDescription> vertexInputBindings = { vInputBindDescription };

		// Attribute descriptions
		VkVertexInputAttributeDescription vInputAttribDescriptionPos{};
		vInputAttribDescriptionPos.location = 0;
		vInputAttribDescriptionPos.binding = 0;
		vInputAttribDescriptionPos.format = VK_FORMAT_R32G32B32_SFLOAT;
		vInputAttribDescriptionPos.offset = 0;

		VkVertexInputAttributeDescription vInputAttribDescriptionColor{};
		vInputAttribDescriptionColor.location = 1;
		vInputAttribDescriptionColor.binding = 0;
		vInputAttribDescriptionColor.format = VK_FORMAT_R32G32B32_SFLOAT;
		vInputAttribDescriptionColor.offset = sizeof(float) * 3;
		std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
			vInputAttribDescriptionPos,	  // Position
			vInputAttribDescriptionColor, // Color
		};

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
		vertexInputInfo.pVertexBindingDescriptions = vertexInputBindings.data();
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
		vertexInputInfo.pVertexAttributeDescriptions = vertexInputAttributes.data();

		// 第二步：顶点数据使用的几何图元
		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		// 第三步：视口和裁剪
		// 视口是输出渲染结果的帧缓冲区域，一般情况下是(0,0)到(width,height)
		// 视口将要显示的是交换链中的图像，所以尺寸应该与交换链中图像代销保持一致
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)swapChainExtent.width;
		viewport.height = (float)swapChainExtent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		// 裁剪定义哪一块区域的像素实际被存储在帧缓存中。任何位于裁剪范围之外都会被光栅化丢弃
		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = swapChainExtent;

		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;

		// 第四步：光栅化，将顶点构成的几何图元转换为片段，交由片段着色器着色
		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;

		// 第五步：多重采样，减少边缘锯齿
		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		// 第六步：深度测试---本程序不涉及

		// 第七步：颜色缓和，片段着色器返回的颜色需要和帧缓冲中对应像素的颜色进行混合
		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;

		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		colorBlending.blendConstants[0] = 0.0f;
		colorBlending.blendConstants[1] = 0.0f;
		colorBlending.blendConstants[2] = 0.0f;
		colorBlending.blendConstants[3] = 0.0f;

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 0;
		pipelineLayoutInfo.pushConstantRangeCount = 0;

		// 第八步：管线布局，
		if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create pipeline layout!");
		}

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = sizeof(shaderStages) / sizeof(shaderStages[0]);
		pipelineInfo.pStages = shaderStages;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.layout = pipelineLayout;
		pipelineInfo.renderPass = renderPass;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

		if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create graphics pipeline!");
		}
		//		vkDestroyShaderModule(device, fragShaderModule, nullptr);
		vkDestroyShaderModule(device, geomShaderModule, nullptr);
		vkDestroyShaderModule(device, vertShaderModule, nullptr);
	}

	// 创建帧缓冲
	void createFramebuffers()
	{
		swapChainFramebuffers.resize(swapChainImageViews.size());

		for (size_t i = 0; i < swapChainImageViews.size(); i++)
		{
			VkImageView attachments[] = {
				swapChainImageViews[i]
			};

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = renderPass;
			framebufferInfo.attachmentCount = 1;			 // 指定附着的个数
			framebufferInfo.pAttachments = attachments;		 // 渲染流程对象用于描述附着信息的pAttachment数组
			framebufferInfo.width = swapChainExtent.width;	 // width和height用于指定帧缓冲的大小
			framebufferInfo.height = swapChainExtent.height; // 交换链图像都是单层，layers设置为1
			framebufferInfo.layers = 1;

			if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create framebuffer!");
			}
		}
	}

	// 创建指令池
	void createCommandPool()
	{
		QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;

		if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create command pool!");
		}
	}

	// 创建指令缓存
	void createCommandBuffers()
	{
		commandBuffers.resize(swapChainFramebuffers.size());

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

		if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to allocate command buffers!");
		}

		for (size_t i = 0; i < commandBuffers.size(); i++)
		{
			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

			if (vkBeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to begin recording command buffer!");
			}

			VkRenderPassBeginInfo renderPassInfo{};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass = renderPass;
			renderPassInfo.framebuffer = swapChainFramebuffers[i];
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = swapChainExtent;

			VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
			renderPassInfo.clearValueCount = 1;
			renderPassInfo.pClearValues = &clearColor;

			vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

			// 该函数用于提交绘制操作到指令缓冲
			// 参数1：需要执行的指令缓冲对象
			// 参数2：顶点缓冲数量
			// 参数3：用于实例渲染，为1时表示不进行实例渲染
			// 参数4：用于定义着色器变量gl_VertexIndex的值
			// 参数5：用于定义着色器变量gl_InstanceIndex的值

			// Render scene
			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, &vertexBuffer, offsets);
			vkCmdBindIndexBuffer(commandBuffers[i], indexBuffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(commandBuffers[i], 3, 1, 0, 0, 0);
			//			vkCmdDraw(commandBuffers[i], 6, 1, 0, 0);

			vkCmdEndRenderPass(commandBuffers[i]);

			if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to record command buffer!");
			}
		}
	}

	void createSyncObjects()
	{
		imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
		imagesInFlight.resize(swapChainImages.size(), VK_NULL_HANDLE);

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS || vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS || vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create synchronization objects for a frame!");
			}
		}
	}

	void drawFrame()
	{
		vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

		uint32_t imageIndex;
		// 参数1：逻辑设备对象
		// 参数2：获取图像的交换链
		// 参数3：获取图像的超时时间
		// 参数4：指定信号量，用于同步
		// 参数5：默认值
		// 参数6：输出可用的交换链图像的索引
		vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

		if (imagesInFlight[imageIndex] != VK_NULL_HANDLE)
		{
			vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
		}
		imagesInFlight[imageIndex] = inFlightFences[currentFrame];

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore			 waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

		VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		vkResetFences(device, 1, &inFlightFences[currentFrame]);

		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to submit draw command buffer!");
		}

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = { swapChain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;

		presentInfo.pImageIndices = &imageIndex;

		vkQueuePresentKHR(presentQueue, &presentInfo);

		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	// 要将着色器字节码在管线上使用，还需要使用VkShaderModule转换
	VkShaderModule createShaderModule(const vector<char>& code)
	{
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

		std::cout << "shader code size: " << code.size() << "\n";
		//		for (int i = 0; i < code.size(); i++)
		//        {
		//		    std::cout << (int32_t)code[i] << ",";
		//        }
		//        std::cout << "\n";

		VkShaderModule shaderModule;
		if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create shader module!");
		}

		return shaderModule;
	}

	// 选择合适的表面格式
	// VkSurfaceFormatKHR包含了format和colorSpace，format指定颜色通道和存储类型，colorSpcae用来表示是否支持SRGB
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
	{
		for (const auto& availableFormat : availableFormats)
		{
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				return availableFormat;
			}
		}
		return availableFormats[0];
	}

	// 选择合适的呈现模式
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
	{
		VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;
		for (const auto& availablePresentMode : availablePresentModes)
		{
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			{ // 该模式是三倍缓冲，效果最好，但是显卡可能不支持
				return availablePresentMode;
			}
			else if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
			{
				bestMode = availablePresentMode;
			}
		}
		return bestMode;
	}

	// 选择交换范围：交换链中的图像分辨率
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
	{
		if (capabilities.currentExtent.width != UINT32_MAX)
		{
			return capabilities.currentExtent;
		}
		else
		{
			VkExtent2D actualExtent = { WIDTH, HEIGHT };
			actualExtent.width = max(capabilities.minImageExtent.width, min(capabilities.maxImageExtent.width, actualExtent.width));
			actualExtent.height = max(capabilities.minImageExtent.height, min(capabilities.maxImageExtent.height, actualExtent.height));
			return actualExtent;
		}
	}

	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device)
	{
		SwapChainSupportDetails details;

		// 与交换链相关的函数都需要device和surface这两个参数
		// 查询基础表面特性
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

		// 查询表面支持格式
		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
		if (formatCount != 0)
		{
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
		}

		// 查询表面支持呈现模式
		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

		if (presentModeCount != 0)
		{
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
		}

		return details;
	}

	// 为了选择合适的设备，我们需要或许详细的设备信息。包括但不限于：名称、类型和支持Vulkan的版本。
	bool isDeviceSuitable(VkPhysicalDevice device)
	{
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);

		// 纹理压缩、64为浮点、多窗口渲染是否支持，通过下面函数查询
		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		// 显卡支持集合着色器的判断条件
		bool isSupportSetShader = (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) && deviceFeatures.geometryShader;

		QueueFamilyIndices indices = findQueueFamilies(device);
		// 由于我们只需要显示三角形，所以不需要额外特性，直接返回true即可，以上代码作为测试

		bool extensionsSupported = checkDeviceExtensionSupport(device);

		bool swapChainAdequate = false;
		if (extensionsSupported)
		{
			SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}

		return indices.isComplete() && extensionsSupported && swapChainAdequate;
	}

	bool checkDeviceExtensionSupport(VkPhysicalDevice device)
	{
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

		for (const auto& extension : availableExtensions)
		{
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();
	}

	// 查询可用的图形队列和呈现队列
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device)
	{
		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
		vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		uint32_t index = 0;
		for (const auto& queueFamily : queueFamilies)
		{
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{ // 目前，我们只要寻找图形队列即可
				indices.graphicsFamily = index;
			}

			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, index, surface, &presentSupport);
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

	vector<const char*> getRequiredExtensions()
	{
		uint32_t	 glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
		vector<const char*> extentions(glfwExtensions, glfwExtensions + glfwExtensionCount);
		if (enableValidationLayers)
		{
			extentions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); // 添加调试扩展
		}
		return extentions;
	}

	bool checkValidationLayerSupport()
	{
		uint32_t layerCount = 0;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		vector<VkLayerProperties> availableLayers(layerCount);
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

	static vector<char> readFile(const string& filename)
	{
		// ate: 表示从文件末未开始读取
		// binary: 表示以二进制方式读取
		ifstream file(filename, std::ios::ate | std::ios::binary);

		if (!file.is_open())
		{
			throw std::runtime_error("Failed to open file!");
		}

		size_t		 fileSize = (size_t)file.tellg();
		vector<char> buffer(fileSize);

		file.seekg(0); // 跳转到文件头
		file.read(buffer.data(), fileSize);

		file.close();
		return buffer;
	}

	// 接受调试信息的回调函数
	// 参数1：诊断信息
	// 参数2：资源创建之类的信息
	// 参数3：警告信息
	// 参数4：不合法和可能造成崩溃的操作信息
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
	{
		if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		{
			std::cerr << "Error Message: " << pCallbackData->pMessage << std::endl;
		}
		else if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		{
			std::cerr << "Warning Message: " << pCallbackData->pMessage << std::endl;
		}
		else if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
		{
			std::cout << "Info Message: " << pCallbackData->pMessage << std::endl;
		} // ignore verbose logs
		return VK_FALSE;
	}
};
		#pragma clang diagnostic pop

RECORD_BOOL(test_GLFW3VulkanWindowRaw)
{
	VulkanLoader::LoadOnce();

	HelloTriangleApplication app;
	try
	{
		app.run();
	}
	catch (const std::exception& e)
	{
		cerr << "Error Code: " << e.what() << endl;
		return false;
	}
	return true;
}
	#endif

//	#include "../Private/RHI/_Deprecated/Vulkan/VulkanRHI.h"
//RECORD_BOOL(test_GLFW3VulkanWindow)
//{
//		// 1 . Create window
//	TSharedPtr<FGLFW3VulkanWindow> gWindow;
//	{
//		IWindow::Properties Properties;
//		Properties.Resizable = false;
//		Properties.Mode = IWindow::EDisplayMode::Windowed;
//		gWindow = MAKE_SHARED(FGLFW3VulkanWindow, Properties);
//	}
//	HLVM_LOG(LogTest, debug, TXT("FGLFW3VulkanWindow created!"));
//
//	// 2 . Create RHI
//	TSharedPtr<FVulkanRHI> gVulkanRHI;
//	{
//		FVulkanRHIInitializer Initializer;
//		Initializer.RequiredExtensions = { gWindow->GetRequiredExtensions() };
//		Initializer.CreateSurfaceFunc = [&gWindow](VkInstance Instance) { return gWindow->CreateSurface(Instance); };
//		Initializer.NativeWindowHandle = gWindow;
//		gVulkanRHI = MAKE_SHARED(FVulkanRHI, Initializer);
//	}
//	HLVM_LOG(LogTest, debug, TXT("FVulkanRHI created!"));
//	gVulkanRHI->Init();
//
//	{
//		// Create buffers
//		FRHIBufferRef VertexBuffer;
//		FRHIBufferRef IndexBuffer;
//		{
//			{
//				TVector<Vertex> vertices = {
//					{ { 0.0f, 0.8f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
//					{ { -0.8f, -0.8f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
//					{ { 0.8f, -0.8f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
//				};
//				const VkDeviceSize vertexBufferSize = vertices.size() * sizeof(Vertex);
//
//				FRHIBufferCreateInfo VertexBufferCreateDesc;
//				VertexBufferCreateDesc.DebugName = TXT("Vertex Test");
//				VertexBufferCreateDesc.UsageFlags = EBufferUsageFlag::Vertex;
//				VertexBufferCreateDesc.UsageFlags |= EBufferUsageFlag::TransferDestination;
//				VertexBufferCreateDesc.Size = vertexBufferSize;
//				VertexBufferCreateDesc.MemoryPropertyFlags = EMemoryPropertyFlag::DeviceLocal;
//				VertexBufferCreateDesc.MemoryPropertyFlags |= EMemoryPropertyFlag::HostVisible;
//				VertexBuffer = gVulkanRHI->CreateBuffer(VertexBufferCreateDesc);
//			}
//
//			{
//				TVector<uint32_t>  indices = { 0, 1, 2 };
//				const VkDeviceSize indexBufferSize = indices.size() * sizeof(uint32_t);
//
//				FRHIBufferCreateInfo IndexBufferCreateDesc;
//				IndexBufferCreateDesc.DebugName = TXT("Index Test");
//				(IndexBufferCreateDesc.UsageFlags = EBufferUsageFlag::Index) |= EBufferUsageFlag::TransferDestination;
//				IndexBufferCreateDesc.Size = indexBufferSize;
//				(IndexBufferCreateDesc.MemoryPropertyFlags = EMemoryPropertyFlag::DeviceLocal) |= EMemoryPropertyFlag::HostVisible;
//				IndexBuffer = gVulkanRHI->CreateBuffer(IndexBufferCreateDesc);
//			}
//		}
//
//		FRHIShaderRef VertShader;
//		FRHIShaderRef FragShader;
//		{
//			const auto DataDir = FString::Format(TXT("{}/../../Test/{}_Data"), *GExecutablePath, *GExecutableName);
//			const bool bDataDirExist = FGenericPlatformFile::Get(EPlatformFileType::Disk)->Exists(DataDir);
//			HLVM_ENSURE_F(bDataDirExist, TXT("Data directory not exist {}"), *DataDir);
//
//			{
//				auto			  vertShaderCode = FGenericPlatformFile::Get()->ReadContent(FPath::Combine(DataDir, TXT("vert.spv")));
//				FShaderCreateInfo vertShaderCreateInfo;
//				vertShaderCreateInfo.DebugName = TXT("vert.spv");
//				vertShaderCreateInfo.Code = MoveTemp(vertShaderCode);
//				vertShaderCreateInfo.Stage = EShaderStage::Vertex;
//				vertShaderCreateInfo.EntryPoints = { TXT("main") };
//				VertShader = gVulkanRHI->CreateShader(vertShaderCreateInfo);
//			}
//
//			{
//				auto			  fragShaderCode = FGenericPlatformFile::Get()->ReadContent(FPath::Combine(DataDir, TXT("frag.spv")));
//				FShaderCreateInfo fragShaderCreateInfo;
//				fragShaderCreateInfo.DebugName = TXT("frag.spv");
//				fragShaderCreateInfo.Code = MoveTemp(fragShaderCode);
//				fragShaderCreateInfo.Stage = EShaderStage::Pixel;
//				fragShaderCreateInfo.EntryPoints = { TXT("main") };
//				FragShader = gVulkanRHI->CreateShader(fragShaderCreateInfo);
//			}
//
//			{
//				auto			  geomShaderCode = FGenericPlatformFile::Get()->ReadContent(FPath::Combine(DataDir, TXT("geom.spv")));
//				FShaderCreateInfo geomShaderCreateInfo;
//				geomShaderCreateInfo.DebugName = TXT("geom.spv");
//				geomShaderCreateInfo.Code = MoveTemp(geomShaderCode);
//				geomShaderCreateInfo.Stage = EShaderStage::Geometry;
//				geomShaderCreateInfo.EntryPoints = { TXT("main") };
//				FRHIShaderRef GeomShader = gVulkanRHI->CreateShader(geomShaderCreateInfo);
//			}
//
//			VkPipelineShaderStageCreateInfo ShaderStages[] = {
//				S_C(FVulkanShaderRef, VertShader)->GetPipelineShaderStageCreateInfo(),
//				S_C(FVulkanShaderRef, FragShader)->GetPipelineShaderStageCreateInfo()
//			};
//		}
//
//		// 2.1 Call begin frame
//		FRHIViewportRef Viewport = gVulkanRHI->GetRHIViewport();
//		{
//			Viewport->BeginFrame(); // Call begin frame in order to acquire next back buffer
//			// 2.2 Get back buffer
//			FRHITextureRef BackBuffer = gVulkanRHI->GetRHIBackBuffer();
//
//			{
//				// Renderpass && Frame buffer
//				gVulkanRHI->RHIBeginRenderPass(FRHIRenderPassInfo{ BackBuffer, ERenderTargetActions::Clear_Store });
//				{
//					// Pipeline (PSO)
//				}
//				gVulkanRHI->RHIEndRenderPass();
//			}
//
//			Viewport->Present();
//		}
//	}
//	gVulkanRHI->Shutdown();
//
//	/*
//	 * initWindow();
//initVulkan();
//mainLoop();
//cleanup();
//	 */
//
//	/*
//	 * cout << "initVulkan IN" << endl;
//createInstance();
//setupDebugMessenger();
//createSurface();
//pickPhysicalDevice();
//createLogicalDevice();
//createSwapChain();
//createImageViews();
//PrepareVertexAndIndexBuffer();
//createRenderPass();
//createGraphicsPipeline();
//createFramebuffers();
//createCommandPool();
//createCommandBuffers();
//createSyncObjects();
//	 */
//	return true;
//}

#endif
