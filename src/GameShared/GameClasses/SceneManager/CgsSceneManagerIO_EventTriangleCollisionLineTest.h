#pragma once

// Minimal owning home for the SceneManager triangle-collision line-test query element
//   CgsSceneManager::SceneManagerIO::InEventTriangleCollisionLineTest
// -- the per-event payload stored in the EventQueue<InEventTriangleCollisionLineTest, 256>
// input queue that the SceneManager IO buffers embed by value. This header exists so the
// explicit-instantiation TU for that queue's GetEvent
// (BaseEventQueue_InEventTriangleCollisionLineTest_GetEvent.cpp) can see a COMPLETE element
// type. Mirrors the committed sibling CgsSceneManagerIO_EventLineTestNearest.h.
//
// SIZE / ALIGNMENT (X360-attested):
//   * GetEvent @ 0x828AD0D0 returns &mpEvents[liIndex] via `slwi r11,r30,1` (liIndex*2),
//     `add r11,r30,r11` (liIndex*3), `slwi r11,r11,4` (*16) == liIndex*48 + mpEvents@0,
//     so sizeof(InEventTriangleCollisionLineTest) == 0x30 (48).
//   => The two leading 16-byte Vector3 lanes force alignas(16); 16+16+4=36 rounds up to 48.
//
// LAYOUT (DWARF CgsSceneManagerIO_FineQuery.h:125): `: public Event { Vector3 mLineStart;
// Vector3 mLineEnd; SceneQueryId mQueryId; }`. GetEvent does not read the element interior
// (it block-returns the whole record by reference), so only the size/alignment is
// load-bearing for the instantiation TU; the named fields are reconstructed from the DWARF
// for a complete element type.

#include "types.hpp"
#include "BrnCommonTypes.h"                                      // Vector3 (16-byte SIMD lane)
#include "GameShared/GameClasses/SceneManager/CgsSceneQueryId.h" // CgsSceneManager::SceneQueryId

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores events
    // by byte image). Distinctly named so this leaf element home never ODR-clashes with the
    // SceneManagerIO::Event / EventBaseLineTest* defined by the other per-element queue homes.
    struct EventBaseTriangleCollisionLineTest {};

    // EventQueue<InEventTriangleCollisionLineTest, 256> element. 16-byte aligned (carries two
    // Vector3 lanes), sizeof 0x30 (48).
    struct alignas(16) InEventTriangleCollisionLineTest : public EventBaseTriangleCollisionLineTest
    {
        Vector3      mLineStart; // +0x00 (16B SIMD lane)
        Vector3      mLineEnd;   // +0x10 (16B SIMD lane)
        SceneQueryId mQueryId;   // +0x20
        // +0x24..0x2F trailing pad to the 16-byte alignment forced by the Vector3 lanes
        // -> sizeof == 0x30.
    };
}
}
