#pragma once

// Minimal owning home for the queue element CgsSceneManager::SceneManagerIO::
// InEventSetPadding -- the per-event payload stored in the
// EventQueue<InEventSetPadding, 1280> set-padding input queue
// (InSceneUpdateInterface::mSetPaddingQueue, typedef InSetPaddingQueue;
// DWARF home CgsSceneManagerIO_SceneUpdate.h:343/346). This header exists so the
// explicit-instantiation TUs for that queue's Construct
// (EventQueue_InEventSetPadding_1280.cpp) and the base queue's Append
// (BaseEventQueue_InEventSetPadding_Append.cpp) can see a COMPLETE element type (the
// queue embeds InEventSetPadding maEvents[1280] inline, and Append block-copies
// sizeof(T)-strided records). The full InSceneUpdateInterface aggregate keeps its own
// placeholder slice and its own ledger TU -- this header only adds the leaf element it
// queues, by name.
//
// ELEMENT LAYOUT (DecFIGS DWARF CgsSceneManagerIO_SceneUpdate.h:240 -- struct
// InEventSetPadding : public Event):
//     +0x00  mVolInstanceId  VolumeInstanceId  (u64 packed handle, 8-aligned)  (:242)
//     +0x10  mPadding        Vector3           (16B SIMD lane, 16-aligned)     (:243)
//     +0x08..0x0F  pad to the 16-byte alignment forced by the trailing Vector3 lane
// -> sizeof == 0x20 (32), alignas(16).
//
// SIZE / ALIGNMENT (X360-attested), two independent confirmations:
//   * Construct @ 0x822E2330 (InEventSetPadding,1280) does `addi r30, this, 0x10` --
//     the 12-byte BaseEventQueue header rounds up to the element's 16-byte alignment
//     (12 -> 16), so maEvents lives at +0x10. (Hex-Rays renders the addi+cmplwi guard
//     as the vacuous `result == -16`.)
//   * Append @ 0x827A6640 strides the copy by `slwi r, count, 5` == 32*count bytes,
//     i.e. sizeof(InEventSetPadding) == 32.
// Construct does not read the element interior and Append block-copies it whole, so
// only the size/alignment is load-bearing for these TUs; the named fields are
// reconstructed for a complete element type.
#include "types.hpp"
#include "BrnCommonTypes.h"                                          // Vector3 (16-byte SIMD lane)
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h" // CgsSceneManager::VolumeInstanceId

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores
    // events by byte image). Distinctly named so this leaf element home never ODR-clashes
    // with the SceneManagerIO::Event defined by the other per-element queue homes.
    struct EventBaseSetPadding {};

    // mSetPaddingQueue element. 16-byte aligned (carries the trailing Vector3 lane),
    // sizeof 0x20 (32).
    struct alignas(16) InEventSetPadding : public EventBaseSetPadding
    {
        VolumeInstanceId mVolInstanceId; // +0x00 (:242)
        // +0x08..0x0F pad to the 16-byte alignment forced by the Vector3 lane.
        Vector3          mPadding;       // +0x10 (16B SIMD lane) (:243)
    };
}
}
