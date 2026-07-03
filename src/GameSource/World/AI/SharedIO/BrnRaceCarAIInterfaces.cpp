#include "GameSource/World/AI/SharedIO/BrnRaceCarAIInterfaces.h"
#include "rw/math/vpu/vector3_operation.h"   // rw::math::vpu::IsValid (SetPlayerActiveRaceCarData)

// Out-of-line bodies of BrnAI::AIModuleIO::RaceCarAIInterface's 17-function TU: the
// management-queue producers (Activate/Deactivate/DetachAIControl/SetUpOutOfRange),
// the player-data setter, and the per-active-car snapshot getters. All index math
// and bit-array reads are X360-verified store-for-store against the grown layout in
// BrnRaceCarAIInterfaces.h. Bounds asserts are non-gating tripwires per the asm.

namespace BrnAI
{
namespace AIModuleIO
{
    // @0x822FD610  Queue an "activate this car under AI control" management event.
    void RaceCarAIInterface::ActivateRaceCar(EGlobalRaceCarIndex leGlobalRaceCarIndex,
                                             EActiveRaceCarIndex leActiveRaceCarIndex)
    {
        CGS_ASSERT(leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0,
                   "leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0");
        CGS_ASSERT(leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT,
                   "leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");
        CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                   "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

        ActivateRaceCarEvent lEvent;
        lEvent.meGlobalRaceCarIndex = leGlobalRaceCarIndex;
        lEvent.meActiveRaceCarIndex = leActiveRaceCarIndex;
        mManagementQueue.AddEvent<ActivateRaceCarEvent>(&lEvent, E_EVENT_ACTIVATE_RACE_CAR);
    }

    // @0x822FD6E0  Queue a "deactivate this car" management event.
    void RaceCarAIInterface::DeactivateRaceCar(EGlobalRaceCarIndex leGlobalRaceCarIndex, bool lbIsInAMode)
    {
        CGS_ASSERT(leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0,
                   "leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0");
        CGS_ASSERT(leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT,
                   "leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");

        DeactivateRaceCarEvent lEvent;
        lEvent.meGlobalRaceCarIndex = leGlobalRaceCarIndex;
        lEvent.mbIsInAMode = lbIsInAMode;
        mManagementQueue.AddEvent<DeactivateRaceCarEvent>(&lEvent, E_EVENT_DEACTIVATE_RACE_CAR);
    }

    // @0x822FD768  Queue a "detach AI control" management event.
    void RaceCarAIInterface::DetachAIControl(EGlobalRaceCarIndex leGlobalRaceCarIndex)
    {
        CGS_ASSERT(leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0,
                   "leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0");
        CGS_ASSERT(leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT,
                   "leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");

        DetachAIControlEvent lEvent;
        lEvent.meGlobalRaceCarIndex = leGlobalRaceCarIndex;
        mManagementQueue.AddEvent<DetachAIControlEvent>(&lEvent, E_EVENT_DETACH_AI_CONTROL);
    }

    // @0x822B22D0  Latch the player active-car snapshot (once per frame).
    void RaceCarAIInterface::SetPlayerActiveRaceCarData(Vector3 lPosition,
                                                        Vector3 lDirection,
                                                        EActiveRaceCarIndex leActiveRaceCarIndex)
    {
        CGS_ASSERT(rw::math::vpu::IsValid(lPosition), "RwMath::IsValid( lPosition )");
        CGS_ASSERT(!mbPlayerDataSet, "!mbPlayerDataSet");

        mePlayerActiveRaceCarIndex = leActiveRaceCarIndex;
        mPlayerCarPosition = lPosition;
        mPlayerCarDirection = lDirection;
        mbPlayerDataSet = true;
    }

    // @0x822FD7E8  Queue a "set up this out-of-range car" management event.
    void RaceCarAIInterface::SetUpOutOfRangeRaceCar(EGlobalRaceCarIndex leGlobalRaceCarIndex,
                                                    Vector3 lPosition,
                                                    Vector3 lAt,
                                                    u16 luSection,
                                                    BrnWorld::EDistrict leDistrict,
                                                    u8 luNumberOfMedalsToUnlock)
    {
        CGS_ASSERT(leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0 &&
                       leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT,
                   "leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0 && leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");

        SetUpOutOfRangeRaceCarEvent lEvent;
        lEvent.meGlobalRaceCarIndex = leGlobalRaceCarIndex;
        lEvent.mPosition = lPosition;
        lEvent.mAt = lAt;
        lEvent.muSection = luSection;
        lEvent.meDistrict = leDistrict;
        lEvent.muNumberOfMedalsToUnlock = luNumberOfMedalsToUnlock;
        mManagementQueue.AddEvent<SetUpOutOfRangeRaceCarEvent>(&lEvent, E_EVENT_SET_UP_OUT_OF_RANGE_RACE_CAR);
    }

    // @0x82764F80  The latched player-car world position.
    Vector3 RaceCarAIInterface::GetPlayerCarPosition() const
    {
        CGS_ASSERT(mbPlayerDataSet, "mbPlayerDataSet");
        return mPlayerCarPosition;
    }

