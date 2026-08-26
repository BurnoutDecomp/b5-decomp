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
// ⛔⛔ MEASURED THE DAY THEY LANDED, AND SAY IT BEFORE ANYONE READS "LANDED" AS "WORKING":
// THE RING IS STILL EMPTY, because the ABOVE-GROUND RAY NEVER HITS. A booted drive run
// (asserts=0, phase=DRIVING) prints, on every sample:
//     [collision-tag] car 0 aboveGroundValid=0 tag=0x-32768 section=32767
//                            wheel0OnGround=1 wheel0Tag=0x-23520 timeInAir=0.000000
//     [rot-ring] player depth=0 aiSection=32767 inSystem=0 resetPos=(3018.9,-9.2,-354.9)
// 0xFFFF8000 is the CLEAR value AboveGroundTestResult::Reset writes; mbValid never goes true.
// THE PRODUCER IS ABSENT: BrnPhysics::Vehicle::VehicleManager::GenerateAboveGroundLineTests
// @0x82633990 (via PhysicsModule::GenerateSceneQueries @0x825A1428, from WorldModule::Update) is
// the ONLY thing in the whole image that posts a race car's downward InEventLineTestNearest, and
// nothing in this tree posts one. The RESULT half of that round trip is entirely live already
// (WorldBridgeSceneToPhysics' case 2 -> VehicleInputInterface::AddLineTestResult ->
// VehicleManager::ProcessAboveGroundLineTestsResults -> SimpleVehiclePhysics::
// SetAboveGroundTestResult, all bodied), so this is a one-function hole, not a subsystem.
// ⭐ The wheel contacts on the SAME frames are real and drivable, which is what proves the
// diagnostic is reading live physics rather than an unpopulated struct.
// DELETE-WHEN GenerateAboveGroundLineTests lands and [rot-ring] reports a non-zero depth.
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

