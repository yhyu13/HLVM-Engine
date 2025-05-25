/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#include "VulkanMisc.h"

namespace VulkanRHI
{
	VkAttachmentLoadOp VulkanAttachmentLoadOpFromRHIAction(ERenderTargetLoadAction RHIState)
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

	VkAttachmentStoreOp VulkanAttachmentStoreOpFromRHIAction(ERenderTargetStoreAction RHIState)
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

	VkFormat VulkanFormatFromRHIFormat(EPixelFormat RHIFormat, bool bSRGBFlag)
	{
		switch (RHIFormat)
		{
			case EPixelFormat::None:
				return VK_FORMAT_UNDEFINED;
			case EPixelFormat::R8_UNorm:
				HLVM_ENSURE(!bSRGBFlag);
				return VK_FORMAT_R8_UNORM;
			case EPixelFormat::R8G8_UNorm:
				HLVM_ENSURE(!bSRGBFlag);
				return VK_FORMAT_R8G8_UNORM;
			case EPixelFormat::R8G8B8A8_UNorm:
				HLVM_ENSURE(!bSRGBFlag);
				return VK_FORMAT_R8G8B8A8_UNORM;
			case EPixelFormat::B8G8R8A8_SRGB:
				HLVM_ENSURE(bSRGBFlag);
				return VK_FORMAT_B8G8R8A8_SRGB;
			case EPixelFormat::R16_UNorm:
				HLVM_ENSURE(!bSRGBFlag);
				return VK_FORMAT_R16_UNORM;
			case EPixelFormat::R16G16_UNorm:
				HLVM_ENSURE(!bSRGBFlag);
				return VK_FORMAT_R16G16_UNORM;
			case EPixelFormat::R16G16B16A16_UNorm:
				HLVM_ENSURE(!bSRGBFlag);
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
				HLVM_ENSURE(!bSRGBFlag);
				return VK_FORMAT_D16_UNORM;
			case EPixelFormat::D24_UNorm_S8_UInt:
				HLVM_ENSURE(!bSRGBFlag);
				return VK_FORMAT_D24_UNORM_S8_UINT;
			case EPixelFormat::D32_Float:
				return VK_FORMAT_D32_SFLOAT;
			case EPixelFormat::D32_Float_S8_UInt:
				return VK_FORMAT_D32_SFLOAT_S8_UINT; // Add more formats as needed
			case EPixelFormat::_NUM:
			default:
				HLVM_ASSERT_F(false, TXT("Unknown RHI format {}"), HLVM_ENUM_TO_TCHAR(RHIFormat));
				return VK_FORMAT_UNDEFINED;
		}
	}

	VkFormat VulkanFormatTryRemoveSRGB(VkFormat VKFormat)
	{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch"
		switch (VKFormat)
		{
			case VK_FORMAT_B8G8R8A8_SRGB:
				return VK_FORMAT_B8G8R8A8_UNORM;
		}
#pragma clang diagnostic pop
		return VKFormat;
	}

	VkImageViewType VulkanImageViewTypeFromRHIDimension(ETextureDimension RHIDimension)
	{
		switch (RHIDimension)
		{
			case ETextureDimension::Texture2D:
				return VK_IMAGE_VIEW_TYPE_2D;
			case ETextureDimension::Texture2DArray:
				return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
			case ETextureDimension::Texture3D:
				return VK_IMAGE_VIEW_TYPE_3D;
			case ETextureDimension::TextureCube:
				return VK_IMAGE_VIEW_TYPE_CUBE;
			case ETextureDimension::TextureCubeArray:
				return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
			case ETextureDimension::None:
			default:
				HLVM_ASSERT_F(false, TXT("Unknown texture dimension {}"), HLVM_ENUM_TO_TCHAR(RHIDimension));
				return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
		}
	}

