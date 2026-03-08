/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "DeviceManagerVk.h"

#if HLVM_VULKAN_RENDERER

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

nvrhi::IFramebuffer* FDeviceManagerVk::GetFramebuffer(TUINT32 Index)
{
	// ensure
	HLVM_ENSURE(Index < m_Framebuffers.size());
	return m_Framebuffers[Index];
}

void FDeviceManagerVk::ResizeSwapChain()
{
	if (!device)
	{
		// Log
		HLVM_LOG(LogRHI, critical, TXT("DeviceManagerVk::ResizeSwapChain: device is null"));
		return;
	}

	// Wait for GPU to finish all work before recreating swapchain
	device->waitIdle();

	// Notify base class - will call OnBeforeSwapchainRecreate
	BackBufferResizing();

	// Destroy old swapchain and resources
	DestroySwapChain();

	// Recreate swapchain with new dimensions
	if (!CreateSwapChain())
	{
		HLVM_LOG(LogRHI, critical, TXT("Failed to recreate swapchain during resize"));
		return;
	}

	// Notify base class - will call OnAfterSwapchainRecreate
	BackBufferResized();
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

	swapChainImageFormat = surfaceFormat.format;
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

	try
	{
		auto sc = device->createSwapchainKHRUnique(createInfo);
		HLVM_ASSERT(sc);
		swapChain = std::move(sc);
	}
	catch (std::system_error& e)
	{
		HLVM_LOG(LogRHI, critical, TO_TCHAR_CSTR(e.what()));
		return false;
	}

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

	// Create framebuffers for each swapchain image
	m_Framebuffers.reserve(m_SwapChainImages.size());
	for (size_t i = 0; i < m_SwapChainImages.size(); i++)
	{
		nvrhi::FramebufferDesc fbDesc = nvrhi::FramebufferDesc()
											.addColorAttachment(m_SwapChainImages[i].rhiHandle);

		nvrhi::FramebufferHandle fb = m_NvrhiDevice->createFramebuffer(fbDesc);
		if (!fb)
		{
			HLVM_LOG(LogRHI, critical, TXT("Failed to create framebuffer %zu"), static_cast<TUINT32>(i));
			return false;
		}
		m_Framebuffers.push_back(fb);
	}

	m_SwapChainIndex = 0;

	return true;
}

void FDeviceManagerVk::DestroySwapChain()
{
	if (!device)
	{
		// Log
		HLVM_LOG(LogRHI, critical, TXT("DeviceManagerVk::ResizeSwapChain: device is null"));
		return;
	}
	device->waitIdle();

	// Destroy framebuffers
	m_Framebuffers.clear();

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
	auto perferredFormat = vk::Format(nvrhi::vulkan::convertFormat(DeviceParams.SwapChainFormat));
	for (const auto& availableFormat : availableFormats)
	{
		if (availableFormat.format == perferredFormat)
		{
			return availableFormat;
		}
	}
	// warn
	HLVM_LOG(LogRHI, warn, TXT("Swap chain format {} not supported. Using {} instead."), *VULKAN_ENUM_TO_FSTRING(perferredFormat), *VULKAN_ENUM_TO_FSTRING(availableFormats[0].format));
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
	// Create semaphores for each swapchain image (present semaphores)
	m_PresentSemaphores.resize(m_SwapChainImages.size());
	for (size_t i = 0; i < m_SwapChainImages.size(); i++)
	{
		m_PresentSemaphores[i] = device->createSemaphore(vk::SemaphoreCreateInfo());
	}

	// Create acquire semaphores based on max frames in flight
	const auto& params = DeviceParams;
	uint32_t	numAcquireSemaphores = std::max(S_C(TUINT32, m_SwapChainImages.size()), params.SwapChainBufferCount);
	m_AcquireSemaphores.reserve(numAcquireSemaphores);
	for (uint32_t i = 0; i < numAcquireSemaphores; i++)
	{
		m_AcquireSemaphores.push_back(device->createSemaphore(vk::SemaphoreCreateInfo()));
	}

	m_AcquireSemaphoreIndex = 0;
}
#endif
