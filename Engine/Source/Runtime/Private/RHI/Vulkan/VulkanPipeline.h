/**
 * Copyright (c) 2025. MIT License. All rights reserved.
 */

#pragma once

#include "VulkanDescriptorSets.h"
#include "VulkanState.h"

/** This represents a vertex declaration that hasn't been combined with a specific shader to create a bound shader. */
class FVulkanVertexDeclaration : public FRHIVertexDeclaration
{
public:
	FVertexDeclarationElementList Elements;
	TUINT32						  Hash;
	TUINT32						  HashNoStrides;

	FVulkanVertexDeclaration(const FVertexDeclarationElementList& InElements, TUINT32 InHash, TUINT32 InHashNoStrides);

	static void EmptyCache();
};

using FVulkanVertexDeclarationRef = TRefCountPtr<FVulkanVertexDeclaration>;

class FVulkanVertexInputStateInfo
{
public:
	FVulkanVertexInputStateInfo();
	~FVulkanVertexInputStateInfo();

	void Generate(FVulkanVertexDeclarationRef VertexDeclaration, TUINT32 VertexHeaderInOutAttributeMask);

	inline FMD5Digest GetHash() const
	{
		HLVM_ASSERT(Info.sType == VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
		return Hash;
	}

	inline const VkPipelineVertexInputStateCreateInfo& GetInfo() const
	{
		return Info;
	}

	bool operator==(const FVulkanVertexInputStateInfo& Other);

private:
	FMD5Digest							 Hash;
	VkPipelineVertexInputStateCreateInfo Info;

	TMapSmall<TUINT32, TUINT32> BindingToStream;
	TMapSmall<TUINT32, TUINT32> StreamToBinding;

	TUINT32							BindingsNum;
	TUINT32							BindingsMask;
	VkVertexInputBindingDescription Bindings[RHI::MAX_VERTEX_ELEMENTS];

	TUINT32							  AttributesNum;
	VkVertexInputAttributeDescription Attributes[RHI::MAX_VERTEX_ELEMENTS];
};

struct FVulkanDescSetLayoutBinding
{
	TUINT32 Binding;
	TUINT32 DescriptorType;
	TUINT32 StageFlags;

	void ReadFrom(const VkDescriptorSetLayoutBinding& InState);
	void WriteInto(VkDescriptorSetLayoutBinding& OutState) const;

	bool operator==(const FVulkanDescSetLayoutBinding& In) const
	{
		return Binding == In.Binding && DescriptorType == In.DescriptorType && StageFlags == In.StageFlags;
	}
};

struct FVulkanGraphicsPSODescription
{
	struct PSOKey
	{
		FMD5Digest Hash;
	};

	PSOKey GeneratePSOKey() const;

	TUINT32 VertexInputKey;
	TUINT8	RasterizationSamples;
	TUINT32 Topology;
	struct FBlendAttachment
	{
		bool   bBlend;
		TUINT8 ColorBlendOp;
		TUINT8 SrcColorBlendFactor;
		TUINT8 DstColorBlendFactor;
		TUINT8 AlphaBlendOp;
		TUINT8 SrcAlphaBlendFactor;
		TUINT8 DstAlphaBlendFactor;
		TUINT8 ColorWriteMask;

		void ReadFrom(const VkPipelineColorBlendAttachmentState& InState);
		void WriteInto(VkPipelineColorBlendAttachmentState& OutState) const;

		bool operator==(const FBlendAttachment& In) const
		{
			return bBlend == In.bBlend && ColorBlendOp == In.ColorBlendOp && SrcColorBlendFactor == In.SrcColorBlendFactor && DstColorBlendFactor == In.DstColorBlendFactor && AlphaBlendOp == In.AlphaBlendOp && SrcAlphaBlendFactor == In.SrcAlphaBlendFactor && DstAlphaBlendFactor == In.DstAlphaBlendFactor && ColorWriteMask == In.ColorWriteMask;
		}
	};
	TVector<FBlendAttachment>					  ColorAttachmentStates;
	TVector<TVector<FVulkanDescSetLayoutBinding>> DescSetLayoutBindings;

	struct FVertexBinding
	{
		TUINT32 Stride;
		TUINT16 Binding;
		TUINT16 InputRate;

		void ReadFrom(const VkVertexInputBindingDescription& InState);
		void WriteInto(VkVertexInputBindingDescription& OutState) const;

		bool operator==(const FVertexBinding& In) const
		{
			return Stride == In.Stride && Binding == In.Binding && InputRate == In.InputRate;
		}
	};
	TVector<FVertexBinding> VertexBindings;
	struct FVertexAttribute
	{
		TUINT32 Location;
		TUINT32 Binding;
		TUINT32 Format;
		TUINT32 Offset;

		void ReadFrom(const VkVertexInputAttributeDescription& InState);
		void WriteInto(VkVertexInputAttributeDescription& OutState) const;

		bool operator==(const FVertexAttribute& In) const
		{
			return Location == In.Location && Binding == In.Binding && Format == In.Format && Offset == In.Offset;
		}
	};
	TVector<FVertexAttribute> VertexAttributes;

