/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 */

// Reference
// https://github.com/RobertBeckebans/RBDOOM-3-BFG/blob/4310fbd200b578014b22dce5fa82a48977eb149a/neo/sys/DeviceManager.h
// https://github.com/NVIDIA-RTX/Donut/blob/2c1077673edb0e0d814c202e6ce8e502d245c2da/include/donut/app/DeviceManager.h
#pragma once

#include "Renderer/Window/WindowDefinition.h"

#if HLVM_VULKAN_RENDERER
	#include "Renderer/Window/GLFW3/GLFW3VulkanWindow.h"
#endif

#include "Renderer/RHI/RHICommon.h"
#include "Renderer/IRenderPass.h"

/*-----------------------------------------------------------------------------
   Device Creation Parameters
-----------------------------------------------------------------------------*/

/**
 * Parameters for device and window creation
 */
struct FDeviceCreationParameters
{
	// Window configuration
	bool		  bStartMaximized = false;
	bool		  bStartFullscreen = false;
	bool		  bAllowModeSwitch = false;
	TINT32		  WindowPosX = -1; // -1 means use default placement
	TINT32		  WindowPosY = -1;
	TUINT32		  BackBufferWidth = 1280;
	TUINT32		  BackBufferHeight = 720;
	TUINT32		  BackBufferSampleCount = 1; // optional HDR Framebuffer MSAA
	TUINT32		  RefreshRate = 0;
	TUINT32		  SwapChainBufferCount = hlvm_rhi::MAX_FRAMES_IN_FLIGHT; // SRS - default matches GPU frames, can be overridden by renderer
	nvrhi::Format SwapChainFormat = nvrhi::Format::RGBA8_UNORM;			 // RB: don't do the sRGB gamma ramp with the swapchain
	TUINT32		  SwapChainSampleCount = 1;
	TUINT32		  SwapChainSampleQuality = 0;
	TINT32		  VSyncMode = 0;

	// Feature flags
	bool bEnableRayTracingExtensions = false; // for vulkan
	bool bEnableComputeQueue = false;
	bool bEnableCopyQueue = false;

	// Debug and validation
	bool bEnableDebugRuntime = false;
	bool bEnableNVRHIValidationLayer = false;

	// Adapter selection
	std::wstring AdapterNameSubstring = L"";

	// DPI scaling
	bool bEnablePerMonitorDPI = false;

#if HLVM_VULKAN_RENDERER
	// Vulkan-specific extensions and layers
	TVector<std::string> RequiredVulkanInstanceExtensions;
	TVector<std::string> RequiredVulkanDeviceExtensions;
	TVector<std::string> RequiredVulkanLayers;
	TVector<std::string> OptionalVulkanInstanceExtensions;
	TVector<std::string> OptionalVulkanDeviceExtensions;
	TVector<std::string> OptionalVulkanLayers;
	TVector<size_t>		 IgnoredVulkanValidationMessageLocations;
#endif

	// Feature flags
	bool	bEnableImageFormatD24S8 = true;
	TUINT32 MaxPushConstantSize = 0;
};

/*-----------------------------------------------------------------------------
   Message Callback System
-----------------------------------------------------------------------------*/

/**
 * Default message callback implementation for NVRHI
 */
struct FNVRHIMessageCallback : public nvrhi::IMessageCallback
{
	static FNVRHIMessageCallback& GetInstance();
	void						  message(nvrhi::MessageSeverity Severity, const char* MessageText) override;
};
/*-----------------------------------------------------------------------------
   Forward Declarations
-----------------------------------------------------------------------------*/

class IRenderBackend;

/*-----------------------------------------------------------------------------
   Main Device Manager Interface
-----------------------------------------------------------------------------*/

/**
 * Manages window creation, graphics device initialization, and swap chain management.
 * Abstract base class that provides common interface for different graphics APIs.
 */
class FDeviceManager
{
public:
	// Factory method
	static TUniquePtr<FDeviceManager> Create(nvrhi::GraphicsAPI Api);

	// Window and device lifecycle
	virtual bool CreateWindowDeviceAndSwapChain(const IWindow::Properties& Params) = 0;
	virtual void Shutdown() = 0;

	// Window management
	void		  GetWindowDimensions(TUINT32& OutWidth, TUINT32& OutHeight) const;
	virtual void  GetDPIScaleInfo(float& OutScaleX, float& OutScaleY) const = 0;
	virtual void  UpdateWindowSize(const FUInt2& Params) = 0;
	virtual void* GetGLFWWindow() const { return nullptr; }

