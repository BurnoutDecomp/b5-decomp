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
//   +0x08, +0x0C : two further query queues NOT attested by any decompiled member -- NOT fabricated;
//                  left as raw pointer-sized placeholders so the attested offsets stay pinned.
//   +0x10  mpFineVolumeTestDeepestQueue BaseEventQueue<InEventVolumeTestDeepest>*   (VolumeTestDeepest: lwz r11,0x10(this); HasData slot 4)
//   +0x14,+0x18,+0x1C,+0x20 : four further query queues HasData also reads (slots 5..8).
//                  Their element types are NOT attested by any decompiled member, so each is
//                  typed as a STRUCTURAL BaseEventQueue<> stand-in: HasData only calls
//                  GetLength(), which reads miLength@+0x08 of the pointee regardless of the
//                  element type T (BaseEventQueue<T> layout is {T* mpEvents; s32 miMaxLength;
//                  s32 miLength;}), so any instantiation yields byte-identical codegen. The
//                  concrete element types are deliberately NOT fabricated.
// (HasData @ 0x82204E48 reads slots 0,1,4,5,6,7,8; gaps at +0x08/+0x0C exist in its scan.)
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

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    struct SceneQueryInterface
    {
        // ---- query-queue table (offsets X360-attested; see file header) ----
        CgsModule::BaseEventQueue<InEventLineTestFine>*        mpFineLineTestQueue;          // +0x00
        CgsModule::BaseEventQueue<InEventLineTestNearest>*     mpFineLineTestNearestQueue;   // +0x04
        void*                                                 mpUnattestedQueue08;          // +0x08 (not attested; HasData does NOT read it)
        void*                                                 mpUnattestedQueue0C;          // +0x0C (not attested; HasData does NOT read it)
        CgsModule::BaseEventQueue<InEventVolumeTestDeepest>*   mpFineVolumeTestDeepestQueue; // +0x10
        // +0x14..+0x20: HasData reads ptr->GetLength() on these four; element types unattested,
        // typed as structural BaseEventQueue<> stand-ins (GetLength reads miLength@+8 for any T).
        CgsModule::BaseEventQueue<InEventLineTestFine>*        mpQueueAt0x14;                // +0x14
        CgsModule::BaseEventQueue<InEventLineTestFine>*        mpQueueAt0x18;                // +0x18
        CgsModule::BaseEventQueue<InEventLineTestFine>*        mpQueueAt0x1C;                // +0x1C
        CgsModule::BaseEventQueue<InEventLineTestFine>*        mpQueueAt0x20;                // +0x20

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
