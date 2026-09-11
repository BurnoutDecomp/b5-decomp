// ============================================================================
// BrnTrafficEntityModule_wG_NearbyTrafficResults.cpp
//
// TrafficEntityModule::ProcessNearbyTrafficSceneQueryResults -- the post-physics drain of
// the player-centred coarse sphere query PostNearbyTrafficSceneQueryRequest posts.
//
// One matching result batch carries the entity ids the sphere swept up. Per traffic id the
// drain publishes a traffic->director record and a traffic->sound record; per traffic AND
// race-car id it records a near-miss candidate. It is the SOLE producer of the module's two
// near-miss collections.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicleTypeRuntime.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficDirectorInterfaces.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficSoundInterfaces.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficToRaceCarInterface.h"
#include "GameSource/World/BrnEntityTypes.h"                                       // EEntityTypeID
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"           // RaceCarState
#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO.h"                 // OutCoarseQueryResult
#include "rw/math/vpu/vector3_operation.h"                                  // Dot, Magnitude
#include <cmath>                                                                   // std::fabs

namespace BrnTraffic
{

namespace
{
    // The query id PostNearbyTrafficSceneQueryRequest stamps into its sphere test. Result
    // batches carrying anything else belong to another producer and are skipped whole.
    const u32 KU_NEARBY_TRAFFIC_SPHERE_QUERY_ID = 99u;

    // The player has to be moving for a pass to count as a near miss at all.
    const f32 KF_NEAR_MISS_MIN_PLAYER_SPEED_MPH = 30.0f;
    // ...and has to pass this close. Traffic and race cars carry different bounds.
    const f32 KF_NEAR_MISS_MAX_TRAFFIC_DISTANCE  = 5.0f;
    const f32 KF_NEAR_MISS_MAX_RACE_CAR_DISTANCE = 10.0f;

    // Showtime keeps at most this many traffic horns sounding at once, and only for cars
    // inside a flat disc about the player's ground position. The radius is carried squared.
    const s32 KI_MAX_HOOTING_VEHICLES           = 10;
    const f32 KF_SHOWTIME_HORN_RADIUS_SQUARED   = 6400.0f;
    const f32 KF_SHOWTIME_HORN_MAX_HEIGHT_DELTA = 4.0f;

    // The per-lane tolerance the two "this position is not the origin" tripwires below use.
    const f32 KF_POSITION_ZERO_TOLERANCE = 1.1920929e-07f;

