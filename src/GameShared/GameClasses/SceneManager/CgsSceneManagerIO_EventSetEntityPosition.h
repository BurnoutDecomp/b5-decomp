#pragma once

// Minimal owning home for the queue element CgsSceneManager::SceneManagerIO::
// InEventSetEntityPosition -- the per-event payload stored in the
// EventQueue<InEventSetEntityPosition, 1024> update-entity-position input queue
// (InSceneUpdateInterface::mUpdatePositionQueue, typedef InUpdateEntityPositionQueue;
// DWARF CgsSceneManagerIO_SceneUpdate.h:266/585). This header exists so the explicit-
// instantiation TU for that queue's Construct (EventQueue_InEventSetEntityPosition_1024.cpp)
// can see a COMPLETE element type (the queue embeds InEventSetEntityPosition maEvents[1024]
// inline, and the base queue's Append/AddEvent block-copy sizeof(T)-strided records). The
// full InSceneUpdateInterface aggregate keeps its own placeholder slice and its own ledger
// TU; this header only adds the leaf element it queues, by name.
//
// ELEMENT LAYOUT (DecFIGS DWARF CgsSceneManagerIO_SceneUpdate.h:88 -- struct
// InEventSetEntityPosition : public Event):
//     +0x00  mPosition   Vector3  (16B SIMD lane)                 (:90)
//     +0x10  mEntityId   EntityId (packed 32-bit scene handle)    (:91)
//     +0x14..0x1F  trailing pad to the 16-byte alignment forced by the leading Vector3 lane
// -> sizeof == 0x20 (32), alignas(16).
//
// SIZE / ALIGNMENT (X360-attested): Construct @ 0x822E1CA0 (InEventSetEntityPosition,1024)
// does `addi r30, this, 0x10` -- the 12-byte BaseEventQueue header rounds up to the element's
// 16-byte alignment (12 -> 16), so maEvents lives at +0x10. Construct does not read the
// element interior, so only the size/alignment is load-bearing for the queue-instantiation
// TU; the named fields are reconstructed for a complete element type.

#include "types.hpp"
#include "BrnCommonTypes.h"                                    // Vector3 (16-byte SIMD lane)
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"   // CgsSceneManager::EntityId

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores
    // events by byte image). Distinctly named so this leaf element home never ODR-clashes
    // with the SceneManagerIO::Event defined by the other per-element queue homes.
    struct EventBaseSetEntityPosition {};

    // EventQueue<InEventSetEntityPosition, N> element. 16-byte aligned (carries the
    // leading Vector3 lane), sizeof 0x20 (32).
    struct alignas(16) InEventSetEntityPosition : public EventBaseSetEntityPosition
    {
        Vector3  mPosition;   // +0x00 (16B SIMD lane)
        EntityId mEntityId;   // +0x10
        // +0x14..0x1F trailing pad to the 16-byte alignment forced by the Vector3 lane
        // -> sizeof == 0x20.
    };
}
}
