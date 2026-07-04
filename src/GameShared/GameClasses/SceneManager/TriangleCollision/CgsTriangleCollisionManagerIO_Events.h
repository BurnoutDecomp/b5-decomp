#pragma once

// ===========================================================================
// Owning home for the two queue-element payloads of
// CgsSceneManager::TriangleCollisionManagerIO -- the per-event records stored in
// its input queues:
//   * InEventAddPolySoupList     -> EventQueue<InEventAddPolySoupList, 20>
//                                   (add-poly-soup-list input queue)
//   * InEventClearPolySoupLists  -> EventQueue<InEventClearPolySoupLists, 20>
//                                   (clear-poly-soup-lists input queue)
// This header exists so the four explicit-instantiation TUs for those queues'
// Construct/AddEvent can see COMPLETE element types (each queue embeds T maEvents[20]
// inline). Both payloads derive from the empty per-namespace Event base and are keyed
// by name from the DecFIGS DWARF (CgsTriangleCollisionManagerIO.h).
//
// DWARF (references/DecFIGS/dwarfdump/.../TriangleCollision/CgsTriangleCollisionManagerIO.h):
//   struct InEventAddPolySoupList : public Event {          // :52
//       ResourceHandle mPolySoupListHandle;                 // :54
//       int32_t        miZoneNumber;                        // :55
//       bool           mbRebuildSpacialPartitioning;        // :56
//   };
//   struct InEventClearPolySoupLists : public Event {       // :68
//       uint32_t miDummy;                                   // :70
//   };
//
// X360 ELEMENT STRIDES (load-bearing for the queue TUs):
//   InEventAddPolySoupList    == 16 bytes  (slwi ...,4 append in AddEvent @0x822C6858;
//        ResourceHandle 8B @+0 [two 32-bit ptrs, DWARF CgsResourceHandle.h:9], int32_t @+8,
//        bool @+12, 4-align -> 0x10). The element is only 4-aligned, which is confirmed by
//        the EventQueue<...,20>::Construct placing maEvents at base+0xC (addi r30,this,0xC).
//   InEventClearPolySoupLists ==  4 bytes  (slwi ...,2 append in AddEvent @0x822C69B0;
//        single uint32_t; Construct maEvents at base+0xC as well).
// NOTE (host widening): on the 64-bit PC compile ResourceHandle holds two host pointers
// (16B), so InEventAddPolySoupList is larger than the X360's 16-byte stride. The generic
// queue templates use sizeof(T), so this compiles; the X360 stride is documented above and
// is NOT preserved on the PC build, per the project semantic-parity-by-name rule.
// ===========================================================================

#include "types.hpp"
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h" // CgsResource::ResourceHandle

namespace CgsSceneManager
{
namespace TriangleCollisionManagerIO
{
    // Empty per-namespace event base (CgsModule event-queue convention; the queue stores
    // events by byte image). DWARF declares both payloads as `: public Event` resolved in
    // this TriangleCollisionManagerIO scope; distinctly named so this leaf element home
    // never ODR-clashes with the SceneManagerIO::Event of the sibling event homes.
    struct Event {};

    // Add-poly-soup-list queue element. X360 sizeof == 16 (stride 0x10, slwi ...,4).
    struct InEventAddPolySoupList : public Event
    {
        CgsResource::ResourceHandle mPolySoupListHandle;          // +0x00 (:54)  8B on X360
        s32                         miZoneNumber;                 // +0x08 (:55)
        bool                        mbRebuildSpacialPartitioning; // +0x0C (:56)
    };

    // Clear-poly-soup-lists queue element. X360 sizeof == 4 (stride 4, slwi ...,2).
    struct InEventClearPolySoupLists : public Event
    {
        u32 miDummy;                                             // +0x00 (:70)
    };
}
}
