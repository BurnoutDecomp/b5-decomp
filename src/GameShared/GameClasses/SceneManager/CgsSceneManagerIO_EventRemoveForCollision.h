#pragma once

// Minimal owning home for the SceneManager "remove for collision" event element
//   CgsSceneManager::SceneManagerIO::InEventRemoveForCollision
// -- the per-event payload stored in the EventQueue<InEventRemoveForCollision, N>
// remove-for-collision input queue that InSceneUpdateInterface embeds by value. This
// header exists so the explicit-instantiation TU for that queue's Construct
// (EventQueue_InEventRemoveForCollision_1536.cpp) can see a COMPLETE element type (the
// queue embeds InEventRemoveForCollision maEvents[N] inline, and the base queue's
// Append/AddEvent block-copy sizeof(T)-strided records). The full InSceneUpdateInterface
// aggregate keeps its own placeholder slice and its own ledger TU; this header only adds
// the leaf element it queues, by name.
//
// SIZE / ALIGNMENT (X360-attested):
//   * Construct @ 0x822E2020 does `addi r30, this, 0x10` -- the 12-byte BaseEventQueue
//     header rounds up to the element's 8-byte alignment (12 -> 16), so maEvents lives
//     at +0x10.
//   * AddEvent @ 0x822AB2C8 stores the element as a SINGLE 64-bit value
//     (`v12 = *a2; HIDWORD(v12) = *a1; *(8*a1[2]++ + HIDWORD(v12)) = v12;`,
//     `ld r10,0(r26)` / `stdx r10,r11,r9`) at a `slwi r,count,3` (x8) stride.
//   * Append @ 0x827A6008 strides its block-copy by `slwi r,count,3` (x8).
//   => sizeof(InEventRemoveForCollision) == 8, alignof 8 (a single u64 -> the 12-byte
//      header rounds to 16, unlike the two-u32 InEventRemoveEntity whose 4-byte alignment
//      leaves the header at +0xC).
//
// LAYOUT: a single 64-bit value -- the collision/volume id whose body is removed from the
// scene collision set (the AddEvent does a single 64-bit store of the source qword).
// Construct does not read the element interior and Append/AddEvent block-copy it whole,
// so only the size/alignment is load-bearing for this queue-instantiation TU; the named
// u64 field is reconstructed for a complete element type.

#include "types.hpp"

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores
    // events by byte image). Distinctly named so this leaf element home never ODR-clashes
    // with the SceneManagerIO::Event defined by the other per-element queue homes.
    struct EventBaseRemoveForCollision {};

    // EventQueue<InEventRemoveForCollision, N> element. 8-byte aligned, sizeof 8
    // (a single 64-bit collision/volume id stored as one std).
    struct alignas(8) InEventRemoveForCollision : public EventBaseRemoveForCollision
    {
        u64 muCollisionId; // +0x00 -- the 64-bit id removed from the collision set
    };
}
}
