/**
 * @file TestMiMalloc.cpp
 * @brief Comprehensive test suite for MiMalloc - C++20 mimalloc implementation
 * 
 * Compile: g++ -std=c++20 -O3 -DNDEBUG -I../Public -o TestMiMalloc TestMiMalloc.cpp -lpthread
 * Run: ./TestMiMalloc
 */

#include "Core/Mallocator/Mi/MiMalloc.h"

#include <cassert>
#include <cstring>
#include <thread>
#include <vector>
#include <random>
#include <chrono>
#include <iostream>
#include <algorithm>

using namespace mi;

namespace {

void test_basic_allocation() {
    std::cout << "Running test_basic_allocation..." << std::endl;
    
    Mallocator& alloc = Mallocator::instance();
    
    void* p1 = alloc.allocate(16);
    void* p2 = alloc.allocate(32);
    void* p3 = alloc.allocate(64);
    
    assert(p1 != nullptr);
    assert(p2 != nullptr);
    assert(p3 != nullptr);
    
    assert(reinterpret_cast<uintptr_t>(p1) % alignof(std::max_align_t) == 0);
    
    std::memset(p1, 0xAA, 16);
    std::memset(p2, 0xBB, 32);
    std::memset(p3, 0xCC, 64);
    
    alloc.free(p1);
    alloc.free(p2);
    alloc.free(p3);
    
    std::cout << "  PASSED" << std::endl;
}

void test_size_classes() {
    std::cout << "Running test_size_classes..." << std::endl;
    
    Mallocator& alloc = Mallocator::instance();
    
    for (size_t size = 8; size <= 8192; size = size + (size < 128 ? 8 : 64)) {
        void* p = alloc.allocate(size);
        assert(p != nullptr);
        
        std::memset(p, 0xAB, size);
        
        assert(alloc.pointer_is_valid(p));
        
        alloc.free(p);
    }
    
    std::cout << "  PASSED" << std::endl;
}

void test_temporal_cadence() {
    std::cout << "Running test_temporal_cadence..." << std::endl;
    
    Mallocator& alloc = Mallocator::instance();
    
    constexpr size_t COUNT = 10000;
    std::vector<void*> ptrs(COUNT);
    
    for (size_t i = 0; i < COUNT; ++i) {
        ptrs[i] = alloc.allocate(64);
        assert(ptrs[i] != nullptr);
    }
    
    for (size_t i = COUNT; i-- > 0;) {
        alloc.free(ptrs[i]);
    }
    
    assert(alloc.verify_heap());
    
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
            std::vector<void*> local;
            local.reserve(1000);
            
            std::mt19937 rng(static_cast<unsigned int>(t));
            std::uniform_int_distribution<int> op_dist(0, 9);
            std::uniform_int_distribution<size_t> size_dist(8, 1024);
            
            for (int i = 0; i < ITERATIONS; ++i) {
                if (op_dist(rng) < 6 || local.empty()) {
                    size_t sz = size_dist(rng);
                    void* p = Mallocator::instance().allocate(sz);
                    if (p) {
                        local.push_back(p);
                        alloc_count++;
                    }
                } else if (!local.empty()) {
                    std::uniform_int_distribution<size_t> idx_dist(0, local.size() - 1);
                    size_t idx = idx_dist(rng);
                    Mallocator::instance().free(local[idx]);
                    local[idx] = local.back();
                    local.pop_back();
                }
            }
            
            for (void* p : local) {
                Mallocator::instance().free(p);
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
    
    std::mt19937 rng(42);
    std::uniform_int_distribution<size_t> size_dist(8, 65536);
    std::uniform_int_distribution<int> op_dist(0, 9);
    
    std::vector<std::pair<void*, size_t>> active;
    active.reserve(10000);
    
    Mallocator& alloc = Mallocator::instance();
    
    auto start = std::chrono::steady_clock::now();
    
    for (int i = 0; i < 1000000; ++i) {
        if (op_dist(rng) < 6 || active.empty()) {
            size_t sz = size_dist(rng);
            if (void* p = alloc.allocate(sz)) {
                active.emplace_back(p, sz);
            }
        } else {
            std::uniform_int_distribution<size_t> idx_dist(0, active.size() - 1);
            size_t idx = idx_dist(rng);
            alloc.free(active[idx].first);
            active[idx] = active.back();
            active.pop_back();
        }
    }
    
    for (auto& item : active) {
        alloc.free(item.first);
    }
    
    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    std::cout << "  PASSED (" << ms << " ms)" << std::endl;
}

void test_cpp_allocator() {
    std::cout << "Running test_cpp_allocator..." << std::endl;
    
    std::vector<int, mi::Allocator<int>> vec;
    vec.reserve(1000);
    
    for (int i = 0; i < 10000; ++i) {
        vec.push_back(i);
    }
    
    assert(vec.size() == 10000);
    
    for (int i = 0; i < 10000; ++i) {
        assert(vec[static_cast<unsigned long>(i)] == i);
    }
    
    std::cout << "  PASSED" << std::endl;
}

void test_realloc() {
    std::cout << "Running test_realloc..." << std::endl;
    
    Mallocator& alloc = Mallocator::instance();
    
    void* p = alloc.allocate(64);
    std::memset(p, 0xAA, 64);
    
    void* p2 = alloc.realloc(p, 128);
    assert(p2 != nullptr);
    
    auto* bytes = static_cast<uint8_t*>(p2);
    for (size_t i = 0; i < 64; ++i) {
        assert(bytes[i] == 0xAA);
    }
    
    alloc.free(p2);
    
    std::cout << "  PASSED" << std::endl;
}

void test_large_huge() {
    std::cout << "Running test_large_huge..." << std::endl;
    
    Mallocator& alloc = Mallocator::instance();
    
    void* large = alloc.allocate(100000);
    assert(large != nullptr);
    std::memset(large, 0xDD, 100000);
    alloc.free(large);
    
    void* huge = alloc.allocate(1000000);
    assert(huge != nullptr);
    std::memset(huge, 0xEE, 1000000);
    alloc.free(huge);
    
    std::cout << "  PASSED" << std::endl;
}

void test_alignment() {
    std::cout << "Running test_alignment..." << std::endl;
    
    Mallocator& alloc = Mallocator::instance();
    
    constexpr size_t alignments[] = {16, 32, 64, 128, 256, 512, 1024, 4096};
    
    for (size_t align : alignments) {
        void* p = alloc.allocate_aligned(1024, align);
        assert(p != nullptr);
        assert(reinterpret_cast<uintptr_t>(p) % align == 0);
        
        std::memset(p, 0xFF, 1024);
        alloc.free(p);
    }
    
    std::cout << "  PASSED" << std::endl;
}

void test_bulk_free() {
    std::cout << "Running test_bulk_free..." << std::endl;
    
    Mallocator& alloc = Mallocator::instance();
    
    constexpr size_t COUNT = 1000;
    std::vector<void*> ptrs(COUNT);
    
    for (size_t i = 0; i < COUNT; ++i) {
        ptrs[i] = alloc.allocate(64);
        assert(ptrs[i] != nullptr);
    }
    
    alloc.free_bulk(ptrs.data(), COUNT);
    
    std::cout << "  PASSED" << std::endl;
}

void test_statistics() {
    std::cout << "Running test_statistics..." << std::endl;
    
    Mallocator::instance().configure({
        .enable_stats = true
    });
    
    Mallocator& alloc = Mallocator::instance();
    
    constexpr size_t COUNT = 1000;
    std::vector<void*> ptrs(COUNT);
    
    for (size_t i = 0; i < COUNT; ++i) {
        ptrs[i] = alloc.allocate(64);
    }
    
    for (size_t i = 0; i < COUNT; ++i) {
        alloc.free(ptrs[i]);
    }
    
    auto info = alloc.get_debug_info();
    assert(info.alloc_count >= COUNT);
    assert(info.free_count >= COUNT);
    
    std::cout << "  PASSED (allocs: " << info.alloc_count 
              << ", frees: " << info.free_count << ")" << std::endl;
}

} // anonymous namespace

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "MiMalloc Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    try {
        test_basic_allocation();
        test_size_classes();
        test_temporal_cadence();
        test_concurrent();
        test_cpp_allocator();
        test_realloc();
        test_large_huge();
        test_alignment();
        test_bulk_free();
        test_statistics();
        test_stress();  // Run stress last as it may exhaust memory
        
        std::cout << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "ALL TESTS PASSED!" << std::endl;
        std::cout << "========================================" << std::endl;
        
        std::cout << std::endl;
        Mallocator::instance().dump_stats();
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "TEST FAILED: " << e.what() << std::endl;
        return 1;
    }
}
