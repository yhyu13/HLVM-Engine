/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#include "DeviceManagerVk.h"

#if HLVM_VULKAN_RENDERER

// Destructor
FDeviceManagerVk::~FDeviceManagerVk()
{
	Shutdown();
}

bool FDeviceManagerVk::IsVulkanInstanceExtensionEnabled(const char* ExtensionName) const
{
	return enabledExtensions.instance.find(ExtensionName) != enabledExtensions.instance.end();
}

void FDeviceManagerVk::GetEnabledVulkanInstanceExtensions(TVector<std::string>& OutExtensions) const
{
	for (const auto& ext : enabledExtensions.instance)
	{
		OutExtensions.push_back(ext);
	}
}

bool FDeviceManagerVk::IsVulkanLayerEnabled(const char* LayerName) const
{
	return enabledExtensions.layers.find(LayerName) != enabledExtensions.layers.end();
}

void FDeviceManagerVk::GetEnabledVulkanLayers(TVector<std::string>& OutLayers) const
{
	for (const auto& layer : enabledExtensions.layers)
	{
		OutLayers.push_back(layer);
	}
}

// =============================================================================
// INSTANCE CREATION
// =============================================================================

bool FDeviceManagerVk::CreateInstance()
{
	// Add GLFW required extensions
	auto glfwExtensions = FGLFW3VulkanWindow::GetRequiredExtensions();
	HLVM_ASSERT(glfwExtensions.size() > 0);
	for (uint32_t i = 0; i < glfwExtensions.size(); i++)
	{
		enabledExtensions.instance.insert(glfwExtensions[i]);
	}

	// Add user-requested extensions
	for (const std::string& name : DeviceParams.RequiredVulkanInstanceExtensions)
	{
		enabledExtensions.instance.insert(name);
	}
	for (const std::string& name : DeviceParams.OptionalVulkanInstanceExtensions)
	{
		optionalExtensions.instance.insert(name);
	}

	// Add user-requested layers
	for (const std::string& name : DeviceParams.RequiredVulkanLayers)
	{
		enabledExtensions.layers.insert(name);
	}
	for (const std::string& name : DeviceParams.OptionalVulkanLayers)
	{
		optionalExtensions.layers.insert(name);
	}

	// Check for validation layer support
	if (DeviceParams.bEnableDebugRuntime)
	{
		enabledExtensions.layers.insert("VK_LAYER_KHRONOS_validation");
	}

	std::unordered_set<std::string> requiredExtensions = enabledExtensions.instance;

	// Enumerate available instance extensions
	auto availableExtensions = vk::enumerateInstanceExtensionProperties();
	for (const auto& ext : availableExtensions)
	{
		const std::string name = ext.extensionName;
		if (optionalExtensions.instance.find(name) != optionalExtensions.instance.end())
		{
			enabledExtensions.instance.insert(name);
		}
		requiredExtensions.erase(name);
	}

	if (!requiredExtensions.empty())
	{
		std::stringstream ss;
		ss << "Cannot create Vulkan instance - missing required extensions:";
		for (const auto& ext : requiredExtensions)
		{
			ss << "\n  - " << ext;
		}
		HLVM_LOG(LogRHI, critical, TO_TCHAR_CSTR(ss.str().c_str()));
		return false;
	}

	{
		FString enabledExtensionsLog = FString::Join(enabledExtensions.instance, [](const auto& ext){return TCHARSTR(ext.c_str());}, TXT("\n"));
		HLVM_LOG(LogRHI, info, TO_TCHAR_CSTR("Enabled Vulkan instance extensions:\n{}"), *enabledExtensionsLog);
	}

	// Check layers
	std::unordered_set<std::string> requiredLayers = enabledExtensions.layers;
	auto							availableLayers = vk::enumerateInstanceLayerProperties();
	for (const auto& layer : availableLayers)
	{
		const std::string name = layer.layerName;
		if (optionalExtensions.layers.find(name) != optionalExtensions.layers.end())
		{
			enabledExtensions.layers.insert(name);
		}
		requiredLayers.erase(name);
	}

	if (!requiredLayers.empty())
	{
		std::stringstream ss;
		ss << "Cannot create Vulkan instance - missing required layers:";
		for (const auto& layer : requiredLayers)
		{
			ss << "\n  - " << layer;
		}
		HLVM_LOG(LogRHI, critical, TO_TCHAR_CSTR(ss.str().c_str()));
		return false;
	}
	{
		FString enabledLayersLog = FString::Join(enabledExtensions.layers, [](const auto& layer){return TCHARSTR(layer.c_str());}, TXT("\n"));
		HLVM_LOG(LogRHI, info, TO_TCHAR_CSTR("Enabled Vulkan layers:\n{}"), *enabledLayersLog);
	}

	// Query the Vulkan API version supported on the system to make sure we use at least 1.3 when that's present.
	vk::ApplicationInfo appInfo(
		"HLVM VK",
		VK_MAKE_VERSION(1, 0, 0),
		"HLVM VK Engine",
		VK_MAKE_VERSION(1, 0, 0),
		VK_API_VERSION_1_3);

	auto extensionsVec = StringSetToVector(enabledExtensions.instance);
	auto layersVec = StringSetToVector(enabledExtensions.layers);


	vk::Result res = vk::enumerateInstanceVersion(&appInfo.apiVersion);

	if (res != vk::Result::eSuccess)
	{
		HLVM_LOG(LogRHI, critical, TXT("Call to vkEnumerateInstanceVersion failed, error code = %s"), TO_TCHAR_CSTR(nvrhi::vulkan::resultToString(VkResult(res))));
		return false;
	}

	const uint32_t minimumVulkanVersion = VK_MAKE_API_VERSION(0, 1, 3, 0);

	// Check if the Vulkan API version is sufficient.
	if (appInfo.apiVersion < minimumVulkanVersion)
	{
		HLVM_LOG(LogRHI, critical, TXT("The Vulkan API version supported on the system ({}.{}.{}) is too low, at least {}.{}.{} is required."),
			VK_API_VERSION_MAJOR(appInfo.apiVersion), VK_API_VERSION_MINOR(appInfo.apiVersion), VK_API_VERSION_PATCH(appInfo.apiVersion),
			VK_API_VERSION_MAJOR(minimumVulkanVersion), VK_API_VERSION_MINOR(minimumVulkanVersion), VK_API_VERSION_PATCH(minimumVulkanVersion));
		return false;
	}

	// Spec says: A non-zero variant indicates the API is a variant of the Vulkan API.
	if (VK_API_VERSION_VARIANT(appInfo.apiVersion) != 0)
	{
		HLVM_LOG(LogRHI, critical, TXT("The Vulkan API supported on the system uses an unexpected variant: {}."), VK_API_VERSION_VARIANT(appInfo.apiVersion));
		return false;
	}

	// Create instance
	vk::InstanceCreateInfo createInfo;
	createInfo.setPApplicationInfo(&appInfo)
		.setEnabledExtensionCount(static_cast<uint32_t>(extensionsVec.size()))
		.setPpEnabledExtensionNames(extensionsVec.data())
		.setEnabledLayerCount(static_cast<uint32_t>(layersVec.size()))
		.setPpEnabledLayerNames(layersVec.data());

	// Debug messenger for instance creation/destruction
	vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo;
	if (DeviceParams.bEnableDebugRuntime)
	{
		PopulateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.setPNext(&debugCreateInfo);
	}

	try
	{
		auto inst = vk::createInstanceUnique(createInfo);
		HLVM_ASSERT(inst);
		instance = std::move(inst);
	}
	catch (std::system_error& e)
	{
		HLVM_LOG(LogRHI, critical, TO_TCHAR_CSTR(e.what()));
		return false;
	}

	hlvm_vk::InitVulkanLoaderInstance(*instance);

	return true;
}

