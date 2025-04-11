/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "VulkanMisc.h"

namespace VulkanRHI
{
	VkAttachmentLoadOp VulkanAttachmentLoadOpFromRHI(ERenderTargetLoadAction RHIState)
	{
		switch (RHIState)
		{
			case ERenderTargetLoadAction::Clear:
				return VK_ATTACHMENT_LOAD_OP_CLEAR;
			case ERenderTargetLoadAction::DontCare:
				return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			case ERenderTargetLoadAction::Load:
				return VK_ATTACHMENT_LOAD_OP_LOAD;
			default:
				HLVM_ASSERT_F(false, TXT("Unknown RHI load action"));
				return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		}
	}

	VkAttachmentStoreOp VulkanAttachmentStoreOpFromRHI(ERenderTargetStoreAction RHIState)
	{
		switch (RHIState)
		{
			case ERenderTargetStoreAction::Store:
				return VK_ATTACHMENT_STORE_OP_STORE;
			case ERenderTargetStoreAction::DontCare:
			case ERenderTargetStoreAction::MultisampleResolve:
				return VK_ATTACHMENT_STORE_OP_DONT_CARE;
			default:
				HLVM_ASSERT_F(false, TXT("Unknown RHI store action"));
				return VK_ATTACHMENT_STORE_OP_DONT_CARE;
		}
	}

	VkFormat VulkanFormatFromRHIFormat(EPixelFormat RHIFormat)
	{
		switch (RHIFormat)
		{
			case EPixelFormat::None:
				return VK_FORMAT_UNDEFINED;
			case EPixelFormat::R8_UNorm:
				return VK_FORMAT_R8_UNORM;
			case EPixelFormat::R8G8_UNorm:
				return VK_FORMAT_R8G8_UNORM;
			case EPixelFormat::R8G8B8A8_UNorm:
				return VK_FORMAT_R8G8B8A8_UNORM;
			case EPixelFormat::B8G8R8A8_SRGB:
				return VK_FORMAT_B8G8R8A8_SRGB;
			case EPixelFormat::R16_UNorm:
				return VK_FORMAT_R16_UNORM;
			case EPixelFormat::R16G16_UNorm:
				return VK_FORMAT_R16G16_UNORM;
			case EPixelFormat::R16G16B16A16_UNorm:
				return VK_FORMAT_R16G16B16A16_UNORM;
			case EPixelFormat::R32_UInt:
				return VK_FORMAT_R32_UINT;
			case EPixelFormat::R32G32_UInt:
				return VK_FORMAT_R32G32_UINT;
			case EPixelFormat::R32G32B32A32_UInt:
				return VK_FORMAT_R32G32B32A32_UINT;
			case EPixelFormat::R32_Float:
				return VK_FORMAT_R32_SFLOAT;
			case EPixelFormat::R32G32_Float:
				return VK_FORMAT_R32G32_SFLOAT;
			case EPixelFormat::R32G32B32A32_Float:
				return VK_FORMAT_R32G32B32A32_SFLOAT;
			case EPixelFormat::D16_UNorm:
				return VK_FORMAT_D16_UNORM;
			case EPixelFormat::D24_UNorm_S8_UInt:
				return VK_FORMAT_D24_UNORM_S8_UINT;
			case EPixelFormat::D32_Float:
				return VK_FORMAT_D32_SFLOAT;
			case EPixelFormat::D32_Float_S8_UInt:
				return VK_FORMAT_D32_SFLOAT_S8_UINT; // Add more formats as needed
			default:
				HLVM_ASSERT_F(false, TXT("Unknown RHI format {}"), HLVM_ENUM_VALUE_TO_TCHAR(RHIFormat));
				return VK_FORMAT_UNDEFINED;
		}
	}

