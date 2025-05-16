/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "RHI/Vulkan/VulkanDefinition.h"

namespace VulkanRHI
{
	HLVM_EXTERN_FUNC VkAttachmentLoadOp VulkanAttachmentLoadOpFromRHIAction(ERenderTargetLoadAction RHIState);
	HLVM_EXTERN_FUNC VkAttachmentStoreOp VulkanAttachmentStoreOpFromRHIAction(ERenderTargetStoreAction RHIState);

	// Helper function to convert RHI pixel format to Vulkan format
	HLVM_EXTERN_FUNC VkFormat VulkanFormatFromRHIFormat(EPixelFormat RHIFormat, bool bSRGB = false);
	// Helper function to convert Vulkan format to RHI format
	HLVM_EXTERN_FUNC EPixelFormat RHIFormatFromVulkanFormat(VkFormat VulkanFormat);
	HLVM_EXTERN_FUNC EPixelFormat RHIFormatFromVulkanFormat(VkFormat VulkanFormat, bool& bSRGB_Out);

	// Helper function to convert RHI Vertex element to vk format
	HLVM_EXTERN_FUNC VkFormat RHIVertexElementTypeToVulkanFormat(EVertexElementType Type);

	// Helper function to convert RHI texture usage flags to Vulkan usage flags
	HLVM_EXTERN_FUNC VkImageUsageFlags VulkanTextureUsageFlagsFromRHIUsageFlags(ETextureCreateFlags RHIFlags);

	// Helper function to convert RHI buffer usage flags to Vulkan usage flags
	HLVM_EXTERN_FUNC VkBufferUsageFlags VulkanBufferUsageFlagsFromRHIUsageFlags(EBufferUsageFlags RHIFlags);

	// Helper function to convert RHI memory property flags to Vulkan memory property flags
	HLVM_EXTERN_FUNC VkMemoryPropertyFlags VulkanMemoryPropertyFlagsFromRHIMemoryPropertyFlags(EMemoryPropertyFlags RHIFlags);

	// Helper function to convert RHI texture filter to Vulkan filter
	HLVM_EXTERN_FUNC VkFilter VulkanFilterFromRHIFilter(ETextureFilter RHIFilter);

	// Helper function to convert RHI texture filter to Vulkan mip filter mode
	HLVM_EXTERN_FUNC VkSamplerMipmapMode VulkanMipFilterFromRHIFilter(ETextureFilter RHIFilter);

	// Helper function to convert RHI texture address mode to Vulkan address mode
	HLVM_EXTERN_FUNC VkSamplerAddressMode VulkanAddressModeFromRHIAddressMode(ETextureAddressMode RHIAddressMode);

	// Helper function to convert RHI compare function to Vulkan compare function
	HLVM_EXTERN_FUNC VkCompareOp VulkanCompareOpFromRHI(ECompareFunction RHIFunction);

	// Helper function to convert stencil op to Vulkan stencil op
	HLVM_EXTERN_FUNC VkStencilOp VulkanStencilOpFromRHI(EStencilOp RHIStencilOp);

	// Helper function to convert RHI primitive type to Vulkan primitive type
	HLVM_EXTERN_FUNC VkPrimitiveTopology VulkanPrimitiveTopologyFromRHIPrimitiveType(EPrimitiveType RHIPrimitiveType);

	// Helper function to convert RHI shader stage to Vulkan shader stage
	HLVM_EXTERN_FUNC VkShaderStageFlagBits VulkanShaderStageFromRHIStage(EShaderStage RHIShaderStage);

	HLVM_EXTERN_FUNC VkImageLayout VulkanGetMergedDepthStencilLayout(VkImageLayout DepthLayout, VkImageLayout StencilLayout);

	HLVM_EXTERN_FUNC bool VulkanFormatHasStencil(VkFormat Format);
} // namespace VulkanMisc
