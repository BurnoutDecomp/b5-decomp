#pragma once

// Minimal owning home for the SceneManager triangle-collision NEAREST line-test query element
//   CgsSceneManager::SceneManagerIO::InEventTriangleCollisionLineTestNearest
// -- the per-event payload stored in the EventQueue<InEventTriangleCollisionLineTestNearest, 256>
// input queue that the SceneManager query IO buffer embeds by value. This header exists so the
// explicit-instantiation TUs for that queue's Construct and the base queue's AddEvent can see a
// COMPLETE element type. Mirrors the committed sibling CgsSceneManagerIO_EventTriangleCollisionLineTest.h
// (the non-"Nearest" line-test variant) and the opaque-payload CgsSceneManagerIO_EventSphereTest.h.
//
// SIZE / ALIGNMENT (X360-attested, two independent confirmations):
//   * Construct @ 0x828C4928 (EventQueue<...,256>::Construct) does `addi r30, this, 0x10` -- the
//     12-byte BaseEventQueue header rounds up to the element's 16-byte alignment (12 -> 16), so
//     maEvents lives at +0x10 (the Hex-Rays `result == -16` is the misread of that addi+cmplwi).
//   * AddEvent @ 0x828B7D98 copies the element as six 8-byte word moves at a `48 * miLength`
//     stride (`v12 = (48*a1[2] + *a1); v13 = 6; do { *v12++ = *v11++; } while(--v13)` over
//     _QWORD slots -- Hex-Rays unrolling the generic `mpEvents[miLength] = lEvent` 48-byte
//     struct copy). So sizeof(InEventTriangleCollisionLineTestNearest) == 0x30 (48).
//
// LAYOUT: no field-level DWARF is recovered in any decompiled TU's scope for THIS "Nearest"
// variant (Construct does not read the element interior; AddEvent block-copies it whole). Only
// the 48-byte size / 16-byte alignment is load-bearing for the queue-instantiation TUs, so the
// payload is modelled as an OPAQUE byte span at the X360-attested stride -- field names are NOT
// fabricated (HARD RULE 3). (The sibling non-"Nearest" line-test element has a DWARF-named field
// set; this "Nearest" queue is a distinct instantiation and gets its own distinctly-named home.)

#include "types.hpp"

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores events by
    // byte image). Distinctly named so this leaf element home never ODR-clashes with the bases
    // defined by the other per-element queue homes (EventBaseTriangleCollisionLineTest etc.).
    struct EventBaseTriangleCollisionLineTestNearest {};

    // EventQueue<InEventTriangleCollisionLineTestNearest, 256> element. 16-byte aligned,
    // X360-attested stride 48 (0x30) -- opaque payload, no field layout recovered in scope.
    struct alignas(16) InEventTriangleCollisionLineTestNearest : public EventBaseTriangleCollisionLineTestNearest
    {
        u8 macOpaquePayload[48]; // +0x00  opaque (X360-attested 48-byte stride)
    };
}
}
