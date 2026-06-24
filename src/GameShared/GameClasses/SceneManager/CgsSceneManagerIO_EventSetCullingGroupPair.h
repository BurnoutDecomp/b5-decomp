#pragma once

// Minimal owning home for the queue element CgsSceneManager::SceneManagerIO::
// InEventSetCullingGroupPair -- the per-event payload stored in the
// EventQueue<InEventSetCullingGroupPair, 64> set-culling-group-pair input queue
// (InSceneUpdateInterface::mSetCullingGroupPairQueue). This header exists so the
// explicit-instantiation TU for that queue's Construct
// (EventQueue_InEventSetCullingGroupPair_64.cpp @ 0x822E21E0) can see a COMPLETE element
// type (the queue embeds InEventSetCullingGroupPair maEvents[64] inline). The full
// InSceneUpdateInterface aggregate keeps its own placeholder slice and its own ledger
// TU; this header only adds the leaf element it queues, by name.
//
// ELEMENT LAYOUT (X360-authoritative, read off the sibling BaseEventQueue<T>::AddEvent
// @ 0x822AB7D0 which appends this element):
//     lwz  r11, 8(this)            ; miLength
//     slwi r9, r11, 1 ; add r11,r11,r9 ; slwi r11,r11,2   ; index = miLength * (1+2) * 4
//                                                          ;       = miLength * 12  (stride 0x0C)
//     lwz r8,0(src);  stw r8,0(dst)    ; word 0  (group A)
//     lwz r10,4(src); stw r10,4(dst)   ; word 1  (group B)
//     lwz r10,8(src); stw r10,8(dst)   ; word 2  (enabled flag, stored as a 32-bit word)
// so the element is 12 bytes (0x0C), 4-byte aligned, three 32-bit words. They map to the
// (groupA, groupB, enabled) arguments of CullingGroupManager::SetCullingGroupPair
// (CgsCullingGroupManager.h). The 4-byte alignment is exactly why the 12-byte
// BaseEventQueue header needs NO padding and maEvents starts at +0x0C (the Construct asm
// at 0x822E21E0 does `addi r30, this, 0xC`). Construct does not read the element
// interior, so only the size/alignment is load-bearing for that TU.

#include "types.hpp"

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Empty per-module event base (CgsModule event-queue convention; the queue stores
    // events by byte image). Distinctly named so this leaf element home never ODR-clashes
    // with the SceneManagerIO::Event defined by the other per-element queue homes.
    struct EventBaseSetCullingGroupPair {};

    // mSetCullingGroupPairQueue element. 4-byte aligned; three 32-bit words
    // (group A, group B, enabled) -- the AddEvent body copies them as three `stw`.
    struct InEventSetCullingGroupPair : public EventBaseSetCullingGroupPair
    {
        u32 muGroupA;   // +0x00  first culling group id
        u32 muGroupB;   // +0x04  second culling group id
        u32 muEnabled;  // +0x08  adjacency-bit enable flag (stored as a 32-bit word)
    };
}
}
