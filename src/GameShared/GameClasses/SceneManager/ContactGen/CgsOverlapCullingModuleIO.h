#pragma once

// CgsSceneManager::OverlapCullingIO — the input/output payload event types for the
// overlap-culling (broadphase narrow-phase request) module. The InputBuffer carries three
// fixed-capacity EventQueues; two of them (the internal-collision-volume add/remove request
// queues) are bounded at guMAX_NUM_INTERNAL_VOLUME_REQUESTS (256).
//
// Event field sets recovered from the DecFIGS DWARF (CgsOverlapCullingModuleIO.h:46-57); the
// per-instantiation EventQueue<...,256>::Construct bodies are reconstructed from the X360 asm
// (0x828C4E20 / 0x828C4E90) in the sibling explicit-instantiation .cpp files.
//
// Each event derives from the empty CgsModule::Event base, so the queue stores them by their
// byte image. AddInternalCollisionVolume is 12 bytes (3 u32), RemoveInternalCollisionVolume is
// 4 bytes (1 u32); both are only 4-aligned, so EventQueue<T,256>'s inline maEvents buffer lands
// right after the 12-byte BaseEventQueue header (no 16-byte padding) -- the Construct stores its
// buffer pointer at this+0xC, exactly as the asm does (addi r30,this,0xC; stw r30,0(this)).

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsEventQueue.h"  // CgsModule::EventQueue, CgsModule::Event

namespace CgsSceneManager
{
namespace OverlapCullingIO
{
    // CgsOverlapCullingModuleIO.h:37/38 (DWARF).
    const u32 guMAX_NUM_BODIES                     = 16384;
    const u32 guMAX_NUM_INTERNAL_VOLUME_REQUESTS   = 256;

    // Empty per-module event base (CgsModule event-queue convention; the queue stores events
    // by byte image). Matches the namespace-local Event base the other SceneManager IO event
    // payloads use (e.g. CgsSceneManager::SceneManagerIO::Event in CgsSceneManagerIO_EventAddForCollision.h).
    struct Event {};

    // CgsOverlapCullingModuleIO.h:46 (DWARF). Request to add an internal collision volume
    // (a volume nested inside another) to the broadphase, carrying the three volume-instance
    // indices that wire the nested/escape relationship.
    struct AddInternalCollisionVolume : public Event
    {
        u32 muVolumeInstanceIndex;          // +0x00 (after the empty Event base)
        u32 muInternalVolumeInstanceIndex;  // +0x04
        u32 muEscapeVolumeInstanceIndex;    // +0x08
    };

    // CgsOverlapCullingModuleIO.h:55 (DWARF). Request to remove a previously-added internal
    // collision volume, identified by its volume-instance index.
    struct RemoveInternalCollisionVolume : public Event
    {
        u32 muVolumeInstanceIndex;  // +0x00 (after the empty Event base)
    };

    // The two bounded request queues (DWARF CgsOverlapCullingModuleIO.h:63/64).
    typedef CgsModule::EventQueue<AddInternalCollisionVolume, 256>    InAddInternalVolumeQueue;
    typedef CgsModule::EventQueue<RemoveInternalCollisionVolume, 256> InRemoveInternalVolumeQueue;
}
}
