// Copyright 2026 HLVM Engine
//
// MIT License

#pragma once

#include "Platform/PlatformDefinition.h"

/**
 * @brief Tracks memory usage against a configurable budget
 *
 * Simple allocator-style tracker for GPU resource memory.
 * Does NOT enforce eviction — just reports usage vs budget.
 */
class FMemoryBudget
{
public:
    FMemoryBudget() = default;

    // Set total budget in bytes (default: 256 MB)
    void SetBudgetBytes(size_t InBytes);

    // Attempt to allocate. Returns false if it would exceed budget.
    bool TryAllocate(size_t RequestedBytes);

    // Release previously allocated bytes
    void Free(size_t ReleasedBytes);

    // Current usage
    size_t GetUsedBytes() const { return UsedBytes; }

    // Total budget
    size_t GetBudgetBytes() const { return BudgetBytes; }

    // Usage ratio [0.0, 1.0+]
    float GetUtilization() const;

    // Bytes still available (clamped to 0)
    size_t GetAvailableBytes() const;

private:
    size_t BudgetBytes = 256ull * 1024 * 1024;  // 256 MB default
    size_t UsedBytes = 0;
};
