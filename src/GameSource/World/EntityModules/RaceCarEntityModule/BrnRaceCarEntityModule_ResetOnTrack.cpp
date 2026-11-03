// =================================================================================================
// GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule_ResetOnTrack.cpp
// (aicar_reset wave, 2026-08-26)
//
// The two per-frame drivers that make the RESET-ON-TRACK RING real on this build:
//   RaceCarEntityModule::UpdateRaceCarCollisionTagging @0x822D2280 (159 insns, a SLICE)
//   RaceCarEntityModule::UpdateActiveRaceCarTransforms @0x822CF1A0 (a bare 0..7 loop)
//
// DWARF home is BrnRaceCarEntityModule.cpp; split out for the same reason
// BrnRaceCarEntityModule_CrashExit.cpp is (that TU is 5000+ lines and carries the whole entity
// module). DELETE-WHEN the home TU can absorb them.
//
// =================================================================================================
// ⭐⭐⭐ WHY THESE TWO, AND WHY NOW
// =================================================================================================
// The crash exit lands RaceCar::mbToBeResetOnTrack and everything below
// ActiveRaceCar::RequestPlaceOnTrack is already live. The missing capability is "CHOOSE WHERE TO
// PUT THE CAR". The AI road-network answer to that question is ~6,000 lines of ResetOnTrackManager
// + the AI-car feed that keeps it fed -- not one wave. But the console ALSO ships a second answer,
// on the SAME code path, for exactly the case where the AI has none:
// ResetOnTrackManager::ProcessResetOnTrackRequest posts a FAILURE result, and
// RaceCarEntityModule::ProcessResetOnTrackResultQueue's failure arm calls
// ActiveRaceCar::GetResetCoords -- which reads ActiveRaceCar::mPrevTransforms, a four-deep ring
// of "the last places I was genuinely on the road".
//
// That ring has never held anything on this build, because its ONLY writer
// (ActiveRaceCar::UpdateResetTransform) had no caller AND its inner gate reads muCurrAISection,
// which nothing in this tree had ever set to anything but 0x7FFF. These two functions are that
// caller and that setter.
//
// The above-ground line-test producer and result consumer are now live. Their
// packed group/material tags also supply the player's oncoming boost classification.
//
// ⭐ THE SURFACE COLLISION TAG *IS* THE AI SECTION INDEX -- no road network required.
// BrnWorld::CollisionTag is {u16 mu16GroupTag; u16 mu16MaterialTag} and its GetAISectionIndex() is
// `mu16GroupTag & KU_MAX_AI_SECTION_INDEX`. Every above-ground ray hit therefore carries the AI
// section the car is standing on, straight out of the world's own collision surfaces. The console
// takes it here (`SetAISection(car, HIWORD(tag) & 0x7FFF)`) and nowhere else.
// =================================================================================================

#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnActiveRaceCar.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"   // RaceCarState
#include <cmath>
#include <cstdlib>
#include "SharedClasses/World/BrnCollisionTag.h"                            // KU_MAX_AI_SECTION_INDEX
#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                  // gpDebugPrint

