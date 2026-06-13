#include "types.hpp"
#include <cstdint>

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x... (CgsGraphics::InstanceList::FixDown)
// First-pass reconstruction: behaviour-faithful to the X360 pseudocode.
// FixDown is the inverse of a load-time pointer fix-up: it clears each instance's
// leading handle and rebases the instance buffer back down by `delta`. Member
// names/types per burnout.wiki (Instance List -> InstanceList); the base pointer is
// kept as uintptr_t for the relocation arithmetic.

namespace CgsGraphics
{
    struct InstanceList
    {
        uintptr_t mpaInstances;    // 0x00 Instance* (base of the instance buffer)
        u32       muArraySize;     // 0x04 total Instance entries
        u32       muNumInstances;  // 0x08 complete Instance entries
        u32       muVersionNumber; // 0x0C

        InstanceList* FixDown(int delta);
    };

    static const u32 kInstanceStride = 80;  // bytes per instance

    InstanceList* InstanceList::FixDown(int delta)
    {
        if (mpaInstances)
        {
            for (u32 i = 0; i < muArraySize; ++i)
                *reinterpret_cast<u32*>(mpaInstances + i * kInstanceStride) = 0;
            mpaInstances -= delta;
        }
        return this;
    }
}
