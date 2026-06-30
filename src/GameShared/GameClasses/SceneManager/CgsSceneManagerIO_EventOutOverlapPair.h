#pragma once

// Minimal owning home for the SceneManager overlap-OUTPUT element
//   CgsSceneManager::SceneManagerIO::OutOverlapPair
// -- the per-event payload stored in the BaseEventQueue<OutOverlapPair> overlap-output
// queue the scene manager bridges overlap generation onto. This header exists so the
// explicit-instantiation TUs for that queue's AddEvent
// (BaseEventQueue_OutOverlapPair_AddEvent.cpp) and Append
// (BaseEventQueue_OutOverlapPair_Append.cpp) can see a COMPLETE element type (AddEvent/
// Append block-copy sizeof(T)-strided records).
//
// NOTE (distinct type): this 24-byte CgsSceneManager::SceneManagerIO::OutOverlapPair is a
// DIFFERENT type from the 16-byte CgsSceneManager::OverlappingPair (CgsOverlappingPair.h,
// BaseEventQueue<OverlappingPair>::AddEvent @ 0x828B8B08, stride 0x10). Do NOT conflate them.
//
// SIZE / ALIGNMENT (X360-attested):
//   * AddEvent @ 0x828AD390 indexes mpEvents[miLength] with `slwi r9,r11,1; add r11,r11,r9;
//     slwi r11,r11,3` (== miLength*24) and copies the element as three 8-byte qword stores
//     (std @ +0x00/+0x08/+0x10). Append @ 0x827A6FE8 strides its block-copy by the same 24.
//     So sizeof(OutOverlapPair) == 0x18 (24), 8-byte aligned (three qwords; no SIMD lane).
//
// LAYOUT: no field-level DWARF covers this element, and neither AddEvent nor Append reads the
// element interior, so the payload is modelled as an OPAQUE 24-byte / 8-byte-aligned byte span
// -- field names are NOT fabricated (HARD RULE 3).

#include "types.hpp"

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores
    // events by byte image). Distinctly named so this leaf element home never ODR-clashes
    // with the bases defined by the other per-element queue homes.
    struct EventBaseOutOverlapPair {};

    // BaseEventQueue<OutOverlapPair> element. 8-byte aligned, X360-attested stride 24 (0x18)
    // -- opaque payload, no field layout recovered in scope.
    struct OutOverlapPair : public EventBaseOutOverlapPair
    {
        u64 maOpaquePayload[3]; // +0x00  opaque (X360-attested 24-byte stride; three qwords)
    };
}
}
