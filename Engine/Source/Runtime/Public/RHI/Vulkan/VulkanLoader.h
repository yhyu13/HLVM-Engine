/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "VulkanDefinition.h"

#if VULKAN_USE_VMA
	#include <vk_mem_alloc.h>
#endif

/*
 * Inspired by https://gitee.com/sumcai/MiniVulkanTriangle to load vulkan api during runtime
 */
#include <dylib.hpp>
#if defined(WIN32) || defined(_WIN32) || defined(_WIN32_) || defined(WIN64) || defined(_WIN64) || defined(_WIN64_)
	#define VULKAN_LIB "vulkan-1.dll"
#elif defined(ANDROID) || defined(_ANDROID_)
	#define VULKAN_LIB "libvulkan.so"
#else
	#define VULKAN_LIB "libvulkan.so.1"
#endif

#define APPLY_PFN_DEF_VK_FUNCTIONS_CORE(PFN_DEF)            \
	PFN_DEF(vkGetInstanceProcAddr)                          \
	PFN_DEF(vkCreateInstance)                               \
	PFN_DEF(vkEnumerateInstanceExtensionProperties)         \
	PFN_DEF(vkEnumerateInstanceLayerProperties)             \
	PFN_DEF(vkDestroyInstance)                              \
	PFN_DEF(vkEnumeratePhysicalDevices)                     \
	PFN_DEF(vkGetPhysicalDeviceFeatures)                    \
	PFN_DEF(vkGetPhysicalDeviceFormatProperties)            \
	PFN_DEF(vkGetPhysicalDeviceImageFormatProperties)       \
	PFN_DEF(vkGetPhysicalDeviceProperties)                  \
	PFN_DEF(vkGetPhysicalDeviceProperties2)                 \
	PFN_DEF(vkGetPhysicalDeviceQueueFamilyProperties)       \
	PFN_DEF(vkGetPhysicalDeviceMemoryProperties)            \
	PFN_DEF(vkGetDeviceProcAddr)                            \
	PFN_DEF(vkCreateDevice)                                 \
	PFN_DEF(vkDestroySurfaceKHR)                            \
	PFN_DEF(vkGetPhysicalDeviceSurfaceSupportKHR)           \
	PFN_DEF(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)      \
	PFN_DEF(vkGetPhysicalDeviceSurfaceFormatsKHR)           \
	PFN_DEF(vkGetPhysicalDeviceSurfacePresentModesKHR)      \
	PFN_DEF(vkCreateSwapchainKHR)                           \
	PFN_DEF(vkDestroySwapchainKHR)                          \
	PFN_DEF(vkGetSwapchainImagesKHR)                        \
	PFN_DEF(vkAcquireNextImageKHR)                          \
	PFN_DEF(vkQueuePresentKHR)                              \
	PFN_DEF(vkDestroyDevice)                                \
	PFN_DEF(vkEnumerateDeviceExtensionProperties)           \
	PFN_DEF(vkEnumerateDeviceLayerProperties)               \
	PFN_DEF(vkGetDeviceQueue)                               \
	PFN_DEF(vkQueueSubmit)                                  \
	PFN_DEF(vkQueueWaitIdle)                                \
	PFN_DEF(vkDeviceWaitIdle)                               \
	PFN_DEF(vkAllocateMemory)                               \
	PFN_DEF(vkFreeMemory)                                   \
	PFN_DEF(vkMapMemory)                                    \
	PFN_DEF(vkUnmapMemory)                                  \
	PFN_DEF(vkFlushMappedMemoryRanges)                      \
	PFN_DEF(vkInvalidateMappedMemoryRanges)                 \
	PFN_DEF(vkGetDeviceMemoryCommitment)                    \
	PFN_DEF(vkBindBufferMemory)                             \
	PFN_DEF(vkBindImageMemory)                              \
	PFN_DEF(vkGetBufferMemoryRequirements)                  \
	PFN_DEF(vkGetImageMemoryRequirements)                   \
	PFN_DEF(vkGetImageSparseMemoryRequirements)             \
	PFN_DEF(vkGetPhysicalDeviceSparseImageFormatProperties) \
	PFN_DEF(vkQueueBindSparse)                              \
	PFN_DEF(vkCreateFence)                                  \
	PFN_DEF(vkDestroyFence)                                 \
	PFN_DEF(vkResetFences)                                  \
	PFN_DEF(vkGetFenceStatus)                               \
	PFN_DEF(vkWaitForFences)                                \
	PFN_DEF(vkCreateSemaphore)                              \
	PFN_DEF(vkDestroySemaphore)                             \
	PFN_DEF(vkCreateEvent)                                  \
	PFN_DEF(vkDestroyEvent)                                 \
	PFN_DEF(vkGetEventStatus)                               \
	PFN_DEF(vkSetEvent)                                     \
	PFN_DEF(vkResetEvent)                                   \
	PFN_DEF(vkCreateQueryPool)                              \
	PFN_DEF(vkDestroyQueryPool)                             \
	PFN_DEF(vkGetQueryPoolResults)                          \
	PFN_DEF(vkCreateBuffer)                                 \
	PFN_DEF(vkDestroyBuffer)                                \
	PFN_DEF(vkCreateBufferView)                             \
	PFN_DEF(vkDestroyBufferView)                            \
	PFN_DEF(vkCreateImage)                                  \
	PFN_DEF(vkDestroyImage)                                 \
	PFN_DEF(vkGetImageSubresourceLayout)                    \
	PFN_DEF(vkCreateImageView)                              \
	PFN_DEF(vkDestroyImageView)                             \
	PFN_DEF(vkCreateShaderModule)                           \
	PFN_DEF(vkDestroyShaderModule)                          \
	PFN_DEF(vkCreatePipelineCache)                          \
	PFN_DEF(vkDestroyPipelineCache)                         \
	PFN_DEF(vkGetPipelineCacheData)                         \
	PFN_DEF(vkMergePipelineCaches)                          \
	PFN_DEF(vkCreateGraphicsPipelines)                      \
	PFN_DEF(vkCreateComputePipelines)                       \
	PFN_DEF(vkDestroyPipeline)                              \
	PFN_DEF(vkCreatePipelineLayout)                         \
	PFN_DEF(vkDestroyPipelineLayout)                        \
	PFN_DEF(vkCreateSampler)                                \
	PFN_DEF(vkDestroySampler)                               \
	PFN_DEF(vkCreateDescriptorSetLayout)                    \
	PFN_DEF(vkDestroyDescriptorSetLayout)                   \
	PFN_DEF(vkCreateDescriptorPool)                         \
	PFN_DEF(vkDestroyDescriptorPool)                        \
	PFN_DEF(vkResetDescriptorPool)                          \
	PFN_DEF(vkAllocateDescriptorSets)                       \
	PFN_DEF(vkFreeDescriptorSets)                           \
	PFN_DEF(vkUpdateDescriptorSets)                         \
	PFN_DEF(vkCreateFramebuffer)                            \
	PFN_DEF(vkDestroyFramebuffer)                           \
	PFN_DEF(vkCreateRenderPass)                             \
	PFN_DEF(vkDestroyRenderPass)                            \
	PFN_DEF(vkGetRenderAreaGranularity)                     \
	PFN_DEF(vkCreateCommandPool)                            \
	PFN_DEF(vkDestroyCommandPool)                           \
	PFN_DEF(vkResetCommandPool)                             \
	PFN_DEF(vkAllocateCommandBuffers)                       \
	PFN_DEF(vkFreeCommandBuffers)                           \
	PFN_DEF(vkBeginCommandBuffer)                           \
	PFN_DEF(vkEndCommandBuffer)                             \
	PFN_DEF(vkResetCommandBuffer)                           \
	PFN_DEF(vkCmdBindPipeline)                              \
	PFN_DEF(vkCmdSetViewport)                               \
	PFN_DEF(vkCmdSetScissor)                                \
	PFN_DEF(vkCmdSetLineWidth)                              \
	PFN_DEF(vkCmdSetDepthBias)                              \
	PFN_DEF(vkCmdSetBlendConstants)                         \
	PFN_DEF(vkCmdSetDepthBounds)                            \
	PFN_DEF(vkCmdSetStencilCompareMask)                     \
	PFN_DEF(vkCmdSetStencilWriteMask)                       \
	PFN_DEF(vkCmdSetStencilReference)                       \
	PFN_DEF(vkCmdBindDescriptorSets)                        \
	PFN_DEF(vkCmdBindIndexBuffer)                           \
	PFN_DEF(vkCmdBindVertexBuffers)                         \
	PFN_DEF(vkCmdDraw)                                      \
	PFN_DEF(vkCmdDrawIndexed)                               \
	PFN_DEF(vkCmdDrawIndirect)                              \
	PFN_DEF(vkCmdDrawIndexedIndirect)                       \
	PFN_DEF(vkCmdDispatch)                                  \
	PFN_DEF(vkCmdDispatchIndirect)                          \
	PFN_DEF(vkCmdCopyBuffer)                                \
	PFN_DEF(vkCmdCopyImage)                                 \
	PFN_DEF(vkCmdBlitImage)                                 \
	PFN_DEF(vkCmdCopyBufferToImage)                         \
	PFN_DEF(vkCmdCopyImageToBuffer)                         \
	PFN_DEF(vkCmdUpdateBuffer)                              \
	PFN_DEF(vkCmdFillBuffer)                                \
	PFN_DEF(vkCmdClearColorImage)                           \
	PFN_DEF(vkCmdClearDepthStencilImage)                    \
	PFN_DEF(vkCmdClearAttachments)                          \
	PFN_DEF(vkCmdResolveImage)                              \
	PFN_DEF(vkCmdSetEvent)                                  \
	PFN_DEF(vkCmdResetEvent)                                \
	PFN_DEF(vkCmdWaitEvents)                                \
	PFN_DEF(vkCmdPipelineBarrier)                           \
	PFN_DEF(vkCmdBeginQuery)                                \
	PFN_DEF(vkCmdEndQuery)                                  \
	PFN_DEF(vkCmdResetQueryPool)                            \
	PFN_DEF(vkCmdWriteTimestamp)                            \
	PFN_DEF(vkCmdCopyQueryPoolResults)                      \
	PFN_DEF(vkCmdPushConstants)                             \
	PFN_DEF(vkCmdBeginRenderPass)                           \
	PFN_DEF(vkCmdNextSubpass)                               \
	PFN_DEF(vkCmdEndRenderPass)                             \
	PFN_DEF(vkCmdExecuteCommands)

