#pragma once

// Minimal owning home for the queue element CgsSceneManager::SceneManagerIO::
// InEventRemoveVolumeInstance -- the per-event payload stored in the
// EventQueue<InEventRemoveVolumeInstance, 1280> remove-volume-instance input queue
// (InSceneUpdateInterface::mRemoveVolumeInstanceQueue). This header exists so the
// explicit-instantiation TUs for that queue (Construct @ 0x822E2100 and the
// BaseEventQueue<T>::AddEvent @ 0x822AB538) can see a COMPLETE element type (the queue
// embeds InEventRemoveVolumeInstance maEvents[1280] inline). The full
// InSceneUpdateInterface aggregate keeps its own placeholder slice and its own ledger
// TU; this header only adds the leaf element it queues, by name.
//
// ELEMENT LAYOUT (X360-authoritative, read directly off the AddEvent body
// @ 0x822AB538): the append copies the element with a single 8-byte move --
//     lwz  r11, 8(this)             ; miLength
//     slwi r11, r11, 3              ; * 8  (== sizeof(InEventRemoveVolumeInstance))
//     ld   r10, 0(src)             ; load the 8-byte element image
//     stdx r10, miLength*8, mpEvents; mpEvents[miLength] = *src
// so the element is exactly 8 bytes, 8-byte aligned, a single 64-bit field. That field
// is the volume-instance handle being removed (CgsSceneManager::VolumeInstanceId, the
// packed 64-bit handle). The 8-byte alignment is exactly what makes the 12-byte
// BaseEventQueue header pad to +0x10 before maEvents (the Construct asm at 0x822E2100
// does `addi r30, this, 0x10`). Construct does not read the element interior, so only
// the size/alignment is load-bearing for that TU.

#include "types.hpp"
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h" // CgsSceneManager::VolumeInstanceId

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores
    // events by byte image). Distinctly named so this leaf element home never ODR-clashes
    // with the SceneManagerIO::Event defined by the other per-element queue homes.
    struct EventBaseRemoveVolumeInstance {};

    // mRemoveVolumeInstanceQueue element. 8-byte aligned; a single 64-bit
    // volume-instance handle (the AddEvent body copies it as one `ld`/`stdx`).
    struct InEventRemoveVolumeInstance : public EventBaseRemoveVolumeInstance
    {
        VolumeInstanceId mVolumeInstanceId; // +0x00 (the removed volume-instance handle)
    };
}
}
