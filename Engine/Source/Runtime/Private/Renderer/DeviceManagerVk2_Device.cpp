/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#include "DeviceManagerVk.h"

#if HLVM_VULKAN_RENDERER

bool FDeviceManagerVk::IsVulkanDeviceExtensionEnabled(const char* ExtensionName) const
{
	return enabledExtensions.device.find(ExtensionName) != enabledExtensions.device.end();
}

void FDeviceManagerVk::GetEnabledVulkanDeviceExtensions(TVector<std::string>& OutExtensions) const
{
	for (const auto& ext : enabledExtensions.device)
	{
		OutExtensions.push_back(ext);
	}
}

// =============================================================================
// PHYSICAL DEVICE SELECTION
// =============================================================================

bool FDeviceManagerVk::PickPhysicalDevice()
{
	auto devices = instance->enumeratePhysicalDevices();
	if (devices.empty())
	{
		HLVM_LOG(LogRHI, critical, TXT("No Vulkan-compatible GPUs found"));
		return false;
	}

	std::stringstream errorStream;
	errorStream << "Cannot find suitable Vulkan device:";

	TVector<vk::PhysicalDevice> discreteGPUs;
	TVector<vk::PhysicalDevice> otherGPUs;

	for (const auto& dev : devices)
	{
		auto props = dev.getProperties();
		errorStream << "\n"
					<< props.deviceName.data() << ":";

		if (!FindQueueFamilies(dev))
		{
			errorStream << "\n  - missing required queue families";
			continue;
		}

		if (!CheckDeviceExtensionSupport(dev))
		{
			errorStream << "\n  - missing required extensions";
			continue;
		}

		auto features = dev.getFeatures();
		if (!features.samplerAnisotropy)
		{
			errorStream << "\n  - no sampler anisotropy";
			continue;
		}
		if (!features.textureCompressionBC)
		{
			errorStream << "\n  - no BC texture compression";
			continue;
		}

		// Check swapchain support
		auto swapChainSupport = QuerySwapChainSupport(dev);
		if (swapChainSupport.formats.empty() || swapChainSupport.presentModes.empty())
		{
			errorStream << "\n  - inadequate swapchain support";
			continue;
		}

		// Check presentation support
		vk::Bool32 canPresent = dev.getSurfaceSupportKHR(m_GraphicsQueueFamily, *surface);
		if (!canPresent)
		{
			errorStream << "\n  - cannot present to surface";
			continue;
		}

		// Clamp swapchain buffer count
		auto surfaceCaps = dev.getSurfaceCapabilitiesKHR(*surface);
		DeviceParams.SwapChainBufferCount = std::max(surfaceCaps.minImageCount, DeviceParams.SwapChainBufferCount);
		if (surfaceCaps.maxImageCount > 0)
		{
			DeviceParams.SwapChainBufferCount = std::min(DeviceParams.SwapChainBufferCount, surfaceCaps.maxImageCount);
		}
		HLVM_ASSERT(DeviceParams.SwapChainBufferCount <= hlvm_rhi::MAX_FRAMES_IN_FLIGHT);

		if (props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
		{
			discreteGPUs.push_back(dev);
		}
		else
		{
			otherGPUs.push_back(dev);
		}
	}

	if (!discreteGPUs.empty())
	{
		// Log
		HLVM_LOG(LogRHI, debug, TXT("Found discrete GPU[0]: {}"), TO_TCHAR_CSTR(discreteGPUs[0].getProperties().deviceName.data()));
		physicalDevice = discreteGPUs[0];
	}
	else if (!otherGPUs.empty())
	{
		HLVM_LOG(LogRHI, debug, TXT("Found other GPU[0]: {}"), TO_TCHAR_CSTR(otherGPUs[0].getProperties().deviceName.data()));
		physicalDevice = otherGPUs[0];
	}
	else
	{
		HLVM_LOG(LogRHI, critical, TO_TCHAR_CSTR(errorStream.str().c_str()));
		return false;
	}

	return true;
}

bool FDeviceManagerVk::FindQueueFamilies(vk::PhysicalDevice InDevice)
{
	auto queueFamilies = InDevice.getQueueFamilyProperties();

	m_GraphicsQueueFamily = INVALID_INDEX_UINT32;
	m_PresentQueueFamily = INVALID_INDEX_UINT32;
	m_ComputeQueueFamily = INVALID_INDEX_UINT32;
	m_TransferQueueFamily = INVALID_INDEX_UINT32;

	for (uint32_t i = 0; i < queueFamilies.size(); i++)
	{
		const auto& queueFamily = queueFamilies[i];

		if (m_GraphicsQueueFamily == INVALID_INDEX_UINT32 && queueFamily.queueCount > 0 && (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics))
		{
			m_GraphicsQueueFamily = i;
		}

		if (m_ComputeQueueFamily == INVALID_INDEX_UINT32 && queueFamily.queueCount > 0 && (queueFamily.queueFlags & vk::QueueFlagBits::eCompute) && !(queueFamily.queueFlags & vk::QueueFlagBits::eGraphics))
		{
			m_ComputeQueueFamily = i;
		}

		if (m_TransferQueueFamily == INVALID_INDEX_UINT32 && queueFamily.queueCount > 0 && (queueFamily.queueFlags & vk::QueueFlagBits::eTransfer) && !(queueFamily.queueFlags & vk::QueueFlagBits::eCompute) && !(queueFamily.queueFlags & vk::QueueFlagBits::eGraphics))
		{
			m_TransferQueueFamily = i;
		}

		if (m_PresentQueueFamily == INVALID_INDEX_UINT32 && queueFamily.queueCount > 0)
		{
			vk::Bool32 presentSupported = InDevice.getSurfaceSupportKHR(i, *surface);
			if (presentSupported)
			{
				m_PresentQueueFamily = i;
			}
		}
	}

	return m_GraphicsQueueFamily != INVALID_INDEX_UINT32 && m_PresentQueueFamily != INVALID_INDEX_UINT32;
}

bool FDeviceManagerVk::CheckDeviceExtensionSupport(vk::PhysicalDevice InDevice)
{
	auto availableExtensions = InDevice.enumerateDeviceExtensionProperties();

	std::set<std::string> required(enabledExtensions.device.begin(), enabledExtensions.device.end());

	for (const auto& extension : availableExtensions)
	{
		required.erase(extension.extensionName);
	}

	return required.empty();
}

SwapChainSupportDetails FDeviceManagerVk::QuerySwapChainSupport(vk::PhysicalDevice InDevice)
{
	SwapChainSupportDetails details;
	details.capabilities = InDevice.getSurfaceCapabilitiesKHR(*surface);
	auto formats = InDevice.getSurfaceFormatsKHR(*surface);
	details.formats = TVector<vk::SurfaceFormatKHR>(formats.begin(), formats.end());
	auto presentModes = InDevice.getSurfacePresentModesKHR(*surface);
	details.presentModes = TVector<vk::PresentModeKHR>(presentModes.begin(), presentModes.end());
	return details;
}

// =============================================================================
// LOGICAL DEVICE CREATION
// =============================================================================

bool FDeviceManagerVk::CreateLogicalDevice()
{
	// Enable optional extensions
	auto deviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
	for (const auto& ext : deviceExtensions)
	{
		const std::string name = ext.extensionName;
		if (optionalExtensions.device.find(name) != optionalExtensions.device.end())
		{
			enabledExtensions.device.insert(name);
		}
		if (DeviceParams.bEnableRayTracingExtensions && m_RayTracingExtensions.find(name) != m_RayTracingExtensions.end())
		{
			enabledExtensions.device.insert(name);
		}
	}

	// Log enabled extensions
	{
		FString enabledExtensionsLog = FString::Join(enabledExtensions.device, [](const auto& ext){return TCHARSTR(ext.c_str());}, TXT("\n"));
		HLVM_LOG(LogRHI, info, TXT("Enabled Vulkan device extensions:\n{}"), *enabledExtensionsLog);
	}

	// Track extension support flags for feature chaining
	bool synchronization2Supported = false;
	bool maintenance4Supported = false;
	for (const auto& ext : enabledExtensions.device)
	{
		if (ext == VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME)
			synchronization2Supported = true;
		else if (ext == VK_KHR_MAINTENANCE_4_EXTENSION_NAME)
			maintenance4Supported = true;
	}

	// Collect unique queue families
	std::set<uint32_t> uniqueQueueFamilies = {
		static_cast<uint32_t>(m_GraphicsQueueFamily),
		static_cast<uint32_t>(m_PresentQueueFamily)
	};

	if (DeviceParams.bEnableComputeQueue && m_ComputeQueueFamily != INVALID_INDEX_UINT32)
	{
		uniqueQueueFamilies.insert(m_ComputeQueueFamily);
	}
	if (DeviceParams.bEnableCopyQueue && m_TransferQueueFamily != INVALID_INDEX_UINT32)
	{
		uniqueQueueFamilies.insert(m_TransferQueueFamily);
	}

	float							   queuePriority = 1.0f;
	TVector<vk::DeviceQueueCreateInfo> queueCreateInfos;
	for (uint32_t queueFamily : uniqueQueueFamilies)
	{
		queueCreateInfos.push_back(
			vk::DeviceQueueCreateInfo()
				.setQueueFamilyIndex(queueFamily)
				.setQueueCount(1)
				.setPQueuePriorities(&queuePriority));
	}

	// Core Device features - matching Donut's enabled features
	vk::PhysicalDeviceFeatures deviceFeatures;
	deviceFeatures
		.setShaderImageGatherExtended(true)
		.setSamplerAnisotropy(true)
		.setTessellationShader(true)
		.setTextureCompressionBC(true)
		.setGeometryShader(true)
		.setFillModeNonSolid(true)
		.setImageCubeArray(true)
		.setDualSrcBlend(true)
		.setShaderInt16(true)
		.setFragmentStoresAndAtomics(true)
		.setVertexPipelineStoresAndAtomics(true)
		.setShaderInt64(true)
		.setShaderStorageImageWriteWithoutFormat(true)
		.setShaderStorageImageReadWithoutFormat(true);
	// Vulkan 1.1 features
	auto vulkan11Features = vk::PhysicalDeviceVulkan11Features()
		.setShaderDrawParameters(true)
		.setPNext(nullptr);

	// Vulkan 1.2 features nvrhi required
	auto vulkan12Features = vk::PhysicalDeviceVulkan12Features()
		.setDescriptorIndexing(true)
		.setRuntimeDescriptorArray(true)
		.setDescriptorBindingPartiallyBound(true)
		.setDescriptorBindingVariableDescriptorCount(true)
		.setTimelineSemaphore(true)
		.setShaderSampledImageArrayNonUniformIndexing(true)
		.setBufferDeviceAddress(IsVulkanDeviceExtensionEnabled(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME))
		.setPNext(&vulkan11Features);
	// Chain ray tracing features only when enabled
	// Declare ray tracing features outside if block to ensure proper lifetime
	vk::PhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures;
	vk::PhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures;
	vk::PhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures;

	if (DeviceParams.bEnableRayTracingExtensions)
	{
		// Initialize ray tracing features with constructor chain
		// Chain: ... -> rayTracingPipeline -> accelerationStructure -> rayQuery -> vulkan11Features -> nullptr
		rayQueryFeatures.setRayQuery(true).setPNext(&vulkan11Features);
		accelerationStructureFeatures.setAccelerationStructure(true).setPNext(&rayQueryFeatures);
		rayTracingPipelineFeatures.setRayTracingPipeline(true).setPNext(&accelerationStructureFeatures);

		// Update vulkan12Features to chain to ray tracing features
		vulkan12Features.setPNext(&rayTracingPipelineFeatures);
	}
	else
	{
		vulkan12Features.setPNext(&vulkan11Features);
	}

	/*
	 * Vulkan 1.3 features nvrhi required
	 */
	auto vulkan13Features = vk::PhysicalDeviceVulkan13Features()
		.setDynamicRendering(true) // Caveat : YuHang NVRHI requires dynamic rendering
		.setSynchronization2(synchronization2Supported) // Caveat : YuHang NVRHI requires synchronization2
		.setPNext(&vulkan12Features);

	auto extensionsVec = StringSetToVector(enabledExtensions.device);

	vk::DeviceCreateInfo createInfo;
	createInfo
		.setQueueCreateInfoCount(static_cast<uint32_t>(queueCreateInfos.size()))
		.setPQueueCreateInfos(queueCreateInfos.data())
		.setPEnabledFeatures(&deviceFeatures)
		.setEnabledExtensionCount(static_cast<uint32_t>(extensionsVec.size()))
		.setPpEnabledExtensionNames(extensionsVec.data())
		.setPNext(&vulkan13Features);

	try
	{
		auto dev = physicalDevice.createDeviceUnique(createInfo);
		HLVM_ASSERT(dev);
		device = std::move(dev);
	}
	catch (std::system_error& e)
	{
		HLVM_LOG(LogRHI, critical, TO_TCHAR_CSTR(e.what()));
		return false;
	}

	hlvm_vk::InitVulkanLoaderDevice(*device);

	// Get queues
	graphicsQueue = device->getQueue(m_GraphicsQueueFamily, 0);
	presentQueue = device->getQueue(m_PresentQueueFamily, 0);

	if (DeviceParams.bEnableComputeQueue && m_ComputeQueueFamily != INVALID_INDEX_UINT32)
	{
		computeQueue = device->getQueue(m_ComputeQueueFamily, 0);
	}
	if (DeviceParams.bEnableCopyQueue && m_TransferQueueFamily != INVALID_INDEX_UINT32)
	{
		transferQueue = device->getQueue(m_TransferQueueFamily, 0);
	}

	// Check D24S8 format support
	vk::ImageFormatProperties imageFormatProperties;
	auto					  formatResult = physicalDevice.getImageFormatProperties(
		 vk::Format::eD24UnormS8Uint,
		 vk::ImageType::e2D,
		 vk::ImageTiling::eOptimal,
		 vk::ImageUsageFlagBits::eDepthStencilAttachment,
		 {},
		 &imageFormatProperties);
	DeviceParams.bEnableImageFormatD24S8 = (formatResult == vk::Result::eSuccess);

	// Query present modes
	auto surfacePModes = physicalDevice.getSurfacePresentModesKHR(*surface);
	enablePModeMailbox = std::find(surfacePModes.begin(), surfacePModes.end(), vk::PresentModeKHR::eMailbox) != surfacePModes.end();
	enablePModeImmediate = std::find(surfacePModes.begin(), surfacePModes.end(), vk::PresentModeKHR::eImmediate) != surfacePModes.end();
	enablePModeFifoRelaxed = std::find(surfacePModes.begin(), surfacePModes.end(), vk::PresentModeKHR::eFifoRelaxed) != surfacePModes.end();

	// Store device info
	auto props = physicalDevice.getProperties();
	m_RendererString = std::string(props.deviceName.data());
	m_DeviceApiVersion = props.apiVersion;

	HLVM_LOG(LogRHI, info, TXT("Created Vulkan device: {}, API version: {}.{}.{}"), TO_TCHAR_CSTR(m_RendererString.c_str()), VK_VERSION_MAJOR(m_DeviceApiVersion), VK_VERSION_MINOR(m_DeviceApiVersion), VK_VERSION_PATCH(m_DeviceApiVersion));

	// Create ImGui descriptor pool
	// ImGui_ImplVulkan requires: 1x CombinedImageSampler, 1x Sampler, 1x UniformBuffer
	TVector<vk::DescriptorPoolSize> poolSizes = {
		vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 1),
		vk::DescriptorPoolSize(vk::DescriptorType::eSampler, 1),
		vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1)
	};

	vk::DescriptorPoolCreateInfo poolInfo;
	poolInfo
		.setPoolSizes(poolSizes)
		.setMaxSets(1);

	try
	{
		auto pool = device->createDescriptorPoolUnique(poolInfo);
		m_ImGuiDescriptorPool = std::move(pool);
		HLVM_LOG(LogRHI, info, TXT("Created ImGui descriptor pool"));
	}
	catch (std::system_error& e)
	{
		HLVM_LOG(LogRHI, err, TXT("Failed to create ImGui descriptor pool: {}"), TO_TCHAR_CSTR(e.what()));
	}

	return true;
}

#endif
