#pragma once

// Minimal owning home for the queue element CgsSceneManager::SceneManagerIO::
// InEventAddEntity -- the per-event payload stored in the
// EventQueue<InEventAddEntity, 5120> add-entity input queue
// (mAddEntityQueue inside InSceneUpdateInterface). This header exists so the
// explicit-instantiation TU for that queue's Construct
// (EventQueue_InEventAddEntity_5120.cpp) can see a COMPLETE element type (the queue
// embeds InEventAddEntity maEvents[5120] inline). The full InSceneUpdateInterface
// aggregate keeps its own placeholder slice and its own ledger TU; this header only
// adds the leaf element it queues, by name.
//
// ELEMENT LAYOUT (X360-authoritative, from the producer
// InSceneUpdateInterface::AddEntity @ 0x822B11F8 that stages the event before
// AddEvent). The staged event local is the 32-byte block passed by &address to
// InEventAddEntity::AddEvent:
//     +0x00  mTransformLane (Vector3, `stvx128 v1, r0, r10` -- a full 16-byte vmx lane)
//     +0x10  mEntityId      (u32,     `v12 = a2`)
//     +0x14  miField14      (s32,     `v13 = a3`)
//     +0x18  mfField18      (f32,     `v14 = a4`)
// alignas(16): the leading 16-byte vmx lane is stored with stvx128, so the element is
// 16-byte aligned and pads to 0x20 (32 bytes). The trailing scalars (entity id, an
// int, a float) round out the record. No DecFIGS DWARF covers this element; the field
// types are taken from the producer's stage stores (the vector lane, then the entity
// id / int / float scalars).

#include "types.hpp"
#include "BrnCommonTypes.h"                                  // Vector3 (16-byte SIMD lane)
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h" // EntityId

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores
    // events by byte image). Matches the SceneManagerIO::Event used by the other
    // SceneManager IO event payloads (see CgsSceneManagerIO_EventAddForCollision.h).
    struct EventBaseAddEntity {};

    // mAddEntityQueue element. 16-byte aligned (carries a vmx Vector3 lane).
    struct alignas(16) InEventAddEntity : public EventBaseAddEntity
    {
        Vector3  mTransformLane;  // +0x00 (stvx128 vmx lane)
        EntityId mEntityId;       // +0x10 (staged from a2)
        s32      miField14;       // +0x14 (staged from a3)
        f32      mfField18;       // +0x18 (staged from a4)
    };
}
}
