#pragma once

// Minimal owning home for the SceneManager "nearest" line-test query element
//   CgsSceneManager::SceneManagerIO::InEventLineTestNearest
// -- the per-event payload stored in the EventQueue<InEventLineTestNearest, N>
// nearest-line-test input queues that the SceneManager IO buffers embed by value.
// This header exists so the explicit-instantiation TUs for those queues' Construct
// (EventQueue_InEventLineTestNearest_40.cpp, and the sibling ,256 instantiation)
// can see a COMPLETE element type (each queue embeds InEventLineTestNearest
// maEvents[N] inline, and the base queue's Append/AddEvent block-copy sizeof(T)-strided
// records). The full IO-buffer aggregates keep their own placeholder slices and their
// own ledger TUs; this header only adds the leaf element they queue, by name.
//
// SIZE / ALIGNMENT (X360-attested, two independent confirmations):
//   * Construct @ 0x8222DA78 (InEventLineTestNearest,40) and 0x828C4688
//     (InEventLineTestNearest,256) both do `addi r30, this, 0x10` -- the 12-byte
//     BaseEventQueue header rounds up to the element's 16-byte alignment (12 -> 16),
//     so maEvents lives at +0x10 (the element carries 16-byte SIMD lanes).
//   * AddEvent @ 0x82210718 copies the element as 8 x 8-byte words
//     (`v12 = (a1[2] << 6) + *a1; v13 = 8; do { *v12++ = *v11++; } while(--v13);`,
//     `std r9,0(r10)`), i.e. a 64-byte record at a `slwi r,count,6` (x64) stride.
//   * Append @ 0x823C2080 strides its block-copy by `slwi r,count,6` (x64).
//   => sizeof(InEventLineTestNearest) == 0x40 (64), alignas(16).
//
// LAYOUT: this "nearest" query shares the line-test query field set used by the
// Fine/FastDoubleSided line-test events (CgsSceneManagerIO_EventLineTest.h): a pair of
// 16-byte Vector3 lanes (line start/end) followed by the query/exclusion scalars,
// the leading lanes forcing 16-byte alignment and a 64-byte record. Construct does not
// read the element interior and Append/AddEvent block-copy it whole, so only the
// size/alignment is load-bearing for the queue-instantiation TUs; the named scalar
// fields are reconstructed for a complete element type. The result-side payload
// (OutEventLineTestNearestResult) is a distinct element with its own home.

#include "types.hpp"
#include "BrnCommonTypes.h"                                      // Vector3 (16-byte SIMD lane)
#include "GameShared/GameClasses/SceneManager/CgsSceneQueryId.h" // CgsSceneManager::SceneQueryId
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"     // CgsSceneManager::EntityId

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores
    // events by byte image). Distinctly named so this leaf element home never ODR-clashes
    // with the SceneManagerIO::Event / EventBaseLineTest defined by the other per-element
    // queue homes.
    struct EventBaseLineTestNearest {};

    // Which entities a nearest line test excludes from its hits. Single 4-byte enum slot
    // (matches the line-test query family's meExclusionMode).
    enum ENearestExclusionMode
    {
        E_NEAREST_EXCLUDE_ENTITY_ONLY     = 0,
        E_NEAREST_EXCLUDE_ALL_CHILD_PARTS = 1,
    };

    // EventQueue<InEventLineTestNearest, N> element. 16-byte aligned (carries two Vector3
    // lanes), sizeof 0x40 (64). Field set mirrors the Fine/FastDoubleSided line-test events.
    struct alignas(16) InEventLineTestNearest : public EventBaseLineTestNearest
    {
        Vector3              mLineStart;          // +0x00 (16B SIMD lane)
        Vector3              mLineEnd;            // +0x10 (16B SIMD lane)
        SceneQueryId         mQueryId;            // +0x20
        u32                  mx32EntityTypeFlags; // +0x24
        EntityId             mExcludeEntityId;    // +0x28
        ENearestExclusionMode meExclusionMode;    // +0x2C (4B enum slot)
        u8                   mxVolumeTypeFlags;   // +0x30
        // +0x31..0x3F trailing pad to the 16-byte alignment forced by the Vector3 lanes
        // -> sizeof == 0x40.
    };
}
}
