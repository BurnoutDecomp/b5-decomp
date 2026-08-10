// ============================================================================
// b5-decomp/src/GameSource/Physics/VehicleManager/BrnVehicleManager_Prepare.cpp
//
// The PREPARE half of BrnPhysics::Vehicle::VehicleManager -- the leg that finally puts the
// cars into the triangle cache (home TU BrnVehicleManager.cpp is still unmounted, so this is a
// slice TU in the established BrnVehicleManager_PerFrameLeaves.cpp pattern; fold back when the
// home mounts):
//
//   VehicleManager::Prepare              @0x8263C688  (75 insns)  DWARF h:~330
//   VehicleManager::PrepareTriangleCache @0x82615BA0  (37 insns)
//
// Reconstructed from the X360 ASM (read line by line 2026-08-10), not from the Hex-Rays.
//
// ⭐⭐ WHY THIS IS THE `usedSlots` LEG, stated once so no later wave re-derives it.
// TriangleCacheManager::mUsedCacheSlots -- the bitset whose popcount is the "usedSlots" probe --
// has exactly ONE setter in the whole console image: ProcessAddToCacheEvents, draining
// InSceneUpdateInterface::mAddToCacheQueue. That queue has exactly one filler on the race-car
// path, and it is PrepareTriangleCache below. PhysicsModule::UpdateCachedPositions fills a
// DIFFERENT queue (mUpdateCachedPositionQueue) whose consumer ASSERTS the used bit is already
// set ("Bit not set for event index "), so the position half can only run AFTER this one.
//
// ⭐⭐ AND WHY STAGING INTO A DOOMED BUFFER IS CORRECT HERE. WorldModule::Prepare's
// eWorldPreparePhysicsModule stage creates a scene input buffer, runs PhysicsModule::Prepare
// into it, calls UpdateScene(..., lbPrepare = TRUE) and destroys the buffer -- all in one call.
// That looks like the silent-drop shape that ate the world-collision events, and it is not:
// SceneManagerModule::BridgeInputSceneUpdateInterfaceToSubModules @0x828D1F88 drains the three
// triangle-cache queues *only* when its lbPrepare argument is non-zero (`stb r7, arg_37` at
// 0x828D1FA4, re-tested at 0x828D290C / 0x828D2D44 / 0x828D388C). The Prepare-time UpdateScene
// is the one call that passes TRUE. So the console consumes these 28 events two lines after
// they are posted, by design. The PC bridge had those legs missing; they were added this wave.
// ============================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                             // CGS_ASSERT
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO.h"             // SceneManagerIO::InputBuffer_Update::GetInSceneUpdateInterface (@0x825BD8C0, write-lock twin)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h" // InSceneUpdateInterface::mAddToCacheQueue (X360 +0xC4930)
#include "GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManagerIO.h" // TriangleCacheManagerIO::InEventAddToCache (8 bytes)
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"             // KF_TRIANGLE_CACHE_SPHERE_RADIUS

namespace BrnPhysics
{
namespace Vehicle
{
    // ------------------------------------------------------------------------------------
    // VehicleManager::PrepareTriangleCache  @0x82615BA0  (37 insns)
    //
    //   0x82615BB4  cmplwi r29, 0 -> assert "lpSceneInputBuffer_Update != NULL"
    //                                BrnVehicleManager.cpp:891   (li r5, 0x37B)
    //   0x82615BE0  bl  GetInSceneUpdateInterface   <- @0x825BD8C0, the NON-CONST (write-lock,
    //                                                  bit 3) twin. Correct: the caller chain
    //                                                  WorldModule::Prepare -> PhysicsModule::
    //                                                  Prepare runs under LockForWrite().
    //   0x82615BE8  addis r30, r3, 0xC ; addi r30, r30, 0x4930  -> +0xC4930 == mAddToCacheQueue
    //   0x82615BF4  lfs f0, flt_8200426C ; stfs f0, var_2C(r1)  -> radius, hoisted OUT of the loop
    //   0x82615BFC  stw r31, var_30(r1)                          -> miCacheSlot = i
    //   0x82615C08  bl  BaseEventQueue<InEventAddToCache>::AddEvent
    //   0x82615C10  cmpwi r31, 8   -> 8 iterations == KI_MAX_ACTIVE_RACE_CARS
    //   0x82615C24  bl  PhysicalTrafficManager::PrepareTriangleCache  (r3 = this + 44768)
    //   0x82615C28  li r3, 1
    //
    // ⭐ The stack record is the two adjacent slots var_30/var_2C -- i.e. {s32, f32} at +0/+4,
    // which is exactly the committed InEventAddToCache (sizeof 8, X360-attested by AddEvent's
    // `slwi r11, miLength, 3`). The console hoists the radius store out of the loop because it
    // never changes; written here as a whole-record assignment per iteration, which is the same
    // source shape and the same bytes appended.
    //
    // ⭐ The traffic call is `addis r3,r28,1 ; addi r3,r3,-0x5120` == this + 0x10000 - 0x5120
    // == this + 44768 == mPhysicalTrafficManager, reached BY NAME here (the console offset is
    // 44768 on X360 and 44768+drift on the host -- see this header's KU_HOST_DRIFT_* block).
    //
    // ⚠️ AS-SHIPPED: the return is the constant 1; there is no failure path. Reproduced because
    // Prepare tests it (`clrlwi r11, r3, 24 ; beq`).
    // ------------------------------------------------------------------------------------
    bool VehicleManager::PrepareTriangleCache(
        CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer_Update)
    {
        CGS_ASSERT(lpSceneInputBuffer_Update != NULL, "lpSceneInputBuffer_Update != NULL");  // :891

        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneUpdate =
            lpSceneInputBuffer_Update->GetInSceneUpdateInterface();

        for (s32 liRaceCar = 0; liRaceCar < KI_MAX_ACTIVE_RACE_CARS; ++liRaceCar)
        {
            CgsSceneManager::TriangleCacheManagerIO::InEventAddToCache lEvent;
            lEvent.miCacheSlot         = liRaceCar;
            lEvent.mfCacheSphereRadius = KF_TRIANGLE_CACHE_SPHERE_RADIUS;
            lpSceneUpdate->mAddToCacheQueue.AddEvent(lEvent);
        }

        mPhysicalTrafficManager.PrepareTriangleCache(lpSceneInputBuffer_Update);

        return true;
    }

