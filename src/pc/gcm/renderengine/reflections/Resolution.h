#pragma once

#include "types.hpp"
#include <atomic>

namespace CgsPC::Reflections
{
    // FLAG PC-platform leaf: the original cube is 128 pixels per face. Larger
    // native targets are optional and keep the same shared vehicle reflection map.
    inline s32& RequestedResolution() { static s32 siPixels = 128; return siPixels; }
    inline std::atomic<u32> suActiveResolution{0};

    inline u32 SanitizeResolution(s32 liPixels)
    {
        switch (liPixels)
        {
        case 128: case 256: case 512: case 1024: case 2048: return static_cast<u32>(liPixels);
        default: return 128;
        }
    }
    inline u32 LimitResolution(s32 liPixels, u32 luMaximum)
    {
        u32 luPixels = SanitizeResolution(liPixels);
        while (luPixels > luMaximum) luPixels >>= 1;
        return luPixels;
    }
    inline void SetActiveResolution(u32 luPixels) { suActiveResolution.store(luPixels, std::memory_order_relaxed); }
    inline u32 CaptureResolution()
    {
        const u32 luActive = suActiveResolution.load(std::memory_order_relaxed);
        return luActive ? luActive : SanitizeResolution(RequestedResolution());
    }
    u32 CreationResolution();
}
