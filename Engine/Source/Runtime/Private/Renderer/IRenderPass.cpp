/**
 * Copyright (c) 2026. MIT License. All rights reserved.
 *
 * Render Pass Interface - Implementation
 */

#include "Renderer/IRenderPass.h"
#include "Renderer/DeviceManager.h"

nvrhi::IDevice* IRenderPass::GetDevice() const
{
    return m_DeviceManager->GetDevice();
}

uint32_t IRenderPass::GetFrameIndex() const
{
    return m_DeviceManager->GetFrameIndex();
}
