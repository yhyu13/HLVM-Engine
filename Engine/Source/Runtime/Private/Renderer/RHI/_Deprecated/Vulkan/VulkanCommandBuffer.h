// Copyright Epic Games, Inc. All Rights Reserved..

/*=============================================================================
	VulkanCommandBuffer.h: Private Vulkan RHI definitions.
=============================================================================*/

#pragma once

#include "VulkanResourcePost.h"

struct FVulkanImageLayout
{
	FVulkanImageLayout(VkImageLayout InitialLayout, TUINT32 InNumMips, TUINT32 InNumLayers, VkImageAspectFlags Aspect)
		: NumMips(InNumMips), NumLayers(InNumLayers), NumPlanes((Aspect == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) ? 2 : 1), MainLayout(InitialLayout)
	{
	}

	TUINT32 NumMips;
	TUINT32 NumLayers;
	TUINT32 NumPlanes;

	// The layout when all the subresources are in the same state.
	VkImageLayout MainLayout;

	// Explicit subresource layouts. Always NumLayers*NumMips elements.
	TVector<VkImageLayout> SubresLayouts;

	inline bool AreAllSubresourcesSameLayout() const
	{
		return SubresLayouts.Num() == 0;
	}

	VkImageLayout GetSubresLayout(TUINT32 Layer, TUINT32 Mip, VkImageAspectFlagBits Aspect) const
	{
		return GetSubresLayout(Layer, Mip, (Aspect == VK_IMAGE_ASPECT_STENCIL_BIT) ? NumPlanes - 1 : 0);
	}

	VkImageLayout GetSubresLayout(TUINT32 Layer, TUINT32 Mip, TUINT32 Plane) const
	{
		if (SubresLayouts.Num() == 0)
		{
			return MainLayout;
		}

		if (Layer == TUINT32_MAX)
		{
			Layer = 0;
		}

		HLVM_ASSERT(Plane < NumPlanes && Layer < NumLayers && Mip < NumMips);
		return SubresLayouts[(Plane * NumLayers * NumMips) + (Layer * NumMips) + Mip];
	}

	bool AreSubresourcesSameLayout(VkImageLayout Layout, const VkImageSubresourceRange& SubresourceRange) const;

	inline TUINT32 GetSubresRangeLayerCount(const VkImageSubresourceRange& SubresourceRange) const
	{
		HLVM_ASSERT(SubresourceRange.baseArrayLayer < NumLayers);
		return (SubresourceRange.layerCount == VK_REMAINING_ARRAY_LAYERS) ? (NumLayers - SubresourceRange.baseArrayLayer) : SubresourceRange.layerCount;
	}

	inline TUINT32 GetSubresRangeMipCount(const VkImageSubresourceRange& SubresourceRange) const
	{
		HLVM_ASSERT(SubresourceRange.baseMipLevel < NumMips);
		return (SubresourceRange.levelCount == VK_REMAINING_MIP_LEVELS) ? (NumMips - SubresourceRange.baseMipLevel) : SubresourceRange.levelCount;
	}

	void CollapseSubresLayoutsIfSame();

	void Set(VkImageLayout Layout, const VkImageSubresourceRange& SubresourceRange);
};

class FVulkanLayoutManager
{
public:
	FVulkanLayoutManager(bool InWriteOnly, FVulkanLayoutManager* InFallback)
		: bWriteOnly(InWriteOnly)
		, Fallback(InFallback)
	{
	}

	void NotifyDeletedImage(VkImage Image);

	// Predetermined layouts for given RHIAccess
	static VkImageLayout GetDefaultLayout(class FVulkanCmdBuffer* CmdBuffer, const FVulkanTextureRef& VulkanTexture, ERHIAccessFlag DesiredAccess);

	// Expected layouts and Hints are workarounds until we can use 'hardcoded layouts' everywhere.
	static VkImageLayout SetExpectedLayout(class FVulkanCmdBuffer* CmdBuffer, const FVulkanTextureRef& VulkanTexture, ERHIAccessFlag DesiredAccess);
	VkImageLayout		 GetDepthStencilHint(const FVulkanTextureRef& VulkanTexture, VkImageAspectFlagBits AspectBit);

	const FVulkanImageLayout* GetFullLayout(VkImage Image) const
	{
		HLVM_ASSERT(!bWriteOnly);
		const FVulkanImageLayout* Layout = Layouts.Find(Image);
		if (!Layout && Fallback)
		{
			return Fallback->GetFullLayoutNoFallBack(Image);
		}
		return Layout;
	}

