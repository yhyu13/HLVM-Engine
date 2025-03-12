/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "RHI/Vulkan/VulkanLoader.h"
#include <mutex>

// Extern
VmaAllocator VULKAN_VMA_ALLOCATOR;
// Extern
VmaVulkanFunctions VULKAN_VMA_FUNCTIONS;
// Extern
VkAllocationCallbacks* VULKAN_CPU_ALLOCATOR = nullptr;

#define DEFINE_VK_FUNCTION_MACRO(function) \
	PFN_##function function = nullptr;
APPLY_PFN_DEF_VK_FUNCTIONS_CORE(DEFINE_VK_FUNCTION_MACRO)
APPLY_PFN_DEF_VK_FUNCTIONS_DISPLAY(DEFINE_VK_FUNCTION_MACRO)
APPLY_PFN_DEF_VK_FUNCTIONS_VMA(DEFINE_VK_FUNCTION_MACRO)

VulkanLoader::~VulkanLoader()
{
	vmaDestroyAllocator(VULKAN_VMA_ALLOCATOR);
}

void VulkanLoader::LoadOnce()
{
	static VulkanLoader   loader;
	static std::once_flag flag;
	std::call_once(flag, [&]() {
		loader.vklib = new dylib(VULKAN_LIB, false);
		auto& vulkanlib = *loader.vklib;
		HLVM_ENSURE_F(vulkanlib.native_handle() != nullptr, TXT("Failed to load vulkan library"));

#define GET_VK_FUNCTION_PROCADDR(function) \
	function = reinterpret_cast<PFN_##function>(vulkanlib.get_function<PFN_##function>(#function)); \
	HLVM_ENSURE_F(function != nullptr, TXT("Failed to load vulkan function: {}"), TXT(#function));

		APPLY_PFN_DEF_VK_FUNCTIONS_CORE(GET_VK_FUNCTION_PROCADDR)
		APPLY_PFN_DEF_VK_FUNCTIONS_DISPLAY(GET_VK_FUNCTION_PROCADDR)
		APPLY_PFN_DEF_VK_FUNCTIONS_VMA(GET_VK_FUNCTION_PROCADDR)
		{
			auto& vulkanFunctions = VULKAN_VMA_FUNCTIONS;
			vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
			vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
			vulkanFunctions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
			vulkanFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
			vulkanFunctions.vkAllocateMemory = vkAllocateMemory;
			vulkanFunctions.vkFreeMemory = vkFreeMemory;
			vulkanFunctions.vkMapMemory = vkMapMemory;
			vulkanFunctions.vkUnmapMemory = vkUnmapMemory;
			vulkanFunctions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
			vulkanFunctions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
			vulkanFunctions.vkBindBufferMemory = vkBindBufferMemory;
			vulkanFunctions.vkBindImageMemory = vkBindImageMemory;
			vulkanFunctions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
			vulkanFunctions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
			vulkanFunctions.vkCreateBuffer = vkCreateBuffer;
			vulkanFunctions.vkDestroyBuffer = vkDestroyBuffer;
			vulkanFunctions.vkCreateImage = vkCreateImage;
			vulkanFunctions.vkDestroyImage = vkDestroyImage;
			vulkanFunctions.vkCmdCopyBuffer = vkCmdCopyBuffer;
#if VMA_DEDICATED_ALLOCATION || VMA_VULKAN_VERSION >= 1001000
			vulkanFunctions.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2;
			vulkanFunctions.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2;
#endif
#if VMA_BIND_MEMORY2 || VMA_VULKAN_VERSION >= 1001000
			vulkanFunctions.vkBindBufferMemory2KHR = vkBindBufferMemory2;
			vulkanFunctions.vkBindImageMemory2KHR = vkBindImageMemory2;
#endif
#if VMA_MEMORY_BUDGET || VMA_VULKAN_VERSION >= 1001000
			vulkanFunctions.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2;
#endif
#if VMA_VULKAN_VERSION >= 1003000
			vulkanFunctions.vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements;
			vulkanFunctions.vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements;
#endif
// TODO : move to RHI
//			VmaAllocatorCreateInfo allocatorCreateInfo = {};
//			allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
//			allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
//			allocatorCreateInfo.physicalDevice = physicalDevice;
//			allocatorCreateInfo.device = device;
//			allocatorCreateInfo.instance = instance;
//			allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;
//
//			vmaCreateAllocator(&allocatorCreateInfo, VULKAN_VMA_ALLOCATOR);
		}
	});
}

