/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * RHI Objects Test
 *
 * This test demonstrates using the new RHI object abstractions (FTexture, FFramebuffer,
 * FVertexBuffer, FIndexBuffer) with NVRHI Vulkan backend.
 *
 * Unlike TestNVRHIVulkanWindow.cpp which uses raw Vulkan-HPP, this test uses our
 * higher-level RHI abstractions for cleaner, more maintainable code.
 */

#include "Test.h"
#include "Renderer/Window/WindowDefinition.h"

DECLARE_LOG_CATEGORY(LogTest)

// deliberately use a wrong wayto handle unique handles lifetime
#define DELIBERATE_UNIQUE_HANDLE_WRONG 0
// for device feature, use physical device feature2 and sub api features
// physical device feature1 (core 1.0) cannot specify vk api 1.x features
// will warn about synchronziation2 and timeline semaphore
// https://docs.vulkan.org/guide/latest/enabling_features.html
// https://community.khronos.org/t/does-synchronization2-have-to-be-enabled-explicitly/110787
#define USE_PHYSICAL_DEVICE_FEATURE2 1

#define TEST_DYNAMIC_BUFFER 1

#if HLVM_VULKAN_RENDERER
	#include "Renderer/Window/GLFW3/GLFW3VulkanWindow.h"
	#include "Renderer/RHI/RHICommon.h"
	#include "Renderer/RHI/Object/Texture.h"
	#include "Renderer/RHI/Object/Frambuffer.h"
	#include "Renderer/RHI/Object/Buffer.h"

	#if 1 // Test RHI Objects with NVRHI
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
		#pragma clang diagnostic ignored "-Wunreachable-code"

using namespace std;

// =============================================================================
// CONFIGURATION
// =============================================================================

const uint32_t	   WIDTH = 800;
const uint32_t	   HEIGHT = 600;
static const char* WINDOW_TITLE = "RHI Objects Test";

		#if !HLVM_BUILD_RELEASE
const bool enableValidationLayers = true;
		#else
const bool enableValidationLayers = false;
		#endif

// =============================================================================
// TEST STRUCTURE
// =============================================================================

struct FRHITestContext
{
	// Window
	GLFWwindow* Window = nullptr;

	// Vulkan instance
	vk::Instance			   Instance;
	vk::DebugUtilsMessengerEXT DebugMessenger;
	vk::PhysicalDevice		   PhysicalDevice;
	vk::Device				   Device;
	vk::Queue				   GraphicsQueue;
	vk::SurfaceKHR			   Surface;
	vk::SwapchainKHR		   Swapchain;
	vk::Format				   SwapchainFormat;
	vk::Extent2D			   SwapchainExtent;

	// NVRHI device
	nvrhi::DeviceHandle NvrhiDevice;
	// Command list
	nvrhi::CommandListHandle NvrhiCommandList;

	// RHI Objects
	TUniquePtr<FTexture>	  ColorTexture;
	TUniquePtr<FTexture>	  DepthTexture;
	TUniquePtr<FFramebuffer>  Framebuffer;
	TUniquePtr<FVertexBuffer> VertexBuffer;
	TUniquePtr<FIndexBuffer>  IndexBuffer;

	// Swapchain resources
	vector<vk::Image> SwapchainImages;

	// Note YuHang, we use a mix of vk handle and vk unique handle
	// to show how to manage their life time
	// vk unique handle needs to release pointer and
	// vk handle needs to call device destroy
	vector<vk::UniqueImageView> SwapchainImageViews;

	// Synchronization
	vector<vk::UniqueSemaphore> ImageAvailableSemaphores;
	vector<vk::UniqueSemaphore> RenderFinishedSemaphores;
	vector<vk::UniqueFence>		InFlightFences;
	vector<vk::Fence>			ImagesInFlight;
	size_t						CurrentFrame = 0;

	static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
};

static FRHITestContext GTestContext;

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