#if VULKAN_DISPLAY_KHR
	#define APPLY_PFN_DEF_VK_FUNCTIONS_DISPLAY(PFN_DEF)       \
		PFN_DEF(vkCreateDisplayModeKHR)                       \
		PFN_DEF(vkCreateDisplayPlaneSurfaceKHR)               \
		PFN_DEF(vkGetDisplayModePropertiesKHR)                \
		PFN_DEF(vkGetDisplayPlaneCapabilitiesKHR)             \
		PFN_DEF(vkGetDisplayPlaneSupportedDisplaysKHR)        \
		PFN_DEF(vkGetPhysicalDeviceDisplayPlanePropertiesKHR) \
		PFN_DEF(vkGetPhysicalDeviceDisplayPropertiesKHR)
#else
	#define APPLY_PFN_DEF_VK_FUNCTIONS_DISPLAY(...)
#endif /* VULKAN_DISPLAY_KHR */

#if VULKAN_RENDERPASS2
	#define APPLY_PFN_DEF_VK_FUNCTIONS_RENDERPASS2(PFN_DEF) \
		PFN_DEF(vkCreateRenderPass2KHR)                     \
		PFN_DEF(vkCmdBeginRenderPass2KHR)                   \
		PFN_DEF(vkCmdNextSubpass2KHR)                       \
		PFN_DEF(vkCmdEndRenderPass2KHR)