// =================================================================================================
// UpdateRaceCarCollisionTagging @0x822D2280   -- A MINIMAL-COMPLETE SLICE
//
//   0x822D2280  three asserts: index >= 0 (:5775), index < KI_MAX_ACTIVE_RACE_CARS (:5776),
//               lpRaceCarState (:5777)
//   0x822D2328  if (!lpRaceCarState->mAboveGroundTestResult.mbValid)   return   (lbz  a3+488)
//   0x822D2330  tag = lpRaceCarState->mAboveGroundTestResult.mCollisionTag      (lwz  a3+484)
//   0x822D2338  if (index != mePlayerActiveRaceCarIndex) goto SET_SECTION       (lwz this+99064)
//               ---- the PLAYER-ONLY wrong-way / oncoming arm (PARKED, see below) ----
//   0x822D2364  heading = atan2(mTransform.zAxis.z (a3+536), mTransform.zAxis.x (a3+528))
//               if (heading < 0) heading += 2*PI                          (flt_82CDB634)
//   0x822D2388  switch (CollisionTag::GetTrafficInfo(&tag, &lfLaneAngle)) ...
//               ... a three-state latch over module+100120 / module+100124 with a 0.75 s
//               dwell, ending in `(*(vtbl(mBoostManager's BoostStrategy*) + 152))(strategy, code)`
//               with code 0 (oncoming) / 1 (normal) / 2 (unknown)
//   SET_SECTION:
//   0x822D2478  ActiveRaceCar::SetAISection( GetActiveRaceCar(index), HIWORD(tag) & 0x7FFF )
//
// ⭐ THE SET_SECTION CALL IS UNCONDITIONAL GIVEN mbValid -- FOR THE PLAYER TOO. Every one of the
// five player-arm paths ends in `goto LABEL_17` (the virtual dispatch) and LABEL_17 falls straight
// into it. That is what makes the park below safe: the leg this wave needs runs for the player
// exactly as the console runs it, and only the oncoming bookkeeping is missing.
//
// ⛔ [FLAG PC bring-up] THE PLAYER-ONLY WRONG-WAY / ONCOMING ARM IS PARKED, LOUDLY.
// Three separate reasons, and none of them is "it looked hard":
//   (a) it dispatches through the BoostStrategy* the boost manager owns at module+97504 with a
//       vtable slot index (152/4 == 38) -- a NUMERIC module-vtable index, which this tree does not
//       take (see the vtable slot-0 Create shim bug: the tree's own slot order differs).
//   (b) its two latch members live at module+100120 / module+100124 and have NO named member in
//       BrnRaceCarEntityModule.h's tail pad. Minting members from raw offsets is the exact
//       live-corruption class this project keeps paying for.
//   (c) CollisionTag::GetTrafficInfo is a BrnWorld::CollisionTag method and the tag arriving here
//       is the placeholder type (see the fork note above); routing one into the other needs the
//       fork retired first, which is its own change.
// What is LOST by the park: the "driving into oncoming traffic" boost-earning state. It is a boost
// bonus, not a safety property, and nothing on the reset-on-track path reads it.
// DELETE-WHEN the ::CollisionTag / BrnWorld::CollisionTag fork is retired and the two oncoming
// latch members are named.
// =================================================================================================
void RaceCarEntityModule::UpdateRaceCarCollisionTagging(
        s32 liActiveRaceCarIndex,
        const BrnPhysics::Vehicle::RaceCarState* lpRaceCarState )
{
    CGS_ASSERT( liActiveRaceCarIndex >= 0, "liActiveRaceCarIndex >= 0" );                    // :5775
    CGS_ASSERT( liActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                "liActiveRaceCarIndex < KI_MAX_ACTIVE_RACE_CARS" );                          // :5776
    CGS_ASSERT( lpRaceCarState != 0, "lpRaceCarState" );                                     // :5777

    if( lpRaceCarState == 0 )
    {
        return;
    }

    // ⭐⭐ [DIAG collision-tag] NOT IN THE X360 BINARY. THE WITNESS THAT OBSERVES THE RIGHT
    // BRANCH. The gate below is an EARLY RETURN, so a silent "nothing happened" is
    // indistinguishable from "this function never ran" -- print BEFORE it, on both outcomes,
    // and print the raw tag so a valid-but-sectionless surface is distinguishable from an
    // invalid ray. Rate-limited to one line per 512 calls.
    if( ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0 && CgsDev::Log::gpDebugPrint != 0 )
    {
        static s32 siTagTick = 0;
        if( ( siTagTick++ & 0x1FF ) == 0 )
        {
            const ::CollisionTag& lrTag = lpRaceCarState->mAboveGroundTestResult.mCollisionTag;
            *CgsDev::Log::gpDebugPrint
                << "[collision-tag] car " << liActiveRaceCarIndex
                << " aboveGroundValid=" << ( lpRaceCarState->mAboveGroundTestResult.mbValid ? 1 : 0 )
                << " tag=0x" << static_cast<s32>( lrTag.muValue )
                << " section=" << static_cast<s32>( TagAISectionIndex( lrTag ) )
                << " wheel0OnGround="
                << ( lpRaceCarState->maWheels[0].mRoadContact.mbIsOnGround ? 1 : 0 )
                << " wheel0Tag=0x"
                << static_cast<s32>( lpRaceCarState->maWheels[0].mRoadContact.mCollisionTag.muValue )
                << " timeInAir=" << lpRaceCarState->mfTimeInAir << "\n";
        }
    }

    // The console's own first gate: no valid ground under the car means no tag to read, and it
    // returns WITHOUT touching the section (so the last known section survives one bad frame).
    if( !lpRaceCarState->mAboveGroundTestResult.mbValid )
    {
        return;
    }

    if( liActiveRaceCarIndex == static_cast<s32>( mePlayerActiveRaceCarIndex ) )
    {
        // ⛔ [FLAG PC bring-up] the wrong-way / oncoming-traffic arm -- see the banner.
        static bool sbReportedParkedOncomingArm = false;
        if( !sbReportedParkedOncomingArm )
        {
            sbReportedParkedOncomingArm = true;
            if( ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0
                && CgsDev::Log::gpDebugPrint != 0 )
            {
                *CgsDev::Log::gpDebugPrint
                    << "[collision-tag] PARKED player-only leg: the wrong-way / oncoming-traffic "
                       "state machine of RaceCarEntityModule::UpdateRaceCarCollisionTagging "
                       "(X360 0x822D2280) is NOT reconstructed -- oncoming boost earning will not "
                       "update. The AI-section store below IS live.\n";
            }
        }
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

    // ⭐⭐ [DIAG rot-ring] NOT IN THE X360 BINARY. THE CONTROL FOR "THE RING IS REAL".
    //
    // A ring that is being Push()ed is not the same claim as a ring that HOLDS A GOOD RESET POSE,
    // and the difference is invisible from a depth counter alone: an empty ring makes
    // GetResetCoords silently fall back to the car's LIVE transform, which for a crashed car is
    // the wreck's own pose -- a "recovery" that puts the car back exactly where it is stuck.
    // ⇒ this prints the depth AND the oldest entry's position, so a later crash-recovery run can
    // be checked against a position that was recorded BEFORE the crash rather than assumed.
    // Rate-limited to one line per 256 calls so a 275 s drive run yields a readable handful.
    if( ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0 && CgsDev::Log::gpDebugPrint != 0 )
    {
        static s32 siTick = 0;
        if( ( siTick++ & 0xFF ) == 0 )
        {
            const ActiveRaceCar* lpPlayer =
                GetActiveRaceCar( mePlayerActiveRaceCarIndex );
            if( lpPlayer != 0 && lpPlayer->IsActive() )
            {
                Vector3 lPosition    = { 0.0f, 0.0f, 0.0f, 0.0f };
                Vector3 lDirection   = { 0.0f, 0.0f, 0.0f, 0.0f };
                lpPlayer->GetResetCoords( &lPosition, &lDirection );
                *CgsDev::Log::gpDebugPrint
                    << "[rot-ring] player depth=" << lpPlayer->GetResetTransformCount()
                    << " aiSection=" << static_cast<s32>( lpPlayer->GetCurrentAISection() )
                    << " inSystem=" << ( lpPlayer->IsInsideAISectionSystem() ? 1 : 0 )
                    << " resetPos=(" << lPosition.x << "," << lPosition.y << "," << lPosition.z
                    << ")\n";
            }
        }
    }
}

}
