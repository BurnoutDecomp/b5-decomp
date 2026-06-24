#ifndef GAMESOURCE_DIRECTOR_SHAREDIO_BRN_DIRECTOR_EVENTS_H
#define GAMESOURCE_DIRECTOR_SHAREDIO_BRN_DIRECTOR_EVENTS_H

#include "types.hpp"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h"  // Attribute::Key

// ============================================================================
// GameSource/Director/SharedIO/BrnDirectorEvents.h
//
// Director shared-IO event payloads. The boot path embeds the NewVehicleEvent queue
// in BrnDirectorVehicleInputInterface (an EventQueue<NewVehicleEvent,50>); world->director
// bridging pushes/merges these as cars enter the simulation.
//
// NewVehicleEvent layout/order/types verbatim from the DecFIGS DWARF
// (BrnDirectorEvents.h:37): Attribute::Key mAttribsKey (u32) + int32_t miEntityIndex.
// ----------------------------------------------------------------------------
// The X360 BaseEventQueue<NewVehicleEvent>::AddEvent @0x822C7308 copies each element as
// TWO qwords (std 0(dst); std 8(dst)) at a `16 * miLength` stride, and Append @0x823C2CB8
// XMemCpy's `16 * count` bytes -- i.e. the event STRIDE is 16 bytes, not the 8 the two
// 4-byte fields would take naturally. The event is therefore 16-byte aligned (the queue's
// inline maEvents buffer is a SIMD-aligned block). alignas(16) reproduces that stride.
// ============================================================================

namespace BrnDirector
{
    // DWARF: BrnDirectorEvents.h:37. A car has entered the simulation: which attribute-system
    // asset it was spawned from (mAttribsKey) and which world entity slot it occupies
    // (miEntityIndex). 16-byte aligned so the queue element stride matches the X360's 16-byte
    // per-event copy (see header note above).
    struct alignas(16) NewVehicleEvent
    {
        Attribute::Key mAttribsKey;     // +0x00  source asset attribute key
        s32            miEntityIndex;   // +0x04  world entity slot index
    };
}

#endif // GAMESOURCE_DIRECTOR_SHAREDIO_BRN_DIRECTOR_EVENTS_H
