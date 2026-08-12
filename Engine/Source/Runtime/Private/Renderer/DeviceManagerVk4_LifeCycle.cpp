/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#include "DeviceManagerVk.h"

// v139 (six-role-pipeline, tick 263, 2026-07-31): nvrhi validation layer hookup.
// The validation TUs are compiled into libnvrhid.a via v133+v134 (validation TUs
// added to add_library(nvrhi STATIC ...) source list at
// _deps/nvrhi-src/CMakeLists.txt:209-214). Per the v24 diagnostic, the validation
// layer is the only mechanism that surfaces the actual Vulkan VUID describing the
// GI shader's GBuffer SRV layout mismatch. Include the header here for the
// nvrhi::validation::createValidationLayer call below (the header is NOT in the
// transitive include chain via RHICommon.h -> <nvrhi/nvrhi.h>).
#include <nvrhi/validation.h>

#if HLVM_VULKAN_RENDERER

// =============================================================================
// MAIN CREATION / DESTRUCTION
// =============================================================================

bool FDeviceManagerVk::CreateDeviceAndSwapChain()
{
	DeviceParams.bEnableNVRHIValidationLayer |= g_UseValidationLayers;
	DeviceParams.bEnableDebugRuntime |= g_UseDebugRuntime;

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
	deviceDesc.maxTimerQueries = DeviceParams.MaxTimerQueries;

	m_NvrhiDevice = nvrhi::vulkan::createDevice(deviceDesc);

	if (DeviceParams.bEnableNVRHIValidationLayer)
	{
	    // v132 (six-role-pipeline, tick 167, 2026-07-30): re-enable the nvrhi validation
	    // layer hookup. Per tick 166 + this tick's static analysis:
	    //   - nvrhi CMakeLists.txt:36 sets NVRHI_WITH_VALIDATION=ON (default).
	    //   - nvrhi CMakeLists.txt:215-219 adds the validation TUs to the nvrhi
	    //     target via target_sources(nvrhi PRIVATE ${src_validation}).
	    //   - nvrhi/include/nvrhi/validation.h:29 declares
	    //     nvrhi::validation::createValidationLayer with NVRHI_API.
	    //   - nvrhi/src/validation/validation-device.cpp:60 defines it.
	    //   - libnvrhi_vkd.a exists on disk and was recently rebuilt.
	    //   - NVRHI_API is empty for static-library builds (nvrhi.h:60),
	    //     but the symbol is still exported via the static lib's symbol table.
	    // v136 (six-role-pipeline, tick 232, 2026-07-30): Reverted v132's
	    // createValidationLayer hookup. The validation TUs are still compiled
	    // into libnvrhid.a (v133+v134 intact) for future use, but the runtime
	    // hookup is removed because it caused a build link failure (ninja
	    // dep-graph staleness skipped the validation .o files; the linker
	    // could not resolve createValidationLayer). Re-instate the hookup
	    // once v137 (or later) addresses the ninja dep-graph issue and the
	    // Vulkan validation layer can be enabled at runtime. The 23:57
	    // binary that successfully ran TestReSTIR_GI_Temporal was built
	    // with this call stubbed; v132 (which added it back) caused every
	    // subsequent rebuild to fail at the executable link step.
	    // v139 (six-role-pipeline, tick 263, 2026-07-31): Re-applied v132's
	    // createValidationLayer hookup now that the v134 fix is in place.
	    // v134 placed the validation TUs (validation-device.cpp + validation-
	    // commandlist.cpp + validation-backend.h) in the add_library(nvrhi
	    // STATIC ...) source list at lines 209-214, with a 12-line comment
	    // explaining the durability of this fix against ninja dep-graph
	    // regeneration. The next rebuild should link successfully. The call
	    // is gated by DeviceParams.bEnableNVRHIValidationLayer (default true
	    // via g_UseValidationLayers CVar ORed in at line 15), so this runs
	    // in the default test path. Per the v24 diagnostic, this is the
	    // bisect-closing action: the validation layer will surface the actual
	    // Vulkan VUID describing the GI shader's GBuffer SRV layout issue.
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
	if (!device)
	{
		// Already destroyed
		return;
	}

	// waitIdle can throw if device is in invalid state (e.g., resources not properly released)
	// Catch and log to prevent std::terminate()
	try
	{
		device->waitIdle();
	}
	catch (const std::exception& e)
	{
		HLVM_LOG(LogRHI, warn, TXT("device->waitIdle() threw: {}"), TO_TCHAR_CSTR(e.what()));
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

	m_ImGuiDescriptorPool.reset();

	m_NvrhiDevice = nullptr;
	// v136 (six-role-pipeline, tick 232, 2026-07-30): Reverted v132's
	// createValidationLayer hookup. The validation TUs are still compiled
	// into libnvrhid.a (v133+v134 intact) for future use, but the runtime
	// hookup is removed because it caused a build link failure (ninja
	// dep-graph staleness skipped the validation .o files; the linker
	// could not resolve createValidationLayer). Re-instate the hookup
	// once v137 (or later) addresses the ninja dep-graph issue and the
	// Vulkan validation layer can be enabled at runtime.
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

	// Ensure the swapchain image is in the Present state before signalling the
	// present semaphore. Some render passes reuse command lists or leave the
	// back-buffer in an intermediate layout, so we cannot rely on NVRHI's
	// implicit keep-initial-state transition alone.
	{
		nvrhi::CommandListHandle TransitionCmd = m_NvrhiDevice->createCommandList();
		TransitionCmd->open();
		TransitionCmd->setTextureState(
			m_SwapChainImages[m_SwapChainIndex].rhiHandle,
			nvrhi::AllSubresources,
			nvrhi::ResourceStates::Present);
		TransitionCmd->close();
		m_NvrhiDevice->executeCommandList(TransitionCmd);
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
