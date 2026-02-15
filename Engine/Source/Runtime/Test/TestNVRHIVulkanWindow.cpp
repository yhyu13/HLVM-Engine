/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 *
 * NVRHI-Style Vulkan Triangle Renderer
 *
 * This implementation translates raw Vulkan C API code to modern C++ Vulkan-HPP
 * with NVRHI-style abstractions. Key design principles:
 *
 * 1. RAII Resource Management: Uses vk::UniqueHandle types for automatic cleanup
 * 2. Type Safety: Strongly-typed enums and flags instead of raw VK_ constants
 * 3. Method Chaining: Fluent API for descriptor configuration
 * 4. Modern C++: std::optional, std::vector, structured bindings
 * 5. NVRHI Compatibility: Proper device description for NVRHI integration
 */

#include "Test.h"
#include <optional>
#include <set>

DECLARE_LOG_CATEGORY(LogTest)

#include "Window/WindowDefinition.h"
#if HLVM_WINDOW_USE_VULKAN
	#include "Window/Vulkan/GLFW3VulkanWindow.h"

// First load vulkan hpp with dynamic dispatch (aka VK_NO_PROTOTYPE)
	#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
	#include <vulkan/vulkan.hpp>
static_assert(VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1, "VULKAN_HPP_DISPATCH_LOADER_DYNAMIC must be defined to 1");
	#if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
	#endif

	#if 1 // Test Vulkan triangle program with NVRHI-style Vulkan-HPP API
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

// =============================================================================
// CONFIGURATION CONSTANTS
// =============================================================================

const uint32_t	   WIDTH = 800;
const uint32_t	   HEIGHT = 600;
static const char* WINDOW_TITLE = "NVRHI-Style Vulkan Test";

// NVRHI-style: Use 2 frames in flight for double buffering (GPU pipelining)
const int MAX_FRAMES_IN_FLIGHT = 2;

// Validation layer configuration - enabled in debug builds
const vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

// Device extensions required for swapchain and presentation
const vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

		#if VULKAN_ENABLE_VALIDATION_LAYERS
const bool enableValidationLayers = true;
		#else
const bool enableValidationLayers = false;
		#endif

// =============================================================================
// DATA STRUCTURES
// =============================================================================

/**
 * QueueFamilyIndices - NVRHI-style queue family discovery
 * Uses std::optional instead of sentinel values for type safety
 */
struct QueueFamilyIndices
{
	// Graphics queue family index (optional for type safety)
	std::optional<uint32_t> graphicsFamily;
	// Presentation queue family index (may differ from graphics on some hardware)
	std::optional<uint32_t> presentFamily;