	// Convert RHI pixel format to Vulkan format
	EPixelFormat RHIFormatFromVulkanFormat(VkFormat VulkanFormat)
	{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch-enum"
		switch (VulkanFormat)
		{
			case VK_FORMAT_UNDEFINED:
				return EPixelFormat::None;
			case VK_FORMAT_R8_UNORM:
				return EPixelFormat::R8_UNorm;
			case VK_FORMAT_R8G8_UNORM:
				return EPixelFormat::R8G8_UNorm;
			case VK_FORMAT_R8G8B8A8_UNORM:
				return EPixelFormat::R8G8B8A8_UNorm;
			case VK_FORMAT_B8G8R8A8_SRGB:
				return EPixelFormat::B8G8R8A8_SRGB;
			case VK_FORMAT_R16_UNORM:
				return EPixelFormat::R16_UNorm;
			case VK_FORMAT_R16G16_UNORM:
				return EPixelFormat::R16G16_UNorm;
			case VK_FORMAT_R16G16B16A16_UNORM:
				return EPixelFormat::R16G16B16A16_UNorm;
			case VK_FORMAT_R32_UINT:
				return EPixelFormat::R32_UInt;
			case VK_FORMAT_R32G32_UINT:
				return EPixelFormat::R32G32_UInt;
			case VK_FORMAT_R32G32B32A32_UINT:
				return EPixelFormat::R32G32B32A32_UInt;
			case VK_FORMAT_R32_SFLOAT:
				return EPixelFormat::R32_Float;
			case VK_FORMAT_R32G32_SFLOAT:
				return EPixelFormat::R32G32_Float;
			case VK_FORMAT_R32G32B32A32_SFLOAT:
				return EPixelFormat::R32G32B32A32_Float;
			case VK_FORMAT_D16_UNORM:
				return EPixelFormat::D16_UNorm;
			case VK_FORMAT_D24_UNORM_S8_UINT:
				return EPixelFormat::D24_UNorm_S8_UInt;
			case VK_FORMAT_D32_SFLOAT:
				return EPixelFormat::D32_Float;
			case VK_FORMAT_D32_SFLOAT_S8_UINT:
				return EPixelFormat::D32_Float_S8_UInt;
				// Add more formats as needed
			default:
				HLVM_ASSERT_F(false, TXT("Unknown Vulkan format {}"), VULKAN_FORMAT_TO_TCHAR(VulkanFormat));
				return EPixelFormat::None;
		}
#pragma clang diagnostic pop
	}

	// Convert RHI buffer usage flags to Vulkan usage flags
	VkBufferUsageFlags VulkanBufferUsageFlagsFromRHIUsageFlags(EBufferUsageFlags RHIFlags)
	{
		VkBufferUsageFlags VulkanFlags = 0;

		if (RHIFlags & EBufferUsageFlag::Vertex)
		{
			VulkanFlags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		}
		if (RHIFlags & EBufferUsageFlag::Index)
		{
			VulkanFlags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		}
		if (RHIFlags & EBufferUsageFlag::Uniform)
		{
			VulkanFlags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		}
		if (RHIFlags & EBufferUsageFlag::Storage)
		{
			VulkanFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		}
		if (RHIFlags & EBufferUsageFlag::ShaderResource)
		{
			VulkanFlags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		}
		if (RHIFlags & EBufferUsageFlag::TransferSource)
		{
			VulkanFlags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		}
		if (RHIFlags & EBufferUsageFlag::TransferDestination)
		{
			VulkanFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		}

		return VulkanFlags;
	}

	VkMemoryPropertyFlags VulkanMemoryPropertyFlagsFromRHIMemoryPropertyFlags(EMemoryPropertyFlags RHIFlags)
	{
		VkMemoryPropertyFlags VulkanFlags = 0;

		if (RHIFlags & EMemoryPropertyFlag::DeviceLocal)
		{
			VulkanFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		}
		if (RHIFlags & EMemoryPropertyFlag::HostVisible)
		{
			VulkanFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		}
		if (RHIFlags & EMemoryPropertyFlag::HostCoherent)
		{
			VulkanFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		}
		if (RHIFlags & EMemoryPropertyFlag::HostCached)
		{
			VulkanFlags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
		}
		if (RHIFlags & EMemoryPropertyFlag::DeviceCoherentAMD)
		{
			VulkanFlags |= VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD;
		}
		if (RHIFlags & EMemoryPropertyFlag::Protected)
		{
			VulkanFlags |= VK_MEMORY_PROPERTY_PROTECTED_BIT;
		}
		if (RHIFlags & EMemoryPropertyFlag::RDMACapableNV)
		{
			VulkanFlags |= VK_MEMORY_PROPERTY_RDMA_CAPABLE_BIT_NV;
		}
		if (RHIFlags & EMemoryPropertyFlag::LazilyAllocated)
		{
			VulkanFlags |= VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
		}
		return VulkanFlags;
	}