    // ------------------------------------------------------------------------------------
    // VehicleManager::Prepare  @0x8263C688  (75 insns)
    //
    // A resumable three-stage fall-through FSM over mePrepareStage, the same shape as
    // PhysicsModule::Prepare's ten-stage one. Each arm stamps its stage number FIRST so a
    // `false` return leaves the cursor exactly where the work stopped and the world spine
    // re-enters next frame. The jump table at 0x8263C718 covers cases 0..3 with 0 and 1
    // sharing an arm; anything above 3 is the "Invalid prepare stage\n" default.
    //
    //   :815 (0x32F)  assert lpPhysicsAllocator != NULL
    //   :816 (0x330)  assert lpSceneInputBuffer_Update != NULL
    //   case 0/1: mePrepareStage = 1; if (!PrepareData(lpPhysicsAllocator)) return false;
    //   case 2:   mePrepareStage = 2; if (!PrepareTriangleCache(lpScene))   return false;
    //   case 3:   meReleaseStage = 0;                 (stw r11=0, 4(r31))
    //             mePrepareStage = 3;                 (stw r10=3, 0(r31))
    //             mn8RoundRobinControlWord = 0;       (stbx r11=0, r31, 0x2A1B0 == +172464)
    //             mbTrafficCheckingAllowed = true;    (stbx r29=1, r31, 0x2A119 == +172313)
    //             return true;
    //   default:  assert "Invalid prepare stage\n"  :867 (0x363);  return false;
    //
    // ⭐ Both tail bytes are `stbx` (BYTE stores), not words -- read off the asm, and both land
    // on members this header already maps by name, so neither console literal appears below.
    // ⚠️ Unlike PhysicsModule::Prepare, this one does NOT rewind its own cursor on success: it
    // leaves mePrepareStage at 3, so a second call re-enters case 3 and re-stamps the same two
    // bytes. Faithful; noted because it differs from its sibling FSM.
    //
    // ⛔ PrepareData is a named LINK STUB (WorldLinkStubs.cpp) -- see the declaration in
    // BrnVehicleManager.h for the two measured reasons. It returns true there, which is what the
    // console body always returns too, so the FSM's control flow is unchanged by the drop; what
    // IS dropped is the per-car data build, and that is stated at the stub.
    // ------------------------------------------------------------------------------------
    bool VehicleManager::Prepare(rw::IResourceAllocator* lpPhysicsAllocator,
                                 CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer)
    {
        CGS_ASSERT(lpPhysicsAllocator != NULL, "lpPhysicsAllocator != NULL");                 // :815
        CGS_ASSERT(lpSceneInputBuffer != NULL, "lpSceneInputBuffer_Update != NULL");          // :816

        switch (mePrepareStage)
        {
        default:
            CGS_ASSERT(false, "Invalid prepare stage\n");                                     // :867
            return false;

        case 0:
        case 1:
            mePrepareStage = 1;
            if (!PrepareData(lpPhysicsAllocator))
            {
                return false;
            }
            // fall through

        case 2:
            mePrepareStage = 2;
            if (!PrepareTriangleCache(lpSceneInputBuffer))
            {
                return false;
            }
            // fall through

        case 3:
            meReleaseStage           = 0;
            mePrepareStage           = 3;
            mn8RoundRobinControlWord = 0;
            mbTrafficCheckingAllowed = true;
            return true;
        }
    }
}
}
