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
//
// CORRECTED 2026-09-25 (crash parity FX-FOLLOWUPS, REVIEW-K on b5 35a5e66c): field-level DWARF DOES cover this
// element. DecFIGS CgsSceneManagerIO_FineQuery.h:101 declares `InEventVolumeTestDeepest : public Event` with seven
// members (:103..:109); the member NAMES below come from that DWARF. They are ADDED beside the opaque payload in an
// anonymous union -- the payload line is kept exactly -- each at the offset the producer
// SceneQueryInterface::VolumeTestDeepest @0x822170B0 stores it (its event is r1+0x50; the store address is beside
// each member) and the consumer SceneManagerModule::ProcessVolumeTestDeepest @0x828D4460 loads it. The
// static_asserts after the struct pin every offset, the size and the alignment.
// Three members hold their DWARF type's IMAGE rather than the type: a union member may not have a user-provided
// constructor (Matrix44Affine's / Vector4's, EntityId's) without deleting this element's default and copy
// constructors, which the producer and the queues need. mTransform (DWARF Matrix44Affine) is its four 16-byte rows
// as f32[4][4]; mExcludeEntityId (DWARF EntityId) is the id's u32 word; meExclusionMode (DWARF EExclusionMode, homed
// in CgsSceneManagerIO_EventLineTest.h) is the enum's u32 word. mQueryId (SceneQueryId), mVolumeBuffer (VolumeSlot),
// mx32EntityTypeFlags (EntityTypeFlags == uint32_t) and mxVolumeTypeFlags (VolumeTypeFlags == uint8_t) keep theirs.

#include "types.hpp"
#include <cstddef>                                                  // offsetof (the pins)
#include "GameShared/GameClasses/SceneManager/CgsSceneQueryId.h"   // SceneQueryId (DWARF :104)
#include "GameShared/GameClasses/SceneManager/CgsVolumeStore.h"    // VolumeSlot (DWARF :108, the 128-byte volume image)

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
    // (2026-09-25: the DWARF layout IS recovered now -- the named members beside the payload; see the banner.)
    struct alignas(16) InEventVolumeTestDeepest : public EventBaseVolumeTestDeepest
    {
        union   // [2026-09-25, REVIEW-K] the DWARF members (CgsSceneManagerIO_FineQuery.h:103..:109) beside the payload
        {
        u8 macOpaquePayload[224]; // +0x00  opaque (X360-attested 224-byte stride; live bytes 0x00..0xD0)
        struct
        {
            f32          mTransform[4][4];     // :103 +0x00  DWARF Matrix44Affine, its four rows -- stvx128 @0x82217150 / 0x82217170 / 0x82217180 / 0x8221718C (a6)
            SceneQueryId mQueryId;             // :104 +0x40  stw r27 (a2) @0x82217154
            u32          mx32EntityTypeFlags;  // :105 +0x44  stw r26 (a3) @0x82217160
            u32          mExcludeEntityId;     // :106 +0x48  DWARF EntityId, its u32 word -- stw r23 (a7) @0x82217188
            u32          meExclusionMode;      // :107 +0x4C  DWARF EExclusionMode, its u32 word -- stw r30 (a8) @0x82217190
            VolumeSlot   mVolumeBuffer;        // :108 +0x50  memcpy(event+0x50, a5, 0x80) @0x82217194
            u8           mxVolumeTypeFlags;    // :109 +0xD0  stb r25 (a4) @0x82217178
        };
        };
    };

    // The pins (REVIEW-K): every named member sits at the producer's store offset, beside the unchanged payload,
    // and the element keeps its X360-attested stride and alignment.
    static_assert(offsetof(InEventVolumeTestDeepest, macOpaquePayload)    == 0x00, "the payload spans the element");
    static_assert(offsetof(InEventVolumeTestDeepest, mTransform)          == 0x00, "stvx128 @0x82217150 (+0x00)");
    static_assert(offsetof(InEventVolumeTestDeepest, mQueryId)            == 0x40, "stw @0x82217154 (+0x40)");
    static_assert(offsetof(InEventVolumeTestDeepest, mx32EntityTypeFlags) == 0x44, "stw @0x82217160 (+0x44)");
    static_assert(offsetof(InEventVolumeTestDeepest, mExcludeEntityId)    == 0x48, "stw @0x82217188 (+0x48)");
    static_assert(offsetof(InEventVolumeTestDeepest, meExclusionMode)     == 0x4C, "stw @0x82217190 (+0x4C)");
    static_assert(offsetof(InEventVolumeTestDeepest, mVolumeBuffer)       == 0x50, "memcpy @0x82217194 (+0x50)");
    static_assert(offsetof(InEventVolumeTestDeepest, mxVolumeTypeFlags)   == 0xD0, "stb @0x82217178 (+0xD0)");
    static_assert(sizeof(InEventVolumeTestDeepest().mTransform)    == 0x40, "four 16-byte rows (Matrix44Affine)");
    static_assert(sizeof(InEventVolumeTestDeepest().mVolumeBuffer) == 0x80, "the 0x80-byte memcpy @0x82217194");
    static_assert(sizeof(InEventVolumeTestDeepest)  == 0xE0, "AddEvent @0x82210870 `mulli r10, r11, 0xE0`");
    static_assert(alignof(InEventVolumeTestDeepest) == 16,   "Construct @0x8222DBD8 `addi r30, this, 0x10`");
}
}
