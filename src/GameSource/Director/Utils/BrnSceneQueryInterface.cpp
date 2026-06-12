#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x8221CD38
//   (BrnDirector::SceneQueryInterface::Clear)
//
// Behaviour-faithful to the X360 pseudocode. The interface holds six sub-object
// pointers in consecutive word slots (a1[1]..a1[6]); Clear resets each that is
// non-null:
//   slot 1: if set, hand off to sub_8221CC98 (the slot's own Reset/Clear) and
//           forward its result as the return value (null otherwise).
//   slot 2..6: zero one owned field at the noted byte offset.
//
// sub_8221CC98 is not yet reconstructed (no recovered name); declared here and
// resolved at link time.

namespace BrnDirector
{
    // Slot-1 reset helper (address-only name in the X360 build).
    u32* sub_8221CC98(void* lpSlot);

    struct SceneQueryInterface
    {
        u32* Clear();
    };

    u32* SceneQueryInterface::Clear()
    {
        void** lppSlots = reinterpret_cast<void**>(this);

        u32* lpResult = nullptr;
        if (lppSlots[1])
            lpResult = sub_8221CC98(lppSlots[1]);

        if (lppSlots[2]) *reinterpret_cast<u32*>(static_cast<u8*>(lppSlots[2]) + 160) = 0;
        if (lppSlots[3]) *reinterpret_cast<u32*>(static_cast<u8*>(lppSlots[3]) +  40) = 0;
        if (lppSlots[4]) *reinterpret_cast<u32*>(static_cast<u8*>(lppSlots[4]) +  40) = 0;
        if (lppSlots[5]) *reinterpret_cast<u32*>(static_cast<u8*>(lppSlots[5]) +   4) = 0;
        if (lppSlots[6]) *reinterpret_cast<u32*>(static_cast<u8*>(lppSlots[6]) +  40) = 0;

        return lpResult;
    }
}
