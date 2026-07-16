// Copyright 2026 HLVM Engine
//
// MIT License

#include "Test.h"
#include "Renderer/Common/FBindingLayoutBuilder.h"

DECLARE_LOG_CATEGORY(LogTest)

RECORD(TestBindingLayoutBuilder_Shifts, true)
{
    HLVM_ASSERT(FBindingLayoutBuilder::TRegShift == 0);
    HLVM_ASSERT(FBindingLayoutBuilder::SRegShift == 128);
    HLVM_ASSERT(FBindingLayoutBuilder::BRegShift == 256);
    HLVM_ASSERT(FBindingLayoutBuilder::URegShift == 384);

    HLVM_ASSERT(FBindingSetBuilder::TRegShift == 0);
    HLVM_ASSERT(FBindingSetBuilder::SRegShift == 128);
    HLVM_ASSERT(FBindingSetBuilder::BRegShift == 256);
    HLVM_ASSERT(FBindingSetBuilder::URegShift == 384);
}

RECORD(TestBindingLayoutBuilder_LayoutBindings, true)
{
    FBindingLayoutBuilder Builder;
    Builder.SetVisibility(nvrhi::ShaderType::Compute)
           .AddConstantBuffer(0)           // b0 -> 256
           .AddConstantBuffer(1)           // b1 -> 257
           .AddTextureSRV(0)               // t0 -> 0
           .AddTextureSRV(1)               // t1 -> 1
           .AddTextureUAV(0)               // u0 -> 384
           .AddSampler(0)                  // s0 -> 128
           .AddStructuredBufferSRV(5)      // t5 -> 5
           .AddStructuredBufferUAV(2)      // u2 -> 386
           .AddRawBufferSRV(10)            // t10 -> 10
           .AddRawBufferUAV(3)             // u3 -> 387
           .AddTypedBufferSRV(7)           // t7 -> 7
           .AddTypedBufferUAV(1)           // u1 -> 385
           .AddRayTracingAccelStruct(0);  // t0 -> 0

    nvrhi::BindingLayoutDesc Desc = Builder.Build();

    HLVM_ASSERT(Desc.visibility == nvrhi::ShaderType::Compute);
    HLVM_ASSERT(Builder.GetItemCount() == 13);

    // Verify computed bindings
    HLVM_ASSERT(Desc.bindings[0].slot == 256);   // CB b0
    HLVM_ASSERT(Desc.bindings[1].slot == 257);   // CB b1
    HLVM_ASSERT(Desc.bindings[2].slot == 0);     // SRV t0
    HLVM_ASSERT(Desc.bindings[3].slot == 1);     // SRV t1
    HLVM_ASSERT(Desc.bindings[4].slot == 384);   // UAV u0
    HLVM_ASSERT(Desc.bindings[5].slot == 128);   // Sampler s0
    HLVM_ASSERT(Desc.bindings[6].slot == 5);     // Structured SRV t5
    HLVM_ASSERT(Desc.bindings[7].slot == 386);   // Structured UAV u2
    HLVM_ASSERT(Desc.bindings[8].slot == 10);    // Raw SRV t10
    HLVM_ASSERT(Desc.bindings[9].slot == 387);   // Raw UAV u3
    HLVM_ASSERT(Desc.bindings[10].slot == 7);    // Typed SRV t7
    HLVM_ASSERT(Desc.bindings[11].slot == 385);  // Typed UAV u1
    HLVM_ASSERT(Desc.bindings[12].slot == 0);    // RTAS t0
}

RECORD(TestBindingLayoutBuilder_Reset, true)
{
    FBindingLayoutBuilder Builder;
    Builder.AddConstantBuffer(0).AddTextureSRV(0);
    HLVM_ASSERT(Builder.GetItemCount() == 2);

    Builder.Reset();
    HLVM_ASSERT(Builder.GetItemCount() == 0);

    nvrhi::BindingLayoutDesc Desc = Builder.Build();
    HLVM_ASSERT(Desc.bindings.empty());
}

// Note: VulkanBindingOffsets control descriptor set placement, not binding numbers.
// The binding slot is always RegisterIndex + Shift. Custom offsets are for
// advanced multi-descriptor-set layouts and are not tested here.

RECORD(TestBindingLayoutBuilder_RawItemPassthrough, true)
{
    FBindingLayoutBuilder Builder;
    Builder.AddConstantBuffer(0)
           .AddItem(nvrhi::BindingLayoutItem::Texture_SRV(999)); // Raw bypass

    nvrhi::BindingLayoutDesc Desc = Builder.Build();
    HLVM_ASSERT(Desc.bindings.size() == 2);
    HLVM_ASSERT(Desc.bindings[0].slot == 256);  // Shifted
    HLVM_ASSERT(Desc.bindings[1].slot == 999);  // Raw (no shift)
}

RECORD(TestBindingSetBuilder_BuildDesc, true)
{
    FBindingSetBuilder Builder;
    Builder.SetConstantBuffer(0, nullptr)      // b0 -> 256
           .SetTextureSRV(0, nullptr)          // t0 -> 0
           .SetTextureUAV(0, nullptr)          // u0 -> 384
           .SetSampler(0, nullptr)             // s0 -> 128
           .SetStructuredBufferSRV(5, nullptr) // t5 -> 5
           .SetStructuredBufferUAV(2, nullptr) // u2 -> 386
           .SetRawBufferSRV(10, nullptr)       // t10 -> 10
           .SetRawBufferUAV(3, nullptr)        // u3 -> 387
           .SetTypedBufferSRV(7, nullptr)      // t7 -> 7
           .SetTypedBufferUAV(1, nullptr)      // u1 -> 385
           .SetRayTracingAccelStruct(0, nullptr); // t0 -> 0

    nvrhi::BindingSetDesc Desc = Builder.Build();
    HLVM_ASSERT(Builder.GetItemCount() == 11);

    HLVM_ASSERT(Desc.bindings[0].slot == 256);
    HLVM_ASSERT(Desc.bindings[1].slot == 0);
    HLVM_ASSERT(Desc.bindings[2].slot == 384);
    HLVM_ASSERT(Desc.bindings[3].slot == 128);
    HLVM_ASSERT(Desc.bindings[4].slot == 5);
    HLVM_ASSERT(Desc.bindings[5].slot == 386);
    HLVM_ASSERT(Desc.bindings[6].slot == 10);
    HLVM_ASSERT(Desc.bindings[7].slot == 387);
    HLVM_ASSERT(Desc.bindings[8].slot == 7);
    HLVM_ASSERT(Desc.bindings[9].slot == 385);
}
