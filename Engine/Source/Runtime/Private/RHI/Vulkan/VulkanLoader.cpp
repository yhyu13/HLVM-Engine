/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "RHI/Vulkan/VulkanLoader.h"
#include <mutex>

#define DEFINE_VK_FUNCTION_MACRO(function) \
	PFN_##function function = nullptr;
APPLY_PFN_DEF_VK_FUNCTIONS(DEFINE_VK_FUNCTION_MACRO)

void VulkanLoader::LoadOnce()
{
	static dylib		  vulkanlib(VULKAN_LIB, false);
	static std::once_flag flag;
	std::call_once(flag, [&]() {
		HLVM_ENSURE(vulkanlib.native_handle() != nullptr, TXT("Failed to load vulkan library"));

#define GET_VK_FUNCTION_PROCADDR(function) \
	function = reinterpret_cast<PFN_##function>(vulkanlib.get_function<PFN_##function>(#function));

		APPLY_PFN_DEF_VK_FUNCTIONS(GET_VK_FUNCTION_PROCADDR)
	});
}
