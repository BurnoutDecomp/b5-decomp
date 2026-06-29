#pragma once

// ===========================================================================
// SDKs/EATech/include/Apt/AptActionQueueC.h
//
// AptActionQueueC -- the Apt animation director's deferred-action ring buffer: a
// fixed-capacity circular deque of 20-byte action records the player drains each
// frame (an action pushed by Flash, or a queued native function call). The
// AptAnimationTarget owns one at +0x0C.
//
// LAYOUT (20 bytes / 5 dwords) proven from the ctor @0x82AE6780 (allocates a
// (20*capacity + 4)-byte block -- a 4-byte size header then the record ring),
// AddActionBack @0x82AD94B0 + ClearActions @0x82ADF1D8 (the wrap test
// `cursor == &mpBuffer[20*mnCapacity]` -> reset to mpBuffer fixes the geometry):
//     +0x00 mpBuffer       AptActionRecord*  ring base (record stride 20 bytes)
//     +0x04 mpReadCursor   AptActionRecord*  drain end (ClearActions/dequeue reads here;
//                                            AddActionBack treats it as the "full" limit)
//     +0x08 mpWriteCursor  AptActionRecord*  enqueue end (AddActionBack writes + advances)
//     +0x0C mpField0C      void*             FLAG: role TBD (untouched by enqueue/clear)
//     +0x10 mnCapacity     u32               record-slot count
//
// Each 20-byte record: +0x00 meType (0 empty / 1 action / 2 function), +0x04 a param,
// +0x08 a target ref, +0x0C TBD, +0x10 a payload ptr -- carried as a small POD so the
// cursors are typed; the exact field semantics are refined when the drain (tick) TU lands.
//
// Member access is BY NAME; the X360 offsets are documentation only.
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "types.hpp"

// One deferred action slot (20 bytes on the X360). FLAG: field roles partially
// recovered (the enqueue path AddActionBack writes meType=1, the target ref and the
// payload ptr); refined when the queue-drain TU is homed.
struct AptActionRecord
{
    u32   meType;      // +0x00  0 = empty, 1 = action, 2 = function
    u32   mnParam;     // +0x04  AddActionBack's 4th arg
    void* mpTargetRef; // +0x08  the bound target/this reference
    u32   mField0C;    // +0x0C  FLAG: role TBD
    void* mpPayload;   // +0x10  AddActionBack's 2nd/3rd arg (the action/function payload)
};

struct AptActionQueueC
{
    AptActionRecord* mpBuffer;       // +0x00  ring base
    AptActionRecord* mpReadCursor;   // +0x04  drain end
    AptActionRecord* mpWriteCursor;  // +0x08  enqueue end
    void*            mpField0C;      // +0x0C  FLAG: role TBD
    u32              mnCapacity;     // +0x10  record-slot count
};