	// Check if all required queue families are available
	[[nodiscard]] bool isComplete() const
	{
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

/**
 * SwapChainSupportDetails - Surface capabilities query result
 * NVRHI-style: Encapsulates all swapchain-related surface properties
 */
struct SwapChainSupportDetails
{
	vk::SurfaceCapabilitiesKHR	 capabilities; // Min/max images, extents, transforms
	vector<vk::SurfaceFormatKHR> formats;	   // Available pixel formats and color spaces
	vector<vk::PresentModeKHR>	 presentModes; // FIFO, Mailbox, Immediate, etc.
};

/**
 * Vertex - Interleaved vertex format (position + color)
 * Matches the shader layout: location 0 = position, location 1 = color
 */
struct Vertex
{
	float position[3]; // XYZ coordinates
	float color[3];	   // RGB values
};

/**
 * FrameData - NVRHI-style per-frame flight data
 * Encapsulates all synchronization primitives for a single frame in flight
 */
struct FrameData
{
	vk::UniqueSemaphore imageAvailableSemaphore; // GPU: Swapchain image ready signal
	vk::UniqueSemaphore renderFinishedSemaphore; // GPU: Rendering complete signal
	vk::UniqueFence		inFlightFence;			 // CPU: Frame completion fence
};

// =============================================================================
// NVRHI-STYLE VULKAN APPLICATION CLASS
// =============================================================================

class NVRHIStyleTriangleApp
{
public:
	/**
	 * Main execution flow - mirrors NVRHI initialization pattern
	 */
	void run()
	{
		initWindow();
		initVulkan();
		mainLoop();
		// NVRHI-style: RAII handles automatic cleanup, no explicit cleanup() needed
	}

private:
	// -------------------------------------------------------------------------
	// WINDOW SYSTEM (GLFW)
	// -------------------------------------------------------------------------
	GLFWwindow* window = nullptr;

	// -------------------------------------------------------------------------
	// VULKAN INSTANCE & SURFACE (Instance-level resources)
	// -------------------------------------------------------------------------
	vk::UniqueInstance				 instance;		 // Vulkan instance (RAII)
	vk::UniqueDebugUtilsMessengerEXT debugMessenger; // Validation layer messenger
	vk::UniqueSurfaceKHR			 surface;		 // Window surface for presentation

	// -------------------------------------------------------------------------
	// PHYSICAL & LOGICAL DEVICE (GPU resources)
	// -------------------------------------------------------------------------
	vk::PhysicalDevice physicalDevice; // Selected GPU handle (non-owning)
	vk::UniqueDevice   device;		   // Logical device (RAII)

	// NVRHI-style: Separate queue handles for graphics and presentation
	// May be the same queue on most hardware, but treated separately for portability
	vk::Queue graphicsQueue; // Graphics command submission
	vk::Queue presentQueue;	 // Presentation command submission

	// -------------------------------------------------------------------------
	// SWAPCHAIN & RENDER TARGETS
	// -------------------------------------------------------------------------
	vk::UniqueSwapchainKHR swapChain;			 // Swapchain handle (RAII)
	vector<vk::Image>	   swapChainImages;		 // Swapchain images (non-owning, managed by swapchain)
	vk::Format			   swapChainImageFormat; // Selected surface format
	vk::Extent2D		   swapChainExtent;		 // Selected swapchain resolution

	// Image views - RAII handles for swapchain image interpretation
	vector<vk::UniqueImageView> swapChainImageViews;

	// Framebuffers - RAII handles binding render passes to image views
	vector<vk::UniqueFramebuffer> swapChainFramebuffers;

	// -------------------------------------------------------------------------
	// GEOMETRY RESOURCES (Vertex/Index buffers)
	// -------------------------------------------------------------------------
	vk::UniqueBuffer	   vertexBuffer; // GPU vertex data buffer
	vk::UniqueDeviceMemory vertexMemory; // Dedicated GPU memory for vertices
	vk::UniqueBuffer	   indexBuffer;	 // GPU index data buffer
	vk::UniqueDeviceMemory indexMemory;	 // Dedicated GPU memory for indices

	// -------------------------------------------------------------------------
	// RENDER PIPELINE STATE
	// -------------------------------------------------------------------------
	vk::UniqueRenderPass	 renderPass;	   // Render pass definition (load/store ops, layouts)
	vk::UniquePipelineLayout pipelineLayout;   // Pipeline resource layout (empty for this demo)
	vk::UniquePipeline		 graphicsPipeline; // Compiled graphics pipeline state object (PSO)

	// -------------------------------------------------------------------------
	// COMMAND INFRASTRUCTURE
	// -------------------------------------------------------------------------
	vk::UniqueCommandPool			commandPool;	// Command buffer memory pool
	vector<vk::UniqueCommandBuffer> commandBuffers; // Per-swapchain-image command buffers

	// -------------------------------------------------------------------------
	// SYNCHRONIZATION (NVRHI-style frame flight management)
	// -------------------------------------------------------------------------
	vector<FrameData> frames;			// Per-frame flight data (size = MAX_FRAMES_IN_FLIGHT)
	vector<vk::Fence> imagesInFlight;	// Per-swapchain-image fence tracking (non-owning)
	size_t			  currentFrame = 0; // Current frame index (rotates 0, 1, ..., MAX-1, 0...)

	// =============================================================================
	// INITIALIZATION PHASE
	// =============================================================================

	/**
	 * Initialize GLFW window with Vulkan compatibility
	 * NVRHI-style: Window creation is separate from Vulkan initialization
	 */
	void initWindow()
	{
		cout << "initWindow IN" << endl;
		glfwInit();

		// Disable OpenGL context creation - we're using Vulkan
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		// Fixed size window - avoids swapchain recreation complexity
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		window = glfwCreateWindow(WIDTH, HEIGHT, WINDOW_TITLE, nullptr, nullptr);
	}

	/**
	 * Main Vulkan initialization sequence
	 * Follows NVRHI device creation pattern: Instance -> Surface -> PhysicalDevice -> Device -> Resources
	 */
	void initVulkan()
	{
		cout << "initVulkan IN" << endl;

		// Phase 1: Instance creation (global Vulkan context)
		createInstance();

		// Phase 2: Debug setup (validation layers)
		setupDebugMessenger();

		// Phase 3: Surface creation (window system integration)
		createSurface();

		// Phase 4: Physical device selection (GPU picking)
		pickPhysicalDevice();

		// Phase 5: Logical device creation (driver interface)
		createLogicalDevice();

		// Phase 6: Swapchain creation (display output setup)
		createSwapChain();
		createImageViews();

		// Phase 7: Geometry preparation (vertex/index data upload)
		prepareGeometry();

		// Phase 8: Render pipeline creation (shaders, state, PSO)
		createRenderPass();
		createGraphicsPipeline();
		createFramebuffers();

		// Phase 9: Command infrastructure (recording draw commands)
		createCommandPool();
		createCommandBuffers();

		// Phase 10: Synchronization primitives (CPU-GPU coordination)
		createSyncObjects();
	}

	// =============================================================================
	// INSTANCE CREATION
	// =============================================================================

	/**
	 * Create Vulkan instance with required extensions and validation layers
	 * NVRHI-style: Uses vk::UniqueInstance for automatic destruction
	 */
	void createInstance()
	{
		cout << "createInstance IN" << endl;

		if (enableValidationLayers && !checkValidationLayerSupport())
		{
			throw std::runtime_error("Validation layers requested but not available");
		}

		// Application metadata for driver optimization
		vk::ApplicationInfo appInfo(
			"NVRHI Triangle",		  // Application name
			VK_MAKE_VERSION(1, 0, 0), // Application version
			"NVRHI Engine",			  // Engine name
			VK_MAKE_VERSION(1, 0, 0), // Engine version
			VK_API_VERSION_1_2		  // Vulkan API version (1.2 for advanced features)
		);

		// Collect required instance extensions (GLFW requirements + debug utils)
		auto extensions = getRequiredExtensions();

		// Instance creation info with validation layer support
		vk::InstanceCreateInfo createInfo;
		createInfo.setPApplicationInfo(&appInfo)
			.setEnabledExtensionCount(static_cast<uint32_t>(extensions.size()))
			.setPpEnabledExtensionNames(extensions.data());

		// Debug messenger setup for instance creation/destruction validation
		vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo;
		if (enableValidationLayers)
		{
			createInfo.setEnabledLayerCount(static_cast<uint32_t>(validationLayers.size()))
				.setPpEnabledLayerNames(validationLayers.data());

			populateDebugMessengerCreateInfo(debugCreateInfo);
			createInfo.setPNext(&debugCreateInfo); // Chain debug info to instance creation
		}

		// Create instance with RAII handle - automatically destroyed when 'instance' goes out of scope
		// Fixed: vk::createInstanceUnique returns UniqueHandle directly, not a pair
		instance = vk::createInstanceUnique(createInfo);
		if (!instance)
		{
			throw std::runtime_error("Failed to create Vulkan instance");
		}
		#if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
		// initialize function pointers for instance
		VULKAN_HPP_DEFAULT_DISPATCHER.init(instance.get());
		#endif

		// NVRHI-style: Store instance handle for later use (device creation, surface, etc.)
		// The UniqueInstance manages lifetime, but we can access the raw handle via .get()
	}

	/**
	 * Configure debug messenger creation parameters
	 * Captures validation errors, warnings, and performance info
	 */
	void populateDebugMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT& createInfo)
	{
		createInfo.setMessageSeverity(
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | // Detailed diagnostics
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | // Potential issues
			vk::DebugUtilsMessageSeverityFlagBitsEXT::eError	 // Critical errors
		);
		createInfo.setMessageType(
			vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |	// General spec violations
			vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | // Validation layer errors
			vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance	// Performance warnings
		);
		createInfo.setPfnUserCallback(debugCallback);
	}

	/**
	 * Setup validation layer debug messenger
	 * Uses extension function pointer (not part of core Vulkan 1.0)
	 */
	void setupDebugMessenger()
	{
		if (!enableValidationLayers)
			return;

		vk::DebugUtilsMessengerCreateInfoEXT createInfo;
		populateDebugMessengerCreateInfo(createInfo);

		// Load extension function manually (Vulkan-HPP doesn't auto-load all extensions)
		auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
			instance->getProcAddr("vkCreateDebugUtilsMessengerEXT"));

		if (!func)
		{
			throw std::runtime_error("Failed to load debug messenger creation function");
		}

		VkDebugUtilsMessengerEXT		   rawMessenger;
		VkDebugUtilsMessengerCreateInfoEXT rawCreateInfo = static_cast<VkDebugUtilsMessengerCreateInfoEXT>(createInfo);

		if (func(*instance, &rawCreateInfo, nullptr, &rawMessenger) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to set up debug messenger");
		}

		// Wrap raw handle in UniqueDebugUtilsMessengerEXT for RAII cleanup
		// Fixed: Use vk::detail::ObjectDestroy instead of vk::ObjectDestroy
		debugMessenger = vk::UniqueDebugUtilsMessengerEXT(
			vk::DebugUtilsMessengerEXT(rawMessenger),
			vk::detail::ObjectDestroy<vk::Instance, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>(*instance));
	}

