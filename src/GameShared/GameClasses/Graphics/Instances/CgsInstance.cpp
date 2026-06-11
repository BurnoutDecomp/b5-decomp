#include "types.hpp"
#include <cstdint>

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x... (CgsGraphics::InstanceList::FixDown)
// First-pass reconstruction: behaviour-faithful to the X360 pseudocode.
// FixDown is the inverse of a load-time pointer fix-up: it clears each instance's
// leading handle and rebases the instance buffer back down by `delta`.

namespace CgsGraphics
{
    struct InstanceList
    {
        uintptr_t instances;  // [0]: base address of the instance buffer
        u32       count;      // [1]: number of instances

        InstanceList* FixDown(int delta);
    };

    static const u32 kInstanceStride = 80;  // bytes per instance

    InstanceList* InstanceList::FixDown(int delta)
    {
        if (instances)
        {
            for (u32 i = 0; i < count; ++i)
                *reinterpret_cast<u32*>(instances + i * kInstanceStride) = 0;
            instances -= delta;
        }
        return this;
    }
}
