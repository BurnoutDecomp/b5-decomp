// BrnRaceCarEntityModule_wS34_00.cpp -- RaceCarEntityModule's release machine and teardown
// (the Release / Destruct half of the lifecycle in BrnRaceCarEntityModule.cpp).

#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnWorld
{
    // ========================================================================
    // RaceCarEntityModule::Release (vtable slot 2, 68 insns). Re-entered from
    // WorldModule::Release stage eWorldReleaseRaceCarEntityModule until it returns true.
    //
    // The release cursor is the word at +0x230, the member this header calls miPrepareCarIndex
    // (the console's meReleaseStage: START 0 / MANAGER 1 / DONE 2; Prepare's tail zeroes it to
    // START). Like PropEntityModule::Release, both exit paths store MANAGER, never DONE.
    //
    //   START, MANAGER  stage = MANAGER; if (!ModuleSingleBuffered::Release()) return false
    //   DONE            (nothing)
    //   then            stage = MANAGER; mePrepareStage = START (+0x22C);
    //                   mfRandomBoostTime = -1.0f (+0x184C4); mbRandomBoostOn = false (+0x184C8);
    //                   mePlayerActiveRaceCarIndex = INVALID (+0x182F8);
    //                   the inlined mAirTimeManager.Release() (+0x180D8: 3, 0.0f, 0.0f, 0.0f) and
    //                   mTrafficCheckManager.Release() (+0x180E8: 0, 6.0f); return true
    //   other           assert "Invalid Stage\n"; return false
    // ========================================================================
    bool RaceCarEntityModule::Release()
    {
        switch( miPrepareCarIndex )
        {
            case 0:     // E_RELEASESTAGE_START
            case 1:     // E_RELEASESTAGE_MANAGER
            {
                miPrepareCarIndex = 1;
                // [FLAG PC bring-up] the console gates on ModuleSingleBuffered::Release() here.
                // This header keeps the base inside maPrecedingState, so it is not reachable --
                // the same seam Prepare stage 1 documents for ModuleSingleBuffered::Prepare, which
                // therefore never ran and left the base nothing to release.
                break;
            }

            case 2:     // E_RELEASESTAGE_DONE
            {
                break;
            }

            default:
            {
                CGS_ASSERT( false, "Invalid Stage\n" );
                return false;
            }
        }

        miPrepareCarIndex  = 1;     // E_RELEASESTAGE_MANAGER
        mePrepareStage     = 0;     // E_PREPARESTAGE_START

        mfRandomBoostTime  = -1.0f;
        mbRandomBoostOn    = false;

        mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;

        mAirTimeManager.Release();
        mTrafficCheckManager.Release();

        return true;
    }

    // ========================================================================
    // RaceCarEntityModule::Destruct (vtable slot 3, 136 insns), in the console's order:
    //
    //   ModuleSingleBuffered::Destruct()                       (see the FLAG below)
    //   mBoostDebugComponent.Destruct()                        (+0x17E50, see the FLAG below)
    //   mRCEntityModuleDebugComponent.Destruct()               (+0x17CF0, see the FLAG below)
    //   mRaceCarStreamer.Destruct()                            (+0x11100)
    //   mfRandomBoostTime = -1.0f; mbRandomBoostOn = false
    //   the inlined mCrashPlayManager.Destruct()               (its debug component, +0x180F0)
    //   the inlined mPowerParkingManager.Destruct()            (its debug component, +0x182D4)
    //   the inlined mAirTimeManager / mTrafficCheckManager Destruct
    //   mbIsInOnlineGameMode = false                           (+0x18345)
    //   the debug race-car request block cleared               (+0x18740..+0x18774)
    // ========================================================================
    void RaceCarEntityModule::Destruct()
    {
        // [FLAG PC bring-up] ModuleSingleBuffered::Destruct() opens the console body. The base
        // lives inside maPrecedingState in this header and is not reachable (Construct and
        // Prepare carry the same seam).

        // [FLAG PC bring-up] the console then destructs the module's two embedded debug
        // components, mBoostDebugComponent (+0x17E50) and mRCEntityModuleDebugComponent
        // (+0x17CF0): each asserts "mpRaceCarEntityModule != NULL", clears that back pointer
        // and runs the empty base DebugComponent::Destruct. Neither component is a member of
        // this header (both sit in the opaque span after mBoostManager) and Construct never
        // builds them, so their asserts would fire on objects that were never constructed.

        mRaceCarStreamer.Destruct();

        mfRandomBoostTime = -1.0f;
        mbRandomBoostOn   = false;

        // CrashPlayManager::Destruct and PowerParkingManager::Destruct are inlined here on the
        // console; the crash-play one is nothing but its debug component's Destruct.
        mCrashPlayManager.mCrashPlayDebugComponent.Destruct();
        mPowerParkingManager.Destruct();

        mAirTimeManager.Destruct();
        mTrafficCheckManager.Destruct();

        mbIsInOnlineGameMode = false;

        DEBUG_mbSpawnDebugRaceCarThisFrame = false;
        DEBUG_mbCleanDebugRaceCarThisFrame = false;
        DEBUG_mbDebugRaceCarIsActive       = false;
        DEBUG_mDebugRaceCarPosition.SetZero();
        DEBUG_mDebugRaceCarDirection.SetZero();
        DEBUG_mfDebugRaceCarVelocity       = 0.0f;
        DEBUG_mpDebugRaceCar               = nullptr;
    }
}
