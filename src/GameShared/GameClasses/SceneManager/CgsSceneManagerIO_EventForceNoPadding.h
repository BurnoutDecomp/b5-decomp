#pragma once

// Minimal owning home for the queue element CgsSceneManager::SceneManagerIO::
// InEventForceNoPadding -- the per-event payload stored in the
// EventQueue<InEventForceNoPadding, 64> force-no-padding input queue
// (InSceneUpdateInterface::mForceNoPaddingQueue, DWARF home
// CgsSceneManagerIO_SceneUpdate.h:283). This header exists so the explicit-instantiation
// TUs for that queue's Construct (EventQueue_InEventForceNoPadding_64.cpp) and the base
// queue's Append (BaseEventQueue_InEventForceNoPadding_Append.cpp) can see a COMPLETE
// element type (the queue embeds InEventForceNoPadding maEvents[64] inline, and Append
// block-copies sizeof(T)-strided records). The full InSceneUpdateInterface aggregate keeps
// its own placeholder slice and its own ledger TU -- this header only adds the leaf element
// it queues, by name.
//
// LAYOUT (DecFIGS DWARF CgsSceneManagerIO_SceneUpdate.h:253) -- derives from the empty
// SceneManagerIO::Event base:
//     VolumeInstanceId  mVolInstanceId  (:255)  u64 packed handle, 8 bytes, 8-aligned
// A single 8-byte VolumeInstanceId, 8-aligned, sizeof 8. Two independent asm confirmations:
//   * Construct @ 0x822E1F40 does `addi r30, this, 0x10` -- the 12-byte BaseEventQueue
//     header rounds up to the element's 8-byte alignment (12 -> 16), so maEvents lives
//     at +0x10.
//   * Append @ 0x827A5E48 strides the copy by `slwi r, count, 3` == 8*count bytes, i.e.
//     sizeof(InEventForceNoPadding) == 8.
// Construct does not read the element interior, and Append block-copies it whole, so only
// the size/alignment is load-bearing for these TUs.
#include "types.hpp"
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h" // CgsSceneManager::VolumeInstanceId

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores
    // events by byte image). Distinctly named so this leaf element home never ODR-clashes
    // with the SceneManagerIO::Event defined by the other per-element queue homes.
    struct EventBaseForceNoPadding {};

    // mForceNoPaddingQueue element. 8-byte aligned (single u64 VolumeInstanceId).
    struct InEventForceNoPadding : public EventBaseForceNoPadding
    {
        VolumeInstanceId mVolInstanceId; // +0x00 (:255)
    };
}
}