#else
	#define APPLY_PFN_DEF_VK_FUNCTIONS_RENDERPASS2(...)
#endif /* VULKAN_RENDERPASS2 */

#if VULKAN_USE_VMA
	#if VMA_DEDICATED_ALLOCATION || VMA_VULKAN_VERSION >= 1001000
		#define APPLY_PFN_DEF_VK_FUNCTIONS_VMA_1(PFN_DEF) \
			PFN_DEF(vkGetBufferMemoryRequirements2)       \
			PFN_DEF(vkGetImageMemoryRequirements2)
	#else
		#define APPLY_PFN_DEF_VK_FUNCTIONS_VMA_1(...)
	#endif
	#if VMA_BIND_MEMORY2 || VMA_VULKAN_VERSION >= 1001000
		#define APPLY_PFN_DEF_VK_FUNCTIONS_VMA_2(PFN_DEF) \
			PFN_DEF(vkBindBufferMemory2)                  \
			PFN_DEF(vkBindImageMemory2)
	#else
		#define APPLY_PFN_DEF_VK_FUNCTIONS_VMA_2(...)
	#endif
	#if VMA_MEMORY_BUDGET || VMA_VULKAN_VERSION >= 1001000
		#define APPLY_PFN_DEF_VK_FUNCTIONS_VMA_3(PFN_DEF) \
			PFN_DEF(vkGetPhysicalDeviceMemoryProperties2)
	#else
		#define APPLY_PFN_DEF_VK_FUNCTIONS_VMA_3(...)
	#endif
	#if VMA_VULKAN_VERSION >= 1003000
		#define APPLY_PFN_DEF_VK_FUNCTIONS_VMA_4(PFN_DEF) \
			PFN_DEF(vkGetDeviceBufferMemoryRequirements)  \
			PFN_DEF(vkGetDeviceImageMemoryRequirements)
	#else
		#define APPLY_PFN_DEF_VK_FUNCTIONS_VMA_4(...)
	#endif
	#define APPLY_PFN_DEF_VK_FUNCTIONS_VMA(PFN_DEF) \
		APPLY_PFN_DEF_VK_FUNCTIONS_VMA_1(PFN_DEF)   \
		APPLY_PFN_DEF_VK_FUNCTIONS_VMA_2(PFN_DEF)   \
		APPLY_PFN_DEF_VK_FUNCTIONS_VMA_3(PFN_DEF)   \
		APPLY_PFN_DEF_VK_FUNCTIONS_VMA_4(PFN_DEF)
