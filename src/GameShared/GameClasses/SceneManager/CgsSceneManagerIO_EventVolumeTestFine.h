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
// LAYOUT: the queues block-copy the element whole, so the 224-byte stride and 16-byte
// alignment are what the queue code needs. The member names come from the declaration of
// InEventVolumeTestFine (CgsSceneManagerIO_FineQuery.h, seven members, the
// same shape as InEventVolumeTestDeepest), and the offsets are the ones its consumer
// SceneManagerModule::ProcessFineVolumeTest loads: the transform rows at +0x00..+0x30, the
// query id +0x40, the entity-type flags +0x44, the exclude entity +0x48, the exclusion mode
// +0x4C, the 128-byte volume image +0x50 and the volume-type flags byte +0xD0. They sit
// beside the opaque payload in an anonymous union, exactly as in the deepest sibling
// (CgsSceneManagerIO_EventVolumeTestDeepest.h), and for the same reason three members hold
// their declared type's image: a union member may not have a user-provided constructor.

#include "types.hpp"
#include <cstddef>                                                  // offsetof (the pins)
#include "GameShared/GameClasses/SceneManager/CgsSceneQueryId.h"   // SceneQueryId
#include "GameShared/GameClasses/SceneManager/CgsVolumeStore.h"    // VolumeSlot (the 128-byte volume image)

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores
    // events by byte image). Distinctly named so this leaf element home never ODR-clashes
    // with the bases defined by the other per-element queue homes (EventBaseLineTest, ...).
    struct EventBaseVolumeTestFine {};

    // EventQueue<InEventVolumeTestFine, N> element. 16-byte aligned, stride 224 (0xE0).
    struct alignas(16) InEventVolumeTestFine : public EventBaseVolumeTestFine
    {
        union
        {
        u8 macOpaquePayload[224]; // +0x00  the whole 224-byte element (live bytes 0x00..0xD0)
        struct
        {
            f32          mTransform[4][4];     //  +0x00  Matrix44Affine, its four rows
            SceneQueryId mQueryId;             //  +0x40
            u32          mx32EntityTypeFlags;  //  +0x44
            u32          mExcludeEntityId;     //  +0x48  EntityId, its u32 word
            u32          meExclusionMode;      //  +0x4C  EExclusionMode, its u32 word
            VolumeSlot   mVolumeBuffer;        //  +0x50
            u8           mxVolumeTypeFlags;    //  +0xD0
        };
        };
    };

    static_assert(offsetof(InEventVolumeTestFine, macOpaquePayload)    == 0x00, "the payload spans the element");
    static_assert(offsetof(InEventVolumeTestFine, mTransform)          == 0x00, "transform rows +0x00");
    static_assert(offsetof(InEventVolumeTestFine, mQueryId)            == 0x40, "query id +0x40");
    static_assert(offsetof(InEventVolumeTestFine, mx32EntityTypeFlags) == 0x44, "entity-type flags +0x44");
    static_assert(offsetof(InEventVolumeTestFine, mExcludeEntityId)    == 0x48, "exclude entity +0x48");
    static_assert(offsetof(InEventVolumeTestFine, meExclusionMode)     == 0x4C, "exclusion mode +0x4C");
    static_assert(offsetof(InEventVolumeTestFine, mVolumeBuffer)       == 0x50, "volume image +0x50");
    static_assert(offsetof(InEventVolumeTestFine, mxVolumeTypeFlags)   == 0xD0, "volume-type flags +0xD0");
    static_assert(sizeof(InEventVolumeTestFine().mVolumeBuffer) == 0x80, "the 0x80-byte volume image");
    static_assert(sizeof(InEventVolumeTestFine)  == 0xE0, "Append strides 0xE0");
    static_assert(alignof(InEventVolumeTestFine) == 16,   "maEvents at +0x10");
}
}
