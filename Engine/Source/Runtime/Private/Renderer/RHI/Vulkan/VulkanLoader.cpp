/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "Renderer/RHI/Vulkan/VulkanLoader.h"
#include <mutex>

#if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#endif

static std::once_flag flag;
void				  InitVulkanLoaderOnce()
{
#if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
	static vk::detail::DynamicLoader dl(VULKAN_LIB);
	std::call_once(flag, [&]() {
		PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
		VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
	});
#endif
}