	const FVulkanImageLayout* GetFullLayout(const FVulkanTextureRef& VulkanTexture, bool bAddIfNotFound = false, VkImageLayout LayoutIfNotFound = VK_IMAGE_LAYOUT_UNDEFINED)
	{
		HLVM_ASSERT(!bWriteOnly);
		const FVulkanImageLayout* Layout = Layouts.Find(VulkanTexture->GetImage());

		if (!Layout && Fallback)
		{
			Layout = Fallback->GetFullLayout(VulkanTexture, false);

			// If the layout was found in the fallback, carry it forward to our current manager for future tracking
			if (Layout)
			{
				return Layouts.Add(VulkanTexture->GetImage(), *Layout);
			}
		}

		if (Layout)
		{
			return Layout;
		}
		else if (!bAddIfNotFound)
		{
			return nullptr;
		}

		return Layouts.Add(VulkanTexture->GetImage(), FVulkanImageLayout(LayoutIfNotFound, VulkanTexture->GetNumMips(), VulkanTexture->GetVulkanArraySize(), VulkanTexture->GetFullAspectFlags()));
	}

	// Not the preferred path because we can't ensure Mip and Layer counts match, but still necessary for images like the backbuffer
	void SetFullLayout(VkImage Image, const FVulkanImageLayout& NewLayout)
	{
		FVulkanImageLayout* Layout = Layouts.Find(Image);
		if (Layout)
		{
			*Layout = NewLayout;
		}
		else
		{
			Layouts.Add(Image, NewLayout);
		}
	}

	void SetFullLayout(const FVulkanTextureRef& VulkanTexture, const FVulkanImageLayout& NewLayout)
	{
		HLVM_ASSERT((VulkanTexture->GetNumMips() == NewLayout.NumMips) && (VulkanTexture->GetVulkanArraySize() == NewLayout.NumLayers));
		SetFullLayout(VulkanTexture->GetImage(), NewLayout);
	}

	void SetFullLayout(const FVulkanTextureRef& VulkanTexture, VkImageLayout InLayout, bool bOnlyIfNotFound = false)
	{
		FVulkanImageLayout* Layout = Layouts.Find(VulkanTexture->GetImage());
		if (Layout)
		{
			if (!bOnlyIfNotFound)
			{
				Layout->Set(InLayout, FVulkanPipelineBarrier::MakeSubresourceRange(VulkanTexture->GetFullAspectFlags()));
			}
		}
		else
		{
			Layouts.Add(VulkanTexture->GetImage(), FVulkanImageLayout(InLayout, VulkanTexture->GetNumMips(), VulkanTexture->GetVulkanArraySize(), VulkanTexture->GetFullAspectFlags()));
		}
	}

	static bool IsPartialResource(const FVulkanTextureRef& VulkanTexture, const VkImageSubresourceRange& InSubresourceRange)
	{
		return (VulkanTexture->GetFullAspectFlags() != InSubresourceRange.aspectMask) || (InSubresourceRange.baseArrayLayer != 0) || (InSubresourceRange.baseMipLevel != 0) || ((InSubresourceRange.levelCount != VK_REMAINING_MIP_LEVELS) && (InSubresourceRange.levelCount != VulkanTexture->GetNumMips())) || ((InSubresourceRange.layerCount != VK_REMAINING_ARRAY_LAYERS) && (InSubresourceRange.layerCount != VulkanTexture->GetVulkanArraySize()));
	}

	void SetLayout(const FVulkanTextureRef& VulkanTexture, const VkImageSubresourceRange& InSubresourceRange, VkImageLayout InLayout)
	{
		FVulkanImageLayout* Layout = Layouts.Find(VulkanTexture->GetImage());

		// If we're not going to overwrite the entire resource, start from its last known state on the queue
		if (!Layout && IsPartialResource(VulkanTexture, InSubresourceRange) && Fallback)
		{
			const FVulkanImageLayout* FallbackLayout = Fallback->GetFullLayout(VulkanTexture, false);
			if (FallbackLayout)
			{
				Layout = Layouts.Add(VulkanTexture->GetImage(), *FallbackLayout);
			}
		}

		if (Layout)
		{
			Layout->Set(InLayout, InSubresourceRange);
		}
		else
		{
			FVulkanImageLayout NewLayout(VK_IMAGE_LAYOUT_UNDEFINED, VulkanTexture->GetNumMips(), VulkanTexture->GetVulkanArraySize(), VulkanTexture->GetFullAspectFlags());
			NewLayout.Set(InLayout, InSubresourceRange);
			Layouts.Add(VulkanTexture->GetImage(), NewLayout);
		}
	}

