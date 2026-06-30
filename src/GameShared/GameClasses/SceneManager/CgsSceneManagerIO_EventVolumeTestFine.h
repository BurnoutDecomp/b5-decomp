#pragma once

// Minimal owning home for the SceneManager fine volume-test query element
//   CgsSceneManager::SceneManagerIO::InEventVolumeTestFine
// -- the per-event payload stored in the EventQueue<InEventVolumeTestFine, N> fine
// volume-test input queues that the SceneManager/Director IO buffers embed by value.
// This header exists so the explicit-instantiation TUs for those queues' Construct
// (EventQueue_InEventVolumeTestFine_{1,64}.cpp) and the base queue's Append
// (BaseEventQueue_InEventVolumeTestFine_Append.cpp) can see a COMPLETE element type
// (each queue embeds the element maEvents[N] inline, and Append block-copies sizeof(T)-
// strided records). The full IO-buffer aggregates keep their own placeholder slices and
// their own ledger TUs; this header only adds the leaf element they queue, by name.
//
// SIZE / ALIGNMENT (X360-attested):
//   * Append @ 0x823C2410 strides its block-copy by 224: `mulli r5,r29,0xE0`
//     (count == lSource.miLength*224) and `mulli r11,r11,0xE0` (dest byte offset ==
//     this->miLength*224), `add r3,r11,r10` == mpEvents + miLength*224. So
//     sizeof(InEventVolumeTestFine) == 0xE0 (224).
//   * Construct @ 0x8222DC38 (,1) / 0x8222DCF8 (,64) does `addi r30, this, 0x10` -- the
//     12-byte BaseEventQueue header rounds up to the element's 16-byte alignment
//     (12 -> 16), so maEvents lives at +0x10 (the element is alignas(16)).
//
// LAYOUT: no field-level DWARF is recovered in any decompiled TU's scope (InEventVolumeTestFine
// is only forward-declared in CgsFineIntersectionTestModule.h:54 -- in the PLAIN
// CgsSceneManager namespace, a DISTINCT forward decl consumed by pointer only -- and never
// defined; Construct does not read the element interior; Append block-copies it whole).
// Only the 224-byte size / 16-byte alignment is load-bearing, so the payload is modelled as
// an OPAQUE byte span at the X360-attested stride -- field names are NOT fabricated
// (HARD RULE 3). Mirrors the committed sibling CgsSceneManagerIO_EventLineTest.h
// (distinctly-named empty base + leaf element) and the EventAddDynamicVolume opaque-blob home.

#include "types.hpp"

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores
    // events by byte image). Distinctly named so this leaf element home never ODR-clashes
    // with the bases defined by the other per-element queue homes (EventBaseLineTest, ...).
    struct EventBaseVolumeTestFine {};

    // EventQueue<InEventVolumeTestFine, N> element. 16-byte aligned, X360-attested stride
    // 224 (0xE0) -- opaque payload, no field layout recovered in scope.
    struct alignas(16) InEventVolumeTestFine : public EventBaseVolumeTestFine
    {
        u8 macOpaquePayload[224]; // +0x00  opaque (X360-attested 224-byte stride)
    };
}
}
