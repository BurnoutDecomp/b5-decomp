#pragma once

// Minimal owning home for the SceneManager triangle-collision sphere-test query element
//   CgsSceneManager::SceneManagerIO::InEventTriangleCollisionSphereTest
// -- the per-event payload stored in the EventQueue<InEventTriangleCollisionSphereTest, 256>
// input queue that the SceneManager query IO buffer embeds by value. This header exists so the
// explicit-instantiation TUs for that queue's Construct and the base queue's Append can see a
// COMPLETE element type. Mirrors the opaque-payload sibling CgsSceneManagerIO_EventSphereTest.h.
//
// SIZE / ALIGNMENT (X360-attested):
//   * Append @ 0x828B9848 (BaseEventQueue<T>::Append) block-copies the source queue's events
//     with `XMemCpy(8 * this->miLength + mpEvents, source.mpEvents, 8 * source.miLength)` -- an
//     8-byte-per-element stride -- and advances `this->miLength += source.miLength`. So
//     sizeof(InEventTriangleCollisionSphereTest) == 8.
//   * Construct @ 0x828C4998 (EventQueue<...,256>::Construct) does `addi r30, this, 0xC` -- the
//     12-byte BaseEventQueue header is NOT padded up, so maEvents lives at +0x0C (element
//     alignment <= 4; the Hex-Rays `result == -12` is the misread of that addi+cmplwi), then sets
//     miMaxLength = 256 and miLength = 0.
//
// LAYOUT: no field-level DWARF is recovered in scope (Construct does not read the element
// interior; Append block-copies it whole). Only the 8-byte size / <=4-byte alignment is
// load-bearing, so the payload is modelled as an OPAQUE byte span at the X360-attested stride --
// field names are NOT fabricated (HARD RULE 3). The default (byte) alignment keeps maEvents at
// the attested +0x0C (a 16-byte alignment would force the header to pad to +0x10, contradicting
// Construct's `addi r30,this,0xC`).

#include "types.hpp"

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores events by
    // byte image). Distinctly named so this leaf element home never ODR-clashes with the bases
    // defined by the other per-element queue homes.
    struct EventBaseTriangleCollisionSphereTest {};

    // EventQueue<InEventTriangleCollisionSphereTest, 256> element. X360-attested stride 8 --
    // opaque payload, no field layout recovered in scope. Default-aligned (<=4) so the 12-byte
    // BaseEventQueue header is not padded (maEvents @ +0x0C, per Construct @ 0x828C4998).
    struct InEventTriangleCollisionSphereTest : public EventBaseTriangleCollisionSphereTest
    {
        u8 macOpaquePayload[8]; // +0x00  opaque (X360-attested 8-byte stride)
    };
}
}
