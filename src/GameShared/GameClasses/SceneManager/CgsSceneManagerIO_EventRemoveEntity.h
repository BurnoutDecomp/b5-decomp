#pragma once

// Minimal owning home for the SceneManager "remove entity" event element
//   CgsSceneManager::SceneManagerIO::InEventRemoveEntity
// -- the per-event payload stored in the EventQueue<InEventRemoveEntity, N> remove-entity
// input queue that InSceneUpdateInterface embeds by value. This header exists so the
// explicit-instantiation TU for that queue's Construct
// (EventQueue_InEventRemoveEntity_10000.cpp) can see a COMPLETE element type (the queue
// embeds InEventRemoveEntity maEvents[N] inline, and the base queue's Append/AddEvent
// block-copy sizeof(T)-strided records). The full InSceneUpdateInterface aggregate keeps
// its own placeholder slice and its own ledger TU; this header only adds the leaf element
// it queues, by name.
//
// SIZE / ALIGNMENT (X360-attested):
//   * Construct @ 0x822E1FB0 does `addi r30, this, 0xC` -- the 12-byte BaseEventQueue
//     header needs no padding, so maEvents lives at +0xC (the element is 4-byte aligned).
//   * AddEvent @ 0x822AB180 copies the element as TWO 32-bit words
//     (`v12 = 8*a1[2] + *a1; *v12 = *a2; v12[1] = a2[1];`, `lwz r9,0(r26)` / `lwz r10,4(r26)`)
//     at a `slwi r,count,3` (x8) stride.
//   * Append @ 0x827A5F28 strides its block-copy by `slwi r,count,3` (x8).
//   => sizeof(InEventRemoveEntity) == 8, alignof 4 (two u32 fields, no 16-byte member).
//
// LAYOUT: an 8-byte record of two 32-bit words. The first word is the EntityId to remove
// (the 32-bit scene-entity handle, CgsEntityId.h); the second is the request's option
// word (the AddEvent copies it verbatim as the high word). Construct does not read the
// element interior and Append/AddEvent block-copy it whole, so only the size/alignment is
// load-bearing for this queue-instantiation TU; the two named u32 fields are reconstructed
// for a complete element type.

#include "types.hpp"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"   // CgsSceneManager::EntityId (u32 handle)

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores
    // events by byte image). Distinctly named so this leaf element home never ODR-clashes
    // with the SceneManagerIO::Event defined by the other per-element queue homes.
    struct EventBaseRemoveEntity {};

    // EventQueue<InEventRemoveEntity, N> element. 4-byte aligned, sizeof 8 (two u32 words).
    struct InEventRemoveEntity : public EventBaseRemoveEntity
    {
        EntityId mEntityId; // +0x00 -- the scene-entity handle to remove
        u32      muOptions;  // +0x04 -- per-request option word (copied verbatim by AddEvent)
    };
}
}
