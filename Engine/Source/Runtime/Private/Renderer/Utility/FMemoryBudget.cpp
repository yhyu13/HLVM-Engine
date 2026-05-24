// Copyright 2026 HLVM Engine
//
// MIT License

#include "Renderer/Utility/FMemoryBudget.h"
#include "Core/Log.h"

DECLARE_LOG_CATEGORY(LogMemoryBudget)

void FMemoryBudget::SetBudgetBytes(size_t InBytes)
{
    BudgetBytes = InBytes;
    HLVM_LOG(LogMemoryBudget, info, TXT("FMemoryBudget: Budget set to {} bytes"), BudgetBytes);
}

bool FMemoryBudget::TryAllocate(size_t RequestedBytes)
{
    if (UsedBytes + RequestedBytes > BudgetBytes)
    {
        HLVM_LOG(LogMemoryBudget, warn,
            TXT("FMemoryBudget: Allocation of {} bytes would exceed budget (used {}, budget {})"),
            RequestedBytes, UsedBytes, BudgetBytes);
        return false;
    }

    UsedBytes += RequestedBytes;
    return true;
}

void FMemoryBudget::Free(size_t ReleasedBytes)
{
    if (ReleasedBytes > UsedBytes)
    {
        HLVM_LOG(LogMemoryBudget, warn,
            TXT("FMemoryBudget: Freeing {} bytes but only {} used — clamping"),
            ReleasedBytes, UsedBytes);
        UsedBytes = 0;
    }
    else
    {
        UsedBytes -= ReleasedBytes;
    }
}

float FMemoryBudget::GetUtilization() const
{
    if (BudgetBytes == 0)
    {
        return 0.0f;
    }
    return static_cast<float>(UsedBytes) / static_cast<float>(BudgetBytes);
}

size_t FMemoryBudget::GetAvailableBytes() const
{
    return UsedBytes >= BudgetBytes ? 0 : BudgetBytes - UsedBytes;
}
