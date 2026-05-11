// HLVM-Engine: Binding cache for renderer
#ifndef HLVM_ENGINE_RENDERER_COMMON_FBINDINGCACHE_H
#define HLVM_ENGINE_RENDERER_COMMON_FBINDINGCACHE_H

#include <nvrhi/nvrhi.h>
#include <shared_mutex>
#include <unordered_map>

/*
 * FBindingCache maintains a dictionary that maps binding set descriptors
 * into actual binding set objects. The binding sets are created on demand when
 * GetOrCreateBindingSet(...) is called and the requested binding set does not exist.
 * Created binding sets are stored for the lifetime of FBindingCache, or until
 * Clear() is called.
 *
 * All FBindingCache methods are thread-safe.
 */
class FBindingCache
{
public:
    FBindingCache() = default;
    explicit FBindingCache(nvrhi::IDevice* InDevice)
        : Device(InDevice)
    { }

    // Initialize or update the device
    void SetDevice(nvrhi::IDevice* InDevice) { Device = InDevice; }

    // Get the device
    nvrhi::IDevice* GetDevice() const { return Device; }

    // Get cached binding set - returns nullptr if not found
    nvrhi::BindingSetHandle GetCachedBindingSet(const nvrhi::BindingSetDesc& Desc, nvrhi::IBindingLayout* Layout);

    // Get or create binding set - creates if not found, caches result
    nvrhi::BindingSetHandle GetOrCreateBindingSet(const nvrhi::BindingSetDesc& Desc, nvrhi::IBindingLayout* Layout);

    // Clear all cached binding sets
    void Clear();

    // Shutdown - clear all and reset device
    void Shutdown() { Clear(); Device = nullptr; }

private:
    nvrhi::DeviceHandle Device;
    std::unordered_map<size_t, nvrhi::BindingSetHandle> BindingSets;
    mutable std::shared_mutex Mutex;
};
#endif // HLVM_ENGINE_RENDERER_COMMON_FBINDINGCACHE_H
