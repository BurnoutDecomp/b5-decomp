#pragma once

// ===========================================================================
// CgsSceneManager::TriangleCollisionManager
//   Home: GameShared/GameClasses/SceneManager/TriangleCollision/
//         CgsTriangleCollisionManager.{h,cpp}
//
// Owns the triangle-collision scene: a PolygonSoupListSpatialMap of the static
// world collision (embedded by value @+0x00), a caller-owned handle array carved
// from a LinearMalloc, a private spatial-partition sub-allocator, and the debug
// overlay component. Embedded BY VALUE in CgsSceneManager::SceneManagerModule
// (mTriangleCollisionManager).
//
// Member LAYOUT + NAMES are DWARF-authoritative (DecFIGS
// CgsTriangleCollisionManager.h), cross-checked against the X360 asm byte offsets
// used by Prepare @0x828B2FF0 and ProcessAddPolySoupListEvents @0x828B3160:
//   mPolySoupListSpacialMap  @+0x00  (PolygonSoupListSpatialMap; its miNumSoupLists
//                                     at result[1]/+0x04 is read by GetNumPolySoupLists)
//   mpaPolySoupListHandles   @+0x70  (a1[28])
//   miMaxNumSoupLists        @+0x74  (a1[29])
//   miNumSoupListsAdded      @+0x78  (a1[30])
//   mSpacialAllocator        @+0x7C  (LinearMalloc; a1+31)
//   mDebugComponent          @+0xA0  (a1+40)
// (X360 32-bit member offsets are NOT preserved on the 64-bit PC compile where the
//  embedded sub-objects hold host-widened pointers; semantic-parity-by-name.)
//
// NOTE: this replaces the earlier minimal-stub header (u8 maEmbeddedState[528] +
// only Prepare, whose param comment '(the X360 passes 512)' was wrong: the budget
// a3 is only guarded to [0, KI_MAX_NUM_ZONES) with no fixed value).
// ===========================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupListSpatialMap.h" // PolygonSoupListSpatialMap (by value)
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"                                          // CgsMemory::LinearMalloc (by value)
#include "GameShared/GameClasses/SceneManager/TriangleCollision/CgsTriangleCollisionDebugComponent.h" // TriangleCollisionDebugComponent (by value)
#include "GameShared/GameClasses/SceneManager/TriangleCollision/CgsTriangleCollisionManagerIO_Events.h" // InEventAddPolySoupList / InEventClearPolySoupLists
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                                            // EventQueue<T,N>

namespace CgsResource { struct ResourceHandle; }

namespace CgsSceneManager
{
    // CgsTriangleCollisionManager.cpp:29 — the collision scene is diced into at most this
    // many zones (soup-list slots).
    const s32 KI_MAX_NUM_ZONES = 1024;

    class TriangleCollisionManager
    {
    public:
        // The add-poly-soup-list input queue element type (DWARF: EventQueue<
        // InEventAddPolySoupList, 20>). Concrete typedef used in lieu of the
        // InSceneUpdateInterface::InAddPolySoupListQueue alias home.
        typedef CgsModule::EventQueue<CgsSceneManager::TriangleCollisionManagerIO::InEventAddPolySoupList, 20>
            InAddPolySoupListQueue;

        // CgsTriangleCollisionManager.h:63
        void Construct();
        // CgsTriangleCollisionManager.h:69 — @0x828B2FF0. Prepare the scene from a linear
        // allocator with a fixed soup-list budget (< KI_MAX_NUM_ZONES). Returns success.
        bool Prepare(CgsMemory::LinearMalloc* lpAllocator, s32 liMaxNumPolySoupLists);
        // CgsTriangleCollisionManager.h:72
        void Destruct();
        // CgsTriangleCollisionManager.h:75
        bool Release();
        // CgsTriangleCollisionManager.h:80 — @0x828B3160. Drain the add-poly-soup-list queue,
        // registering each list and rebuilding the spatial partition if any event requests it.
        void ProcessAddPolySoupListEvents(const InAddPolySoupListQueue& lAddPolySoupListQueue);
        // CgsTriangleCollisionManager.h:85
        void ProcessClearPolySoupListEvents(
            const CgsModule::EventQueue<CgsSceneManager::TriangleCollisionManagerIO::InEventClearPolySoupLists, 20>& lClearPolySoupListsQueue);
        // CgsTriangleCollisionManager.h:90
        const CgsGeometric::PolygonSoupList* GetPolySoupList(s32 liIndex) const;
        // CgsTriangleCollisionManager.h:93
        const CgsGeometric::PolygonSoupListSpatialMap* GetPolySoupListSpacialMap() const;
        // HOST OVERLOAD (scene-query wave 1b, 2026-09-02): the non-const view the synchronous
        // nearest line test needs -- BaseCollisionGenerator::CollideLineAgainstPolySoupListNearest
        // @0x828131C0 runs PolygonSoupListSpatialMap::RunQuery @0x82843A80 on it, and RunQuery is
        // non-const in DecFIGS (_ZN12CgsGeometric25PolygonSoupListSpatialMap8RunQueryE...; it
        // publishes mpOutputQueryBuffer / miLastQueryResultCount). On the console the caller
        // (SceneManagerModule::ProcessLineTestNearest @0x828D3BB0) passes this member's address,
        // which is the manager's own (+0x00). Not attested as a DWARF declaration by itself.
        CgsGeometric::PolygonSoupListSpatialMap* GetPolySoupListSpacialMap() { return &mPolySoupListSpacialMap; }
        // CgsTriangleCollisionManager.h:96
        s32 GetNumPolySoupLists() const;

    private:
        CgsGeometric::PolygonSoupListSpatialMap mPolySoupListSpacialMap;            // +0x00  (h:101)
        CgsResource::ResourceHandle*            mpaPolySoupListHandles;             // +0x70  (h:102)
        s32                                     miMaxNumSoupLists;                  // +0x74  (h:103)
        s32                                     miNumSoupListsAdded;                // +0x78  (h:104)
        CgsMemory::LinearMalloc                 mSpacialAllocator;                  // +0x7C  (h:105)
        CgsSceneManager::TriangleCollisionDebugComponent mDebugComponent;          // +0xA0  (h:108)
    };
}
