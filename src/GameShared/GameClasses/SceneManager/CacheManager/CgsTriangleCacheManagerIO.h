#pragma once

// Owning home for the CgsSceneManager::TriangleCacheManagerIO per-event queue element
// records -- the payloads stored in the TriangleCacheManagerIO InputBuffer's three fixed
// EventQueue<T, 298> input queues (add / remove / update-position). These structs exist so
// the explicit-instantiation TUs for those queues' Construct/AddEvent can see a COMPLETE
// element type (the queue embeds T maEvents[298] inline, and the base queue's
// AddEvent/Append block-copy sizeof(T)-strided records).
//
// LAYOUT IS DWARF-AUTHORITATIVE (DecFIGS dwarfdump CgsTriangleCacheManagerIO.h):
//   struct InEventAddToCache          : public Event { s32 miCacheSlot; f32 mfCacheSphereRadius; }  (:47-50)
//   struct InEventUpdateCachedPosition: public Event { s32 miCacheSlot; Vector3Plus mNewPositionAndRadius; }  (:63-67)
//   struct InEventRemoveFromCache     : public Event { s32 miCacheSlot; }  (:106-108)
// Event is CgsModule::Event {} (empty; 0 bytes via empty-base optimisation).
//
// SIZE / ALIGNMENT (X360-attested):
//   * InEventAddToCache      Construct @ 0x822E24F0 does `addi r30, this, 0xC` -- the 12-byte
//     BaseEventQueue header needs no padding (element is 4-aligned: two u32/f32 words) so
//     maEvents lands at +0xC. sizeof == 8.
//   * InEventRemoveFromCache Construct @ 0x822E25D0 does `addi r30, this, 0xC` likewise;
//     AddEvent @ 0x825E48C0 copies the element as ONE 32-bit word (`slwi r11,miLength,2;
//     stwx *a2`) at a stride-4 offset -- sizeof == 4, alignof 4 (single s32 field).
//   * InEventUpdateCachedPosition carries a Vector3Plus (alignas(16)); reconstructed per
//     DWARF for a complete element type (sizeof 32, alignof 16). Not exercised by this batch.

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::Event (empty event base)
#include "BrnCommonTypes.h"                                        // Vector3Plus (alignas(16))

namespace CgsSceneManager
{
namespace TriangleCacheManagerIO
{
    // EventQueue<InEventAddToCache, 298> element. 4-aligned, sizeof 8.
    struct InEventAddToCache : public CgsModule::Event
    {
        s32 miCacheSlot;          // +0x00 -- destination cache slot for the added triangle set
        f32 mfCacheSphereRadius;  // +0x04 -- bounding-sphere radius of the cached triangles
    };

    // EventQueue<InEventUpdateCachedPosition, 298> element. 16-aligned (carries Vector3Plus),
    // sizeof 32. Homed here by DWARF; produced by DetachedWheelManager::UpdateTriangleCache.
    struct InEventUpdateCachedPosition : public CgsModule::Event
    {
        s32         miCacheSlot;             // +0x00 -- cache slot to reposition
        Vector3Plus mNewPositionAndRadius;   // +0x10 -- new xyz position, w = sphere radius
    };

    // EventQueue<InEventRemoveFromCache, 298> element. 4-aligned, sizeof 4 (single s32).
    struct InEventRemoveFromCache : public CgsModule::Event
    {
        s32 miCacheSlot;  // +0x00 -- cache slot to evict
    };
}
}
