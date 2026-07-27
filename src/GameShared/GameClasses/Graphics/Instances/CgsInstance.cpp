#include "GameShared/GameClasses/Graphics/Instances/CgsInstance.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (GetInstance bounds check)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsGraphics::InstanceList::FixUp       @ 0x827F9318 (load-time fix-up)
//   CgsGraphics::InstanceList::FixDown     @ 0x827F9478 (inverse load-time fix-up)
//   CgsGraphics::InstanceList::GetInstance @ 0x822A3D90 (indexed element accessor)
// Behaviour-faithful to the X360 pseudocode / asm. FixUp rebases the instance buffer
// slot up by `delta` (only when non-zero) and tripwires the data version; FixDown is
// its inverse and additionally clears each instance's leading model handle (the model
// pointers are re-supplied by the bundle's import table on the way back in).
//
// The instance buffer slot is the console's 32-BIT on-disc slot -- see the layout note
// in CgsInstance.h. The relocation arithmetic runs in u32 exactly as the console does
// (the whole resource heap is reserved below 4 GB).

namespace CgsGraphics
{
    // X360 on-disk stride 80 == sizeof(Instance) (pinned by the header's static_assert).
    static const u32 kInstanceStride = sizeof(Instance);

    // X360 0x827F9318. `if (*this) *this += delta;` then the version tripwire
    // ("Instance data version mismatch. Code version = " ... the X360 streams both
    // versions through StrStream; CGS_ASSERT carries the static lead text).
    InstanceList* InstanceList::FixUp(int delta)
    {
        if (muaInstances)
        {
            muaInstances += static_cast<u32>(delta);
        }
        CGS_ASSERT(muVersionNumber == 1, "Instance data version mismatch. Code version = ");
        return this;
    }

    InstanceList* InstanceList::FixDown(int delta)
    {
        if (muaInstances)
        {
            for (u32 i = 0; i < muArraySize; ++i)
                *reinterpret_cast<u32*>(static_cast<uintptr_t>(muaInstances) + i * kInstanceStride) = 0;
            muaInstances -= static_cast<u32>(delta);
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
        return reinterpret_cast<Instance*>(static_cast<uintptr_t>(muaInstances) + luIndex * kInstanceStride);
    }
}
