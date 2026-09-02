#pragma once

// Owning home for the SceneManager triangle-collision NEAREST line-test query element
//   CgsSceneManager::SceneManagerIO::InEventTriangleCollisionLineTestNearest
// -- the per-event payload stored in the EventQueue<InEventTriangleCollisionLineTestNearest, 256>
// input queues that InputBuffer_Query and TriCacheQueryBuffer embed by value.
//
// SIZE / ALIGNMENT (X360-attested, two independent confirmations):
//   * Construct @ 0x828C4928 (EventQueue<...,256>::Construct) does `addi r30, this, 0x10` -- the
//     12-byte BaseEventQueue header rounds up to the element's 16-byte alignment (12 -> 16), so
//     maEvents lives at +0x10 (the Hex-Rays `result == -16` is the misread of that addi+cmplwi).
//   * AddEvent @ 0x828B7D98 copies the element as six 8-byte word moves at a `48 * miLength`
//     stride. So sizeof(InEventTriangleCollisionLineTestNearest) == 0x30 (48).
//
// LAYOUT (scene-query wave 1, 2026-09-02 -- the "opaque payload, no field layout recovered" note
// that stood here is RETIRED): the DecFIGS DWARF names the fields (CgsSceneManagerIO_FineQuery.h
// :135/:136/:137 -- `Vector3 mLineStart; Vector3 mLineEnd; SceneQueryId mQueryId;`, the same
// field set as the sibling non-Nearest InEventTriangleCollisionLineTest), and the X360 PRODUCER
// SceneManagerModule::ProcessLineTestNearest @0x828D38C0 stores exactly those three at the
// element's +0x00 (`stvx128 v0` from the query's mLineStart), +0x10 (`stvx128 v0` from the
// query's mLineEnd) and +0x20 (`stw r10` from the query's mQueryId) before AddEvent'ing it
// (0x828D3904..0x828D3930, and again at 0x828D3B54..0x828D3B7C). 16+16+4 = 36 rounds up to
// the attested 48 under alignas(16).

#include "types.hpp"
#include "BrnCommonTypes.h"                                      // Vector3 (16-byte SIMD lane)
#include "GameShared/GameClasses/SceneManager/CgsSceneQueryId.h" // CgsSceneManager::SceneQueryId

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores events by
    // byte image). Distinctly named so this leaf element home never ODR-clashes with the bases
    // defined by the other per-element queue homes (EventBaseTriangleCollisionLineTest etc.).
    struct EventBaseTriangleCollisionLineTestNearest {};

    // EventQueue<InEventTriangleCollisionLineTestNearest, 256> element. 16-byte aligned (two
    // Vector3 lanes), sizeof 0x30 (48). DWARF CgsSceneManagerIO_FineQuery.h:133.
    struct alignas(16) InEventTriangleCollisionLineTestNearest : public EventBaseTriangleCollisionLineTestNearest
    {
        Vector3      mLineStart; // +0x00 (16B SIMD lane)   DWARF :135
        Vector3      mLineEnd;   // +0x10 (16B SIMD lane)   DWARF :136
        SceneQueryId mQueryId;   // +0x20                    DWARF :137
        // +0x24..+0x2F trailing pad to the 16-byte alignment -> sizeof == 0x30.
    };

    static_assert(sizeof(InEventTriangleCollisionLineTestNearest) == 0x30,
                  "InEventTriangleCollisionLineTestNearest must keep the X360-attested 48-byte stride");
}
}