    // @0x82764FF0  The latched player-car facing.
    Vector3 RaceCarAIInterface::GetPlayerCarDirection() const
    {
        CGS_ASSERT(mbPlayerDataSet, "mbPlayerDataSet");
        return mPlayerCarDirection;
    }

    // @0x82765060  The latched player active-race-car index.
    EActiveRaceCarIndex RaceCarAIInterface::GetPlayerActiveRaceCarIndex() const
    {
        CGS_ASSERT(mbPlayerDataSet, "mbPlayerDataSet");
        return mePlayerActiveRaceCarIndex;
    }

    // @0x8276BA80  The active car's world matrix.
    const Matrix44Affine& RaceCarAIInterface::GetActiveRaceCarMatrix(EActiveRaceCarIndex leActiveRaceCarIndex) const
    {
        CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                   "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
        CGS_ASSERT(mSetActiveRaceCars.IsBitSet(leActiveRaceCarIndex),
                   "mSetActiveRaceCars.IsBitSet( leActiveRaceCarIndex )");

        return maMatrices[leActiveRaceCarIndex];
    }

    // @0x8276BF18  The active car's AI section index.
    u16 RaceCarAIInterface::GetActiveRaceCarSection(EActiveRaceCarIndex leActiveRaceCarIndex) const
    {
        CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                   "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
        CGS_ASSERT(mSetActiveRaceCars.IsBitSet(leActiveRaceCarIndex),
                   "mSetActiveRaceCars.IsBitSet( leActiveRaceCarIndex )");

        return mauSectionIndices[leActiveRaceCarIndex];
    }

    // @0x8276C0A0  Is the active car airborne?
    bool RaceCarAIInterface::IsActiveRaceCarInAir(EActiveRaceCarIndex leActiveRaceCarIndex) const
    {
        CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                   "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
        CGS_ASSERT(mSetActiveRaceCars.IsBitSet(leActiveRaceCarIndex),
                   "mSetActiveRaceCars.IsBitSet( leActiveRaceCarIndex )");

        return mInAirBits.IsBitSet(leActiveRaceCarIndex);
    }

    // @0x8276C2D0  Is the active car crashing?
    bool RaceCarAIInterface::IsActiveRaceCarCrashing(EActiveRaceCarIndex leActiveRaceCarIndex) const
    {
        CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                   "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
        CGS_ASSERT(mSetActiveRaceCars.IsBitSet(leActiveRaceCarIndex),
                   "mSetActiveRaceCars.IsBitSet( leActiveRaceCarIndex )");

        return mCrashingBits.IsBitSet(leActiveRaceCarIndex);
    }

    // @0x8276C500  Is the active car in showtime?
    bool RaceCarAIInterface::IsActiveRaceCarInShowtime(EActiveRaceCarIndex leActiveRaceCarIndex) const
    {
        CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                   "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
        CGS_ASSERT(mSetActiveRaceCars.IsBitSet(leActiveRaceCarIndex),
                   "mSetActiveRaceCars.IsBitSet( leActiveRaceCarIndex )");

        return mShowtimeBits.IsBitSet(leActiveRaceCarIndex);
    }

    // @0x8276C730  Is the active car on the start line?
    bool RaceCarAIInterface::IsActiveRaceCarOnStartLine(EActiveRaceCarIndex leActiveRaceCarIndex) const
    {
        CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                   "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
        CGS_ASSERT(mSetActiveRaceCars.IsBitSet(leActiveRaceCarIndex),
                   "mSetActiveRaceCars.IsBitSet( leActiveRaceCarIndex )");

        return mOnStartLineBits.IsBitSet(leActiveRaceCarIndex);
    }

    // @0x8276C960  Is the active car drifting?
    bool RaceCarAIInterface::IsActiveRaceCarDrifting(EActiveRaceCarIndex leActiveRaceCarIndex) const
    {
        CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                   "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
        CGS_ASSERT(mSetActiveRaceCars.IsBitSet(leActiveRaceCarIndex),
                   "mSetActiveRaceCars.IsBitSet( leActiveRaceCarIndex )");

        return mDriftingBits.IsBitSet(leActiveRaceCarIndex);
    }

    // @0x8276CB90  Is the active car touching another car?
    bool RaceCarAIInterface::IsActiveRaceCarTouchingAnother(EActiveRaceCarIndex leActiveRaceCarIndex) const
    {
        CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                   "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
        CGS_ASSERT(mSetActiveRaceCars.IsBitSet(leActiveRaceCarIndex),
                   "mSetActiveRaceCars.IsBitSet( leActiveRaceCarIndex )");

        return mRaceCarContactBits.IsBitSet(leActiveRaceCarIndex);
    }

    // @0x8276CDC0  Is the active car touching the player?
    bool RaceCarAIInterface::IsActiveRaceCarTouchingPlayer(EActiveRaceCarIndex leActiveRaceCarIndex) const
    {
        CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                   "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
        CGS_ASSERT(mSetActiveRaceCars.IsBitSet(leActiveRaceCarIndex),
                   "mSetActiveRaceCars.IsBitSet( leActiveRaceCarIndex )");

        return mPlayerContactBits.IsBitSet(leActiveRaceCarIndex);
    }
}
}
