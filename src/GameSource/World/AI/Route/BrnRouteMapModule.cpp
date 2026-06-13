#include "BrnRouteMapModule.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827E23F0
//   BrnAI::RouteMapModule::RouteMapModule
//
// Constructs the two read/write mutexes (handled by member construction),
// clears the intrusive-list anchor and wires its node pointers back to the
// anchor (empty circular list), and installs the static dispatch table.

namespace BrnAI
{
RouteMapModule::RouteMapModule()
{
    mAnchorState = 0;
    mUnk6505     = 0;
    mUnk6506     = 0;

    mpListHead   = &mAnchorState;
    mpListTail   = &mAnchorState;
    mpListCursor = &mAnchorState;
    mUnk6510     = 0;

    // Guest static dispatch table at 0x820CDFC4.
    mpAllocatorIface = reinterpret_cast<void*>(0x820CDFC4);
}
}
