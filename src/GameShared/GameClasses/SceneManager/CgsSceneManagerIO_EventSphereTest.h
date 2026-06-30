#pragma once

// Minimal owning home for the SceneManager fast sphere-test query element
//   CgsSceneManager::SceneManagerIO::InEventSphereTestFast
// -- the per-event payload stored in the EventQueue<InEventSphereTestFast, N> fast
// sphere-test input queues that the SceneManager/Director IO buffers embed by value.
// This header exists so the explicit-instantiation TUs for those queues' Construct
// (EventQueue_InEventSphereTestFast_{10,16}.cpp) and the base queue's Append
// (BaseEventQueue_InEventSphereTestFast_Append.cpp) can see a COMPLETE element type
// (each queue embeds the element maEvents[N] inline, and Append block-copies sizeof(T)-
// strided records). The full IO-buffer aggregates keep their own placeholder slices and
// their own ledger TUs; this header only adds the leaf element they queue, by name.
//
// SIZE / ALIGNMENT (X360-attested):
//   * Append @ 0x823C2240 strides its block-copy by 48: the source-count arm
//     `slwi r9,r29,1; add r9,r29,r9` (== lSource.miLength*3) then `slwi r5,r9,4`
//     (== lSource.miLength*48), mirrored on the dest-offset arm
//     `slwi r8,r11,1; add r11,r11,r8; slwi r11,r11,4` (== this->miLength*48), and
//     `add r3,r11,r10` == mpEvents + miLength*48. So sizeof(InEventSphereTestFast)==0x30.
//   * Construct @ 0x8222DB58 (,10) / 0x8222DC18 (,16) does `addi r30, this, 0x10` --
//     the 12-byte BaseEventQueue header rounds up to the element's 16-byte alignment
//     (12 -> 16), so maEvents lives at +0x10 (the element is alignas(16)).
//
// LAYOUT: no field-level DWARF is recovered in any decompiled TU's scope (Construct does
// not read the element interior; Append block-copies it whole). Only the 48-byte size /
// 16-byte alignment is load-bearing, so the payload is modelled as an OPAQUE byte span at
// the X360-attested stride -- field names are NOT fabricated (HARD RULE 3). Mirrors the
// committed sibling line-test home CgsSceneManagerIO_EventLineTest.h (distinctly-named
// empty base + leaf element) and the EventAddDynamicVolume opaque-blob home.

#include "types.hpp"

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores
    // events by byte image). Distinctly named so this leaf element home never ODR-clashes
    // with the bases defined by the other per-element queue homes.
    struct EventBaseSphereTestFast {};

    // EventQueue<InEventSphereTestFast, N> element. 16-byte aligned, X360-attested stride
    // 48 (0x30) -- opaque payload, no field layout recovered in scope.
    struct alignas(16) InEventSphereTestFast : public EventBaseSphereTestFast
    {
        u8 macOpaquePayload[48]; // +0x00  opaque (X360-attested 48-byte stride)
    };
}
}