	// Transfers our layouts into the destination
	void TransferTo(FVulkanLayoutManager& Destination);

private:
	const FVulkanImageLayout* GetFullLayoutNoFallBack(VkImage Image) const
	{
		HLVM_ASSERT(!bWriteOnly);
		const FVulkanImageLayout* Layout = Layouts.Find(Image);
		return Layout;
	}

private:
	TMap<VkImage, FVulkanImageLayout> Layouts;

	// If we're WriteOnly, we should never read layout from this instance.  This is important for parallel rendering.
	// When WriteOnly, this instance of the layout manager should only collect layouts to later feed them to the another central mgr.
	const bool bWriteOnly;

	// If parallel command list creation is NOT supported, then the queue's layout mgr can be used as a fallback to fetch previous layouts.
	FVulkanLayoutManager* Fallback;
};

extern bool GVulkanProfileCmdBuffers;
extern bool GVulkanUseCmdBufferTimingForGPUTime;

class FVulkanDevice;
class FVulkanCommandBufferPool;
class FVulkanCommandBufferManager;
class FVulkanRenderTargetLayout;
class FVulkanQueue;
class FVulkanDescriptorPoolSetContainer;
class FVulkanGPUTiming;

class FVulkanCmdBuffer
{
protected:
	friend class FVulkanCommandBufferManager;
	friend class FVulkanCommandBufferPool;
	friend class FVulkanQueue;

	FVulkanCmdBuffer(FVulkanDevice* InDevice, FVulkanCommandBufferPool* InCommandBufferPool, bool bInIsUploadOnly);
	~FVulkanCmdBuffer();

public:
	FVulkanCommandBufferPool* GetOwner()
	{
		return CommandBufferPool;
	}

	FVulkanDevice* GetDevice()
	{
		return Device;
	}

	bool IsUniformBufferBarrierAdded() const
	{
		return bIsUniformBufferBarrierAdded;
	}

	inline bool IsInsideRenderPass() const
	{
		return State == EState::IsInsideRenderPass;
	}

	inline bool IsOutsideRenderPass() const
	{
		return State == EState::IsInsideBegin;
	}

	inline bool HasBegun() const
	{
		return State == EState::IsInsideBegin || State == EState::IsInsideRenderPass;
	}

	inline bool HasEnded() const
	{
		return State == EState::HasEnded;
	}

	inline bool IsSubmitted() const
	{
		return State == EState::Submitted;
	}

	inline bool IsAllocated() const
	{
		return State != EState::NotAllocated;
	}

	inline VkCommandBuffer GetHandle()
	{
		return CommandBufferHandle;
	}

	inline TUINT64 GetFenceSignaledCounter() const
	{
		return FenceSignaledCounter;
	}

	// #todo-rco: Temp to help find out where the crash is coming from!
	inline TUINT64 GetFenceSignaledCounterA() const
	{
		return FenceSignaledCounter;
	}

	inline TUINT64 GetFenceSignaledCounterB() const
	{
		return FenceSignaledCounter;
	}

	inline TUINT64 GetFenceSignaledCounterC() const
	{
		return FenceSignaledCounter;
	}

	inline TUINT64 GetFenceSignaledCounterD() const
	{
		return FenceSignaledCounter;
	}

	inline TUINT64 GetFenceSignaledCounterE() const
	{
		return FenceSignaledCounter;
	}

	inline TUINT64 GetFenceSignaledCounterF() const
	{
		return FenceSignaledCounter;
	}

	inline TUINT64 GetFenceSignaledCounterG() const
	{
		return FenceSignaledCounter;
	}

	inline TUINT64 GetFenceSignaledCounterH() const
	{
		return FenceSignaledCounter;
	}

	inline TUINT64 GetFenceSignaledCounterI() const
	{
		return FenceSignaledCounter;
	}

	inline TUINT64 GetSubmittedFenceCounter() const
	{
		return SubmittedFenceCounter;
	}

	inline FVulkanLayoutManager& GetLayoutManager()
	{
		return LayoutManager;
	}

	void AddWaitSemaphore(VkPipelineStageFlags InWaitFlags, FVulkanSemaphoreRef InWaitSemaphore)
	{
		AddWaitSemaphore(InWaitFlags, TVectorView<FVulkanSemaphoreRef>(&InWaitSemaphore, 1));
	}

	void AddWaitSemaphore(VkPipelineStageFlags InWaitFlags, TVectorView<FVulkanSemaphoreRef> InWaitSemaphores);

	void Begin();
	void End();