static void CreateVulkanInstance(FRHITestContext& Context)
{
	// Application info
	// CommandList->writeBuffer requires sync2 which is a vk 1.3 feature
	// (otherwise throw error on dynamic api not loaded)
	// https://docs.vulkan.org/guide/latest/extensions/VK_KHR_synchronization2.html
	vk::ApplicationInfo AppInfo(
		"RHI Objects Test",
		VK_MAKE_VERSION(1, 0, 0),
		"HLVM Engine",
		VK_MAKE_VERSION(1, 0, 0),
		VK_API_VERSION_1_3);

	// Extensions
	vector<const char*> Extensions = {
		VK_KHR_SURFACE_EXTENSION_NAME,
		"VK_KHR_xcb_surface"
	};
	if (enableValidationLayers)
	{
		Extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	// Validation layers
	vector<const char*> Layers;
	if (enableValidationLayers)
	{
		Layers.push_back("VK_LAYER_KHRONOS_validation");
	}

	// Create instance
	vk::InstanceCreateInfo CreateInfo;
	CreateInfo.setPApplicationInfo(&AppInfo)
		.setEnabledExtensionCount(static_cast<uint32_t>(Extensions.size()))
		.setPpEnabledExtensionNames(Extensions.data())
		.setEnabledLayerCount(static_cast<uint32_t>(Layers.size()))
		.setPpEnabledLayerNames(Layers.data());

	Context.Instance = vk::createInstance(CreateInfo);
	if (!Context.Instance)
	{
		throw runtime_error("Failed to create Vulkan instance");
	}

		#if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
	VULKAN_HPP_DEFAULT_DISPATCHER.init(Context.Instance);
		#endif
}

static void PickPhysicalDevice(FRHITestContext& Context)
{
	auto Devices = Context.Instance.enumeratePhysicalDevices();
	if (Devices.empty())
	{
		throw runtime_error("No Vulkan-compatible GPUs found");
	}
	Context.PhysicalDevice = Devices[0];

	auto Props = Context.PhysicalDevice.getProperties();
	cout << "Selected GPU: " << Props.deviceName << endl;
}

static void CreateLogicalDevice(FRHITestContext& Context)
{
	// Queue family
	float					  QueuePriority = 1.0f;
	vk::DeviceQueueCreateInfo QueueCreateInfo(
		vk::DeviceQueueCreateFlags(),
		0,
		1,
		&QueuePriority);

	vector<const char*> DeviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

		#if !USE_PHYSICAL_DEVICE_FEATURE2
	vk::PhysicalDeviceFeatures DeviceFeatures;
	DeviceFeatures.setGeometryShader(true);

	vk::DeviceCreateInfo CreateInfo;
	CreateInfo.setQueueCreateInfoCount(1)
		.setPQueueCreateInfos(&QueueCreateInfo)
		.setPEnabledFeatures(&DeviceFeatures)
		.setEnabledExtensionCount(static_cast<uint32_t>(DeviceExtensions.size()))
		.setPpEnabledExtensionNames(DeviceExtensions.data());
		#else

	vk::PhysicalDeviceVulkan11Features DeviceFeatures11;
	vk::PhysicalDeviceVulkan12Features DeviceFeatures12;
	DeviceFeatures12.setTimelineSemaphore(true);
	DeviceFeatures12.setPNext(&DeviceFeatures11);
	vk::PhysicalDeviceVulkan13Features DeviceFeatures13;
	DeviceFeatures13.setSynchronization2(true);
	DeviceFeatures13.setPNext(&DeviceFeatures12);

	vk::PhysicalDeviceFeatures DeviceFeatures;
	DeviceFeatures.setGeometryShader(true);
	vk::PhysicalDeviceFeatures2 DeviceFeatures2;
	DeviceFeatures2.setPNext(&DeviceFeatures13);
	DeviceFeatures2.setFeatures(DeviceFeatures);

	vk::DeviceCreateInfo CreateInfo;
	CreateInfo.setQueueCreateInfoCount(1)
		.setPQueueCreateInfos(&QueueCreateInfo)
		.setPNext(&DeviceFeatures2)
		.setPEnabledFeatures(nullptr)
		.setEnabledExtensionCount(static_cast<uint32_t>(DeviceExtensions.size()))
		.setPpEnabledExtensionNames(DeviceExtensions.data());
		#endif
	Context.Device = Context.PhysicalDevice.createDevice(CreateInfo);
	if (!Context.Device)
	{
		throw runtime_error("Failed to create logical device");
	}

		#if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
	VULKAN_HPP_DEFAULT_DISPATCHER.init(Context.Device);
		#endif

	Context.GraphicsQueue = Context.Device.getQueue(0, 0);
}

static void CreateSwapchain(FRHITestContext& Context)
{
	// Surface capabilities
	auto Caps = Context.PhysicalDevice.getSurfaceCapabilitiesKHR(Context.Surface);

	// Format
	auto				 Formats = Context.PhysicalDevice.getSurfaceFormatsKHR(Context.Surface);
	vk::SurfaceFormatKHR SurfaceFormat = Formats[0];
	for (const auto& Fmt : Formats)
	{
		if (Fmt.format == vk::Format::eB8G8R8A8Srgb)
		{
			SurfaceFormat = Fmt;
			break;
		}
	}

	// Present mode
	auto			   PresentModes = Context.PhysicalDevice.getSurfacePresentModesKHR(Context.Surface);
	vk::PresentModeKHR PresentMode = vk::PresentModeKHR::eFifo;
	for (const auto& Mode : PresentModes)
	{
		if (Mode == vk::PresentModeKHR::eMailbox)
		{
			PresentMode = Mode;
			break;
		}
	}

	// Extent
	vk::Extent2D Extent = { WIDTH, HEIGHT };
	Extent.width = std::clamp(Extent.width, Caps.minImageExtent.width, Caps.maxImageExtent.width);
	Extent.height = std::clamp(Extent.height, Caps.minImageExtent.height, Caps.maxImageExtent.height);

	// Create swapchain
	vk::SwapchainCreateInfoKHR CreateInfo;
	CreateInfo.setSurface(Context.Surface)
		.setMinImageCount(2)
		.setImageFormat(SurfaceFormat.format)
		.setImageColorSpace(SurfaceFormat.colorSpace)
		.setImageExtent(Extent)
		.setImageArrayLayers(1)
		.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
		.setPreTransform(Caps.currentTransform)
		.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
		.setPresentMode(PresentMode)
		.setClipped(true);

	Context.Swapchain = Context.Device.createSwapchainKHR(CreateInfo);
	Context.SwapchainFormat = SurfaceFormat.format;
	Context.SwapchainExtent = Extent;

	// Get images
	Context.SwapchainImages = Context.Device.getSwapchainImagesKHR(Context.Swapchain);
}

static void CreateImageViews(FRHITestContext& Context)
{
	Context.SwapchainImageViews.resize(Context.SwapchainImages.size());

	for (size_t i = 0; i < Context.SwapchainImages.size(); i++)
	{
		vk::ImageViewCreateInfo CreateInfo;
		CreateInfo.setImage(Context.SwapchainImages[i])
			.setViewType(vk::ImageViewType::e2D)
			.setFormat(Context.SwapchainFormat)
			.setSubresourceRange(vk::ImageSubresourceRange(
				vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

		try
		{
			auto ImageView = Context.Device.createImageViewUnique(CreateInfo);
			HLVM_ASSERT(ImageView);
			Context.SwapchainImageViews[i] = MoveTemp(ImageView);
		}
		catch (std::system_error& e)
		{
			HLVM_LOG(LogRHI, critical, TO_TCHAR_CSTR(e.what()));
		}
	}
}

static void CreateSyncObjects(FRHITestContext& Context)
{
	Context.ImageAvailableSemaphores.resize(Context.MAX_FRAMES_IN_FLIGHT);
	Context.RenderFinishedSemaphores.resize(Context.MAX_FRAMES_IN_FLIGHT);
	Context.InFlightFences.resize(Context.MAX_FRAMES_IN_FLIGHT);
	Context.ImagesInFlight.resize(Context.SwapchainImages.size(), nullptr);

	vk::SemaphoreCreateInfo SemaphoreInfo;
	vk::FenceCreateInfo		FenceInfo(vk::FenceCreateFlagBits::eSignaled);

	try
	{
		for (size_t i = 0; i < Context.MAX_FRAMES_IN_FLIGHT; i++)
		{
			auto ImageAvailableSemaphore = Context.Device.createSemaphoreUnique(SemaphoreInfo);
			HLVM_ASSERT(ImageAvailableSemaphore);
			auto RenderFinishedSemaphore = Context.Device.createSemaphoreUnique(SemaphoreInfo);
			HLVM_ASSERT(RenderFinishedSemaphore);
			auto Fence = Context.Device.createFenceUnique(FenceInfo);
			HLVM_ASSERT(Fence);
			Context.ImageAvailableSemaphores[i] = MoveTemp(ImageAvailableSemaphore);
			Context.RenderFinishedSemaphores[i] = MoveTemp(RenderFinishedSemaphore);
			Context.InFlightFences[i] = MoveTemp(Fence);
		}
	}
	catch (std::system_error& e)
	{
		HLVM_LOG(LogRHI, critical, TO_TCHAR_CSTR(e.what()));
	}
}

static void CreateRHIResources(FRHITestContext& Context)
{
	// Create NVRHI device
	nvrhi::vulkan::DeviceDesc DeviceDesc;
	DeviceDesc.errorCB = nullptr;
	DeviceDesc.instance = Context.Instance;
	DeviceDesc.physicalDevice = Context.PhysicalDevice;
	DeviceDesc.device = Context.Device;
	DeviceDesc.graphicsQueue = Context.GraphicsQueue;
	DeviceDesc.graphicsQueueIndex = 0;

	Context.NvrhiDevice = nvrhi::vulkan::createDevice(DeviceDesc);
	if (!Context.NvrhiDevice)
	{
		throw runtime_error("Failed to create NVRHI device");
	}

	nvrhi::CommandListParameters params = {};
	params.enableImmediateExecution = false;
	{
		// SRS - set upload buffer size to avoid Vulkan staging buffer fragmentation
		size_t maxBufferSize = (size_t)(1 * 1024 * 1024);
		params.setUploadChunkSize(maxBufferSize);
	}
	Context.NvrhiCommandList = Context.NvrhiDevice->createCommandList(params);
	if (!Context.NvrhiCommandList)
	{
		throw runtime_error("Failed to create NVRHI command list");
	}

	Context.NvrhiCommandList->open();

	// Create color texture (render target)
	Context.ColorTexture = TUniquePtr<FTexture>(new FTexture());
	Context.ColorTexture->InitializeRenderTarget(
		WIDTH, HEIGHT, ETextureFormat::RGBA8_UNORM, Context.NvrhiDevice.Get());
	Context.ColorTexture->SetDebugName(TXT("ColorRenderTarget"));

	// Create depth texture
	Context.DepthTexture = TUniquePtr<FTexture>(new FTexture());
	Context.DepthTexture->InitializeRenderTarget(
		WIDTH, HEIGHT, ETextureFormat::D32, Context.NvrhiDevice.Get());
	Context.DepthTexture->SetDebugName(TXT("DepthRenderTarget"));

	// Create framebuffer
	Context.Framebuffer = TUniquePtr<FFramebuffer>(new FFramebuffer());
	Context.Framebuffer->Initialize(Context.NvrhiDevice.Get());
	Context.Framebuffer->AddColorAttachment(Context.ColorTexture->GetTextureHandle());
	Context.Framebuffer->SetDepthAttachment(Context.DepthTexture->GetTextureHandle());
	Context.Framebuffer->CreateFramebuffer();
	Context.Framebuffer->SetDebugName(TXT("MainFramebuffer"));

	// Create vertex buffer
	struct FVertex
	{
		float Position[3];
		float Color[3];
	};

	FVertex Vertices[] = {
		{ { 0.0f, 0.8f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
		{ { -0.8f, -0.8f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
		{ { 0.8f, -0.8f, 0.0f }, { 0.0f, 0.0f, 1.0f } }
	};

	// Create index buffer
	uint32_t Indices[] = { 0, 1, 2 };

		#if !TEST_DYNAMIC_BUFFER
	Context.VertexBuffer = TUniquePtr<FStaticVertexBuffer>(new FStaticVertexBuffer());
	static_cast<FStaticVertexBuffer*>(Context.VertexBuffer.get())->Initialize(Context.NvrhiCommandList, Context.NvrhiDevice.Get(), Vertices, sizeof(Vertices));
	Context.VertexBuffer->SetDebugName(TXT("StaticTriangleVertexBuffer"));

	Context.IndexBuffer = TUniquePtr<FStaticIndexBuffer>(new FStaticIndexBuffer());
	static_cast<FStaticIndexBuffer*>(Context.IndexBuffer.get())->Initialize(Context.NvrhiCommandList, Context.NvrhiDevice.Get(), Indices, sizeof(Indices), nvrhi::Format::R32_UINT);
	Context.IndexBuffer->SetDebugName(TXT("StaticTriangleIndexBuffer"));
		#else
	Context.VertexBuffer = TUniquePtr<FDynamicVertexBuffer>(new FDynamicVertexBuffer());
	static_cast<FDynamicVertexBuffer*>(Context.VertexBuffer.get())->Initialize(Context.NvrhiDevice.Get(), sizeof(Vertices));
	static_cast<FDynamicVertexBuffer*>(Context.VertexBuffer.get())->Update(Context.NvrhiCommandList, Vertices, sizeof(Vertices));
	Context.VertexBuffer->SetDebugName(TXT("DynamicTriangleVertexBuffer"));

	Context.IndexBuffer = TUniquePtr<FDynamicIndexBuffer>(new FDynamicIndexBuffer());
	static_cast<FDynamicIndexBuffer*>(Context.IndexBuffer.get())->Initialize(Context.NvrhiDevice.Get(), sizeof(Indices), nvrhi::Format::R32_UINT);
	static_cast<FDynamicIndexBuffer*>(Context.IndexBuffer.get())->Update(Context.NvrhiCommandList, Indices, sizeof(Indices));
	Context.IndexBuffer->SetDebugName(TXT("DynamicTriangleIndexBuffer"));
		#endif

	Context.NvrhiCommandList->close();
	Context.NvrhiDevice->executeCommandList(Context.NvrhiCommandList);

	HLVM_LOG(LogTest, info, TXT("RHI resources (textures, framebuffer, buffers) created successfully"));
	HLVM_LOG(LogTest, info, TXT("Note: Shader and pipeline creation omitted - NVRHI API requires further adaptation"));
}

static void CleanupRHIResources(FRHITestContext& Context)
{
	Context.IndexBuffer.reset();
	Context.VertexBuffer.reset();
	Context.Framebuffer.reset();
	Context.DepthTexture.reset();
	Context.ColorTexture.reset();

	if (Context.NvrhiCommandList)
	{
		Context.NvrhiCommandList.Reset();
	}

	if (Context.NvrhiDevice)
	{
		Context.NvrhiDevice.Reset();
	}
}

// =============================================================================
// TEST ENTRY POINT
// =============================================================================

RECORD_BOOL(test_RHI_Objects_Triangle)
{
		#if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
	static vk::detail::DynamicLoader dl(VULKAN_LIB);
	PFN_vkGetInstanceProcAddr		 vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
	VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
		#endif

	FRHITestContext& Ctx = GTestContext;

	try
	{
		// Initialize window
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
		Ctx.Window = glfwCreateWindow(WIDTH, HEIGHT, WINDOW_TITLE, nullptr, nullptr);
		if (!Ctx.Window)
		{
			throw runtime_error("Failed to create window");
		}

		// Initialize Vulkan
		CreateVulkanInstance(Ctx);

		// Create Vulkan surface
		if (glfwCreateWindowSurface(Ctx.Instance, Ctx.Window, nullptr, reinterpret_cast<VkSurfaceKHR*>(&Ctx.Surface)) != VK_SUCCESS)
		{
			throw runtime_error("Failed to create surface");
		}

		PickPhysicalDevice(Ctx);
		CreateLogicalDevice(Ctx);
		CreateSwapchain(Ctx);
		CreateImageViews(Ctx);
		CreateSyncObjects(Ctx);

		// Create RHI resources
		CreateRHIResources(Ctx);

		// Render loop (simplified - just clear and present)
		FTimer Timer;
		while (!glfwWindowShouldClose(Ctx.Window))
		{
			glfwPollEvents();

			// Auto-close after 2 seconds for testing
			if (Timer.MarkSec() > 2.0)
			{
				glfwSetWindowShouldClose(Ctx.Window, GLFW_TRUE);
			}
		}

		HLVM_LOG(LogTest, warn, TXT("0"));

		// Wait for GPU to finish
		Ctx.Device.waitIdle();
		HLVM_LOG(LogTest, warn, TXT("1"));

		// Cleanup
		CleanupRHIResources(Ctx);

		HLVM_LOG(LogTest, warn, TXT("2"));

			// Cleanup semaphores
		#if DELIBERATE_UNIQUE_HANDLE_WRONG
		for (auto& Fence : Ctx.InFlightFences)
		{
			Ctx.Device.destroyFence(Fence.get());
		}
		HLVM_LOG(LogTest, warn, TXT("2.1"));
		for (auto& Semaphore : Ctx.RenderFinishedSemaphores)
		{
			Ctx.Device.destroySemaphore(Semaphore.get());
		}
		HLVM_LOG(LogTest, warn, TXT("2.2"));
		for (auto& Semaphore : Ctx.ImageAvailableSemaphores)
		{
			Ctx.Device.destroySemaphore(Semaphore.get());
		}
		#else
		// UniqueHandle auto destory on pointer release, no need to call device destroy
		Ctx.InFlightFences.clear();
		Ctx.ImageAvailableSemaphores.clear();
		Ctx.RenderFinishedSemaphores.clear();
		#endif
		HLVM_LOG(LogTest, warn, TXT("2.3"));

		HLVM_LOG(LogTest, warn, TXT("3"));

			// Cleanup swapchain and images
		#if DELIBERATE_UNIQUE_HANDLE_WRONG
		for (auto& View : Ctx.SwapchainImageViews)
		{
			Ctx.Device.destroyImageView(View.get());
		}
		#else
		HLVM_LOG(LogTest, warn, TXT("3.1"));
		Ctx.SwapchainImageViews.clear();
		#endif
		// Note, we don't destroy images here because they are owned by the swapchain
		// Just destroy the swapchain image view before destroying the swapchain
		//		for (auto& Image : Ctx.SwapchainImages)
		//		{
		//			Ctx.Device.destroyImage(Image);
		//		}
		//		Ctx.SwapchainImages.clear();

		HLVM_LOG(LogTest, warn, TXT("4"));

		Ctx.Device.destroySwapchainKHR(Ctx.Swapchain);
		Ctx.SwapchainImages.clear();

		HLVM_LOG(LogTest, warn, TXT("5"));
		Ctx.Device.destroy(nullptr);
		HLVM_LOG(LogTest, warn, TXT("6"));
		Ctx.Instance.destroy(Ctx.Surface, nullptr);
		Ctx.Instance.destroy(nullptr);
		HLVM_LOG(LogTest, warn, TXT("7"));
		glfwDestroyWindow(Ctx.Window);
		HLVM_LOG(LogTest, warn, TXT("8"));
		glfwTerminate();
		HLVM_LOG(LogTest, warn, TXT("9"));

		cout << "RHI Objects test completed successfully!" << endl;
		return true;
	}
	catch (const exception& e)
	{
		cerr << "Fatal Error: " << e.what() << endl;

		// Cleanup on error
		CleanupRHIResources(Ctx);
		if (Ctx.Window)
		{
			glfwDestroyWindow(Ctx.Window);
		}
		glfwTerminate();

		return false;
	}
}

		#pragma clang diagnostic pop
	#endif // Test RHI Objects
#endif	   // HLVM_WINDOW_USE_VULKAN
