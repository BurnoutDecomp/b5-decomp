#ifndef BRN_ROUTE_MAP_MODULE_H
#define BRN_ROUTE_MAP_MODULE_H

#include "types.hpp"
#include <eathread/eathread_rwmutex.h>

namespace BrnAI
{
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827E23F0.
// The X360 layout places a large embedded working set between the two
// read/write mutexes and the trailing intrusive-list anchor; the unknown
// span is preserved with an explicit padding buffer so every recovered
// member is accessed by name (no raw offset casts).
class RouteMapModule
{
public:
    RouteMapModule();

private:
    EA::Thread::RWMutex mReadWriteMutexA;   // guest index 4
    EA::Thread::RWMutex mReadWriteMutexB;   // guest index 70
    u8                  mWorkingSetPad[25472]; // unknown embedded span up to the anchor

    // Trailing intrusive-list anchor (circular list head: the three node
    // pointers are initialised to the anchor itself when empty).
    u32   mAnchorState;     // guest index 6504
    u32   mUnk6505;
    u32   mUnk6506;
    void* mpListHead;       // guest index 6507
    void* mpListTail;       // guest index 6508
    void* mpListCursor;     // guest index 6509
    u32   mUnk6510;
    u32   mUnk6511;
    u32   mUnk6512;
    void* mpAllocatorIface; // guest index 6513 -> static dispatch table
};
}

#endif
