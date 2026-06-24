#pragma once

// Minimal owning home for the queue element CgsSceneManager::SceneManagerIO::
// InEventClearCullingTable -- the per-event payload stored in the
// EventQueue<InEventClearCullingTable, 64> clear-culling-table input queue
// (InSceneUpdateInterface::mClearCullingTableQueue, DWARF home
// CgsSceneManagerIO_SceneUpdate.h:332). This header exists so the explicit-
// instantiation TU for that queue's Construct (EventQueue_InEventClearCullingTable_64.cpp)
// can see a COMPLETE element type (the queue embeds InEventClearCullingTable maEvents[64]
// inline). The full InSceneUpdateInterface aggregate keeps its own placeholder slice and
// its own ledger TU -- this header only adds the leaf element it queues, by name.
//
// LAYOUT (DecFIGS DWARF CgsSceneManagerIO_SceneUpdate.h:234) -- derives from the empty
// SceneManagerIO::Event base:
//     bool  mbCullAll  (:236)  single byte
// A single byte, 1-aligned: the element places no alignment requirement on the queue, so
// the 12-byte BaseEventQueue header needs no padding and maEvents lives at +0x0C (the
// Construct asm at 0x822E2250 does `addi r30, this, 0xC`). Construct does not read the
// element interior, so only the size/alignment is load-bearing for this TU.
#include "types.hpp"

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores
    // events by byte image). Distinctly named so this leaf element home never ODR-clashes
    // with the SceneManagerIO::Event defined by the other per-element queue homes.
    struct EventBaseClearCullingTable {};

    // mClearCullingTableQueue element. 1-byte aligned (single bool flag).
    struct InEventClearCullingTable : public EventBaseClearCullingTable
    {
        bool mbCullAll; // +0x00 (:236)
    };
}
}