	enum class EState : TUINT8
	{
		ReadyForBegin,
		IsInsideBegin,
		IsInsideRenderPass,
		HasEnded,
		Submitted,
		NotAllocated,
		NeedReset,
	};

	TStaticVector<VkViewport, 2> CurrentViewports;
	TStaticVector<VkRect2D, 2>	 CurrentScissors;
	TUINT32						 CurrentStencilRef;
	EState						 State;
	bool						 bNeedsDynamicStateSet;
	bool						 bHasPipeline;
	bool						 bHasViewport;
	bool						 bHasScissor;
	bool						 bHasStencilRef;
	bool						 bIsUploadOnly;
	bool						 bIsUniformBufferBarrierAdded;

	// You never want to call Begin/EndRenderPass directly as it will mess up the layout manager.
	void BeginRenderPass(const FVulkanRenderTargetLayout& Layout, class FVulkanRenderPass* RenderPass, class FVulkanFramebuffer* Framebuffer, const VkClearValue* AttachmentClearValues);
	void EndRenderPass();

	void BeginUniformUpdateBarrier();
	void EndUniformUpdateBarrier();
	// #todo-rco: Hide this
	FVulkanDescriptorPoolSetContainer* CurrentDescriptorPoolSetContainer = nullptr;

	bool AcquirePoolSetAndDescriptorsIfNeeded(const class FVulkanDescriptorSetsLayout& Layout, bool bNeedDescriptors, VkDescriptorSet* OutDescriptors);

	struct PendingQuery
	{
		TUINT64		Index;
		TUINT64		Count;
		VkBuffer	BufferHandle;
		VkQueryPool PoolHandle;
		bool		bBlocking;
	};
	void AddPendingTimestampQuery(TUINT64 Index, TUINT64 Count, VkQueryPool PoolHandle, VkBuffer BufferHandle, bool bBlocking);

private:
	FVulkanDevice*	Device;
	VkCommandBuffer CommandBufferHandle;
	double			SubmittedTime = 0.0;

	TVector<VkPipelineStageFlags> WaitFlags;
	TVector<FVulkanSemaphoreRef>  WaitSemaphores;
	TVector<FVulkanSemaphoreRef>  SubmittedWaitSemaphores;
	TVector<PendingQuery>		  PendingTimestampQueries;

	void MarkSemaphoresAsSubmitted()
	{
		WaitFlags.Reset();
		// Move to pending delete list
		SubmittedWaitSemaphores = WaitSemaphores;
		WaitSemaphores.Reset();
	}

	// Do not cache this pointer as it might change depending on VULKAN_REUSE_FENCES
	FVulkanFenceRef Fence;

	// Last value passed after the fence got signaled
	TUINT64 FenceSignaledCounter;
	// Last value when we submitted the cmd buffer; useful to track down if something waiting for the fence has actually been submitted
	TUINT64 SubmittedFenceCounter;

	void RefreshFenceStatus();
	void InitializeTimings(FVulkanCommandListContext* InContext);

	FVulkanCommandBufferPool* CommandBufferPool;

	TUINT64 LastValidTiming;

	void AcquirePoolSetContainer();

	void AllocMemory();
	void FreeMemory();

	FVulkanLayoutManager LayoutManager;

public:
	// #todo-rco: Hide this
	TMap<TUINT32, class FVulkanTypedDescriptorPoolSet*> TypedDescriptorPoolSets;

	friend class FVulkanDynamicRHI;
	friend class FVulkanLayoutManager;
};

class FVulkanCommandBufferPool
{
public:
	FVulkanCommandBufferPool(FVulkanDevice* InDevice, FVulkanCommandBufferManager& InMgr);
	~FVulkanCommandBufferPool();

	void RefreshFenceStatus(FVulkanCmdBuffer* SkipCmdBuffer = nullptr);

	inline VkCommandPool GetHandle() const
	{
		return Handle;
	}

	inline FCriticalSection* GetCS()
	{
		return &CS;
	}

	void FreeUnusedCmdBuffers(FVulkanQueue* Queue, bool bTrimMemory);

	inline FVulkanCommandBufferManager& GetMgr()
	{
		return Mgr;
	}

private:
	VkCommandPool Handle;

	TVector<FVulkanCmdBuffer*> CmdBuffers;
	TVector<FVulkanCmdBuffer*> FreeCmdBuffers;

	FCriticalSection CS;
	FVulkanDevice*	 Device;

	FVulkanCommandBufferManager& Mgr;

	FVulkanCmdBuffer* Create(bool bIsUploadOnly);

