#pragma once

// Minimal owning home for two SceneManager line-test query elements:
//   CgsSceneManager::SceneManagerIO::InEventLineTestFine            (DWARF CgsSceneManagerIO_FineQuery.h:50)
//   CgsSceneManager::SceneManagerIO::InEventLineTestFastDoubleSided (DWARF CgsSceneManagerIO_FineQuery.h:76)
// -- the per-event payloads stored in the EventQueue<InEventLineTestFine, N> and
// EventQueue<InEventLineTestFastDoubleSided, N> fine-line-test input queues that the
// SceneManager/Director IO buffers embed by value. This header exists so the explicit-
// instantiation TUs for those queues' Construct (EventQueue_InEventLineTestFine_10.cpp,
// EventQueue_InEventLineTestFine_256.cpp, EventQueue_InEventLineTestFastDoubleSided_10.cpp,
// EventQueue_InEventLineTestFastDoubleSided_16.cpp) and the base queue's Append
// (BaseEventQueue_InEventLineTestFastDoubleSided_Append.cpp) can see a COMPLETE element type
// (each queue embeds the element maEvents[N] inline, and Append block-copies sizeof(T)-strided
// records). The full IO-buffer aggregates keep their own placeholder slices and their own
// ledger TUs; this header only adds the leaf elements they queue, by name.
//
// LAYOUT (DecFIGS DWARF CgsSceneManagerIO_FineQuery.h) -- both elements share an identical
// field set (the DWARF lists them member-for-member identically) and derive from the empty
// SceneManagerIO::Event base:
//     +0x00  mLineStart           Vector3  (16B SIMD lane)      (:52 / :78)
//     +0x10  mLineEnd             Vector3  (16B SIMD lane)      (:53 / :79)
//     +0x20  mQueryId             SceneQueryId  (u32)           (:55 / :81)
//     +0x24  mx32EntityTypeFlags  u32                           (:56 / :82)
//     +0x28  mExcludeEntityId     EntityId  (u32)               (:57 / :83)
//     +0x2C  meExclusionMode      EExclusionMode  (enum, 4B)    (:58 / :84)
//     +0x30  mxVolumeTypeFlags    u8                            (:59 / :85)
//     +0x31..0x3F  padding (16-byte alignment from the leading Vector3 lanes)
// 49 payload bytes, alignas(16) (carries two Vector3 lanes) -> sizeof == 0x40 (64).
//
// Two independent X360 confirmations of the 64-byte size/16-byte alignment:
//   * Construct @ 0x822E2790 (InEventLineTestFine,256) / 0x8222DA08 (,10) /
//     0x8222DAE8 (FastDoubleSided,10) / 0x828C46F8 (FastDoubleSided,16) all do
//     `addi r30, this, 0x10` -- the 12-byte BaseEventQueue header rounds up to the
//     element's 16-byte alignment (12 -> 16), so maEvents lives at +0x10.
//   * Append @ 0x823C2160 (FastDoubleSided) strides the block-copy by `slwi r, count, 6`
//     == 64*count bytes, i.e. sizeof(InEventLineTestFastDoubleSided) == 64.
// Construct does not read the element interior, and Append block-copies it whole, so only
// the size/alignment is load-bearing for these TUs. The matching EventQueue<...,256> size
// 16400 == 16 + 256*64 is already documented in CgsSceneManagerModuleIO.h.

#include "types.hpp"
#include "BrnCommonTypes.h"                                          // Vector3 (16-byte SIMD lane)
#include "GameShared/GameClasses/SceneManager/CgsSceneQueryId.h"     // CgsSceneManager::SceneQueryId
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"         // CgsSceneManager::EntityId

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores
    // events by byte image). Distinctly named so this leaf element home never ODR-clashes
    // with the SceneManagerIO::Event defined by the other per-element queue homes.
    struct EventBaseLineTest {};

    // Which entities a line test excludes from its hits (DWARF CgsSceneManagerIO_FineQuery.h:43).
    enum EExclusionMode
    {
        E_EXCLUDE_ENTITY_ONLY     = 0,
        E_EXCLUDE_ALL_CHILD_PARTS = 1,
    };

    // EventQueue<InEventLineTestFine, N> element. 16-byte aligned (carries two Vector3 lanes).
    struct alignas(16) InEventLineTestFine : public EventBaseLineTest
    {
        Vector3        mLineStart;          // +0x00
        Vector3        mLineEnd;            // +0x10
        SceneQueryId   mQueryId;            // +0x20
        u32            mx32EntityTypeFlags; // +0x24
        EntityId       mExcludeEntityId;    // +0x28
        EExclusionMode meExclusionMode;     // +0x2C
        u8             mxVolumeTypeFlags;   // +0x30
    };

    // EventQueue<InEventLineTestFastDoubleSided, N> element. Identical field set/layout to
    // InEventLineTestFine per the DWARF; 16-byte aligned, sizeof 0x40.
    struct alignas(16) InEventLineTestFastDoubleSided : public EventBaseLineTest
    {
        Vector3        mLineStart;          // +0x00
        Vector3        mLineEnd;            // +0x10
        SceneQueryId   mQueryId;            // +0x20
        u32            mx32EntityTypeFlags; // +0x24
        EntityId       mExcludeEntityId;    // +0x28
        EExclusionMode meExclusionMode;     // +0x2C
        u8             mxVolumeTypeFlags;   // +0x30
    };
}
}
