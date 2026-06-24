#pragma once

// Minimal owning home for the queue element CgsSceneManager::SceneManagerIO::
// InEventRemoveVolume -- the per-event payload stored in the
// EventQueue<InEventRemoveVolume, 1344> remove-dynamic-volume input queue
// (mRemoveDynamicVolumeQueue inside InSceneUpdateInterface). This header exists so the
// explicit-instantiation TU for that queue's Construct
// (EventQueue_InEventRemoveVolume_1344.cpp) can see a COMPLETE element type (the queue
// embeds InEventRemoveVolume maEvents[1344] inline, and the base queue's Append/AddEvent
// block-copy sizeof(T)-strided records). The full InSceneUpdateInterface aggregate keeps
// its own placeholder slice and its own ledger TU; this header only adds the leaf element
// it queues, by name.
//
// ELEMENT LAYOUT (DecFIGS DWARF CgsSceneManagerIO_SceneUpdate.h:123 -- struct
// InEventRemoveVolume : public Event):
//     +0x00  mVolumeId   VolumeId (packed 64-bit collision-volume handle)   (:125)
// -> 8-byte aligned (the leading u64 VolumeId), sizeof 0x08 (8).
//
// SIZE / ALIGNMENT (X360-attested): Construct @ 0x822E2090 (InEventRemoveVolume,1344) does
// `addi r30, this, 0x10` -- the 12-byte BaseEventQueue header rounds up to the element's
// alignment before maEvents; 12 rounds to 0x10 for any alignment >= 8, consistent with the
// 8-byte-aligned VolumeId element. Construct does not read the element interior, so only the
// size/alignment is load-bearing for the queue-instantiation TU; the named field is
// reconstructed for a complete element type.

#include "types.hpp"
#include "GameShared/GameClasses/SceneManager/CgsVolumeId.h"  // CgsSceneManager::VolumeId

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores
    // events by byte image). Distinctly named so this leaf element home never ODR-clashes
    // with the SceneManagerIO::Event defined by the other per-element queue homes.
    struct EventBaseRemoveVolume {};

    // EventQueue<InEventRemoveVolume, N> element. 8-byte aligned (carries the u64 VolumeId).
    struct InEventRemoveVolume : public EventBaseRemoveVolume
    {
        VolumeId mVolumeId;   // +0x00
    };
}
}
