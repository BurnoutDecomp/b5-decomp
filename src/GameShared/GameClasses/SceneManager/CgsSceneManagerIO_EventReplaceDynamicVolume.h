#pragma once

// Minimal owning home for the queue element CgsSceneManager::SceneManagerIO::
// InEventReplaceDynamicVolume -- the per-event payload stored in the
// EventQueue<InEventReplaceDynamicVolume, 64> replace-dynamic-volume input queue
// (InSceneUpdateInterface::mReplaceDynamicVolumeQueue). This header exists so the
// explicit-instantiation TU for that queue's Construct
// (EventQueue_InEventReplaceDynamicVolume_64.cpp @ 0x822E2170) can see a COMPLETE
// element type (the queue embeds InEventReplaceDynamicVolume maEvents[64] inline). The
// full InSceneUpdateInterface aggregate keeps its own placeholder slice and its own
// ledger TU; this header only adds the leaf element it queues, by name.
//
// ELEMENT LAYOUT (X360-authoritative, read off the sibling BaseEventQueue<T>::AddEvent
// @ 0x822AB670 which appends this element):
//     lwz  r11, 8(this)            ; miLength
//     slwi r7, r11, 3 ; add r11,r11,r7 ; slwi r11,r11,4   ; index = miLength * (1+8) * 16
//                                                          ;       = miLength * 144  (stride 0x90)
//     mtctr 0x12 (=18); loop { ld r9,0(src); std r9,0(dst); src+=8; dst+=8 }
//                                  ; copies 18 * 8 = 144 bytes
// so the element is 144 bytes (0x90), 8-byte aligned (the std-stride block copy). This
// matches InEventAddDynamicVolume's 144-byte image: it carries the same opaque
// collision-volume blob plus the leading id/flags header. The interior of the
// 128-byte volume blob is an opaque rw::collision::Volume image (no DecFIGS DWARF covers
// this element); it is sized to the asm-attested 144-byte stride and flagged opaque.
// The 8-byte alignment is exactly what makes the 12-byte BaseEventQueue header pad to
// +0x10 before maEvents (the Construct asm at 0x822E2170 does `addi r30, this, 0x10`).
// Construct does not read the element interior, so only the size/alignment is
// load-bearing for that TU.

#include "types.hpp"

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores
    // events by byte image). Distinctly named so this leaf element home never ODR-clashes
    // with the SceneManagerIO::Event defined by the other per-element queue homes.
    struct EventBaseReplaceDynamicVolume {};

    // mReplaceDynamicVolumeQueue element. 8-byte aligned; 144-byte image carrying the
    // id/flags header and an opaque 128-byte collision-volume blob (same shape as
    // InEventAddDynamicVolume, attested by the 144-byte AddEvent stride).
    struct InEventReplaceDynamicVolume : public EventBaseReplaceDynamicVolume
    {
        u64 muId;              // +0x00  volume-instance / id handle
        u8  mu8Flags;          // +0x08  flags
        u8  maPad09[7];        // +0x09  pad up to the +0x10 volume blob
        u8  maVolumeData[128]; // +0x10  opaque rw::collision::Volume image (block-copied)
    };
}
}
