#include "GameShared/GameClasses/Gui/CgsEventInterpreterModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// CgsGui::EventInterpreterModule::ObjectEventObserver member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU
// (class:CgsGui::EventInterpreterModule::ObjectEventObserver) bodies the three X360-emitted
// functions:
//
//   RegisterForEvent     @ 0x8284EA28  -> set   bit luEventIndex (this+8 BitArray)
//   UnRegisterForEvent   @ 0x8284EB88  -> clear bit luEventIndex
//   IsRegisteredForEvent @ 0x8284EC78  -> test  bit luEventIndex
//
// X360 store-for-store notes:
//   - Bit-array math: the registration mask is an array of 64-bit words based at this+8
//     (`v10 = this + 2 dwords` in Register/UnRegister; IsRegistered reads
//     `*(8*((i>>6)+1) + this)` == word i/64 of the array at this+8). The set/clear/test bodies
//     are the inlined SetBit/UnSetBit/IsBitSet of CgsContainers::BitArray<600>:
//       field = i / 64                       (`rlwinm r11,i,29,3,28` == (i>>3)&0x1FFFFFF8 byte
//                                              offset == (i/64)*8 bytes per 64-bit word; or
//                                              `srwi r11,i,6` word index in IsRegistered)
//       mask  = (u64)1 << (i & 63)           (`clrldi r10,i,58; sld r10,1,r10`)
//       Register   : word |= mask            (`or  r10,mask,word`)
//       UnRegister : word &= ~mask           (`andc r10,word,mask`)
//       IsRegistered: (word & mask) != 0     (`and r11,mask,word; cmpldi; bne`)
//   - Range checks (two asserts, both reproduced):
//       1. "Cannot register for invalid events" -- the X360 fires when (s32)i < 0 or i > 600
//          (`cmpwi i,-1; ble` / `cmpwi i,0x258; ble`); the valid index range is 0..600.
//       2. The CgsBitArray bound -- the X360 fires when i >= 600 (`cmplwi i,0x258; blt`).
//          RegisterForEvent/IsRegisteredForEvent stream a dynamic "Index: <i>, Number of bits:
//          600" message (reduced to a static CGS_ASSERT string); UnRegisterForEvent uses the
//          baked "luIndex < NUMBITS" string verbatim.
//   - The X360-baked d:\p4 CgsEventInterpreterModule.h / CgsBitArray.h file/line cites
//     (h:344/362/380, CgsBitArray.h:203/222/241) are discarded per project convention.

namespace CgsGui
{
    // X360 0x8284EA28.
    void EventInterpreterModule::ObjectEventObserver::RegisterForEvent(u32 luEventIndex)
    {
        static_assert(offsetof(ObjectEventObserver, mxRegisteredEvents) == 0x08,
                      "mxRegisteredEvents @ +0x08");
        CGS_ASSERT(luEventIndex <= KU_NUM_EVENTS, "Cannot register for invalid events");
        CGS_ASSERT(luEventIndex < KU_NUM_EVENTS, "luIndex < NUMBITS");
        mxRegisteredEvents.SetBit(luEventIndex);
    }

    // X360 0x8284EB88.
    void EventInterpreterModule::ObjectEventObserver::UnRegisterForEvent(u32 luEventIndex)
    {
        CGS_ASSERT(luEventIndex <= KU_NUM_EVENTS, "Cannot register for invalid events");
        CGS_ASSERT(luEventIndex < KU_NUM_EVENTS, "luIndex < NUMBITS");
        mxRegisteredEvents.UnSetBit(luEventIndex);
    }

    // X360 0x8284EC78.
    bool EventInterpreterModule::ObjectEventObserver::IsRegisteredForEvent(u32 luEventIndex) const
    {
        CGS_ASSERT(luEventIndex <= KU_NUM_EVENTS, "Cannot register for invalid events");
        CGS_ASSERT(luEventIndex < KU_NUM_EVENTS, "luIndex < NUMBITS");
        return mxRegisteredEvents.IsBitSet(luEventIndex);
    }
}
