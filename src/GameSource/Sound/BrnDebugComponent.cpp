#include "GameSource/Sound/BrnDebugComponent.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnSound::Debug::DebugComponent::DebugComponent @ 0x827E0720
//
// The sound module's debug component constructor. The X360 stores the component vtable at +0x00
// (here installed by the C++ vtable of this derived type), then for each of the two embedded
// queue-like accumulators (+0x0C and +0x14) it writes a shared empty vtable, zeroes the entry
// count (the folded BasePriorityQueue::Clear), and writes the object's own vtable; finally it
// constructs the embedded LogWindow (+0x1C) and the Statistics block (+0x358).
//
// The two accumulators' exact element type is out of scope (see the header FLAG); their observable
// construction (vtable + count = 0) is reproduced by name. The LogWindow and Statistics members are
// real types and construct via their default constructors.

namespace BrnSound
{
namespace Debug
{
DebugComponent::DebugComponent()
    : CgsDev::DebugComponent()   // installs the base, then this type's vtable (the +0x00 store)
{
    // Two embedded event-queue accumulators: install a vtable slot and zero the count (the X360's
    // folded BasePriorityQueue::Clear). The X360 writes the guest vtable addresses off_820CDEF0 /
    // off_820CDEF8 here; those are guest data-segment pointers with NO host mapping, so storing
    // their raw values into a host pointer field would arm a wild indirect jump. Until the
    // accumulators' real class (and thus a real host vtable) is reconstructed, the slot is nulled
    // -- the observable "a vtable pointer is installed + count is zeroed" is modelled with the
    // guest values kept HERE in the comment only (fixed 2026-08-25, audio-faithfulness wave 1).
    mEventQueueA.mpVtable     = nullptr;   // X360: off_820CDEF0
    mEventQueueA.muNumEntries = 0;
    mEventQueueB.mpVtable     = nullptr;   // X360: off_820CDEF8
    mEventQueueB.muNumEntries = 0;

    // mLogWindow and mStatistics construct via their member default constructors (the X360's
    // explicit LogWindow(+0x1C) / Statistics(+0x358) ctor calls).
}
}
}
