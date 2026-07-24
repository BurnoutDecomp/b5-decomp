#include "GameShared/GameClasses/Graphics/Instances/CgsInstance.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (GetInstance bounds check)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsGraphics::InstanceList::FixDown     @ 0x...      (inverse load-time fix-up)
//   CgsGraphics::InstanceList::GetInstance @ 0x822A3D90 (indexed element accessor)
// First-pass reconstruction: behaviour-faithful to the X360 pseudocode / asm.
// FixDown is the inverse of a load-time pointer fix-up: it clears each instance's
// leading handle and rebases the instance buffer back down by `delta`. Member
// names/types per burnout.wiki (Instance List -> InstanceList); the base pointer is
// kept as uintptr_t for the relocation arithmetic.
//
// The 80-byte Instance element (an ON-DISK relocated resource body) is NOT committed
// to a shared header; only its stride is attested here, so GetInstance walks it via
// the documented opaque-element-stride precedent (the same reinterpret used by FixDown
// above) rather than a member-by-name access into an undeclared element layout.

namespace CgsGraphics
{
    // X360 on-disk stride 80; the PC element follows sizeof(Instance) (the model
    // pointer widens on x64 -- semantic-by-NAME, x64 gate).
    static const u32 kInstanceStride = sizeof(Instance);

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

    // GetInstance @ 0x822A3D90. Bounds-checks the index against muArraySize (a1[1],
    // the asm reads offset 4 -- the total entry count) and returns the address of the
    // luIndex-th Instance: mpaInstances + luIndex * 80. The asm computes the stride as
    // (luIndex + luIndex*4) << 4 == luIndex * 80 (CgsInstance.h:268 assert text).
    Instance* InstanceList::GetInstance(u32 luIndex) const
    {
        CGS_ASSERT(luIndex < muArraySize, "Instance index out of range");
        return reinterpret_cast<Instance*>(mpaInstances + luIndex * kInstanceStride);
    }
}
