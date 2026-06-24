#pragma once

// Minimal owning home for the queue element CgsSceneManager::SceneManagerIO::
// InEventSetEntityRadius -- the per-event payload stored in the
// EventQueue<InEventSetEntityRadius, 512> set-entity-radius input queue
// (InSceneUpdateInterface::mSetEntityRadiusQueue, typedef InSetEntityRadiusQueue;
// DWARF home CgsSceneManagerIO_SceneUpdate.h:336/339). This header exists so the
// explicit-instantiation TU for that queue's Construct
// (EventQueue_InEventSetEntityRadius_512.cpp) can see a COMPLETE element type (the
// queue embeds InEventSetEntityRadius maEvents[512] inline). The full
// InSceneUpdateInterface aggregate keeps its own placeholder slice and its own
// ledger TU -- this header only adds the leaf element it queues, by name.
//
// ELEMENT LAYOUT (DecFIGS DWARF CgsSceneManagerIO_SceneUpdate.h:96 -- struct
// InEventSetEntityRadius : public Event):
//     +0x00  mEntityId   EntityId  (packed 32-bit scene handle)   (:98)
//     +0x04  mfRadius    float32_t (DWARF) -> f32                  (:99)
// Two adjacent 4-byte fields, 4-aligned: sizeof == 8.
//
// SIZE / ALIGNMENT (X360-attested): Construct @ 0x822E22C0 (InEventSetEntityRadius,512)
// does `addi r30, this, 0xC` -- 12 is already a multiple of the element's 4-byte
// alignment, so the 12-byte BaseEventQueue header needs no padding and maEvents lives
// at +0x0C. (Hex-Rays renders the addi+cmplwi guard as the vacuous `result == -12`.)
// Construct does not read the element interior, so only the size/alignment is
// load-bearing for the queue-instantiation TU; the named fields are reconstructed for
// a complete element type.
#include "types.hpp"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h" // CgsSceneManager::EntityId

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores
    // events by byte image). Distinctly named so this leaf element home never ODR-clashes
    // with the SceneManagerIO::Event defined by the other per-element queue homes.
    struct EventBaseSetEntityRadius {};

    // mSetEntityRadiusQueue element. 4-byte aligned (two adjacent 4-byte fields), sizeof 8.
    struct InEventSetEntityRadius : public EventBaseSetEntityRadius
    {
        EntityId mEntityId; // +0x00 (:98)
        f32      mfRadius;   // +0x04 (:99)
    };
}
}
