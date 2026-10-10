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
// LAYOUT: the member names and types are the declaration's (CgsSceneManagerIO_FineQuery.h,
// six members); the offsets are the ones its consumer
// SceneManagerModule::ProcessSphereTestFast loads: the sphere lane (centre xyz, radius in w)
// +0x00, the query id +0x10, the entity-type flags +0x14, the exclude entity +0x18, the
// exclusion mode +0x1C and the volume-type flags byte +0x20. Same shape as the line-test
// elements in CgsSceneManagerIO_EventLineTest.h, whose EExclusionMode it shares.

#include "types.hpp"
#include <cstddef>                                                    // offsetof (the pins)
#include "BrnCommonTypes.h"                                           // Vector3Plus (16-byte SIMD lane)
#include "GameShared/GameClasses/SceneManager/CgsSceneQueryId.h"      // SceneQueryId
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"          // EntityId
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventLineTest.h"   // EExclusionMode

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores
    // events by byte image). Distinctly named so this leaf element home never ODR-clashes
    // with the bases defined by the other per-element queue homes.
    struct EventBaseSphereTestFast {};

    // EventQueue<InEventSphereTestFast, N> element. 16-byte aligned, stride 48 (0x30).
    struct alignas(16) InEventSphereTestFast : public EventBaseSphereTestFast
    {
        Vector3Plus    mSpherePosPlusRadius; //  +0x00  centre xyz, radius in w
        SceneQueryId   mQueryId;             //  +0x10
        u32            mx32EntityTypeFlags;  //  +0x14
        EntityId       mExcludeEntityId;     //  +0x18
        EExclusionMode meExclusionMode;      //  +0x1C
        u8             mxVolumeTypeFlags;    //  +0x20
    };

    static_assert(offsetof(InEventSphereTestFast, mQueryId)            == 0x10, "query id +0x10");
    static_assert(offsetof(InEventSphereTestFast, mx32EntityTypeFlags) == 0x14, "entity-type flags +0x14");
    static_assert(offsetof(InEventSphereTestFast, mExcludeEntityId)    == 0x18, "exclude entity +0x18");
    static_assert(offsetof(InEventSphereTestFast, meExclusionMode)     == 0x1C, "exclusion mode +0x1C");
    static_assert(offsetof(InEventSphereTestFast, mxVolumeTypeFlags)   == 0x20, "volume-type flags +0x20");
    static_assert(sizeof(InEventSphereTestFast)  == 0x30, "Append strides 0x30");
    static_assert(alignof(InEventSphereTestFast) == 16,   "maEvents at +0x10");
}
}
