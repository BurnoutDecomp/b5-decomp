#pragma once

// CgsSceneManager::SceneManagerIO::SceneQueryInterface (DWARF/asserts CgsSceneManagerModuleIO.h
// :995/996 etc.) -- the client-facing scene-query producer the game/physics side calls to
// stage line/sphere/volume test queries onto the scene-manager input queues. It holds a small
// table of BaseEventQueue<T>* pointers (one per query kind); each public test method packs its
// args into a stack-local element record and AddEvent's it onto the matching queue.
//
// MEMBER OFFSETS (X360-attested by the four decompiled members of this class):
//   +0x00  mpFineLineTestQueue          BaseEventQueue<InEventLineTestFine>*        (LineTestFine: lwz r11,0(this); HasData slot 0)
//   +0x04  mpFineLineTestNearestQueue   BaseEventQueue<InEventLineTestNearest>*     (LineTestNearest: lwz r11,4(this); HasData slot 1)
//   +0x08  mpFineLineTestFastDoubleSidedQueue
//   +0x0C  mpFineSphereTestFastQueue
//   +0x10  mpFineVolumeTestDeepestQueue BaseEventQueue<InEventVolumeTestDeepest>*   (VolumeTestDeepest: lwz r11,0x10(this); HasData slot 4)
//   +0x14  mpFineVolumeTestQueue
//   +0x18  mpTriangleCollisionLineTestQueue
//   +0x1C  mpTriangleCollisionLineTestNearestQueue
//   +0x20  mpTriangleCollisionSphereTestQueue
// (HasData @ 0x82204E48 reads slots 0,1,4,5,6,7,8; gaps at +0x08/+0x0C exist in its scan.)
//
// ⭐ ALL NINE SLOTS ATTESTED (scene-query wave 1, 2026-09-02). The six slots that used to be
// "NOT attested / structural stand-ins" are pinned by the ONE writer of this table,
// SceneManagerIO::InputBuffer_Query::Construct @0x828C7BC0 (0x828C7C74..0x828C7C94): it stores
// the addresses of the buffer's nine typed fine/triangle-collision queues into this+4..+36 --
// i.e. into THIS interface, which is InputBuffer_Query's first member (DWARF
// CgsSceneManagerModuleIO.h:540) -- in exactly the order of the buffer's own members
// (DWARF :547..:555): FineLineTest(+28752), FineLineTestNearest(+45152),
// FineLineTestFastDoubleSided(+61552), FineSphereTestFast(+62592), FineVolumeTestDeepest(+63376),
// FineVolumeTest(+120736), TriangleCollisionLineTest(+135088),
// TriangleCollisionLineTestNearest(+147392), TriangleCollisionSphereTest(+159696). The element
// type of each slot is therefore the element type of the queue whose address it holds.
//
// The bodies of LineTestFine / LineTestNearest / VolumeTestDeepest live in their own TUs
// (CgsSceneManagerIO_SceneQueryInterface.cpp, CgsSceneManagerIO_SceneQueryInterface_LineTestNearest.cpp);
// HasData lives in CgsSceneManagerModuleIO.cpp. This header only declares the class + pins offsets.

#include "types.hpp"
#include "BrnCommonTypes.h"                                                       // Vector3
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                      // CgsModule::BaseEventQueue<T>
#include "GameShared/GameClasses/SceneManager/CgsSceneQueryId.h"                  // CgsSceneManager::SceneQueryId
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"                      // CgsSceneManager::EntityId
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventLineTest.h"          // InEventLineTestFine + EExclusionMode
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventLineTestNearest.h"   // InEventLineTestNearest + ENearestExclusionMode
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventVolumeTestDeepest.h" // InEventVolumeTestDeepest
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventSphereTest.h"                        // InEventSphereTestFast
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventVolumeTestFine.h"                    // InEventVolumeTestFine
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventTriangleCollisionLineTest.h"        // InEventTriangleCollisionLineTest
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventTriangleCollisionLineTestNearest.h" // InEventTriangleCollisionLineTestNearest
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventTriangleCollisionSphereTest.h"      // InEventTriangleCollisionSphereTest

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    struct SceneQueryInterface
    {
        // ---- query-queue table (offsets X360-attested; see file header) ----
        // Slot ORDER and element TYPES are pinned by InputBuffer_Query::Construct @0x828C7BC0,
        // the table's only writer (see the file header). Names follow the DWARF names of the
        // queues each slot points at (CgsSceneManagerModuleIO.h:547..:555).
        CgsModule::BaseEventQueue<InEventLineTestFine>*                        mpFineLineTestQueue;                      // +0x00
        CgsModule::BaseEventQueue<InEventLineTestNearest>*                     mpFineLineTestNearestQueue;               // +0x04
        CgsModule::BaseEventQueue<InEventLineTestFastDoubleSided>*             mpFineLineTestFastDoubleSidedQueue;       // +0x08
        CgsModule::BaseEventQueue<InEventSphereTestFast>*                      mpFineSphereTestFastQueue;                // +0x0C
        CgsModule::BaseEventQueue<InEventVolumeTestDeepest>*                   mpFineVolumeTestDeepestQueue;             // +0x10
        CgsModule::BaseEventQueue<InEventVolumeTestFine>*                      mpFineVolumeTestQueue;                    // +0x14
        CgsModule::BaseEventQueue<InEventTriangleCollisionLineTest>*           mpTriangleCollisionLineTestQueue;         // +0x18
        CgsModule::BaseEventQueue<InEventTriangleCollisionLineTestNearest>*    mpTriangleCollisionLineTestNearestQueue;  // +0x1C
        CgsModule::BaseEventQueue<InEventTriangleCollisionSphereTest>*         mpTriangleCollisionSphereTestQueue;       // +0x20

        // ---- producer methods (bodies in their own TUs) ----
        // @ 0x82216EF0 -- stages an InEventLineTestFine and pushes it onto mpFineLineTestQueue.
        bool LineTestFine(const Vector3&  lLineStart,
                          const Vector3&  lLineEnd,
                          SceneQueryId    lQueryId,
                          u32             lx32EntityTypeFlags,
                          u8              lxVolumeTypeFlags,
                          EntityId        lExcludeEntityId,
                          EExclusionMode  leExclusionMode);

        // @ 0x82216FD0 -- stages an InEventLineTestNearest and pushes it onto mpFineLineTestNearestQueue.
        bool LineTestNearest(const Vector3&        lLineStart,
                             const Vector3&        lLineEnd,
                             SceneQueryId          lQueryId,
                             u32                   lx32EntityTypeFlags,
                             u8                    lxVolumeTypeFlags,
                             EntityId              lExcludeEntityId,
                             ENearestExclusionMode leExclusionMode);

        // @ 0x822170B0 -- stages an InEventVolumeTestDeepest and pushes it onto mpFineVolumeTestDeepestQueue.
        // Returns int (Hex-Rays shape; AddEvent's bool widened back to r3).
        int VolumeTestDeepest(u32            lQueryId,
                              u32            lx32EntityTypeFlags,
                              u8             lxVolumeTypeFlags,
                              const void*    lpVolumeData,
                              const void*    lpTransform,
                              u32            lExcludeEntityId,
                              EExclusionMode leExclusionMode);

        // @ 0x82204E48 -- true iff any tested query queue is non-null and non-empty. Body in
        // CgsSceneManagerModuleIO.cpp.
        bool HasData();
    };
}
}
