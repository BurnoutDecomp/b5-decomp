// =================================================================================================
// BrnWorld::RaceCarEntityModule -- the tailgate predicate and the near-miss tick.
//
//   RaceCarEntityModule::IsPlayerCarTailgatingOtherRaceCars
//   RaceCarEntityModule::UpdateNearMisses
//   RaceCarEntityModule::UpdateTrafficAndRaceCarNearMisses
//
// Two halves of the same boost-credit chain, neither of which had a body anywhere in the tree.
//
// IsPlayerCarTailgatingOtherRaceCars is called once per pre-physics frame by UpdateTailgateTimer,
// whose answer UpdateBoost feeds to BoostStrategy::SetTailgating; without it the tailgate duration
// never left 0.0f and meIndexOfCarPlayerIsTailgating never left INVALID, so sitting on a rival's
// bumper earned nothing.
//
// UpdateNearMisses is the consumer half of the fine crashing-traffic feed that
// PhysicalTrafficManager::PassNearbyCrashingTrafficIdsToRaceCarModule fills every frame, and the
// only driver of NearMissManager::Update: without it the near-miss / crash-escape chain never
// aged, never fired and never posted its chain game events.
//
// UpdateTrafficAndRaceCarNearMisses is the other half of that chain and the sole caller of
// NearMissManager::AddNearTraffic / AddNearRaceCar: it drains the traffic module's two proximity
// collections off the post-scene input buffer into the near lists the tick above matches its
// remembered crashes against.
// =================================================================================================
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnActiveRaceCar.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"  // InputBuffer_PostPhysics / OutputBuffer_PostPhysics accessors
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"         // VehicleManagerOutputInterface::FineTrafficCrashedEventQueue
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"                  // TrafficCrashedEvent / RaceCarState
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"                      // VolumeInstanceId packed-field accessors
#include "GameSource/BurnoutConstants.h"                                                  // EActiveRaceCarIndex + its range-guarded operator++
#include "GameShared/GameClasses/Core/CgsAssert.h"                                        // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"                                                // Dot / Normalize / operator-
#include <cmath>                                                                          // std::cos (the console cosines the cone angle per candidate)

namespace BrnWorld
{

namespace
{
    // ---- the tailgate cone. Every value is read out of the decrypted image at the symbol the
    // console loads; nothing here is inferred and nothing is a flagged zero.

    // The reference point is pushed this far along the OTHER car's heading before the
    // separation is taken.
    const f32 KF_TAILGATE_REFERENCE_OFFSET = 2.0f;

    // In RADIANS (19.8 degrees). The console splats the angle and cosines it inside the loop,
    // then demands the separation-versus-heading cosine be at least that, so it is kept as the
    // angle and cosined per candidate rather than folded to a constant.
    const f32 KF_TAILGATE_CONE_HALF_ANGLE_RADIANS = 0.346f;

    // Bound on the separation resolved ALONG the other car's reversed heading; both sides are
    // squared, so the sign of the resolved distance does not matter.
    const f32 KF_TAILGATE_ALONG_HEADING_RANGE = 20.0f;

    // Compared against RaceCarState::mfSpeedMPH, so it is miles per hour.
    const f32 KF_TAILGATE_MIN_SPEED_MPH = 30.0f;

    // This constant reads 0x00000000 straight out of the image -- it is a CRT-initialised slot,
    // not a zero. Its initialiser thunk computes 0.44704 (the mph -> m/s factor) * 90.0, so it
    // is 90 mph in m/s: the bound on the magnitude of the velocity DIFFERENCE between the two
    // cars, with both sides squared.
    const f32 KF_TAILGATE_RELATIVE_SPEED = 40.2336f;

    // The miles-per-hour to metres-per-second factor. The near-miss manager is fed the player's
    // speed in m/s, not mph.
    const f32 KF_MPH_TO_METRES_PER_SECOND = 0.44704f;