	// Convert RHI texture usage flags to Vulkan usage flags
	VkImageUsageFlags VulkanTextureUsageFlagsFromRHIUsageFlags(ETextureCreateFlags RHIFlags)
	{
		VkImageUsageFlags VulkanFlags = 0;

		if (RHIFlags & ETextureCreateFlag::RenderTarget)
		{
			VulkanFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		}
		if (RHIFlags & ETextureCreateFlag::DepthStencil)
		{
			VulkanFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		}
		if (RHIFlags & ETextureCreateFlag::ShaderResource)
		{
			VulkanFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
		}
		if (RHIFlags & ETextureCreateFlag::ShaderWrite)
		{
			VulkanFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
		}
		if (RHIFlags & ETextureCreateFlag::Transient)
		{
			VulkanFlags |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
		}

		return VulkanFlags;
	}

	// Helper function to convert RHI texture filter to Vulkan filter
	VkFilter VulkanFilterFromRHIFilter(ETextureFilter RHIFilter)
	{
		switch (RHIFilter)
		{
			case ETextureFilter::None:
				return VK_FILTER_NEAREST;
			case ETextureFilter::Point:
				return VK_FILTER_NEAREST;
			case ETextureFilter::Linear:
				return VK_FILTER_LINEAR;
			case ETextureFilter::Anisotropic:
				return VK_FILTER_LINEAR; // Vulkan does not have a direct anisotropic filter mode
			// Add more cases as needed
			default:
				HLVM_ASSERT_F(false, TXT("Unknown RHI filter"));
				return VK_FILTER_NEAREST;
		}
	}

	// Helper function to convert RHI texture address mode to Vulkan address mode
	VkSamplerAddressMode VulkanAddressModeFromRHIAddressMode(ETextureAddressMode RHIAddressMode)
	{
		switch (RHIAddressMode)
		{
			case ETextureAddressMode::None:
				return VK_SAMPLER_ADDRESS_MODE_REPEAT;
			case ETextureAddressMode::Wrap:
				return VK_SAMPLER_ADDRESS_MODE_REPEAT;
			case ETextureAddressMode::Clamp:
				return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			case ETextureAddressMode::Mirror:
				return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
			case ETextureAddressMode::Border:
				return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			// Add more cases as needed
			default:
				HLVM_ASSERT_F(false, TXT("Unknown RHI address mode"));
				return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		}
	}

	// Helper function to convert RHI compare function to Vulkan compare function
	VkCompareOp VulkanCompareOpFromRHICompareFunction(ERHICompare RHIFunction)
	{
		switch (RHIFunction)
		{
			case ERHICompare::Never:
				return VK_COMPARE_OP_NEVER;
			case ERHICompare::Less:
				return VK_COMPARE_OP_LESS;
			case ERHICompare::Equal:
				return VK_COMPARE_OP_EQUAL;
			case ERHICompare::LessEqual:
				return VK_COMPARE_OP_LESS_OR_EQUAL;
			case ERHICompare::Greater:
				return VK_COMPARE_OP_GREATER;
			case ERHICompare::NotEqual:
				return VK_COMPARE_OP_NOT_EQUAL;
			case ERHICompare::GreaterEqual:
				return VK_COMPARE_OP_GREATER_OR_EQUAL;
			case ERHICompare::Always:
				return VK_COMPARE_OP_ALWAYS;
			default:
				HLVM_ASSERT_F(false, TXT("Unknown RHI compare function"));
				return VK_COMPARE_OP_NEVER;
		}
	}