// =============================================================================
// DEBUG MESSENGER
// =============================================================================

void FDeviceManagerVk::PopulateDebugMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT& createInfo)
{
	createInfo.setMessageSeverity(
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
	createInfo.setMessageType(
		vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance);
	createInfo.setPfnUserCallback(DebugCallback);
	createInfo.setPUserData(this);
}

void FDeviceManagerVk::SetupDebugMessenger()
{
	if (!DeviceParams.bEnableDebugRuntime)
		return;

	vk::DebugUtilsMessengerCreateInfoEXT createInfo;
	PopulateDebugMessengerCreateInfo(createInfo);

	try
	{
		auto messenger = instance->createDebugUtilsMessengerEXTUnique(createInfo);
		HLVM_ASSERT(messenger);
		debugMessenger = std::move(messenger);
	}
	catch (std::system_error& e)
	{
		HLVM_LOG(LogRHI, critical, TO_TCHAR_CSTR(e.what()));
	}
}

// =============================================================================
// SURFACE CREATION
// =============================================================================

bool FDeviceManagerVk::CreateWindowSurface()
{
	FGLFW3VulkanWindow* windowHandle = static_cast<FGLFW3VulkanWindow*>(WindowHandle.get());
	VkSurfaceKHR		rawSurface;
	rawSurface = windowHandle->CreateSurface(*instance);
	if (!rawSurface)
	{
		HLVM_LOG(LogRHI, critical, TXT("Failed to create window surface"));
		return false;
	}

	surface = vk::UniqueSurfaceKHR(
		vk::SurfaceKHR(rawSurface),
		vk::detail::ObjectDestroy<vk::Instance, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>(*instance));

	return true;
}
#endif