    // An ordinary rodata zero: a crashing player loses the chain timeout outright.
    const f32 KF_NEAR_MISS_TIMEOUT_WHEN_CRASHING = 0.0f;
}

// =================================================================================================
// IsPlayerCarTailgatingOtherRaceCars
//
// lpPlayerActiveRaceCar is the BASE of the active-car array, not the player's slot: the console
// forms the player's slot from it with the array stride, and walks the same base around the loop.
// UpdateTailgateTimer duly passes GetActiveRaceCar(E_ACTIVE_RACE_CAR_INDEX_0). The declaration's
// parameter name is left as it stands.
//
// The predicate, arm for arm, for every slot that is not the player's and is active:
//   1. the reference point is the other car's position pushed 2 m along its own heading;
//   2. the separation (player position - that point) resolved along the other car's REVERSED
//      heading must be within 20 m;
//   3. the unit separation must sit inside a 0.346 rad cone about that reversed heading;
//   4. the other car must be doing at least 30 mph;
//   5. the two cars' linear velocities must be within 90 mph of each other.
// The first slot that passes all five latches its index into meIndexOfCarPlayerIsTailgating and
// returns true; if none does, the member is reset to INVALID and the answer is false.
//
// Both velocity reads are the PHYSICS snapshot (RaceCarState::mLinearVelocity, the console's
// direct load off the slot at +0x410 == mPhysicsState +0x330), not ActiveRaceCar::GetVelocity(),
// which forwards to the paired global slot.
// =================================================================================================
bool RaceCarEntityModule::IsPlayerCarTailgatingOtherRaceCars(
        EActiveRaceCarIndex lePlayerActiveRaceCarIndex,
        const ActiveRaceCar* lpPlayerActiveRaceCar )
{
    const ActiveRaceCar* lpPlayerCar =
            lpPlayerActiveRaceCar + static_cast<s32>( lePlayerActiveRaceCarIndex );

    const Vector3 lPlayerPosition = lpPlayerCar->GetPosition();

    CGS_ASSERT( lpPlayerCar->IsAttached(), "IsAttached()" );

    const Vector3 lPlayerVelocity = lpPlayerCar->GetPhysicsState()->mLinearVelocity;

    for( EActiveRaceCarIndex leIndex = E_ACTIVE_RACE_CAR_INDEX_0;
         leIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT;
         leIndex++ )
    {
        if( leIndex == lePlayerActiveRaceCarIndex )
        {
            continue;
        }

        const ActiveRaceCar* lpOtherCar =
                lpPlayerActiveRaceCar + static_cast<s32>( leIndex );

        if( !lpOtherCar->IsActive() )
        {
            continue;
        }

        const Vector3 lOtherDirection = lpOtherCar->GetDirection();
        const Vector3 lOtherPosition  = lpOtherCar->GetPosition();

        const Vector3 lReversedHeading = -lOtherDirection;
        const Vector3 lReferencePoint  =
                lOtherDirection * KF_TAILGATE_REFERENCE_OFFSET + lOtherPosition;
        const Vector3 lSeparation      = lPlayerPosition - lReferencePoint;

        // Written as the same `>` the console's skip branch is, so an unordered compare falls
        // through instead of skipping. Do not rewrite this as a negated `<=`.
        const f32 lfAlongHeading = rw::math::vpu::Dot( lReversedHeading, lSeparation );
        if( lfAlongHeading * lfAlongHeading
                > KF_TAILGATE_ALONG_HEADING_RANGE * KF_TAILGATE_ALONG_HEADING_RANGE )
        {
            continue;
        }

        const f32 lfConeCosine = rw::math::vpu::Dot(
                rw::math::vpu::Normalize( lSeparation ), lReversedHeading );

        if( std::cos( KF_TAILGATE_CONE_HALF_ANGLE_RADIANS ) > lfConeCosine )
        {
            continue;
        }

        const BrnPhysics::Vehicle::RaceCarState* lpOtherPhysicsState =
                lpOtherCar->GetPhysicsState();

        if( lpOtherPhysicsState->mfSpeedMPH < KF_TAILGATE_MIN_SPEED_MPH )
        {
            continue;
        }

        const Vector3 lVelocityDifference =
                lpOtherPhysicsState->mLinearVelocity - lPlayerVelocity;
        const f32 lfVelocityDifferenceSquared =
                rw::math::vpu::Dot( lVelocityDifference, lVelocityDifference );

        if( lfVelocityDifferenceSquared
                <= KF_TAILGATE_RELATIVE_SPEED * KF_TAILGATE_RELATIVE_SPEED )
        {
            meIndexOfCarPlayerIsTailgating = leIndex;
            return true;
        }
    }

    meIndexOfCarPlayerIsTailgating = E_ACTIVE_RACE_CAR_INDEX_INVALID;
    return false;
}

// =================================================================================================
// UpdateNearMisses
//
// One frame of near-miss bookkeeping:
//   * publish the player's speed to the manager in m/s and mirror the player's crashing flag onto
//     it; a crashing player also drops the chain timeout to zero and raises the failed-chain flag,
//     which is what makes a crash break a near-miss chain;
//   * remember every OTHER active race car that is crashing, so a pass by a car that has just
//     crashed scores as the crash-escape variant;
//   * drain the vehicle manager's 10-slot fine crashing-traffic queue into the traffic half;
//   * step NearMissManager::Update on the output buffer's game-event queue and this module's own
//     boost manager.
//
// The console reads the vehicle-manager output interface off the post-physics INPUT buffer and
// folds the queue accessor to a bare displacement of 752 -- the same seat the producer appends to.
//
// WARNING -- THE TRAFFIC DRAIN READS THE FIELD THE PRODUCER ZEROES, AND THAT IS THE CONSOLE'S OWN
// BEHAVIOUR, REPRODUCED RATHER THAN REPAIRED. The console copies each 16-byte TrafficCrashedEvent
// to the stack and then extracts the 14-bit entity index out of the FIRST field, i.e. out of
// mTrafficVolumeInstanceID's embedded entity id -- while its own producer,
// PhysicalTrafficManager::PassNearbyCrashingTrafficIdsToRaceCarModule, stores a ZERO
// volume-instance id there and puts the real traffic entity id in the SECOND field. So the id
// logged here is 0 on the shipped build, and the traffic half of the chain only ever remembers
// entity 0 as "recently crashed". Do not "fix" this by reading mCrasherEntityID instead: that is
// a different game. Both sides were re-derived from the image for this wave and agree.
// =================================================================================================
void RaceCarEntityModule::UpdateNearMisses(
        RaceCarEntityModuleIO::InputBuffer_PostPhysics* lpInput,
        RaceCarEntityModuleIO::OutputBuffer_PostPhysics* lpOutput )
{
    CGS_ASSERT( static_cast<u32>( mePlayerActiveRaceCarIndex ) < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                "( mePlayerActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID ) && "
                "( mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT )" );

    ActiveRaceCar* lpPlayerCar = GetActiveRaceCar( mePlayerActiveRaceCarIndex );

    CGS_ASSERT( lpPlayerCar->IsAttached(), "IsAttached()" );

    mNearMissManager.SetSpeed(
            lpPlayerCar->GetPhysicsState()->mfSpeedMPH * KF_MPH_TO_METRES_PER_SECOND );

    const bool lbPlayerCrashing = lpPlayerCar->GetPhysicsState()->mbCrashing;
    mNearMissManager.SetCrashing( lbPlayerCrashing );

    if( lbPlayerCrashing )
    {
        mNearMissManager.SetNearMissTimeout( KF_NEAR_MISS_TIMEOUT_WHEN_CRASHING );
        mNearMissManager.SetFailedNearMissChain( true );
    }

    for( EActiveRaceCarIndex leIndex = E_ACTIVE_RACE_CAR_INDEX_0;
         leIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT;
         leIndex++ )
    {
        ActiveRaceCar* lpOtherCar = GetActiveRaceCar( leIndex );

        // The console tests IsActive BEFORE the player-slot comparison here, the reverse of the
        // tailgate loop's order. Both are side-effect free, but keep the order as written.
        if( !lpOtherCar->IsActive() || leIndex == mePlayerActiveRaceCarIndex )
        {
            continue;
        }

        CGS_ASSERT( lpOtherCar->IsAttached(), "IsAttached()" );

        if( lpOtherCar->GetPhysicsState()->mbCrashing )
        {
            mNearMissManager.GetRaceCarNearMissData().AddCrashed(
                    static_cast<u32>( leIndex ) );
        }
    }

    const BrnPhysics::Vehicle::VehicleManagerOutputInterface::FineTrafficCrashedEventQueue&
            lrFineCrashedQueue =
                lpInput->GetVehicleManagerOutputInterface()->GetFineTrafficCrashedEventQueue();

    for( s32 liEvent = 0; liEvent < lrFineCrashedQueue.GetLength(); ++liEvent )
    {
        // The console copies the whole 16-byte record before reading a field out of it.
        const BrnPhysics::Vehicle::TrafficCrashedEvent loEvent =
                lrFineCrashedQueue.GetEvent( liEvent );

        mNearMissManager.GetTrafficNearMissData().AddCrashed(
                loEvent.mTrafficVolumeInstanceID.GetEntityIDEntityIndex() );
    }

    mNearMissManager.Update( mfTimeStep, lpOutput->GetGameEventQueue(), &mBoostManager );
}

// =================================================================================================
// RaceCarEntityModule::UpdateTrafficAndRaceCarNearMisses -- THE PRODUCER OF BOTH NEAR LISTS.
//
// This is the only caller of NearMissManager::AddNearTraffic / AddNearRaceCar anywhere in the
// game, so until it landed the near-miss tick above aged, snapshotted and remembered crashes
// correctly but could never FIRE: a near miss is a remembered CRASHED id that was also on the
// NEAR list, and the near lists had no writer.
//
// It is not a detector. The proximity work is the TRAFFIC module's: its scene-query pass fills
// the two Array<NearMissData,N> collections inside the traffic->race-car pre-scene interface,
// which the post-scene input buffer carries. This body just drains them, once per post-scene
// tick, and re-fetches the interface separately for each half exactly as the console does (the
// accessor carries the buffer's locked-for-reading assert, so the second fetch is not dead).
//
// The traffic half is ALSO the power-parking near-traffic tally, counted only when power parking
// can be scored -- outside a game mode, or in the online free-burn lobby. That flag is computed
// once, ahead of both drains, and re-tested per record.
//
// ⛔ MEASURED, AND IT IS WHY NOTHING FIRES YET: the collections' producer,
// TrafficEntityModule::ProcessNearbyTrafficSceneQueryResults, has no body anywhere in this tree
// (it is the gate that TU's own banner already names). Both collections are Construct'd, so they
// read length 0 every frame and this drain is a well-formed no-op until that gate closes. That
// is honest state -- the pipe is complete on this side.
// =================================================================================================
void RaceCarEntityModule::UpdateTrafficAndRaceCarNearMisses(
        RaceCarEntityModuleIO::InputBuffer_PostScene* lpInput )
{
    typedef RaceCarEntityModuleIO::InputBuffer_PostScene::TrafficToRaceCarInterface_PreScene
            TrafficInterface;

    const TrafficInterface::NearMissTrafficCollection* lpNearMissTrafficCollection =
            lpInput->GetTrafficToRaceCarInterface_PreScene()->GetNearMissTrafficCollection();

    CGS_ASSERT( lpNearMissTrafficCollection != 0, "lpNearMissTrafficCollection != NULL" );

    const bool lbCountForPowerParking =
            !mbIsInGameMode ||
            meGameModeType == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY;

    for( u32 luIndex = 0; luIndex < lpNearMissTrafficCollection->GetLength(); ++luIndex )
    {
        mNearMissManager.AddNearTraffic( ( *lpNearMissTrafficCollection )[ luIndex ].muCarId );

        if( lbCountForPowerParking )
        {
            ++miPowerParkingNearTrafficCount;
        }
    }

    const TrafficInterface::NearMissRaceCarCollection* lpNearMissRaceCarCollection =
            lpInput->GetTrafficToRaceCarInterface_PreScene()->GetNearMissRaceCarCollection();

    CGS_ASSERT( lpNearMissRaceCarCollection != 0, "lpNearMissRaceCarCollection != NULL" );

    for( u32 luIndex = 0; luIndex < lpNearMissRaceCarCollection->GetLength(); ++luIndex )
    {
        mNearMissManager.AddNearRaceCar( ( *lpNearMissRaceCarCollection )[ luIndex ].muCarId );
    }
}

}   // namespace BrnWorld
