// BrnPhysics::Vehicle::VehicleDriverInputInterface ledger TU.
//
// Home of the six X360 leaf functions for the driver-input seam:
//   Construct                  @0x822CBF60
//   AddTargetAssist            @0x822A0398
//   GetTargetAssistParams      @0x823A7B40
//   CopyBaseDeformationParams  @0x8279C290
//   GetBaseDeformationAmount   @0x825B3588
//   Ge (truncated symbol)      @0x825B3600
//
// Member layout + the per-active-race-car loop idiom (post-increment EActiveRaceCarIndex with
// the BurnoutConstants.h:39 range guard) are recovered from the X360 assembly. The inline
// queue accessors (GetUpdateDriverQueue) live in the header.
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverInputInterface.h"

#include <cstring>   // memcpy
#include <cstddef>   // offsetof (layout oracle)

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnPhysics
{
namespace Vehicle
{
    // Compile-time layout oracle (never called) -- pins the X360-authoritative member offsets
    // recovered from the leaf-function immediates so the gate catches any silent drift in the
    // inline VariableEventQueue<5040,16> size or the SIMD-array alignment.
    void VehicleDriverInputInterface::_AssertLayout()
    {
        static_assert(offsetof(VehicleDriverInputInterface, mTargetAssistPositions)  == 5056, "mTargetAssistPositions @ +5056");
        static_assert(offsetof(VehicleDriverInputInterface, mTargetAssistIDs)        == 5184, "mTargetAssistIDs @ +5184");
        static_assert(offsetof(VehicleDriverInputInterface, miTargetAssistCount)     == 5216, "miTargetAssistCount @ +5216");
        static_assert(offsetof(VehicleDriverInputInterface, maBaseDeformationAmounts) == 5220, "maBaseDeformationAmounts @ +5220");
        static_assert(offsetof(VehicleDriverInputInterface, maBaseDeformationFrames)  == 5252, "maBaseDeformationFrames @ +5252");
    }
    // @0x822CBF60  VehicleDriverInputInterface::Construct
    //
    // Constructs the inline driver-update queue, zeroes the target-assist count, then walks the
    // active-race-car slots clearing the per-car base-deformation snapshot: amount -> 0.0f
    // (flt_82001CC0), frame -> -1. The loop is the inlined post-increment over
    // EActiveRaceCarIndex (each iteration range-guards the index >=0 / <COUNT before each store,
    // then the post-increment guards index <= COUNT -- the BurnoutConstants.h:39 assert).
    void VehicleDriverInputInterface::Construct()
    {
        mDriverUpdateQueue.Construct();
        miTargetAssistCount = 0;

        for (EActiveRaceCarIndex leRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
             leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT;
             leRaceCarIndex++)
        {
            CGS_ASSERT(leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,   "leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
            CGS_ASSERT(leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
            maBaseDeformationAmounts[leRaceCarIndex] = 0.0f;

            CGS_ASSERT(leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,   "leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
            CGS_ASSERT(leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
            maBaseDeformationFrames[leRaceCarIndex] = -1;
        }
    }

    // @0x822A0398  VehicleDriverInputInterface::AddTargetAssist
    //
    // Appends one stomped/target car to this frame's target-assist list at the current count
    // index, then bumps the count. The position is stored with a full 16-byte vector store
    // (stvx128) into mTargetAssistPositions[count]; the ID into mTargetAssistIDs[count].
    void VehicleDriverInputInterface::AddTargetAssist(Vector3 lTargetAssistPosition, EntityId lTargetAssistID)
    {
        CGS_ASSERT(miTargetAssistCount < KI_MAX_TARGET_ASSIST_CARS,
                   "miTargetAssistCount < BrnTraffic::KI_MAX_STOMPED_CARS");

        mTargetAssistPositions[miTargetAssistCount] = lTargetAssistPosition;
        mTargetAssistIDs[miTargetAssistCount]       = lTargetAssistID;
        ++miTargetAssistCount;
    }

    // @0x823A7B40  VehicleDriverInputInterface::GetTargetAssistParams
    //
    // Writes the current target-assist count, and when non-zero memcpy's the packed position
    // (16 bytes/entry) and ID (4 bytes/entry) arrays out to the caller's buffers.
    void VehicleDriverInputInterface::GetTargetAssistParams(
            Vector3* lpOutTargetAssistPositions,
            EntityId* lpOutTargetAssistIDs,
            s32* lpiOutTargetAssistCount) const
    {
        *lpiOutTargetAssistCount = miTargetAssistCount;

        if (miTargetAssistCount > 0)
        {
            CGS_ASSERT(miTargetAssistCount <= KI_MAX_TARGET_ASSIST_CARS,
                       "miTargetAssistCount <= BrnTraffic::KI_MAX_STOMPED_CARS");

            memcpy(lpOutTargetAssistPositions, mTargetAssistPositions,
                   sizeof(Vector3) * miTargetAssistCount);
            memcpy(lpOutTargetAssistIDs, mTargetAssistIDs,
                   sizeof(EntityId) * miTargetAssistCount);
        }
    }

    // @0x823DB640  VehicleDriverInputInterface::Append
    //
    // ⭐ RECONSTRUCTED 2026-08-09 (feed wave) -- the callee of
    // WorldModule::BridgeInputToPhysicsModule @0x827AB830, which had no body anywhere in the
    // tree. Three steps, verbatim from the 31-instruction X360 body:
    //   1. merge the source's update-driver queue into ours (`bl VariableEventQueue<5040,16>::
    //      Append`, both sides at +0 -- the queue is the interface's leading member);
    //   2. assert that at most ONE side carries a target-assist list (the console fires only
    //      when BOTH counts are non-zero -- `lwz r11,0x1460(r31)` / `lwz r11,0x1460(r30)`,
    //      X360 BrnVehicleDriverInputInterface.h:164, string @0x82039118);
    //   3. when OUR count is zero, adopt the source's list wholesale by calling ITS
    //      GetTargetAssistParams into our own arrays + count (`r3=other, r4=this+0x13C0,
    //      r5=this+0x1440, r6=this+0x1460`). No copy at all when we already hold one.
    void VehicleDriverInputInterface::Append(const VehicleDriverInputInterface* lpInterfaceToAppend)
    {
        mDriverUpdateQueue.Append(*lpInterfaceToAppend->GetUpdateDriverQueue());

        CGS_ASSERT(miTargetAssistCount == 0 || lpInterfaceToAppend->GetTargetAssistCount() == 0,
                   "miTargetAssistCount==0 || lpInterfaceToAppend->GetTargetAssistCount()==0");

        if (miTargetAssistCount == 0)
        {
            lpInterfaceToAppend->GetTargetAssistParams(mTargetAssistPositions, mTargetAssistIDs,
                                                       &miTargetAssistCount);
        }
    }

    // @0x8279C290  VehicleDriverInputInterface::CopyBaseDeformationParams
    //
    // Copies the per-active-race-car base-deformation snapshot (amount + frame arrays) from
    // another interface into this one. The X360 walks the active-race-car slots, range-guarding
    // the index before each read/write (the inlined EActiveRaceCarIndex post-increment idiom).
    void VehicleDriverInputInterface::CopyBaseDeformationParams(
            const VehicleDriverInputInterface* lpInterfaceToCopy)
    {
        for (EActiveRaceCarIndex leRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
             leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT;
             leRaceCarIndex++)
        {
            CGS_ASSERT(leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,   "leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
            CGS_ASSERT(leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
            const f32 lfAmount = lpInterfaceToCopy->maBaseDeformationAmounts[leRaceCarIndex];

            CGS_ASSERT(leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,   "leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
            CGS_ASSERT(leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
            maBaseDeformationAmounts[leRaceCarIndex] = lfAmount;

            CGS_ASSERT(leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,   "leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
            CGS_ASSERT(leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
            const s32 liFrame = lpInterfaceToCopy->maBaseDeformationFrames[leRaceCarIndex];

            CGS_ASSERT(leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,   "leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
            CGS_ASSERT(leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
            maBaseDeformationFrames[leRaceCarIndex] = liFrame;
        }
    }

    // @0x825B3588  VehicleDriverInputInterface::GetBaseDeformationAmount
    //
    // Per-active-race-car base-deformation amount accessor (maBaseDeformationAmounts +0x519
    // words from `this`). Range-guards the index, then returns the float.
    f32 VehicleDriverInputInterface::GetBaseDeformationAmount(EActiveRaceCarIndex leRaceCarIndex) const
    {
        CGS_ASSERT(leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,   "leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

        return maBaseDeformationAmounts[leRaceCarIndex];
    }

    // @0x825B3600  VehicleDriverInputInterface::Ge  (exported symbol truncated)
    //
    // Per-active-race-car base-deformation frame accessor (maBaseDeformationFrames +0x521 words
    // from `this`). Range-guards the index, then returns the s32.
    s32 VehicleDriverInputInterface::Ge(EActiveRaceCarIndex leRaceCarIndex) const
    {
        CGS_ASSERT(leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,   "leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

        return maBaseDeformationFrames[leRaceCarIndex];
    }
}
}