	struct FRasterizer
	{
		TUINT8 PolygonMode;
		TUINT8 CullMode;
		float  DepthBiasSlopeScale;
		float  DepthBiasConstantFactor;

		void ReadFrom(const VkPipelineRasterizationStateCreateInfo& InState);
		void WriteInto(VkPipelineRasterizationStateCreateInfo& OutState) const;

		bool operator==(const FRasterizer& In) const
		{
			return PolygonMode == In.PolygonMode && CullMode == In.CullMode && DepthBiasSlopeScale == In.DepthBiasSlopeScale && DepthBiasConstantFactor == In.DepthBiasConstantFactor;
		}
	};
	FRasterizer Rasterizer;

	struct FDepthStencil
	{
		TUINT8	DepthCompareOp;
		bool	bDepthTestEnable;
		bool	bDepthWriteEnable;
		bool	bStencilTestEnable;
		bool	bDepthBoundsTestEnable;
		TUINT8	FrontFailOp;
		TUINT8	FrontPassOp;
		TUINT8	FrontDepthFailOp;
		TUINT8	FrontCompareOp;
		TUINT32 FrontCompareMask;
		TUINT32 FrontWriteMask;
		TUINT32 FrontReference;
		TUINT8	BackFailOp;
		TUINT8	BackPassOp;
		TUINT8	BackDepthFailOp;
		TUINT8	BackCompareOp;
		TUINT32 BackCompareMask;
		TUINT32 BackWriteMask;
		TUINT32 BackReference;

		void ReadFrom(const VkPipelineDepthStencilStateCreateInfo& InState);
		void WriteInto(VkPipelineDepthStencilStateCreateInfo& OutState) const;

		bool operator==(const FDepthStencil& In) const
		{
			return DepthCompareOp == In.DepthCompareOp && bDepthTestEnable == In.bDepthTestEnable && bDepthWriteEnable == In.bDepthWriteEnable && bDepthBoundsTestEnable == In.bDepthBoundsTestEnable && bStencilTestEnable == In.bStencilTestEnable && FrontFailOp == In.FrontFailOp && FrontPassOp == In.FrontPassOp && FrontDepthFailOp == In.FrontDepthFailOp && FrontCompareOp == In.FrontCompareOp && FrontCompareMask == In.FrontCompareMask && FrontWriteMask == In.FrontWriteMask && FrontReference == In.FrontReference && BackFailOp == In.BackFailOp && BackPassOp == In.BackPassOp && BackDepthFailOp == In.BackDepthFailOp && BackCompareOp == In.BackCompareOp && BackCompareMask == In.BackCompareMask && BackWriteMask == In.BackWriteMask && BackReference == In.BackReference;
		}
	};
	FDepthStencil DepthStencil;

#if VULKAN_USE_SHADERKEYS
	TUINT64 ShaderKeys[RHI::NUM_GFX_SHADER_STAGES];
	TUINT64 ShaderKeyShared;
#else
	FVulkanShaderHashes ShaderHashes;
#endif

	struct FRenderTargets
	{
		struct FAttachmentRef
		{
			TUINT32 Attachment;
			TUINT64 Layout;

			void ReadFrom(const VkAttachmentReference& InState);
			void WriteInto(VkAttachmentReference& OutState) const;
			bool operator==(const FAttachmentRef& In) const
			{
				return Attachment == In.Attachment && Layout == In.Layout;
			}
		};

		struct FStencilAttachmentRef
		{
			TUINT64 Layout;

			void ReadFrom(const VkAttachmentReferenceStencilLayout& InState);
			void WriteInto(VkAttachmentReferenceStencilLayout& OutState) const;
			bool operator==(const FStencilAttachmentRef& In) const
			{
				return Layout == In.Layout;
			}
		};

		TVector<FAttachmentRef> ColorAttachments;
		TVector<FAttachmentRef> ResolveAttachments;
		FAttachmentRef			Depth;
		FStencilAttachmentRef	Stencil;
		FAttachmentRef			FragmentDensity;

		struct FAttachmentDesc
		{
			TUINT32 Format;
			TUINT8	Flags;
			TUINT8	Samples;
			TUINT8	LoadOp;
			TUINT8	StoreOp;
			TUINT8	StencilLoadOp;
			TUINT8	StencilStoreOp;
			TUINT64 InitialLayout;
			TUINT64 FinalLayout;

			bool operator==(const FAttachmentDesc& In) const
			{
				return Format == In.Format && Flags == In.Flags && Samples == In.Samples && LoadOp == In.LoadOp && StoreOp == In.StoreOp && StencilLoadOp == In.StencilLoadOp && StencilStoreOp == In.StencilStoreOp && InitialLayout == In.InitialLayout && FinalLayout == In.FinalLayout;
			}

			void ReadFrom(const VkAttachmentDescription& InState);
			void WriteInto(VkAttachmentDescription& OutState) const;
		};

		struct FStencilAttachmentDesc
		{
			TUINT64 InitialLayout;
			TUINT64 FinalLayout;

