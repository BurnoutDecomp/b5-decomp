#pragma once

// Minimal owning home for the SceneManager generic line-test query element
//   CgsSceneManager::SceneManagerIO::InEventLineTest
// -- the per-event payload stored in the EventQueue<InEventLineTest, N> line-test
// input queues that the SceneManager IO buffers embed by value. This header exists so
// the explicit-instantiation TU for that queue's Construct
// (EventQueue_InEventLineTest_256.cpp) can see a COMPLETE element type (the queue
// embeds InEventLineTest maEvents[N] inline, and the base queue's Append/AddEvent
// block-copy sizeof(T)-strided records). The full IO-buffer aggregates (InputBuffer_Query
// etc.) keep their own placeholder slices and their own ledger TUs; this header only adds
// the leaf element they queue, by name.
//
// ELEMENT LAYOUT (DecFIGS DWARF CgsSceneManagerModuleIO.h:155 -- struct InEventLineTest
// : public Event):
//     +0x00  mLineStart           Vector3  (16B SIMD lane)      (:156)
//     +0x10  mLineEnd             Vector3  (16B SIMD lane)      (:157)
//     +0x20  mQueryId             SceneQueryId  (u32)           (:159)
//     +0x24  mx32EntityTypeFlags  EntityTypeFlags (u32)         (:160)
//     +0x28..0x3F  trailing pad to the 16-byte alignment forced by the leading Vector3 lanes
// -> sizeof == 0x40 (64), alignas(16). Mirrors the Fine/FastDoubleSided/Nearest line-test
// query field set (CgsSceneManagerIO_EventLineTest.h / _EventLineTestNearest.h), minus the
// exclusion scalars this base "LineTest" event omits.
//
// SIZE / ALIGNMENT (X360-attested): Construct @ 0x828C4618 (InEventLineTest,256) does
// `addi r30, this, 0x10` -- the 12-byte BaseEventQueue header rounds up to the element's
// 16-byte alignment (12 -> 16), so maEvents lives at +0x10. Construct does not read the
// element interior, so only the size/alignment is load-bearing for the queue-instantiation
// TU; the named scalar fields are reconstructed for a complete element type.

#include "types.hpp"
#include "BrnCommonTypes.h"                                      // Vector3 (16-byte SIMD lane)
#include "GameShared/GameClasses/SceneManager/CgsSceneQueryId.h" // CgsSceneManager::SceneQueryId

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores
    // events by byte image). Distinctly named so this leaf element home never ODR-clashes
    // with the SceneManagerIO::Event / EventBaseLineTest defined by the other per-element
    // queue homes.
    struct EventBaseLineTestQuery {};

    // EventQueue<InEventLineTest, N> element. 16-byte aligned (carries two Vector3 lanes),
    // sizeof 0x40 (64).
    struct alignas(16) InEventLineTest : public EventBaseLineTestQuery
    {
        Vector3      mLineStart;          // +0x00 (16B SIMD lane)
        Vector3      mLineEnd;            // +0x10 (16B SIMD lane)
        SceneQueryId mQueryId;            // +0x20
        u32          mx32EntityTypeFlags; // +0x24 (EntityTypeFlags bitset)
        // +0x28..0x3F trailing pad to the 16-byte alignment forced by the Vector3 lanes
        // -> sizeof == 0x40.
    };
}
}