#else
	#define APPLY_PFN_DEF_VK_FUNCTIONS_VMA(...)
#endif /* VULKAN_USE_VMA */

#define APPLY_TO_ALL_PFN(macro)                   \
	APPLY_PFN_DEF_VK_FUNCTIONS_CORE(macro)        \
	APPLY_PFN_DEF_VK_FUNCTIONS_DISPLAY(macro)     \
	APPLY_PFN_DEF_VK_FUNCTIONS_RENDERPASS2(macro) \
	APPLY_PFN_DEF_VK_FUNCTIONS_VMA(macro)

namespace VulkanRHI
{
#define DECLARE_VK_FUNCTION_MACRO(function) \
	HLVM_EXTERN_FUNC PFN_##function function;

	APPLY_TO_ALL_PFN(DECLARE_VK_FUNCTION_MACRO)

	// VK RHI Globals
	HLVM_EXTERN_VAR VmaAllocator		   VULKAN_VMA_ALLOCATOR;
	HLVM_EXTERN_VAR VmaVulkanFunctions	   VULKAN_VMA_FUNCTIONS;
	HLVM_EXTERN_VAR VkAllocationCallbacks* VULKAN_CPU_ALLOCATOR; // the cpu allocator is just a placeholder now
} // namespace VulkanRHI

class VulkanLoader
{
public:
	NOCOPYMOVE(VulkanLoader)
	VulkanLoader() = default;
	~VulkanLoader();

public:
	HLVM_STATIC_FUNC void LoadOnce();

private:
	dylib* vklib;
};
