#pragma once

#include <atomic>
#include <cstdint>

// FLAG PC-platform leaf: the update thread may expand a completed GDL while
// the renderer consumes the previous frame. Resource publication remains a
// joined phase. A fixup, import change, retirement or relocation invalidates
// that expansion before either frame is handed to the renderer.
namespace renderengine::MeshPreparationPC
{
    inline std::atomic<std::uint64_t> gResourceEpoch{0};

    inline std::uint64_t ResourceEpoch()
    {
        return gResourceEpoch.load(std::memory_order_relaxed);
    }

    inline void ResourceChanged()
    {
        gResourceEpoch.fetch_add(1, std::memory_order_relaxed);
    }
}
