#pragma once

// Minimal owning home for the queue element CgsSceneManager::SceneManagerIO::
// InEventAddDynamicVolume -- the per-event payload stored in the
// EventQueue<InEventAddDynamicVolume, 1280> add-dynamic-volume input queue
// (mAddDynamicVolumeQueue inside InSceneUpdateInterface). This header exists so the
// explicit-instantiation TU for that queue's Construct
// (EventQueue_InEventAddDynamicVolume_1280.cpp) can see a COMPLETE element type (the
// queue embeds InEventAddDynamicVolume maEvents[1280] inline). The full
// InSceneUpdateInterface aggregate keeps its own placeholder slice and its own ledger
// TU; this header only adds the leaf element it queues, by name.
//
// ELEMENT LAYOUT (X360-authoritative, from the producer
// InSceneUpdateInterface::AddDynamicVolume @ 0x822B1518 that stages the event before
// AddEvent). The staged event local is the 144-byte block passed by &address to
// InEventAddDynamicVolume::AddEvent:
//     +0x00  mVolumeId / id pair    (__int64, stored as `v10 = (a2 in low dword)`)
//     +0x08  mu8Flags               (char,    `v11 = a4`)
//     +0x10  maVolumeData[128]      (memcpy(.., a3, 128) -- the rw collision-volume blob)
// Total 0x90 (144 bytes), 8-byte aligned (the leading __int64). The interior of the
// 128-byte volume blob is an opaque rw::collision::Volume image the producer block-
// copies wholesale; it is sized to the asm-attested 128-byte copy and flagged opaque
// (no DecFIGS DWARF covers this element). Construct does not read the element interior,
// so only the size/alignment is load-bearing for this TU.

#include "types.hpp"

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores
    // events by byte image). Matches the SceneManagerIO::Event used by the other
    // SceneManager IO event payloads (see CgsSceneManagerIO_EventAddForCollision.h).
    struct EventBaseAddDynamicVolume {};

    // mAddDynamicVolumeQueue element. 8-byte aligned; carries an opaque 128-byte
    // collision-volume image the producer memcpy's in (asm at 0x822B1518).
    struct InEventAddDynamicVolume : public EventBaseAddDynamicVolume
    {
        u64 muId;                // +0x00  (staged from `v10`)
        u8  mu8Flags;            // +0x08  (staged from `v11 = a4`)
        u8  maPad09[7];          // +0x09  (pad up to the +0x10 volume blob)
        u8  maVolumeData[128];   // +0x10  (opaque rw::collision::Volume image, memcpy'd)
    };
}
}
