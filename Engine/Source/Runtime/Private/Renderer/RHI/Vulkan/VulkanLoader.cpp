/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

#include "Renderer/RHI/Vulkan/VulkanLoader.h"
#include <mutex>

#if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#endif

namespace hlvm_vk
{
	static std::once_flag flag;
	static bool			  g_bVulkanLoaderInitialized = false;
	static bool			  g_bVulkanLoaderInstanceInitialized = false;
	static bool			  g_bVulkanLoaderDeviceInitialized = false;

	bool IsVulkanLoaderAPIInitialized()
	{
#if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
		return g_bVulkanLoaderInitialized;
#else
		return true;
#endif
	}

	void InitVulkanLoaderAPIOnce()
	{
#if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
		static vk::detail::DynamicLoader dl(VULKAN_LIB);
		std::call_once(flag, [&]() {
			PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
			VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
			g_bVulkanLoaderInitialized = true;
		});
#endif
	}

	bool IsVulkanLoaderInstanceAPIInitialized()
	{
#if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
		return g_bVulkanLoaderInstanceInitialized;
#else
		return true;
#endif
	}

	void InitVulkanLoaderInstance(vk::Instance& instance)
	{
#if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
		HLVM_ENSURE(IsVulkanLoaderAPIInitialized());
		VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);
		g_bVulkanLoaderInstanceInitialized = true;
#endif
	}

	bool IsVulkanLoaderDeviceAPIInitialized()
	{
#if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
		return g_bVulkanLoaderDeviceInitialized;
#else
		return true;
#endif
	}

	void InitVulkanLoaderDevice(vk::Device& device)
	{
#if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
		HLVM_ENSURE(IsVulkanLoaderInstanceAPIInitialized());
		VULKAN_HPP_DEFAULT_DISPATCHER.init(device);
		g_bVulkanLoaderDeviceInitialized = true;
#endif
	}
}