	// =============================================================================
	// SURFACE CREATION (Window System Integration)
	// =============================================================================

	/**
	 * Create Vulkan surface bound to GLFW window
	 * NVRHI-style: Surface is instance-level resource, created before device selection
	 */
	void createSurface()
	{
		// GLFW creates platform-appropriate surface (Win32, X11, Wayland, etc.)
		VkSurfaceKHR rawSurface;
		if (glfwCreateWindowSurface(*instance, window, nullptr, &rawSurface) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create window surface");
		}

		// Wrap in UniqueSurfaceKHR for automatic cleanup
		surface = vk::UniqueSurfaceKHR(
			vk::SurfaceKHR(rawSurface),
			vk::detail::ObjectDestroy<vk::Instance, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>(*instance));
	}

	// =============================================================================
	// PHYSICAL DEVICE SELECTION
	// =============================================================================

	/**
	 * Select optimal GPU for rendering
	 * NVRHI-style: Enumerate all devices, score them, pick best candidate
	 */
	void pickPhysicalDevice()
	{
		// Enumerate all physical devices (GPUs) available on system
		auto devices = instance->enumeratePhysicalDevices();
		if (devices.empty())
		{
			throw std::runtime_error("No Vulkan-compatible GPUs found");
		}

		// Score and select best device (discrete GPU preferred)
		for (const auto& device : devices)
		{
			if (isDeviceSuitable(device))
			{
				physicalDevice = device;
				break;
			}
		}

		if (!physicalDevice)
		{
			throw std::runtime_error("No suitable GPU found");
		}

		// Log selected device info
		auto props = physicalDevice.getProperties();
		cout << "Selected GPU: " << props.deviceName << endl;
	}

	/**
	 * Check if device meets application requirements
	 * Requirements: Graphics queue, presentation support, swapchain extension
	 */
	bool isDeviceSuitable(vk::PhysicalDevice device)
	{
		// Query queue families (graphics + presentation)
		QueueFamilyIndices indices = findQueueFamilies(device);

		// Check device extension support (swapchain required for presentation)
		bool extensionsSupported = checkDeviceExtensionSupport(device);

		// Verify swapchain capabilities are adequate (formats and present modes available)
		bool swapChainAdequate = false;
		if (extensionsSupported)
		{
			SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}

		return indices.isComplete() && extensionsSupported && swapChainAdequate;
	}