			bool operator==(const FStencilAttachmentDesc& In) const
			{
				return InitialLayout == In.InitialLayout && FinalLayout == In.FinalLayout;
			}

			void ReadFrom(const VkAttachmentDescriptionStencilLayout& InState);
			void WriteInto(VkAttachmentDescriptionStencilLayout& OutState) const;
		};

		TVector<FAttachmentDesc> Descriptions;
		FStencilAttachmentDesc	 StencilDescription;

		TUINT8	  NumAttachments;
		TUINT8	  NumColorAttachments;
		TUINT8	  bHasDepthStencil;
		TUINT8	  bHasResolveAttachments;
		TUINT8	  bHasDepthStencilResolve;
		TUINT8	  bHasFragmentDensityAttachment;
		TUINT8	  NumUsedClearValues;
		TUINT32	  RenderPassCompatibleHash;
		FUIntVec3 Extent3D;

		void ReadFrom(const FVulkanRenderTargetLayout& InState);
		void WriteInto(FVulkanRenderTargetLayout& OutState) const;

		bool operator==(const FRenderTargets& In) const
		{
			return ColorAttachments == In.ColorAttachments && ResolveAttachments == In.ResolveAttachments && Depth == In.Depth && Stencil == In.Stencil && FragmentDensity == In.FragmentDensity && Descriptions == In.Descriptions && StencilDescription == In.StencilDescription && NumAttachments == In.NumAttachments && NumColorAttachments == In.NumColorAttachments && bHasDepthStencil == In.bHasDepthStencil && bHasResolveAttachments == In.bHasResolveAttachments && bHasDepthStencilResolve == In.bHasDepthStencilResolve && bHasFragmentDensityAttachment == In.bHasFragmentDensityAttachment && NumUsedClearValues == In.NumUsedClearValues && RenderPassCompatibleHash == In.RenderPassCompatibleHash && Extent3D == In.Extent3D;
		}
	};
	FRenderTargets RenderTargets;
	TUINT8		   SubpassIndex;
	TUINT8		   UseAlphaToCoverage;

	bool operator==(const FVulkanGraphicsPSODescription& In) const
	{
		if (VertexInputKey != In.VertexInputKey)
		{
			return false;
		}

		if (RasterizationSamples != In.RasterizationSamples)
		{
			return false;
		}

		if (Topology != In.Topology)
		{
			return false;
		}

		if (ColorAttachmentStates != In.ColorAttachmentStates)
		{
			return false;
		}

		if (DescSetLayoutBindings != In.DescSetLayoutBindings)
		{
			return false;
		}

		if (!(Rasterizer == In.Rasterizer))
		{
			return false;
		}

		if (!(DepthStencil == In.DepthStencil))
		{
			return false;
		}

		if (!(SubpassIndex == In.SubpassIndex))
		{
			return false;
		}

		if (!(UseAlphaToCoverage == In.UseAlphaToCoverage))
		{
			return false;
		}
#if 0 == VULKAN_USE_SHADERKEYS
       if (!(ShaderHashes == In.ShaderHashes))
       {
          return false;
       }
#else
		if (0 != FMemory::Memcmp(ShaderKeys, In.ShaderKeys, sizeof(ShaderKeys)))
		{
			return false;
		}
#endif

		if (!(RenderTargets == In.RenderTargets))
		{
			return false;
		}

		if (VertexBindings != In.VertexBindings)
		{
			return false;
		}

		if (VertexAttributes != In.VertexAttributes)
		{
			return false;
		}

		//		if (ShadingRate != In.ShadingRate)
		//		{
		//			return false;
		//		}
		//
		//		if (Combiner != In.Combiner)
		//		{
		//			return false;
		//		}

		return true;
	}
};

class FVulkanGraphicsPSO : public FRHIGraphicsPSO, public FVulkanResource, public FVulkanMinimalContext
{
public:
	void GeneratePSOMetadata(const FGraphicsPSOInitializer& PSOInitializer, FVulkanDescriptorSetsLayoutInfo& LayoutInfoOut, FVulkanGraphicsPSODescription& DescOut);
};

// Vulkan-specific RHI query
class FVulkanQuery : public FRHIQuery, public FVulkanResource
{
public:
	FVulkanQuery(VkQueryPool InQueryPool, TUINT32 InQueryIndex, ERHIQueryType InQueryType)
		: QueryPool(InQueryPool), QueryIndex(InQueryIndex), QueryType(InQueryType) {}

	// Returns the type of the query (e.g., occlusion, timestamp)
	virtual ERHIQueryType GetQueryType() const override { return QueryType; }

	// Returns the Vulkan query pool handle
	VkQueryPool GetQueryPool() const { return QueryPool; }

	// Returns the query index within the pool
	TUINT32 GetQueryIndex() const { return QueryIndex; }

private:
	VkQueryPool	  QueryPool;
	TUINT32		  QueryIndex;
	ERHIQueryType QueryType;
};

using FVulkanGraphicsPSORef = TRefCountPtr<FVulkanGraphicsPSO>;
using FVulkanQueryRef = TRefCountPtr<FVulkanQuery>;