	// Helper function to convert RHI shader stage to Vulkan shader stage
	VkShaderStageFlagBits VulkanShaderStageFromRHIStage(EShaderStage RHIShaderStage)
	{
		switch (RHIShaderStage)
		{
			case EShaderStage::Vertex:
				return VK_SHADER_STAGE_VERTEX_BIT;
			case EShaderStage::Pixel:
				return VK_SHADER_STAGE_FRAGMENT_BIT;
			case EShaderStage::Compute:
				return VK_SHADER_STAGE_COMPUTE_BIT;
			case EShaderStage::Geometry:
				return VK_SHADER_STAGE_GEOMETRY_BIT;
			case EShaderStage::Hull:
				return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
			case EShaderStage::Domain:
				return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
			case EShaderStage::RayGeneration:
				return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
			case EShaderStage::Intersection:
				return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
			case EShaderStage::AnyHit:
				return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
			case EShaderStage::ClosestHit:
				return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
			case EShaderStage::Miss:
				return VK_SHADER_STAGE_MISS_BIT_KHR;
			case EShaderStage::Callable:
				return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
			default:
				HLVM_ASSERT_F(false, TXT("Unknown RHI shader stage"));
				return VK_SHADER_STAGE_VERTEX_BIT;
		}
	}

	VkImageLayout VulkanGetMergedDepthStencilLayout(VkImageLayout DepthLayout, VkImageLayout StencilLayout)
	{
		if ((DepthLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) || (StencilLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL))
		{
			HLVM_ASSERT_F(StencilLayout == DepthLayout,
				TXT("You can't merge transfer src layout without anything else than transfer src ({} != {}). {}"),
				VULKAN_TYPE_TO_FSTRING(VkImageLayout, DepthLayout), VULKAN_TYPE_TO_FSTRING(VkImageLayout, StencilLayout),
				TXT("You need either VK_KHR_separate_depth_stencil_layouts or GRHISupportsSeparateDepthStencilCopyAccess enabled."));
			return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		}

		if ((DepthLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) || (StencilLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL))
		{
			HLVM_ASSERT_F(StencilLayout == DepthLayout,
				TXT("You can't merge transfer dst layout without anything else than transfer dst ({} != {}). {}"),
				VULKAN_TYPE_TO_FSTRING(VkImageLayout, DepthLayout), VULKAN_TYPE_TO_FSTRING(VkImageLayout, StencilLayout),
				TXT("You need either VK_KHR_separate_depth_stencil_layouts or GRHISupportsSeparateDepthStencilCopyAccess enabled."));
			return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		}

		if ((DepthLayout == VK_IMAGE_LAYOUT_UNDEFINED) && (StencilLayout == VK_IMAGE_LAYOUT_UNDEFINED))
		{
			return VK_IMAGE_LAYOUT_UNDEFINED;
		}

		// Depth formats used on textures that aren't targets (like GBlackTextureDepthCube)
		if ((DepthLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) && (StencilLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
		{
			return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}

		auto IsMergedLayout = [](VkImageLayout Layout) {
			return (Layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) || (Layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL) || (Layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL) || (Layout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL);
		};

		if (IsMergedLayout(DepthLayout) || IsMergedLayout(StencilLayout))
		{
			HLVM_ASSERT_F(StencilLayout == DepthLayout,
				TXT("Layouts were already merged but they are mismatched ({} != {})."),
				VULKAN_TYPE_TO_FSTRING(VkImageLayout, DepthLayout), VULKAN_TYPE_TO_FSTRING(VkImageLayout, StencilLayout));
			return DepthLayout;
		}

		if (DepthLayout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL)
		{
			if ((StencilLayout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL) || (StencilLayout == VK_IMAGE_LAYOUT_UNDEFINED))
			{
				return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			}
			else
			{
				HLVM_ASSERT(StencilLayout == VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);
				return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;
			}
		}
		else if (DepthLayout == VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL)
		{
			if ((StencilLayout == VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL) || (StencilLayout == VK_IMAGE_LAYOUT_UNDEFINED))
			{
				return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			}
			else
			{
				HLVM_ASSERT(StencilLayout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
				return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
			}
		}
		else
		{
			return (StencilLayout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		}
	}

	bool VulkanFormatHasStencil(VkFormat Format)
	{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch-enum"
		switch (Format)
		{
			case VK_FORMAT_D16_UNORM_S8_UINT:
			case VK_FORMAT_D24_UNORM_S8_UINT:
			case VK_FORMAT_D32_SFLOAT_S8_UINT:
			case VK_FORMAT_S8_UINT:
				return true;
			default:
				return false;
		}
#pragma clang diagnostic pop
	}
} // namespace VulkanRHI
