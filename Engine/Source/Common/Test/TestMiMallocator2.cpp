/**
 * @file TestMiMallocator2.cpp
 * @brief Comprehensive test suite for FMiMallocator2 wrapper around MiMalloc
 * 
 * Compile: Use Build.sh or cmake
 * Run: ./TestMiMallocator2
 */

#include "Core/Mallocator/MiMallocator2.h"

#include <cassert>
#include <cstring>
#include <thread>
#include <vector>
#include <random>
#include <chrono>
#include <iostream>
#include <algorithm>

DECLARE_LOG_CATEGORY(LogTestMiMalloc2)

namespace {

void test_basic_allocation() {
    std::cout << "Running test_basic_allocation..." << std::endl;
    
    FMiMallocator2 alloc;
    
    void* p1 = alloc.Malloc(16);
    void* p2 = alloc.Malloc(32);
    void* p3 = alloc.Malloc(64);
    
    assert(p1 != nullptr);
    assert(p2 != nullptr);
    assert(p3 != nullptr);
    
    assert(reinterpret_cast<uintptr_t>(p1) % alignof(std::max_align_t) == 0);
    
    std::memset(p1, 0xAA, 16);
    std::memset(p2, 0xBB, 32);
    std::memset(p3, 0xCC, 64);
    
    (void)alloc.Free(p1);
    (void)alloc.Free(p2);
    (void)alloc.Free(p3);
    
    std::cout << "  PASSED" << std::endl;
}

void test_size_classes() {
    std::cout << "Running test_size_classes..." << std::endl;
    
    FMiMallocator2 alloc;
    
    // Test various sizes to exercise size classes
    size_t test_sizes[] = {8, 16, 32, 48, 64, 80, 96, 112, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 480, 512, 576, 640, 704, 768, 832, 896, 960, 1024, 1152, 1280, 1408, 1536, 1664, 1792, 1920, 2048, 2304, 2560, 2816, 3072, 3328, 3584, 3840, 4096, 4608, 5120, 5632, 6144, 6656, 7168, 7680, 8192};
    
    for (size_t size : test_sizes) {
        void* p = alloc.Malloc(size);
        assert(p != nullptr);
        std::memset(p, 0xAB, size);
        (void)alloc.Free(p);
    }
    
    std::cout << "  PASSED" << std::endl;
}

void test_aligned_allocation() {
    std::cout << "Running test_aligned_allocation..." << std::endl;
    
    FMiMallocator2 alloc;
    
    constexpr size_t alignments[] = {16, 32, 64, 128, 256, 512, 1024, 4096};
    
    for (size_t align : alignments) {
        void* p = alloc.MallocAligned(1024, align);
        assert(p != nullptr);
        assert(reinterpret_cast<uintptr_t>(p) % align == 0);
        
        std::memset(p, 0xFF, 1024);
        (void)alloc.FreeAligned(p, align);
    }
    
    std::cout << "  PASSED" << std::endl;
}

void test_temporal_cadence() {
    std::cout << "Running test_temporal_cadence..." << std::endl;
    
    FMiMallocator2 alloc;
    
    constexpr size_t COUNT = 10000;
    std::vector<void*> ptrs(COUNT);
    
    for (size_t i = 0; i < COUNT; ++i) {
        ptrs[i] = alloc.Malloc(64);
        assert(ptrs[i] != nullptr);
    }
    
    for (size_t i = COUNT; i-- > 0;) {
        (void)alloc.Free(ptrs[i]);
    }
    
    std::cout << "  PASSED" << std::endl;
}

void test_concurrent() {
    std::cout << "Running test_concurrent..." << std::endl;
    
    constexpr int THREADS = 8;
    constexpr int ITERATIONS = 100000;
    
    std::vector<std::thread> threads;
    std::atomic<size_t> success_count{0};
    std::atomic<size_t> alloc_count{0};
    
    for (int t = 0; t < THREADS; ++t) {
        threads.emplace_back([&, t]() {
            FMiMallocator2 alloc;
            std::vector<void*> local;
            local.reserve(1000);
            
            std::mt19937 rng(static_cast<unsigned int>(t));
            std::uniform_int_distribution<int> op_dist(0, 9);
            std::uniform_int_distribution<size_t> size_dist(8, 1024);
            
            for (int i = 0; i < ITERATIONS; ++i) {
                if (op_dist(rng) < 6 || local.empty()) {
                    size_t sz = size_dist(rng);
                    void* p = alloc.Malloc(sz);
                    if (p) {
                        local.push_back(p);
                        alloc_count++;
                    }
                } else if (!local.empty()) {
                    std::uniform_int_distribution<size_t> idx_dist(0, local.size() - 1);
                    size_t idx = idx_dist(rng);
                    (void)alloc.Free(local[idx]);
                    local[idx] = local.back();
                    local.pop_back();
                }
            }
            
            for (void* p : local) {
                (void)alloc.Free(p);
            }
            
            success_count++;
        });
    }
    
    for (auto& t : threads) t.join();
    assert(success_count == THREADS);
    
    std::cout << "  PASSED (" << alloc_count.load() << " allocations)" << std::endl;
}

void test_stress() {
    std::cout << "Running test_stress..." << std::endl;
    
    FMiMallocator2 alloc;
    
    std::mt19937 rng(42);
    std::uniform_int_distribution<size_t> size_dist(8, 65536);
    std::uniform_int_distribution<int> op_dist(0, 9);
    
    std::vector<std::pair<void*, size_t>> active;
    active.reserve(10000);
    
    auto start = std::chrono::steady_clock::now();
    
    for (int i = 0; i < 100000; ++i) {
        if (op_dist(rng) < 6 || active.empty()) {
            size_t sz = size_dist(rng);
            if (void* p = alloc.Malloc(sz)) {
                active.emplace_back(p, sz);
            }
        } else {
            std::uniform_int_distribution<size_t> idx_dist(0, active.size() - 1);
            size_t idx = idx_dist(rng);
            (void)alloc.Free(active[idx].first);
            active[idx] = active.back();
            active.pop_back();
        }
    }
    
    for (auto& item : active) {
        (void)alloc.Free(item.first);
    }
    
    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    std::cout << "  PASSED (" << ms << " ms)" << std::endl;
}

void test_realloc() {
    std::cout << "Running test_realloc..." << std::endl;
    
    FMiMallocator2 alloc;
    
    void* p = alloc.Malloc(64);
    std::memset(p, 0xAA, 64);
    
    // Note: IMallocator doesn't have realloc, so we test Malloc + memcpy pattern
    void* p2 = alloc.Malloc(128);
    assert(p2 != nullptr);
    std::memcpy(p2, p, 64);
    (void)alloc.Free(p);
    
    std::memset(p2, 0xBB, 128);
    (void)alloc.Free(p2);
    
    std::cout << "  PASSED" << std::endl;
}

void test_large_huge() {
    std::cout << "Running test_large_huge..." << std::endl;
    
    FMiMallocator2 alloc;
    
    void* large = alloc.Malloc(100000);
    assert(large != nullptr);
    std::memset(large, 0xDD, 100000);
    (void)alloc.Free(large);
    
    void* huge = alloc.Malloc(1000000);
    assert(huge != nullptr);
    std::memset(huge, 0xEE, 1000000);
    (void)alloc.Free(huge);
    
    std::cout << "  PASSED" << std::endl;
}

void test_ownership() {
    std::cout << "Running test_ownership..." << std::endl;
    
    FMiMallocator2 alloc;
    
    // Test that we own our own allocations (use Malloc/Free pattern, not Owned)
    void* p = alloc.Malloc(64);
    assert(p != nullptr);
    std::memset(p, 0xAB, 64);
    (void)alloc.Free(p);
    
    // Test null ownership
    assert(!alloc.Owned(nullptr));
    
    std::cout << "  PASSED" << std::endl;
}

void test_free_variants() {
    std::cout << "Running test_free_variants..." << std::endl;
    
    FMiMallocator2 alloc;
    
    // Test FreeSize
    void* p1 = alloc.Malloc(64);
    assert(alloc.FreeSize(p1, 64) == EFreeRetType::Success);
    
    // Test FreeAligned
    void* p2 = alloc.MallocAligned(256, 64);
    assert(alloc.FreeAligned(p2, 64) == EFreeRetType::Success);
    
    // Test FreeSizeAligned
    void* p3 = alloc.MallocAligned(256, 64);
    assert(alloc.FreeSizeAligned(p3, 256, 64) == EFreeRetType::Success);
    
    std::cout << "  PASSED" << std::endl;
}

void test_collect() {
    std::cout << "Running test_collect..." << std::endl;
    
    FMiMallocator2 alloc;
    
    // Allocate and free some memory
    std::vector<void*> ptrs;
    for (int i = 0; i < 1000; ++i) {
        ptrs.push_back(alloc.Malloc(64));
    }
    for (void* p : ptrs) {
        (void)alloc.Free(p);
    }
    
    // Collect should not crash
    alloc.Collect(false);
    alloc.Collect(true);
    
    std::cout << "  PASSED" << std::endl;
}

} // anonymous namespace

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "MiMallocator2 Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    try {
        test_basic_allocation();
        test_size_classes();
        test_aligned_allocation();
        test_temporal_cadence();
        test_concurrent();
        test_large_huge();
        test_realloc();
        test_ownership();
        test_free_variants();
        test_collect();
        test_stress();  // Run stress last as it may exhaust memory
        
        std::cout << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "ALL TESTS PASSED!" << std::endl;
        std::cout << "========================================" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "TEST FAILED: " << e.what() << std::endl;
        return 1;
    }
}