	VkComponentMapping VulkanFormatComponentMappingFromRHIFormat(EPixelFormat RHIFormat)
	{
		const VkComponentMapping ComponentMappingRGBA = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		const VkComponentMapping ComponentMappingRGB1 = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_ONE };
		const VkComponentMapping ComponentMappingRG01 = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ONE };
		const VkComponentMapping ComponentMappingR001 = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ONE };
		const VkComponentMapping ComponentMappingRIII = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
		const VkComponentMapping ComponentMapping000R = { VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_R };
		const VkComponentMapping ComponentMappingR000 = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO };
		const VkComponentMapping ComponentMappingRR01 = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ONE };

		switch (RHIFormat)
		{
			case EPixelFormat::R8_UNorm:
				return ComponentMappingR001;
			case EPixelFormat::R8G8_UNorm:
				return ComponentMappingRG01;
			case EPixelFormat::R8G8B8A8_UNorm:
				return ComponentMappingRGBA;
			case EPixelFormat::B8G8R8A8_SRGB:
				return ComponentMappingRGBA;
			case EPixelFormat::R16_UNorm:
				return ComponentMappingR001;
			case EPixelFormat::R16G16_UNorm:
				return ComponentMappingRG01;
			case EPixelFormat::R16G16B16A16_UNorm:
				return ComponentMappingRGBA;
			case EPixelFormat::R32_UInt:
				return ComponentMappingR001;
			case EPixelFormat::R32G32_UInt:
				return ComponentMappingRG01;
			case EPixelFormat::R32G32B32A32_UInt:
				return ComponentMappingRGBA;
			case EPixelFormat::R32_Float:
				return ComponentMappingR001;
			case EPixelFormat::R32G32_Float:
				return ComponentMappingRG01;
			case EPixelFormat::R32G32B32A32_Float:
				return ComponentMappingRGBA;
			case EPixelFormat::D16_UNorm:
				return ComponentMappingR000;
			case EPixelFormat::D24_UNorm_S8_UInt:
				return ComponentMappingRR01;
			case EPixelFormat::D32_Float:
				return ComponentMappingR000;
			case EPixelFormat::D32_Float_S8_UInt:
				return ComponentMappingRR01; // Add more formats as needed
			case EPixelFormat::None:
			case EPixelFormat::_NUM:
			default:
				HLVM_ASSERT_F(false, TXT("Unknown RHI format {}"), HLVM_ENUM_TO_TCHAR(RHIFormat));
				return ComponentMappingRGBA;
		}
	}

	// Convert RHI pixel format to Vulkan format
	EPixelFormat RHIFormatFromVulkanFormat(VkFormat VulkanFormat, bool& bSRGB_Out)
	{
		bSRGB_Out = false;
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
				bSRGB_Out = true;
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

	EPixelFormat RHIFormatFromVulkanFormatNoSRGB(VkFormat VulkanFormat)
	{
		bool bSRGB_Out = false;
		auto Ret = RHIFormatFromVulkanFormat(VulkanFormat, bSRGB_Out);
		HLVM_ENSURE(!bSRGB_Out);
		return Ret;
	}

	VkFormat RHIVertexElementTypeToVulkanFormat(EVertexElementType Type)
	{
		switch (Type)
		{
			case EVertexElementType::Float1:
				return VK_FORMAT_R32_SFLOAT;
			case EVertexElementType::Float2:
				return VK_FORMAT_R32G32_SFLOAT;
			case EVertexElementType::Float3:
				return VK_FORMAT_R32G32B32_SFLOAT;
			case EVertexElementType::PackedNormal:
				return VK_FORMAT_R8G8B8A8_SNORM;
			case EVertexElementType::UByte4:
				return VK_FORMAT_R8G8B8A8_UINT;
			case EVertexElementType::UByte4N:
				return VK_FORMAT_R8G8B8A8_UNORM;
			case EVertexElementType::Color:
				return VK_FORMAT_B8G8R8A8_UNORM;
			case EVertexElementType::Short2:
				return VK_FORMAT_R16G16_SINT;
			case EVertexElementType::Short4:
				return VK_FORMAT_R16G16B16A16_SINT;
			case EVertexElementType::Short2N:
				return VK_FORMAT_R16G16_SNORM;
			case EVertexElementType::Half2:
				return VK_FORMAT_R16G16_SFLOAT;
			case EVertexElementType::Half4:
				return VK_FORMAT_R16G16B16A16_SFLOAT;
			case EVertexElementType::Short4N: // 4 X 16 bit word: normalized
				return VK_FORMAT_R16G16B16A16_SNORM;
			case EVertexElementType::UShort2:
				return VK_FORMAT_R16G16_UINT;
			case EVertexElementType::UShort4:
				return VK_FORMAT_R16G16B16A16_UINT;
			case EVertexElementType::UShort2N: // 16 bit word normalized to (value/65535.0:value/65535.0:0:0:1)
				return VK_FORMAT_R16G16_UNORM;
			case EVertexElementType::UShort4N: // 4 X 16 bit word unsigned: normalized
				return VK_FORMAT_R16G16B16A16_UNORM;
			case EVertexElementType::Float4:
				return VK_FORMAT_R32G32B32A32_SFLOAT;
			case EVertexElementType::URGB10A2N:
				return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
			case EVertexElementType::UInt:
				return VK_FORMAT_R32_UINT;
			case EVertexElementType::_NUM:
			case EVertexElementType::None:
			default:
				HLVM_ASSERT_F(false, TXT("Unknown EVertexElementType format {}"), HLVM_ENUM_TO_TCHAR(Type));
				return VK_FORMAT_UNDEFINED;
		}
	}

	VkImageAspectFlags GetAspectMaskFromRHIFormat(EPixelFormat Format, bool bIncludeDepth, bool bIncludeStencil)
	{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch-enum"
		switch (Format)
		{
			case EPixelFormat::D24_UNorm_S8_UInt:
			case EPixelFormat::D32_Float_S8_UInt:
				return (bIncludeDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : 0) | (bIncludeStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
			case EPixelFormat::D16_UNorm:
			case EPixelFormat::D32_Float:
				return VK_IMAGE_ASPECT_DEPTH_BIT;
			default:
				return VK_IMAGE_ASPECT_COLOR_BIT;
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

	VkSamplerMipmapMode VulkanMipFilterFromRHIFilter(ETextureFilter RHIFilter)
	{
		switch (RHIFilter)
		{
			case ETextureFilter::None:
				return VK_SAMPLER_MIPMAP_MODE_NEAREST;
			case ETextureFilter::Point:
				return VK_SAMPLER_MIPMAP_MODE_NEAREST;
			case ETextureFilter::Linear:
				return VK_SAMPLER_MIPMAP_MODE_LINEAR;
			case ETextureFilter::Anisotropic:
				return VK_SAMPLER_MIPMAP_MODE_LINEAR; // Vulkan does
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
	VkCompareOp VulkanCompareOpFromRHI(ECompareFunction RHIFunction)
	{
		switch (RHIFunction)
		{
			case ECompareFunction::Never:
				return VK_COMPARE_OP_NEVER;
			case ECompareFunction::Less:
				return VK_COMPARE_OP_LESS;
			case ECompareFunction::Equal:
				return VK_COMPARE_OP_EQUAL;
			case ECompareFunction::LessEqual:
				return VK_COMPARE_OP_LESS_OR_EQUAL;
			case ECompareFunction::Greater:
				return VK_COMPARE_OP_GREATER;
			case ECompareFunction::NotEqual:
				return VK_COMPARE_OP_NOT_EQUAL;
			case ECompareFunction::GreaterEqual:
				return VK_COMPARE_OP_GREATER_OR_EQUAL;
			case ECompareFunction::Always:
				return VK_COMPARE_OP_ALWAYS;
			case ECompareFunction::NUM:
			default:
				HLVM_ASSERT_F(false, TXT("Unknown RHI compare function"));
				return VK_COMPARE_OP_NEVER;
		}
	}

	VkPrimitiveTopology VulkanPrimitiveTopologyFromRHIPrimitiveType(EPrimitiveType RHIPrimitiveType)
	{
		switch (RHIPrimitiveType)
		{
			case EPrimitiveType::LineList:
				return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
			case EPrimitiveType::TriangleList:
				return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			case EPrimitiveType::TriangleStrip:
				return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
			case EPrimitiveType::Num:
				HLVM_ASSERT_F(false, TXT("Unknown RHI compare function"));
				return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
		}
	}

	VkStencilOp VulkanStencilOpFromRHI(EStencilOp RHIStencilOp)
	{
		switch (RHIStencilOp)
		{
			case EStencilOp::Keep:
				return VK_STENCIL_OP_KEEP;
			case EStencilOp::Zero:
				return VK_STENCIL_OP_ZERO;
			case EStencilOp::Replace:
				return VK_STENCIL_OP_REPLACE;
			case EStencilOp::SaturatedIncrement:
				return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
			case EStencilOp::SaturatedDecrement:
				return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
			case EStencilOp::Invert:
				return VK_STENCIL_OP_INVERT;
			case EStencilOp::Increment:
				return VK_STENCIL_OP_INCREMENT_AND_WRAP;
			case EStencilOp::Decrement:
				return VK_STENCIL_OP_DECREMENT_AND_WRAP;
			case EStencilOp::_NUM:
			default:
				HLVM_ASSERT_F(false, TXT("Unknown RHI stencil op"));
				return VK_STENCIL_OP_KEEP;
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
			case EShaderStage::Mesh:
				return VK_SHADER_STAGE_MESH_BIT_EXT;
			case EShaderStage::Task:
				return VK_SHADER_STAGE_TASK_BIT_EXT;
			case EShaderStage::RayGeneration:
				return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
			case EShaderStage::RayIntersection:
				return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
			case EShaderStage::RayAnyHit:
				return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
			case EShaderStage::RayClosestHit:
				return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
			case EShaderStage::RayMiss:
				return VK_SHADER_STAGE_MISS_BIT_KHR;
			case EShaderStage::RayCallable:
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
				HLVM_ENSURE(StencilLayout == VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);
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
				HLVM_ENSURE(StencilLayout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
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