	void Create(TUINT32 QueueFamilyIndex);
	friend class FVulkanCommandBufferManager;
};

class FVulkanCommandBufferManager
{
public:
	FVulkanCommandBufferManager(FVulkanDevice* InDevice, FVulkanCommandListContext* InContext);
	~FVulkanCommandBufferManager();

	void Init(FVulkanCommandListContext* InContext);

	inline FVulkanCmdBuffer* GetActiveCmdBuffer()
	{
		if (UploadCmdBuffer)
		{
			SubmitUploadCmdBuffer();
		}

		return ActiveCmdBuffer;
	}

	inline FVulkanCmdBuffer* GetActiveCmdBufferDirect()
	{
		return ActiveCmdBuffer;
	}

	inline bool HasPendingUploadCmdBuffer() const
	{
		return UploadCmdBuffer != nullptr;
	}

	inline bool HasPendingActiveCmdBuffer() const
	{
		return ActiveCmdBuffer != nullptr;
	}

	FVulkanCmdBuffer* GetUploadCmdBuffer();

	void SubmitUploadCmdBuffer(TUINT32 NumSignalSemaphores = 0, VkSemaphore* SignalSemaphores = nullptr);

	void SubmitActiveCmdBuffer(TVectorView<FVulkanSemaphoreRef> SignalSemaphores);

	void SubmitActiveCmdBuffer()
	{
		SubmitActiveCmdBuffer(TVectorView<FVulkanSemaphoreRef>{});
	}

	void SubmitActiveCmdBuffer(FVulkanSemaphoreRef SignalSemaphore)
	{
		SubmitActiveCmdBuffer(TVectorView<FVulkanSemaphoreRef>(&SignalSemaphore, 1));
	}

	/** Regular SACB() expects not-ended and would rotate the command buffer immediately, but Present has a special logic */
	void SubmitActiveCmdBufferFromPresent(FVulkanSemaphoreRef SignalSemaphore = nullptr);

	void WaitForCmdBuffer(FVulkanCmdBuffer* CmdBuffer, float TimeInSecondsToWait = 10.0f);

	void AddQueryPoolForReset(VkQueryPool Pool, TUINT32 Size);
	void FlushResetQueryPools();

	// Update the fences of all cmd buffers except SkipCmdBuffer
	void RefreshFenceStatus(FVulkanCmdBuffer* SkipCmdBuffer = nullptr)
	{
		Pool.RefreshFenceStatus(SkipCmdBuffer);
	}

	void PrepareForNewActiveCommandBuffer();

	inline VkCommandPool GetHandle() const
	{
		return Pool.GetHandle();
	}

	void FreeUnusedCmdBuffers(bool bTrimMemory);

	inline FVulkanCommandListContext* GetCommandListContext()
	{
		return Context;
	}

	inline FVulkanQueue* GetQueue()
	{
		return Queue;
	}

	inline void NotifyDeletedImage(VkImage Image)
	{
		if (UploadCmdBuffer)
		{
			UploadCmdBuffer->GetLayoutManager().NotifyDeletedImage(Image);
		}
		if (ActiveCmdBuffer)
		{
			ActiveCmdBuffer->GetLayoutManager().NotifyDeletedImage(Image);
		}
	}

private:
	struct FQueryPoolReset
	{
		VkQueryPool Pool;
		TUINT32		Size;
	};

	FVulkanDevice*			   Device;
	FVulkanCommandListContext* Context;
	FVulkanCommandBufferPool   Pool;
	FVulkanQueue*			   Queue;
	FVulkanCmdBuffer*		   ActiveCmdBuffer;
	FVulkanCmdBuffer*		   UploadCmdBuffer;
	TVector<FQueryPoolReset>   PoolResets;

	/** This semaphore is used to prevent overlaps between the (current) graphics cmdbuf and next upload cmdbuf. */
	FVulkanSemaphoreRef ActiveCmdBufferSemaphore;

	/** Holds semaphores associated with the recent upload cmdbuf(s) - waiting to be added to the next graphics cmdbuf as WaitSemaphores. */
	TVector<FVulkanSemaphoreRef> RenderingCompletedSemaphores;

	/** This semaphore is used to prevent overlaps between (current) upload cmdbuf and next graphics cmdbuf. */
	FVulkanSemaphoreRef UploadCmdBufferSemaphore;

	/** Holds semaphores associated with the recent upload cmdbuf(s) - waiting to be added to the next graphics cmdbuf as WaitSemaphores. */
	TVector<FVulkanSemaphoreRef> UploadCompletedSemaphores;
};
