#pragma once

// Minimal owning home for the SceneManager deepest volume-test query element
//   CgsSceneManager::SceneManagerIO::InEventVolumeTestDeepest
// -- the per-event payload stored in the EventQueue<InEventVolumeTestDeepest, N> deepest
// volume-test input queues that the SceneManager/Director IO buffers embed by value.
// This header exists so the explicit-instantiation TUs for those queues' Construct
// (EventQueue_InEventVolumeTestDeepest_10.cpp), the base queue's AddEvent
// (BaseEventQueue_InEventVolumeTestDeepest_AddEvent.cpp) / Append
// (BaseEventQueue_InEventVolumeTestDeepest_Append.cpp), and the producer
// SceneQueryInterface::VolumeTestDeepest can see a COMPLETE element type (each queue
// embeds the element maEvents[N] inline, and AddEvent/Append block-copy sizeof(T)-strided
// records). The full IO-buffer aggregates keep their own placeholder slices and their own
// ledger TUs; this header only adds the leaf element they queue, by name.
//
// NOTE (distinct type): this CgsSceneManager::SceneManagerIO::InEventVolumeTestDeepest is a
// DIFFERENT type from the forward-declared CgsSceneManager::InEventVolumeTestDeepest used by
// FineIntersectionTestModule (plain CgsSceneManager namespace vs ::SceneManagerIO::). Do NOT
// conflate/reuse them.
//
// SIZE / ALIGNMENT (X360-attested, two independent confirmations):
//   * AddEvent @ 0x82210870 indexes mpEvents[miLength] with `mulli r10,r11,0xE0`
//     (stride 224) and Append @ 0x823C2330 strides its block-copy by `mulli ...,0xE0`
//     (== miLength*224). So sizeof(InEventVolumeTestDeepest) == 0xE0 (224).
//   * Construct @ 0x8222DBD8 (,10) does `addi r30, this, 0x10` -- the 12-byte
//     BaseEventQueue header rounds up to the element's 16-byte alignment (12 -> 16), so
//     maEvents lives at +0x10 (the element carries four 16-byte SIMD lanes -> alignas(16)).
//
// LAYOUT: AddEvent's element copy touches bytes 0x00..0xD0 (64B of four SIMD lanes + 16B of
// four words + a 128B memcpy + one trailing byte = 209 live bytes), padded out to the
// attested 224-byte stride. No field-level DWARF covers this element, so the payload is
// modelled as an OPAQUE byte span at the X360-attested stride -- field names are NOT
// fabricated (HARD RULE 3). The producer SceneQueryInterface::VolumeTestDeepest stages the
// record into this span at the asm-attested byte offsets (+0x00 transform/64B, +0x40 query,
// +0x44 entity-type-flags, +0x48 exclude-entity-id, +0x4C exclusion-mode, +0x50 volume/128B,
// +0xD0 volume-type-flags) without naming interior fields. Mirrors the committed sibling
// CgsSceneManagerIO_EventLineTest.h and the EventAddDynamicVolume opaque-blob home.

#include "types.hpp"

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores
    // events by byte image). Distinctly named so this leaf element home never ODR-clashes
    // with the bases defined by the other per-element queue homes.
    struct EventBaseVolumeTestDeepest {};

    // EventQueue<InEventVolumeTestDeepest, N> element. 16-byte aligned (carries four SIMD
    // lanes), X360-attested stride 224 (0xE0) -- opaque payload, no field layout recovered.
    struct alignas(16) InEventVolumeTestDeepest : public EventBaseVolumeTestDeepest
    {
        u8 macOpaquePayload[224]; // +0x00  opaque (X360-attested 224-byte stride; live bytes 0x00..0xD0)
    };
}
}