	// Rendering interface
	virtual bool BeginFrame() = 0;
	virtual bool EndFrame() = 0;
	virtual bool Present() = 0;

	// Resource access
	[[nodiscard]] virtual nvrhi::IDevice*	 GetDevice() const = 0;
	[[nodiscard]] virtual const char*		 GetRendererString() const = 0;
	[[nodiscard]] virtual nvrhi::GraphicsAPI GetGraphicsAPI() const = 0;

	virtual nvrhi::ITexture* GetCurrentBackBuffer() = 0;
	virtual nvrhi::ITexture* GetBackBuffer(TUINT32 Index) = 0;
	virtual TUINT32			 GetCurrentBackBufferIndex() = 0;
	virtual TUINT32			 GetBackBufferCount() = 0;
	// Depth texture access
	virtual nvrhi::ITexture* GetDepthTexture(TUINT32 Index) = 0;
	// Framebuffer management
	nvrhi::IFramebuffer*		 GetCurrentFramebuffer();
	virtual nvrhi::IFramebuffer* GetFramebuffer(TUINT32 Index) = 0;

	// Configuration
	const FDeviceCreationParameters& GetDeviceParams() const { return DeviceParams; }
	virtual void					 SetVSyncMode(TINT32 VSyncMode) = 0;

	// Utility methods
	[[nodiscard]] TUINT32 GetFrameIndex() const { return FrameIndex; }
	TUINT32				  GetMaxPushConstantSize() const { return DeviceParams.MaxPushConstantSize; }

	// Vulkan-specific extension queries (only meaningful when using Vulkan)
#if HLVM_VULKAN_RENDERER
	virtual bool IsVulkanInstanceExtensionEnabled(const char* /*ExtensionName*/) const { return false; }
	virtual bool IsVulkanDeviceExtensionEnabled(const char* /*ExtensionName*/) const { return false; }
	virtual bool IsVulkanLayerEnabled(const char* /*LayerName*/) const { return false; }
	virtual void GetEnabledVulkanInstanceExtensions(TVector<std::string>& /*OutExtensions*/) const {}
	virtual void GetEnabledVulkanDeviceExtensions(TVector<std::string>& /*OutExtensions*/) const {}
	virtual void GetEnabledVulkanLayers(TVector<std::string>& /*OutLayers*/) const {}

	// ImGui integration - Vulkan handles
	virtual void*  GetVkInstance() const { return nullptr; }
	virtual void*  GetVkPhysicalDevice() const { return nullptr; }
	virtual void*  GetVkDevice() const { return nullptr; }
	virtual void*  GetGraphicsQueue() const { return nullptr; }
	virtual void*  GetRenderPass() const { return nullptr; }
	virtual void*  GetImGuiDescriptorPool() const { return nullptr; }
	virtual TINT32 GetImGuiQueueFamilyIndex() const { return -1; }
#endif

	// OpenVR integration
	virtual TINT32 GetGraphicsFamilyIndex() const { return -1; }

	virtual ~FDeviceManager() = default;

	// Render pass management
	void AddRenderPassToFront(TSharedPtr<IRenderPass> pass);
	void AddRenderPassToBack(TSharedPtr<IRenderPass> pass);
	void RemoveRenderPass(TSharedPtr<IRenderPass> pass);
	void RunMessageLoop();
	void StopMessageLoop();

protected:
	// Construction
	FDeviceManager() = default;

	// Friends
	friend class IRenderBackend;
	friend class FImage;

	// Protected members
	TUniquePtr<IWindow> WindowHandle = nullptr;
	bool				bWindowVisible = false;

	FDeviceCreationParameters DeviceParams;
	FString					  WindowTitle;

	float	DPIScaleFactorX = 1.0f;
	float	DPIScaleFactorY = 1.0f;
	TINT32	RequestedVSync = 0;
	TUINT32 FrameIndex = 0;

	TVector<TSharedPtr<IRenderPass>> m_vRenderPasses;
	bool							 m_bIsRendering = false;

	// Framebuffer lifecycle management (called by derived classes)
	virtual void   OnBeforeSwapchainRecreate() {}
	virtual void   OnAfterSwapchainRecreate() {}
	::EGpuVendorID GetGPUVendor(TUINT32 VendorID) const;
	void		   BackBufferResizing();
	void		   BackBufferResized();

	// Pure virtual methods for derived classes
	virtual bool CreateDeviceAndSwapChain() = 0;
	virtual void DestroyDeviceAndSwapChain() = 0;
	virtual void ResizeSwapChain() = 0;
};
