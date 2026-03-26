/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#include "DeviceManagerVk.h"

#if HLVM_VULKAN_RENDERER

// =============================================================================
// MAIN CREATION / DESTRUCTION
// =============================================================================

bool FDeviceManagerVk::CreateDeviceAndSwapChain()
{
	DeviceParams.bEnableNVRHIValidationLayer = g_UseValidationLayers;
	DeviceParams.bEnableDebugRuntime = g_UseDebugRuntime;

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
	else
	{
		// Already destroyed
		return;
	}

	// Clean up frame sync resources
	while (!m_FramesInFlight.empty())
	{
		auto query = m_FramesInFlight.front();
		m_FramesInFlight.pop_front();
		query = nullptr;
	}
	m_QueryPool.clear();

	// Clean up semaphores
	for (auto& semaphore : m_PresentSemaphores)
	{
		if (semaphore)
		{
			device->destroySemaphore(semaphore);
			semaphore = vk::Semaphore();
		}
	}
	m_PresentSemaphores.clear();

	for (auto& semaphore : m_AcquireSemaphores)
	{
		if (semaphore)
		{
			device->destroySemaphore(semaphore);
			semaphore = vk::Semaphore();
		}
	}
	m_AcquireSemaphores.clear();

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

bool FDeviceManagerVk::BeginFrame()
{
	if (!swapChain || !device)
	{
		HLVM_LOG(LogRHI, err, TXT("BeginFrame called but swapchain or device is null"));
		return false;
	}

	const auto& semaphore = m_AcquireSemaphores[m_AcquireSemaphoreIndex];
	vk::Result	result = vk::Result::eErrorUnknown;
	int			maxAttempts = 3;

	for (int attempt = 0; attempt < maxAttempts; ++attempt)
	{
		try
		{
			auto [acquireResult, index] = device->acquireNextImageKHR(
				*swapChain,
				std::numeric_limits<uint64_t>::max(),
				semaphore,
				vk::Fence());

			result = acquireResult;
			m_SwapChainIndex = index;
		}
		catch (const vk::OutOfDateKHRError&)
		{
			result = vk::Result::eErrorOutOfDateKHR;
		}
		catch (const std::exception& e)
		{
			HLVM_LOG(LogRHI, warn, TXT("AcquireNextImage failed: {} - attempting recreation"), TO_TCHAR_CSTR(e.what()));
			result = vk::Result::eErrorOutOfDateKHR;
		}

		if ((result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR) && attempt < maxAttempts - 1)
		{
			HLVM_LOG(LogRHI, warn, TXT("Recreating swapchain (attempt {}/{})"), attempt + 1, maxAttempts);
			BackBufferResizing();
			ResizeSwapChain();
			BackBufferResized();
		}
		else
		{
			break;
		}
	}

	if (result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR)
	{
		m_AcquireSemaphoreIndex = (m_AcquireSemaphoreIndex + 1) % m_AcquireSemaphores.size();
		m_NvrhiDevice->queueWaitForSemaphore(nvrhi::CommandQueue::Graphics, semaphore, 0);
		return true;
	}
	else
	{
		HLVM_LOG(LogRHI, err, TXT("Failed to acquire swapchain image after retries - result: %d"), static_cast<int>(result));
		return false;
	}
}

bool FDeviceManagerVk::EndFrame()
{
	if (m_SwapChainIndex >= m_SwapChainImages.size())
	{
		HLVM_LOG(LogRHI, err, TXT("Present called with invalid swapchain index: {}"), m_SwapChainIndex);
		return false;
	}

	// Signal the present semaphore we're done with this image
	const auto& presentSemaphore = m_PresentSemaphores[m_SwapChainIndex];
	m_NvrhiDevice->queueSignalSemaphore(nvrhi::CommandQueue::Graphics, presentSemaphore, 0);
	bCanPresent = true;

	return true;
}

bool FDeviceManagerVk::Present()
{
	if (!swapChain || !presentQueue)
	{
		HLVM_LOG(LogRHI, err, TXT("Present called with invalid state"));
		return false;
	}

	if (m_SwapChainIndex >= m_SwapChainImages.size())
	{
		HLVM_LOG(LogRHI, err, TXT("Present called with invalid swapchain index: {}"), m_SwapChainIndex);
		return false;
	}

	if (!bCanPresent)
	{
		HLVM_LOG(LogRHI, err, TXT("Present called without calling EndFrame()"));
		return false;
	}

	// Execute command lists to actually signal the semaphore
	// This is necessary because NVRHI buffers the semaphore signals
	m_NvrhiDevice->executeCommandLists(nullptr, 0);

	const auto&		   presentSemaphore = m_PresentSemaphores[m_SwapChainIndex];
	vk::PresentInfoKHR presentInfo;
	presentInfo
		.setWaitSemaphoreCount(1)
		.setPWaitSemaphores(&presentSemaphore)
		.setSwapchainCount(1)
		.setPSwapchains(&*swapChain)
		.setPImageIndices(&m_SwapChainIndex);

	vk::Result result;
	try
	{
		result = presentQueue.presentKHR(presentInfo);
	}
	catch (const vk::OutOfDateKHRError&)
	{
		result = vk::Result::eErrorOutOfDateKHR;
	}
	catch (const std::exception& e)
	{
		HLVM_LOG(LogRHI, err, TXT("Failed to present: {}"), TO_TCHAR_CSTR(e.what()));
		return false;
	}

	// Handle out-of-date/suboptimal - normal during resize
	if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR)
	{
		HLVM_LOG(LogRHI, debug, TXT("Present returned suboptimal/out-of-date - swapchain may need recreation"));
	}
	else if (result != vk::Result::eSuccess)
	{
		HLVM_LOG(LogRHI, err, TXT("Present failed - result: %d"), static_cast<int>(result));
		return false;
	}

	// Frame in-flight tracking following Donuts pattern
	while (m_FramesInFlight.size() >= DeviceParams.SwapChainBufferCount)
	{
		auto query = m_FramesInFlight.front();
		m_FramesInFlight.pop_front();

		m_NvrhiDevice->waitEventQuery(query);

		m_QueryPool.push_back(query);
	}

	// Get or create event query for this frame
	nvrhi::EventQueryHandle query;
	if (!m_QueryPool.empty())
	{
		query = m_QueryPool.back();
		m_QueryPool.pop_back();
	}
	else
	{
		query = m_NvrhiDevice->createEventQuery();
	}

	m_NvrhiDevice->resetEventQuery(query);
	m_NvrhiDevice->setEventQuery(query, nvrhi::CommandQueue::Graphics);
	m_FramesInFlight.push_back(query);

	FrameIndex++;
	return true;
}

#endif