namespace BrnWorld
{

namespace
{
    // BrnWorld::CollisionTag::GetAISectionIndex() on the PLACEHOLDER tag type.
    //
    // ⚠️ TYPE FORK, PRE-EXISTING AND FLAGGED WHERE IT LIVES (BrnVehicleManager_TractionLineTests
    // .cpp:317): AboveGroundTestResult::mCollisionTag is `::CollisionTag { u32 muValue; }`
    // (BrnCommonTypes.h:29), NOT BrnWorld::CollisionTag, so the field is reached by SHIFT on the
    // u32 -- the idiom VehiclePhysics::GetSurfaceLinearDrag and the traction-line harvest already
    // use. The console reads `HIWORD(tag) & 0x7FFF`, i.e. the +0x24 halfword (the GROUP tag) --
    // which SimpleVehiclePhysics packs into the HIGH 16 bits of muValue.
    // ⚠️ `::CollisionTag` is spelled EXPLICITLY GLOBAL: unqualified inside namespace BrnWorld it
    // would bind to BrnWorld::CollisionTag, a different four bytes with the halves the other way
    // round. [[shadowing redeclarations]] in miniature.
    inline u16 TagAISectionIndex( const ::CollisionTag& lrTag )
    {
        return static_cast<u16>( ( lrTag.muValue >> 16 ) & BrnWorld::KU_MAX_AI_SECTION_INDEX );
    }
}

// ARTIST 0x822D2280: ground tags drive both the reset section and player oncoming boost.
// UNKNOWN lane direction preserves the previous state; NO_LANES expires it after 0.75s.
void RaceCarEntityModule::UpdateRaceCarCollisionTagging(
        s32 liActiveRaceCarIndex,
        const BrnPhysics::Vehicle::RaceCarState* lpRaceCarState )
{
    CGS_ASSERT( liActiveRaceCarIndex >= 0, "liActiveRaceCarIndex >= 0" );                    // :5775
    CGS_ASSERT( liActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                "liActiveRaceCarIndex < KI_MAX_ACTIVE_RACE_CARS" );                          // :5776
    CGS_ASSERT( lpRaceCarState != 0, "lpRaceCarState" );                                     // :5777

    // The console's own first gate: no valid ground under the car means no tag to read, and it
    // returns WITHOUT touching the section (so the last known section survives one bad frame).
    if( !lpRaceCarState->mAboveGroundTestResult.mbValid )
    {
        return;
    }

    if( liActiveRaceCarIndex == static_cast<s32>( mePlayerActiveRaceCarIndex ) )
    {
        const u32 luPackedTag = lpRaceCarState->mAboveGroundTestResult.mCollisionTag.muValue;
        BrnWorld::CollisionTag lTag;
        lTag.Construct(static_cast<u16>(luPackedTag >> 16), static_cast<u16>(luPackedTag));
        f32 lfHeading = std::atan2(lpRaceCarState->mTransform.zAxis.GetZ().GetFloat(),
                                    lpRaceCarState->mTransform.zAxis.GetX().GetFloat());
        if (lfHeading < 0.0f) lfHeading += 6.2831855f;
        f32 lfLaneAngle = 0.0f;
        const TrafficDirection leTraffic = lTag.GetTrafficInfo(&lfLaneAngle);
        BoostStrategy* lpStrategy = mBoostManager.GetBoostStrategy();
        OncomingState leState = E_ONCOMING_STATE_FALSE;
        if (leTraffic == E_TRAFFIC_DIRECTION_VALID)
        {
            const f32 lfAngleRatio = std::fabs(lfLaneAngle - lfHeading) * 0.15915494f;
            if (lfAngleRatio > 0.4f && lfAngleRatio < 0.6f) leState = E_ONCOMING_STATE_TRUE;
            mbOncomingTimerActive = false;
            mfOncomingNoClueTimer = 0.0f;
        }
        else if (leTraffic == E_TRAFFIC_DIRECTION_UNKNOWN)
        {
            mbOncomingTimerActive = false;
            mfOncomingNoClueTimer = 0.0f;
            leState = E_ONCOMING_STATE_PREVIOUS;
        }
        else if (lpStrategy->GetPreviousOncomingState() != E_ONCOMING_STATE_FALSE)
        {
            mfOncomingNoClueTimer += mfTimeStep;
            if (!mbOncomingTimerActive)
            {
                mbOncomingTimerActive = true;
                leState = E_ONCOMING_STATE_PREVIOUS;
            }
            else if (mfOncomingNoClueTimer <= 0.75f)
                leState = E_ONCOMING_STATE_PREVIOUS;
            else
            {
                mbOncomingTimerActive = false;
                mfOncomingNoClueTimer = 0.0f;
            }
        }
        lpStrategy->SetOncomingState(leState);

    }

    ActiveRaceCar* lpActiveRaceCar =
        GetActiveRaceCar( static_cast<EActiveRaceCarIndex>( liActiveRaceCarIndex ) );

    lpActiveRaceCar->SetAISection(
        TagAISectionIndex( lpRaceCarState->mAboveGroundTestResult.mCollisionTag ) );
}

// =================================================================================================
// UpdateActiveRaceCarTransforms @0x822CF1A0
//
//   for (i = 0; i < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++i)
//       ActiveRaceCar::UpdateResetTransform( GetActiveRaceCar(i) );
//
// The console's loop carries the usual `leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT`
// post-increment assert (BurnoutConstants.h:39); it is the enum operator++'s, not this function's,
// and a plain `< COUNT` loop cannot trip it. Written as the plain loop.
//
// ⭐ NO IsActive() TEST HERE -- UpdateResetTransform does its own, first thing. Faithful.
// =================================================================================================
void RaceCarEntityModule::UpdateActiveRaceCarTransforms()
{
    for( s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar )
    {
        GetActiveRaceCar( static_cast<EActiveRaceCarIndex>( liCar ) )->UpdateResetTransform();
    }


}

}