	/**
	 * Find queue families supporting required operations
	 * NVRHI-style: Separate graphics and present queues (may be same index)
	 */
	QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device)
	{
		QueueFamilyIndices indices;

		auto queueFamilies = device.getQueueFamilyProperties();

		for (uint32_t i = 0; i < queueFamilies.size(); i++)
		{
			// Check for graphics support
			if (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics)
			{
				indices.graphicsFamily = i;
			}

			// Check for presentation support (surface compatibility)
			vk::Bool32 presentSupport = device.getSurfaceSupportKHR(i, *surface);
			if (presentSupport)
			{
				indices.presentFamily = i;
			}

			if (indices.isComplete())
				break;
		}

		return indices;
	}

	/**
	 * Verify all required device extensions are available
	 */
	bool checkDeviceExtensionSupport(vk::PhysicalDevice device)
	{
		auto availableExtensions = device.enumerateDeviceExtensionProperties();

		std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

		for (const auto& extension : availableExtensions)
		{
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();
	}

	// =============================================================================
	// LOGICAL DEVICE CREATION
	// =============================================================================

	/**
	 * Create logical device (driver interface) with required queues
	 * NVRHI-style: This is where NVRHI would wrap the device for high-level usage
	 */
	void createLogicalDevice()
	{
		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

		// Create queue for each unique family (may be same for graphics/present)
		std::set<uint32_t> uniqueQueueFamilies = {
			indices.graphicsFamily.value(),
			indices.presentFamily.value()
		};

		float							  queuePriority = 1.0f;
		vector<vk::DeviceQueueCreateInfo> queueCreateInfos;

		for (uint32_t queueFamily : uniqueQueueFamilies)
		{
			vk::DeviceQueueCreateInfo queueCreateInfo(
				vk::DeviceQueueCreateFlags(),
				queueFamily,
				1, // Queue count
				&queuePriority);
			queueCreateInfos.push_back(queueCreateInfo);
		}

		// Request geometry shader support (for this demo's geometry shader stage)
		vk::PhysicalDeviceFeatures deviceFeatures;
		deviceFeatures.setGeometryShader(true);

		// Device creation info
		vk::DeviceCreateInfo createInfo;
		createInfo.setQueueCreateInfoCount(static_cast<uint32_t>(queueCreateInfos.size()))
			.setPQueueCreateInfos(queueCreateInfos.data())
			.setPEnabledFeatures(&deviceFeatures)
			.setEnabledExtensionCount(static_cast<uint32_t>(deviceExtensions.size()))
			.setPpEnabledExtensionNames(deviceExtensions.data());

		// Create device with RAII handle
		device = physicalDevice.createDeviceUnique(createInfo);
		if (!device)
		{
			throw std::runtime_error("Failed to create logical device");
		}
		#if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
		// initialize function pointers for device
		VULKAN_HPP_DEFAULT_DISPATCHER.init(device.get());
		#endif
		// Retrieve queue handles from created device
		graphicsQueue = device->getQueue(indices.graphicsFamily.value(), 0);
		presentQueue = device->getQueue(indices.presentFamily.value(), 0);

		// NVRHI INTEGRATION POINT:
		// At this point, you would create an nvrhi::vulkan::DeviceDesc and call nvrhi::vulkan::createDevice()
		// to wrap this Vulkan device with NVRHI's high-level abstraction
		/*
		nvrhi::vulkan::DeviceDesc deviceDesc;
		deviceDesc.errorCB = &DefaultMessageCallback::GetInstance();
		deviceDesc.instance = *instance;
		deviceDesc.physicalDevice = physicalDevice;
		deviceDesc.device = *device;
		deviceDesc.graphicsQueue = graphicsQueue;
		deviceDesc.graphicsQueueIndex = indices.graphicsFamily.value();
		deviceDesc.deviceExtensions = deviceExtensions.data();
		deviceDesc.numDeviceExtensions = deviceExtensions.size();
		nvrhi::DeviceHandle nvrhiDevice = nvrhi::vulkan::createDevice(deviceDesc);
		*/
	}

	// =============================================================================
	// SWAPCHAIN CREATION
	// =============================================================================

	/**
	 * Create swapchain for presentation
	 * NVRHI-style: Triple buffering if available, otherwise double buffer
	 */
	void createSwapChain()
	{
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

		// Select best available settings
		vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
		vk::PresentModeKHR	 presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
		vk::Extent2D		 extent = chooseSwapExtent(swapChainSupport.capabilities);

		// Request one more than minimum for triple buffering (if available)
		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
		{
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}

		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
		uint32_t		   queueFamilyIndices[] = {
			  indices.graphicsFamily.value(),
			  indices.presentFamily.value()
		};

		// Swapchain creation info
		vk::SwapchainCreateInfoKHR createInfo;
		createInfo.setSurface(*surface)
			.setMinImageCount(imageCount)
			.setImageFormat(surfaceFormat.format)
			.setImageColorSpace(surfaceFormat.colorSpace)
			.setImageExtent(extent)
			.setImageArrayLayers(1)
			.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);

		// Handle queue family sharing (concurrent if different, exclusive if same)
		if (indices.graphicsFamily != indices.presentFamily)
		{
			createInfo.setImageSharingMode(vk::SharingMode::eConcurrent)
				.setQueueFamilyIndexCount(2)
				.setPQueueFamilyIndices(queueFamilyIndices);
		}
		else
		{
			createInfo.setImageSharingMode(vk::SharingMode::eExclusive);
		}

		createInfo.setPreTransform(swapChainSupport.capabilities.currentTransform)
			.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
			.setPresentMode(presentMode)
			.setClipped(true)
			.setOldSwapchain(nullptr);

		// Create swapchain
		swapChain = device->createSwapchainKHRUnique(createInfo);
		if (!swapChain)
		{
			throw std::runtime_error("Failed to create swapchain");
		}

		// Retrieve swapchain images (non-owning, managed by swapchain)
		swapChainImages = device->getSwapchainImagesKHR(*swapChain);
		swapChainImageFormat = surfaceFormat.format;
		swapChainExtent = extent;
	}

	/**
	 * Select optimal surface format (prefer SRGB for better color accuracy)
	 */
	vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const vector<vk::SurfaceFormatKHR>& availableFormats)
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

	/**
	 * Select optimal presentation mode (prefer Mailbox for low latency, no tearing)
	 */
	vk::PresentModeKHR chooseSwapPresentMode(const vector<vk::PresentModeKHR>& availablePresentModes)
	{
		for (const auto& availablePresentMode : availablePresentModes)
		{
			if (availablePresentMode == vk::PresentModeKHR::eMailbox)
			{
				return availablePresentMode; // Triple buffering, lowest latency
			}
		}
		return vk::PresentModeKHR::eFifo; // Guaranteed to be available (VSync)
	}

	/**
	 * Select swapchain resolution (match window size, clamp to capabilities)
	 */
	vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities)
	{
		if (capabilities.currentExtent.width != UINT32_MAX)
		{
			return capabilities.currentExtent;
		}
		else
		{
			vk::Extent2D actualExtent = { WIDTH, HEIGHT };
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
	}

	/**
	 * Query surface capabilities from physical device
	 */
	SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice device)
	{
		SwapChainSupportDetails details;
		details.capabilities = device.getSurfaceCapabilitiesKHR(*surface);
		details.formats = device.getSurfaceFormatsKHR(*surface);
		details.presentModes = device.getSurfacePresentModesKHR(*surface);
		return details;
	}

	// =============================================================================
	// IMAGE VIEW CREATION
	// =============================================================================

	/**
	 * Create image views for swapchain images (interpretation descriptors)
	 * NVRHI-style: Image views define how to interpret image data (2D, color, etc.)
	 */
	void createImageViews()
	{
		swapChainImageViews.resize(swapChainImages.size());

		for (size_t i = 0; i < swapChainImages.size(); i++)
		{
			vk::ImageViewCreateInfo createInfo;
			createInfo.setImage(swapChainImages[i])
				.setViewType(vk::ImageViewType::e2D)
				.setFormat(swapChainImageFormat)
				.setComponents(vk::ComponentMapping(
					vk::ComponentSwizzle::eIdentity,
					vk::ComponentSwizzle::eIdentity,
					vk::ComponentSwizzle::eIdentity,
					vk::ComponentSwizzle::eIdentity))
				.setSubresourceRange(vk::ImageSubresourceRange(
					vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

			swapChainImageViews[i] = device->createImageViewUnique(createInfo);
			if (!swapChainImageViews[i])
			{
				throw std::runtime_error("Failed to create image view");
			}
		}
	}

	// =============================================================================
	// GEOMETRY PREPARATION
	// =============================================================================

	/**
	 * Create and upload vertex and index buffers
	 * NVRHI-style: Host-visible device memory for simplicity (staging for GPU-only is production pattern)
	 */
	void prepareGeometry()
	{
		// Triangle vertices with per-vertex colors
		vector<Vertex> vertices = {
			{ { 0.0f, 0.8f, 0.0f }, { 1.0f, 0.0f, 0.0f } },	  // Top - Red
			{ { -0.8f, -0.8f, 0.0f }, { 0.0f, 1.0f, 0.0f } }, // Bottom Left - Green
			{ { 0.8f, -0.8f, 0.0f }, { 0.0f, 0.0f, 1.0f } }	  // Bottom Right - Blue
		};
		vector<uint32_t> indices = { 0, 1, 2 };

		vk::DeviceSize vertexBufferSize = sizeof(vertices[0]) * vertices.size();
		vk::DeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();

		// Create vertex buffer with host-visible memory (simplified approach)
		createBuffer(
			vertexBufferSize,
			vk::BufferUsageFlagBits::eVertexBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			vertexBuffer,
			vertexMemory);

		// Upload vertex data
		void* data = device->mapMemory(*vertexMemory, 0, vertexBufferSize);
		memcpy(data, vertices.data(), static_cast<size_t>(vertexBufferSize));
		device->unmapMemory(*vertexMemory);

		// Create index buffer
		createBuffer(
			indexBufferSize,
			vk::BufferUsageFlagBits::eIndexBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			indexBuffer,
			indexMemory);

		// Upload index data
		data = device->mapMemory(*indexMemory, 0, indexBufferSize);
		memcpy(data, indices.data(), static_cast<size_t>(indexBufferSize));
		device->unmapMemory(*indexMemory);
	}

	/**
	 * Helper: Create buffer with specified usage and memory properties
	 * NVRHI-style: Encapsulates buffer creation and memory allocation
	 */
	void createBuffer(
		vk::DeviceSize			size,
		vk::BufferUsageFlags	usage,
		vk::MemoryPropertyFlags properties,
		vk::UniqueBuffer&		buffer,
		vk::UniqueDeviceMemory& bufferMemory)
	{
		// Create buffer handle
		vk::BufferCreateInfo bufferInfo;
		bufferInfo.setSize(size)
			.setUsage(usage)
			.setSharingMode(vk::SharingMode::eExclusive);

		auto uniqueBuffer = device->createBufferUnique(bufferInfo);
		if (!uniqueBuffer)
		{
			throw std::runtime_error("Failed to create buffer");
		}
		buffer = std::move(uniqueBuffer);

		// Query memory requirements
		vk::MemoryRequirements memRequirements = device->getBufferMemoryRequirements(*buffer);

		// Allocate memory
		vk::MemoryAllocateInfo allocInfo;
		allocInfo.setAllocationSize(memRequirements.size)
			.setMemoryTypeIndex(findMemoryType(memRequirements.memoryTypeBits, properties));

		bufferMemory = device->allocateMemoryUnique(allocInfo);
		if (!bufferMemory)
		{
			throw std::runtime_error("Failed to allocate buffer memory");
		}

		// Bind memory to buffer
		device->bindBufferMemory(*buffer, *bufferMemory, 0);
	}

	/**
	 * Find memory type index matching requirements
	 */
	uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
	{
		vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
		{
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}
		throw std::runtime_error("Failed to find suitable memory type");
	}

	// =============================================================================
	// RENDER PASS & PIPELINE
	// =============================================================================

	/**
	 * Create render pass (attachment descriptions and subpass dependencies)
	 * NVRHI-style: Render pass defines load/store operations and image layout transitions
	 */
	void createRenderPass()
	{
		// Color attachment description
		vk::AttachmentDescription colorAttachment;
		colorAttachment.setFormat(swapChainImageFormat)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(vk::AttachmentLoadOp::eClear)   // Clear on load
			.setStoreOp(vk::AttachmentStoreOp::eStore) // Store results
			.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
			.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
			.setInitialLayout(vk::ImageLayout::eUndefined)	  // Don't care about initial layout
			.setFinalLayout(vk::ImageLayout::ePresentSrcKHR); // Ready for presentation

		// Attachment reference for subpass
		vk::AttachmentReference colorAttachmentRef(
			0,										 // Attachment index
			vk::ImageLayout::eColorAttachmentOptimal // Layout during subpass
		);

		// Subpass description
		vk::SubpassDescription subpass;
		subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
			.setColorAttachmentCount(1)
			.setPColorAttachments(&colorAttachmentRef);

		// Subpass dependency (external -> subpass)
		// Ensures swapchain image is available before writing
		vk::SubpassDependency dependency;
		dependency.setSrcSubpass(VK_SUBPASS_EXTERNAL)
			.setDstSubpass(0)
			.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setSrcAccessMask(vk::AccessFlagBits::eNone)
			.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

		// Create render pass
		vk::RenderPassCreateInfo renderPassInfo;
		renderPassInfo.setAttachmentCount(1)
			.setPAttachments(&colorAttachment)
			.setSubpassCount(1)
			.setPSubpasses(&subpass)
			.setDependencyCount(1)
			.setPDependencies(&dependency);

		renderPass = device->createRenderPassUnique(renderPassInfo);
		if (!renderPass)
		{
			throw std::runtime_error("Failed to create render pass");
		}
	}

	/**
	 * Create graphics pipeline (PSO - Pipeline State Object)
	 * NVRHI-style: Immutable pipeline containing all rendering state
	 */
	void createGraphicsPipeline()
	{
		const auto DataDir = FString::Format(TXT("{}/../../Test/{}_Data"), *GExecutablePath, *GExecutableName);
		const bool bDataDirExist = FGenericPlatformFile::Get(EPlatformFileType::Disk)->Exists(DataDir);
		HLVM_ENSURE_F(bDataDirExist, TXT("Data directory not exist {}"), *DataDir);

		// Load SPIR-V shader bytecode
		auto vertShaderCode = readFile(FPath::Combine(DataDir, TXT("vert.spv")).string());
		auto fragShaderCode = readFile(FPath::Combine(DataDir, TXT("frag.spv")).string());

		// Create shader modules (temporary, destroyed after pipeline creation)
		vk::UniqueShaderModule vertShaderModule = createShaderModule(vertShaderCode);
		vk::UniqueShaderModule fragShaderModule = createShaderModule(fragShaderCode);

		// Shader stage creation info
		vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
		vertShaderStageInfo.setStage(vk::ShaderStageFlagBits::eVertex)
			.setModule(*vertShaderModule)
			.setPName("main");

		vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
		fragShaderStageInfo.setStage(vk::ShaderStageFlagBits::eFragment)
			.setModule(*fragShaderModule)
			.setPName("main");

		vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

		// Vertex input state (matches Vertex struct layout)
		vk::VertexInputBindingDescription bindingDescription(
			0,				// Binding
			sizeof(Vertex), // Stride
			vk::VertexInputRate::eVertex);

		// Position attribute (location = 0)
		vk::VertexInputAttributeDescription positionAttribute(
			0,							  // Location
			0,							  // Binding
			vk::Format::eR32G32B32Sfloat, // Format: vec3
			offsetof(Vertex, position)	  // Offset
		);

		// Color attribute (location = 1)
		vk::VertexInputAttributeDescription colorAttribute(
			1,							  // Location
			0,							  // Binding
			vk::Format::eR32G32B32Sfloat, // Format: vec3
			offsetof(Vertex, color)		  // Offset
		);

		vk::VertexInputAttributeDescription attributeDescriptions[] = { positionAttribute, colorAttribute };

		vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
		vertexInputInfo.setVertexBindingDescriptionCount(1)
			.setPVertexBindingDescriptions(&bindingDescription)
			.setVertexAttributeDescriptionCount(2)
			.setPVertexAttributeDescriptions(attributeDescriptions);

		// Input assembly (triangle list)
		vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
		inputAssembly.setTopology(vk::PrimitiveTopology::eTriangleList)
			.setPrimitiveRestartEnable(false);

		// Viewport and scissor (dynamic state allows changing at draw time)
		vk::Viewport viewport(
			0.0f, 0.0f,									// x, y
			static_cast<float>(swapChainExtent.width),	// width
			static_cast<float>(swapChainExtent.height), // height
			0.0f, 1.0f									// minDepth, maxDepth
		);

		vk::Rect2D scissor(
			{ 0, 0 },		// offset
			swapChainExtent // extent
		);

		vk::PipelineViewportStateCreateInfo viewportState;
		viewportState.setViewportCount(1)
			.setPViewports(&viewport)
			.setScissorCount(1)
			.setPScissors(&scissor);

		// Rasterizer state
		vk::PipelineRasterizationStateCreateInfo rasterizer;
		rasterizer.setDepthClampEnable(false)
			.setRasterizerDiscardEnable(false)
			.setPolygonMode(vk::PolygonMode::eFill)
			.setLineWidth(1.0f)
			.setCullMode(vk::CullModeFlagBits::eBack)
			.setFrontFace(vk::FrontFace::eCounterClockwise)
			.setDepthBiasEnable(false);

		// Multisampling (disabled for this demo)
		vk::PipelineMultisampleStateCreateInfo multisampling;
		multisampling.setSampleShadingEnable(false)
			.setRasterizationSamples(vk::SampleCountFlagBits::e1);

		// Color blending (disabled, but show write mask)
		vk::PipelineColorBlendAttachmentState colorBlendAttachment;
		colorBlendAttachment.setColorWriteMask(
			vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
		colorBlendAttachment.setBlendEnable(false);

		vk::PipelineColorBlendStateCreateInfo colorBlending;
		colorBlending.setLogicOpEnable(false)
			.setAttachmentCount(1)
			.setPAttachments(&colorBlendAttachment);

		// Pipeline layout (empty - no push constants or descriptor sets)
		vk::PipelineLayoutCreateInfo pipelineLayoutInfo;

		auto uniqueLayout = device->createPipelineLayoutUnique(pipelineLayoutInfo);
		if (!uniqueLayout)
		{
			throw std::runtime_error("Failed to create pipeline layout");
		}
		pipelineLayout = std::move(uniqueLayout);

		// Graphics pipeline creation
		vk::GraphicsPipelineCreateInfo pipelineInfo;
		pipelineInfo.setStageCount(2)
			.setPStages(shaderStages)
			.setPVertexInputState(&vertexInputInfo)
			.setPInputAssemblyState(&inputAssembly)
			.setPViewportState(&viewportState)
			.setPRasterizationState(&rasterizer)
			.setPMultisampleState(&multisampling)
			.setPColorBlendState(&colorBlending)
			.setLayout(*pipelineLayout)
			.setRenderPass(*renderPass)
			.setSubpass(0);

		auto [pipeResult, uniquePipeline] = device->createGraphicsPipelineUnique(nullptr, pipelineInfo);
		if (pipeResult != vk::Result::eSuccess)
		{
			throw std::runtime_error("Failed to create graphics pipeline");
		}
		graphicsPipeline = std::move(uniquePipeline);
	}

	/**
	 * Create shader module from SPIR-V bytecode
	 */
	vk::UniqueShaderModule createShaderModule(const vector<char>& code)
	{
		vk::ShaderModuleCreateInfo createInfo;
		createInfo.setCodeSize(code.size())
			.setPCode(reinterpret_cast<const uint32_t*>(code.data()));

		auto module = device->createShaderModuleUnique(createInfo);
		if (!module)
		{
			throw std::runtime_error("Failed to create shader module");
		}
		return module;
	}

	// =============================================================================
	// FRAMEBUFFER & COMMAND INFRASTRUCTURE
	// =============================================================================

	/**
	 * Create framebuffers binding render passes to swapchain image views
	 */
	void createFramebuffers()
	{
		swapChainFramebuffers.resize(swapChainImageViews.size());

		for (size_t i = 0; i < swapChainImageViews.size(); i++)
		{
			vk::ImageView attachments[] = { *swapChainImageViews[i] };

			vk::FramebufferCreateInfo framebufferInfo;
			framebufferInfo.setRenderPass(*renderPass)
				.setAttachmentCount(1)
				.setPAttachments(attachments)
				.setWidth(swapChainExtent.width)
				.setHeight(swapChainExtent.height)
				.setLayers(1);

			auto fb = device->createFramebufferUnique(framebufferInfo);
			if (!fb)
			{
				throw std::runtime_error("Failed to create framebuffer");
			}
			swapChainFramebuffers[i] = std::move(fb);
		}
	}

	/**
	 * Create command pool for graphics queue
	 * NVRHI-style: One pool per queue family, resettable for per-frame recording
	 */
	void createCommandPool()
	{
		QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

		vk::CommandPoolCreateInfo poolInfo;
		poolInfo.setQueueFamilyIndex(queueFamilyIndices.graphicsFamily.value())
			.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer); // Allow individual buffer reset

		auto pool = device->createCommandPoolUnique(poolInfo);
		if (!pool)
		{
			throw std::runtime_error("Failed to create command pool");
		}
		commandPool = std::move(pool);
	}

	/**
	 * Allocate and record command buffers for each swapchain image
	 * NVRHI-style: Pre-recorded command buffers for static geometry (could be re-recorded per-frame)
	 */
	void createCommandBuffers()
	{
		commandBuffers.resize(swapChainFramebuffers.size());

		vk::CommandBufferAllocateInfo allocInfo;
		allocInfo.setCommandPool(*commandPool)
			.setLevel(vk::CommandBufferLevel::ePrimary)
			.setCommandBufferCount(static_cast<uint32_t>(commandBuffers.size()));

		auto buffers = device->allocateCommandBuffersUnique(allocInfo);
		if (buffers.size() != commandBuffers.size())
		{
			throw std::runtime_error("Failed to allocate command buffers");
		}

		// Move handles from returned vector to member vector
		for (size_t i = 0; i < buffers.size(); i++)
		{
			commandBuffers[i] = std::move(buffers[i]);
		}

		// Record commands for each buffer
		for (size_t i = 0; i < commandBuffers.size(); i++)
		{
			vk::CommandBufferBeginInfo beginInfo;
			beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse); // Can be submitted multiple times

			commandBuffers[i]->begin(beginInfo);

			// Begin render pass
			vk::ClearValue clearColor(vk::ClearColorValue(std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f }));

			vk::RenderPassBeginInfo renderPassInfo;
			renderPassInfo.setRenderPass(*renderPass)
				.setFramebuffer(*swapChainFramebuffers[i])
				.setRenderArea(vk::Rect2D({ 0, 0 }, swapChainExtent))
				.setClearValueCount(1)
				.setPClearValues(&clearColor);

			commandBuffers[i]->beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

			// Bind pipeline
			commandBuffers[i]->bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);

			// Bind vertex buffer
			vk::Buffer	   vertexBuffers[] = { *vertexBuffer };
			vk::DeviceSize offsets[] = { 0 };
			commandBuffers[i]->bindVertexBuffers(0, 1, vertexBuffers, offsets);

			// Bind index buffer
			commandBuffers[i]->bindIndexBuffer(*indexBuffer, 0, vk::IndexType::eUint32);

			// Draw indexed (3 vertices, 1 instance, starting at 0)
			commandBuffers[i]->drawIndexed(3, 1, 0, 0, 0);

			commandBuffers[i]->endRenderPass();
			commandBuffers[i]->end();
		}
	}

	// =============================================================================
	// SYNCHRONIZATION
	// =============================================================================

	/**
	 * Create per-frame synchronization primitives
	 * NVRHI-style: FrameData struct encapsulates all flight data
	 */
	void createSyncObjects()
	{
		frames.resize(MAX_FRAMES_IN_FLIGHT);
		imagesInFlight.resize(swapChainImages.size(), nullptr);

		vk::SemaphoreCreateInfo semaphoreInfo;
		vk::FenceCreateInfo		fenceInfo;
		fenceInfo.setFlags(vk::FenceCreateFlagBits::eSignaled); // Start signaled to avoid initial wait

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			auto imgAvail = device->createSemaphoreUnique(semaphoreInfo);
			auto rndFin = device->createSemaphoreUnique(semaphoreInfo);
			auto fence = device->createFenceUnique(fenceInfo);

			if (!imgAvail || !rndFin || !fence)
			{
				throw std::runtime_error("Failed to create synchronization objects");
			}

			frames[i].imageAvailableSemaphore = std::move(imgAvail);
			frames[i].renderFinishedSemaphore = std::move(rndFin);
			frames[i].inFlightFence = std::move(fence);
		}
	}

	// =============================================================================
	// RENDER LOOP
	// =============================================================================

	/**
	 * Main rendering loop
	 * NVRHI-style: Frame-based rendering with proper CPU-GPU synchronization
	 */
	void mainLoop()
	{
		cout << "mainLoop IN" << endl;

		FTimer timer;
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
			drawFrame();

			// Auto-close after 2 seconds for testing
			if (timer.MarkSec() > 2.0)
			{
				glfwSetWindowShouldClose(window, GLFW_TRUE);
			}
		}

		// Wait for GPU to finish before cleanup (RAII handles will destroy resources)
		device->waitIdle();
	}

	/**
	 * Draw single frame with proper synchronization
	 * NVRHI-style: Acquire -> Submit -> Present pattern with frame flight management
	 */
	void drawFrame()
	{
		FrameData& currentFrameData = frames[currentFrame];

		// 1. WAIT for previous frame using this flight index to complete (CPU-GPU sync)
		// This prevents CPU from running too far ahead of GPU
		auto waitResult = device->waitForFences(
			*currentFrameData.inFlightFence,
			true,	   // Wait for all (just one here)
			UINT64_MAX // Infinite timeout
		);

		// 2. ACQUIRE next swapchain image (GPU operation, async)
		uint32_t imageIndex;
		auto	 acquireResult = device->acquireNextImageKHR(
			*swapChain,
			UINT64_MAX,								   // Timeout
			*currentFrameData.imageAvailableSemaphore, // Signal when ready
			nullptr,								   // No fence
			&imageIndex);

		// Check for swapchain recreation needs (out of date, suboptimal, etc.)
		if (acquireResult == vk::Result::eErrorOutOfDateKHR)
		{
			// Handle swapchain recreation (omitted for brevity)
			return;
		}
		else if (acquireResult != vk::Result::eSuccess && acquireResult != vk::Result::eSuboptimalKHR)
		{
			throw std::runtime_error("Failed to acquire swapchain image");
		}

		// 3. CHECK if this specific swapchain image is still in use by previous frame
		// This handles case where MAX_FRAMES_IN_FLIGHT > swapchain image count
		if (imagesInFlight[imageIndex])
		{
			auto waitResult = device->waitForFences(imagesInFlight[imageIndex], true, UINT64_MAX);
			if (waitResult != vk::Result::eSuccess)
			{
				throw std::runtime_error("Failed to wait for fence");
			}
		}
		// Mark this image as being used by current frame's fence
		imagesInFlight[imageIndex] = *currentFrameData.inFlightFence;

		// 4. RESET fence for this frame (must be done after wait, before submit)
		device->resetFences(*currentFrameData.inFlightFence);

		// 5. SUBMIT command buffer to graphics queue
		// Wait for imageAvailable before starting color output
		vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };

		vk::SubmitInfo submitInfo;
		submitInfo.setWaitSemaphoreCount(1)
			.setPWaitSemaphores(&*currentFrameData.imageAvailableSemaphore)
			.setPWaitDstStageMask(waitStages)
			.setCommandBufferCount(1)
			.setPCommandBuffers(&*commandBuffers[imageIndex])
			.setSignalSemaphoreCount(1)
			.setPSignalSemaphores(&*currentFrameData.renderFinishedSemaphore);

		graphicsQueue.submit(submitInfo, *currentFrameData.inFlightFence);

		// 6. PRESENT rendered image to surface
		vk::PresentInfoKHR presentInfo;
		presentInfo.setWaitSemaphoreCount(1)
			.setPWaitSemaphores(&*currentFrameData.renderFinishedSemaphore)
			.setSwapchainCount(1)
			.setPSwapchains(&*swapChain)
			.setPImageIndices(&imageIndex);

		auto presentResult = presentQueue.presentKHR(presentInfo);

		// Check presentation results
		if (presentResult == vk::Result::eErrorOutOfDateKHR || presentResult == vk::Result::eSuboptimalKHR)
		{
			// Handle swapchain recreation
		}
		else if (presentResult != vk::Result::eSuccess)
		{
			throw std::runtime_error("Failed to present swapchain image");
		}

		// 7. ADVANCE to next frame index (round-robin through MAX_FRAMES_IN_FLIGHT)
		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	// =============================================================================
	// UTILITY FUNCTIONS
	// =============================================================================

	/**
	 * Get required instance extensions (GLFW + debug utils)
	 */
	vector<const char*> getRequiredExtensions()
	{
		uint32_t	 glfwExtensionCount = 0;
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		if (enableValidationLayers)
		{
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		return extensions;
	}

	/**
	 * Check if all requested validation layers are available
	 */
	bool checkValidationLayerSupport()
	{
		auto availableLayers = vk::enumerateInstanceLayerProperties();

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
				return false;
		}
		return true;
	}

	/**
	 * Read binary file (SPIR-V shaders)
	 */
	static vector<char> readFile(const string& filename)
	{
		ifstream file(filename, std::ios::ate | std::ios::binary);
		if (!file.is_open())
		{
			throw std::runtime_error("Failed to open file: " + filename);
		}

		size_t		 fileSize = static_cast<size_t>(file.tellg());
		vector<char> buffer(fileSize);

		file.seekg(0);
		file.read(buffer.data(), fileSize);
		file.close();

		return buffer;
	}

	/**
	 * Debug callback for validation layers
	 */
	static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
		vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		vk::DebugUtilsMessageTypeFlagsEXT /*messageType*/,
		const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* /*pUserData*/
	)
	{
		if (messageSeverity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
		{
			std::cerr << "Validation Error: " << pCallbackData->pMessage << std::endl;
		}
		else if (messageSeverity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
		{
			std::cerr << "Validation Warning: " << pCallbackData->pMessage << std::endl;
		}
		else if (messageSeverity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo)
		{
			std::cout << "Validation Info: " << pCallbackData->pMessage << std::endl;
		}
		return VK_FALSE;
	}
};

		#pragma clang diagnostic pop

// =============================================================================
// TEST ENTRY POINT
// =============================================================================

RECORD_BOOL(test_NVRHI_Style_Vulkan)
{
		// VulkanLoader::LoadOnce();

		#if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
	static vk::detail::DynamicLoader dl(VULKAN_LIB);
	PFN_vkGetInstanceProcAddr		 vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
	VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
		#endif

	NVRHIStyleTriangleApp app;
	try
	{
		app.run();
	}
	catch (const std::exception& e)
	{
		std::cerr << "Fatal Error: " << e.what() << std::endl;
		return false;
	}
	return true;
}

	#endif // Test Vulkan triangle program
#endif	   // HLVM_WINDOW_USE_VULKAN
