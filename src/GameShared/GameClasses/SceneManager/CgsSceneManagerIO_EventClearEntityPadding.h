#pragma once

// Minimal owning home for the queue element CgsSceneManager::SceneManagerIO::
// InEventClearEntityPadding -- the per-event payload stored in the
// EventQueue<InEventClearEntityPadding, 16> clear-entity-padding input queue
// (InSceneUpdateInterface::mClearEntityPaddingQueue, DWARF home
// CgsSceneManagerIO_SceneUpdate.h:360). This header exists so the explicit-
// instantiation TU for that queue's Construct (EventQueue_InEventClearEntityPadding_16.cpp)
// can see a COMPLETE element type (the queue embeds InEventClearEntityPadding maEvents[16]
// inline). The full InSceneUpdateInterface aggregate keeps its own placeholder slice and
// its own ledger TU -- this header only adds the leaf element it queues, by name.
//
// LAYOUT (DecFIGS DWARF CgsSceneManagerIO_SceneUpdate.h:247) -- derives from the empty
// SceneManagerIO::Event base:
//     EntityId  mEntity  (:249)  u32 packed handle, 4-aligned
// A single 4-byte EntityId, 4-aligned: 12 is already a multiple of 4, so the 12-byte
// BaseEventQueue header needs no padding and maEvents lives at +0x0C (the Construct asm at
// 0x822E2410 does `addi r30, this, 0xC`). Construct does not read the element interior, so
// only the size/alignment is load-bearing for this TU.
#include "types.hpp"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h" // CgsSceneManager::EntityId

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores
    // events by byte image). Distinctly named so this leaf element home never ODR-clashes
    // with the SceneManagerIO::Event defined by the other per-element queue homes.
    struct EventBaseClearEntityPadding {};

    // mClearEntityPaddingQueue element. 4-byte aligned (single u32 EntityId).
    struct InEventClearEntityPadding : public EventBaseClearEntityPadding
    {
        EntityId mEntity; // +0x00 (:249)
    };
}
}
