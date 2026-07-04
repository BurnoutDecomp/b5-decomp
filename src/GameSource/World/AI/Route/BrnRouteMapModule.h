#ifndef BRN_ROUTE_MAP_MODULE_H
#define BRN_ROUTE_MAP_MODULE_H

#include "types.hpp"
#include <eathread/eathread_rwmutex.h>

namespace BrnAI
{
struct AISectionsData;

// Reconstructed from DWARF (BrnRouteMapModule.h:48-52). An 8-byte (section,portal) index pair;
// the element type of the RacingLineGenerator's ExtrapolatedIndexArray
// (CgsContainers::Array<BrnAI::SectionAndPortalIndices,16u>, typedef ExtrapolatedIndexArray).
// Two u32 words -> stride 8, matching the X360 Array<...,16>::operator[] `index*8 + base`
// accessor @0x8276AA08 (slwi r11,r28,3; add r3,r11,r29) and the live-count word at byte +0x80
// (== 16*8).
struct SectionAndPortalIndices
{
    u32 muSection; // DWARF BrnRouteMapModule.h:50
    u32 muPortal;  // DWARF BrnRouteMapModule.h:51
};

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827E23F0.
// The X360 layout places a large embedded working set between the two
// read/write mutexes and the trailing intrusive-list anchor; the unknown
// span is preserved with an explicit padding buffer so every recovered
// member is accessed by name (no raw offset casts).
class RouteMapModule
{
public:
    RouteMapModule();

    // Declared-only (bodied in the RouteMapModule TU). The route map owns a CgsResourcePtr to the
    // loaded AISectionsData; RouteMapDebugComponent::OnActivate (@0x8277FE50) resolves it via
    // AISectionsData_::GetMemoryResource(this + 0x65A0). Exposed as a named accessor so callers need
    // no raw offset into the working-set span. X360 offset 0x65A0 (26016).
    AISectionsData* GetAISectionsData() const;

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