    // The vector zero test both tripwires expand inline. rw::math::vpu::IsZero is declared in
    // the vendor math home but has no body in this tree, so the lane comparison is written
    // out here rather than left as an unresolved call. The ship's fourth lane is a copy of
    // the first, so only x/y/z are distinct.
    bool IsZeroPosition( const Vector3& lrPosition )
    {
        return std::fabs( lrPosition.x ) <= KF_POSITION_ZERO_TOLERANCE
            && std::fabs( lrPosition.y ) <= KF_POSITION_ZERO_TOLERANCE
            && std::fabs( lrPosition.z ) <= KF_POSITION_ZERO_TOLERANCE;
    }
}

// ----------------------------------------------------------------------------
// ProcessNearbyTrafficSceneQueryResults
//
// ⭐ THE ONLY WRITER OF mNearMissTrafficCollection / mNearMissRaceCarCollection. With it
// absent both collections read empty for the whole session, so the race-car module's drain
// (UpdateTrafficAndRaceCarNearMisses) and NearMissManager's tick ran on empty lists and no
// near miss could ever fire.
//
// Asserts that belong to an accessor are left where they belong: the "IsAlive()" tripwires
// the ship inlines ahead of GetVehicleType / GetSpeed / IsHornOn / IsAlarmOn live in those
// accessors, and the "luIndex < KU_MAX_TOTAL_TRAFFIC" bounds live in GetVehicle /
// GetVehicleTransform / GetVehicleSpecies. Only the ones this function owns are written here.
// ----------------------------------------------------------------------------
void TrafficEntityModule::ProcessNearbyTrafficSceneQueryResults(
        const BrnTrafficIO::InputBuffer_PostPhysics* lpInput,
        BrnTrafficIO::OutputBuffer_PostPhysics*      lpOutput )
{
    CGS_ASSERT( lpOutput != NULL, "lpOutput != NULL" );                      // baked line 14212

    const BrnTrafficIO::InputBuffer_PostPhysics::SceneResultQueue* lpResultQueue =
        lpInput->GetSceneResultQueue();

    if ( lpResultQueue->GetLength() == 0 )
    {
        return;
    }

    // The hooting tally spans the WHOLE drain: it is set up once, before the first batch, and
    // is never reset per batch or per result.
    s32 liHootingVehicles = 0;

    const CgsModule::Event* lpEvent     = NULL;
    s32                     liEventSize = 0;
    lpResultQueue->GetFirstEvent( &lpEvent, &liEventSize );

    while ( lpEvent != NULL )
    {
        const CgsSceneManager::SceneManagerIO::OutCoarseQueryResult* lpResults =
            static_cast<const CgsSceneManager::SceneManagerIO::OutCoarseQueryResult*>( lpEvent );

        if ( lpResults->mQueryId.mId == KU_NEARBY_TRAFFIC_SPHERE_QUERY_ID )
        {
            const Vector3 lPlayerPosition =
                lpInput->GetActiveRaceCarOutputInterface()->GetPlayerPosition();

            const EActiveRaceCarIndex lePlayerCar =
                lpInput->GetActiveRaceCarOutputInterface()->GetPlayerActiveRaceCarIndex();

            const f32 lfPlayerSpeedMPH =
                lpInput->GetActiveRaceCarOutputInterface()
                       ->GetRaceCarState( lePlayerCar )->mfSpeedMPH;

            // Both collections are rebuilt from scratch out of THIS batch.
            mNearMissTrafficCollection.Clear();
            mNearMissRaceCarCollection.Clear();

            for ( s32 liResult = 0; liResult < lpResults->miNumResults; ++liResult )
            {
                const CgsSceneManager::EntityId lEntityId = lpResults->GetEntityIds()[ liResult ];
                const u32                       luIndex   = lEntityId.GetEntityIndex();

                if ( lEntityId.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE )
                {
                    CGS_ASSERT( luIndex < KU_MAX_TOTAL_TRAFFIC,
                                "lEntityId.GetEntityIndex() < KU_MAX_TOTAL_TRAFFIC" );  // baked line 14255

                    // The duplicate tripwire walks the rest of the batch. ⚠️ Its bound is the
                    // ATTEMPTED count, not the WRITTEN one -- the producer keeps the two equal,
                    // and this is the bound the ship uses.
                    for ( s32 liOther = liResult + 1;
                          liOther < lpResults->miNumResultsAttempted;
                          ++liOther )
                    {
                        CGS_ASSERT( static_cast<u32>( lpResults->GetEntityIds()[ liOther ] )
                                        != static_cast<u32>( lEntityId ),
                                    "Found duplicate traffic entity in scene query result" ); // baked line 14261
                    }

                    const Vehicle* lpVehicle = GetVehicle( luIndex );

                    if ( lpVehicle->IsAlive() )
                    {
                        const Matrix44Affine lTransform = GetVehicleTransform( luIndex );

                        CGS_ASSERT( lpVehicle->HasEntity(), "lpVehicle->HasEntity()" ); // baked line 14273

                        const u32              luVehicleType = lpVehicle->GetVehicleType();
                        const VehicleTypeData* lpVehicleTypeData =
                            &mpData->mpaVehicleTypes[ luVehicleType ];
                        const CgsID lVehicleId =
                            mpData->mpaVehicleAssets[ lpVehicleTypeData->muAssetId ].GetVehicleId();

                        const VehicleTypeRuntime* lpVehicleTypeRuntime =
                            GetVehicleTypeRuntime( luVehicleType );
                        const Vector3 lBBoxOffset   = lpVehicleTypeRuntime->GetBBoxOffset();
                        const Vector3 lBBoxHalfSize = lpVehicleTypeRuntime->GetBBoxHalfSize();

                        // The director is handed the BOX centre in world space, not the
                        // vehicle origin; the rest of the transform rides through unchanged.
                        BrnTrafficIO::TrafficDirectorEntity lDirectorEntity;
                        lDirectorEntity.mLocalTransform       = lTransform;
                        lDirectorEntity.mLocalTransform.Pos() = lTransform.Pos()
                                                              + lTransform.Right() * lBBoxOffset.x
                                                              + lTransform.Up()    * lBBoxOffset.y
                                                              + lTransform.At()    * lBBoxOffset.z;
                        lDirectorEntity.mVelocity       = lTransform.At() * lpVehicle->GetSpeed().x;
                        lDirectorEntity.mHalfExtents    = lBBoxHalfSize;
                        lDirectorEntity.mVehicleId      = lVehicleId;
                        lDirectorEntity.mu16EntityIndex = static_cast<u16>( luIndex );

                        Array<BrnTrafficIO::TrafficDirectorEntity, 32u>& lrDirectorEntities =
                            lpOutput->GetTrafficDirectorOutputInterface()
                                    ->GetTrafficDirectorEntityArray();

                        if ( !lrDirectorEntities.IsFull() )
                        {
                            lrDirectorEntities.Append( lDirectorEntity );
                        }

                        // The sound system gets everything EXCEPT the trailer slot.
                        if ( GetVehicleSpecies( luIndex ) != Vehicle::E_SPECIES_TRAILER )
                        {
                            CGS_ASSERT( lpVehicle->HasEntity(), "lpVehicle->HasEntity()" ); // baked line 14298

                            const u8 lu8VehicleClass =
                                mpData->mpaVehicleTypes[ lpVehicle->GetVehicleType() ].muVehicleClass;

                            bool lbIsEngineOn = true;
                            bool lbIsHooting  = lpVehicle->IsHornOn();
                            u8   lu8AlarmType = BrnTrafficIO::TrafficSoundEntity::E_ALARM_NONE;

                            if ( lpVehicle->IsOfStaticSpecies() )
                            {
                                // A parked car has no engine running and never sounds its horn
                                // as a horn. What it can do is set an alarm off, and the two
                                // alarm flavours alternate by vehicle index so a street of
                                // parked cars does not all wail the same way.
                                lbIsEngineOn = false;
                                lbIsHooting  = false;

                                if ( lpVehicle->IsAlarmOn() )
                                {
                                    if ( ( luIndex & 1 ) != 0 )
                                    {
                                        lbIsHooting  = lpVehicle->IsHornOn();
                                        lu8AlarmType = BrnTrafficIO::TrafficSoundEntity::E_ALARM_HORN;
                                    }
                                    else
                                    {
                                        lu8AlarmType = BrnTrafficIO::TrafficSoundEntity::E_ALARM_CLASSIC;
                                    }
                                }
                            }

                            if ( mbPlayingShowtimeMode && lbIsHooting )
                            {
                                if ( liHootingVehicles >= KI_MAX_HOOTING_VEHICLES )
                                {
                                    lbIsHooting = false;
                                }
                                else
                                {
                                    const Vector3 lToShowtimePlayer =
                                        lTransform.Pos() - mShowtimePlayerGroundPos;

                                    if ( rw::math::vpu::Dot( lToShowtimePlayer, lToShowtimePlayer )
                                             > KF_SHOWTIME_HORN_RADIUS_SQUARED
                                         || std::fabs( lToShowtimePlayer.y )
                                             > KF_SHOWTIME_HORN_MAX_HEIGHT_DELTA )
                                    {
                                        lbIsHooting = false;
                                    }
                                }
                            }

                            if ( lbIsHooting )
                            {
                                ++liHootingVehicles;
                            }

                            const bool lbIsPhysical = lpVehicle->IsPhysical();

                            BrnTrafficIO::TrafficSoundEntity lSoundEntity;
                            lSoundEntity.mLocalTransform   = lTransform;
                            lSoundEntity.mEntityId.muValue = static_cast<u32>( lEntityId );
                            lSoundEntity.mfSpeed           = lpVehicle->GetSpeed().x;
                            lSoundEntity.mu16EntityIndex   = static_cast<u16>( luIndex );
                            lSoundEntity.muVehicleClass    = lu8VehicleClass;
                            lSoundEntity.mbIsEngineOn      = lbIsEngineOn;
                            lSoundEntity.mbIsHooting       = lbIsHooting;
                            lSoundEntity.mbIsCrashed       = lpVehicle->IsCrashing();
                            lSoundEntity.mbIsPhysical      = lbIsPhysical;
                            lSoundEntity.muAlarmType       = lu8AlarmType;

                            lpOutput->GetTrafficSoundOutputInterface()->AddTrafficEntity( lSoundEntity );
                        }
                    }

                    // ⚠️ OUTSIDE the IsAlive() arm, deliberately: the near-miss leg runs for
                    // every traffic id the query returned, alive or not, and re-reads the
                    // transform rather than reusing the one the alive arm loaded.
                    if ( lfPlayerSpeedMPH > KF_NEAR_MISS_MIN_PLAYER_SPEED_MPH )
                    {
                        const f32 lfDistance = rw::math::vpu::Magnitude(
                            GetVehicleTransform( luIndex ).Pos() - lPlayerPosition );

                        if ( lfDistance <= KF_NEAR_MISS_MAX_TRAFFIC_DISTANCE )
                        {
                            BrnTrafficIO::NearMissData lNearMiss;
                            lNearMiss.muCarId    = luIndex;
                            lNearMiss.mfDistance = lfDistance;

                            if ( !mNearMissTrafficCollection.IsFull() )
                            {
                                mNearMissTrafficCollection.Append( lNearMiss );
                            }
                        }
                    }
                }
                else if ( lEntityId.GetOwner() == BrnWorld::E_ENTITYTYPE_RACECAR
                          && lfPlayerSpeedMPH > KF_NEAR_MISS_MIN_PLAYER_SPEED_MPH )
                {
                    CGS_ASSERT( !IsZeroPosition( lPlayerPosition ),
                                "!RwMathVPU::IsZero(lPlayerPosition)" );        // baked line 14402

                    const Vector3 lActiveRaceCarPosition =
                        lpInput->GetVehicleOutputInterface()->GetRaceCar( luIndex )->mTransform.Pos();

                    CGS_ASSERT( !IsZeroPosition( lActiveRaceCarPosition ),
                                "!RwMathVPU::IsZero(lActiveRaceCarPosition)" ); // baked line 14405

                    // The player's own slot is not a near miss with itself.
                    if ( static_cast<u16>( lePlayerCar ) != static_cast<u16>( luIndex ) )
                    {
                        const f32 lfDistance = rw::math::vpu::Magnitude(
                            lPlayerPosition - lActiveRaceCarPosition );

                        if ( lfDistance <= KF_NEAR_MISS_MAX_RACE_CAR_DISTANCE )
                        {
                            BrnTrafficIO::NearMissData lNearMiss;
                            lNearMiss.muCarId    = luIndex;
                            lNearMiss.mfDistance = lfDistance;

                            if ( !mNearMissRaceCarCollection.IsFull() )
                            {
                                mNearMissRaceCarCollection.Append( lNearMiss );
                            }
                        }
                    }
                }
            }
        }

        lpResultQueue->GetNextEvent( lpEvent, &lpEvent, &liEventSize );
    }
}

}
