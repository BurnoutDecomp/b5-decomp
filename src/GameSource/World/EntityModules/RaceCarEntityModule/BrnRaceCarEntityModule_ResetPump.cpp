// =================================================================================================
// BrnRaceCarEntityModule_ResetPump.cpp -- the RACE-CAR MODULE's half of the reset-on-track round
// trip (resetpump wave, 2026-08-26).
//
//   RaceCarEntityModule::WriteUpdatedAIData             @0x822D1FC8  (172)   PreScene
//   RaceCarEntityModule::SendResetOnTrackRequests       @0x822CE178  (57)    PostScene
//   RaceCarEntityModule::ProcessResetOnTrackResultQueue @0x822F4580  (192)   PrePhysics
//
// ⭐⭐⭐ WHY THESE THREE, AND WHY THEY CLOSE A CAMPAIGN
// The crash exit has run end to end since 2026-08-25: a heavy crash opens a RaceCarCrash, ticks
// its cleanup timer, posts RaceCarCrashCompleteEvent and delivers it to
// ProcessRaceCarCrashCompleteEvents -- which, because the event arrives with mbCrashing == 1,
// takes RaceCar::RequestResetOnTrack and sets mbToBeResetOnTrack. NOTHING IN THIS TREE HAS EVER
// READ THAT FLAG. SendResetOnTrackRequests below is the only function in the whole image that
// does, and the two siblings are the rest of the loop it closes:
//
//   [PreScene ] WriteUpdatedAIData        -> OutputBuffer_PreScene::mRaceCarAIInterface
//               ...also sets mbPlayerDataSet, and AIModule::Update @0x8279B478 wraps its ENTIRE
//               body in that one flag, so without this function the AI module is a no-op.
//   [bridge   ] BridgeRaceCarModuleToAIModule_PreScene   (SetRaceCarAIInterface)
//   [PostScene] SendResetOnTrackRequests  -> OutputBuffer_PostScene::mAIModuleRequestInterface
//   [bridge   ] BridgeRaceCarModuleToAIModule_PostScene  (AppendAIModuleRequestInterface)
//   [AI       ] AIModule::ProcessRequestInterface -> ResetOnTrackManager::PushResetOnTrackRequest
//               AIModule::UpdateResetOnTrackManager -> ResetOnTrackManager::Update
//                 -> ProcessResetOnTrackRequest -> a ResetOnTrackResult on the AI result ring
//   [bridge   ] BridgeAIToEntityModules_PrePhysics       (SetAIModuleResultInterface)
//   [PrePhys  ] ProcessResetOnTrackResultQueue -> ActiveRaceCar::RequestPlaceOnTrack
//               -> PlaceOnTrackManager::PrePhysicsUpdate -> RCEM::ResetActiveRaceCar (all REAL)
//
// Every hop happens inside ONE WorldModule::Update: the post-scene spine runs at
// BrnWorldModule.cpp:2665, the AI input bridges at :2694..:2700, AIModule::Update at :2715, the
// AI -> entity-module bridge at :2755 and RCEM::PrePhysicsUpdate at :2769.
//
// ⭐⭐ AND THE RECOVERY DOES **NOT** DEPEND ON THE AI ROAD NETWORK.
// ResetOnTrackManager::ProcessResetOnTrackRequest resolves every request on this build to
// ResetOnTrackResult::E_STATE_FAILURE (its ComputeInitialCoordinatesStandard refuses at the
// console's own `if (!IsActive()) return false`, because nothing on this build activates an
// AICar). THE FAILURE ARM IS THE CONSOLE'S DESIGNED FALLBACK, not a hole: the consumer below
// answers a FAILURE by calling ActiveRaceCar::GetResetCoords, whose empty-ring arm (asm
// 0x822BF37C) hands back the car's LIVE transform. So the pump recovers the car with the AI
// placement still parked; what the AI would add is a BETTER pose, not the only pose.
//
// ⛔ NOT LANDED HERE, and named so the next wave does not re-derive it:
//   * RCEM::CheckForResetOnTrackConditions @0x822CE9E0 -- the OTHER producer of
//     mbToBeResetOnTrack (the "you have been stuck/off-road too long" watchdog). Absent; it
//     reaches the module's un-homed timer interior. Its absence costs nothing on the crash path.
//   * the console's `if (!(lUpdateSet & 1))` guard around the PostScene call site -- see
//     PostSceneUpdate's own slice banner.
// =================================================================================================

#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"
#include "GameSource/World/AI/SharedIO/BrnRaceCarAIInterfaces.h"      // RaceCarAIInterface
#include "GameSource/World/AI/SharedIO/BrnAIModuleRequestInterface.h" // ResetOnTrackRequest
#include "GameSource/World/AI/SharedIO/BrnAIModuleResultInterface.h"  // ResetOnTrackResult / PlaceOnTrackRequest
#include "SharedClasses/World/BrnCollisionTag.h"                      // BrnWorld::KI_INVALID_SECTION_INDEX (witness)
#include "GameSource/World/AI/BrnAISharedConstants.h"                 // BrnAI::EResetType
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h" // Vehicle::RaceCarState

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

namespace BrnWorld
{

namespace
{
    // flt_82F31928, READ OUT OF THE IMAGE (0x3EE4E26D). The console scales the physics
    // snapshot's mfSpeedMPH by it before publishing the AI's per-car speed: 0.44704 is
    // exactly miles-per-hour -> metres-per-second, so the AI's mafSpeeds[] is in m/s.
    const f32 KF_MPH_TO_METRES_PER_SECOND = 0.44704f;

    // flt_82FAD610. ProcessResetOnTrackResultQueue's FAILURE arm clamps the requested reset
    // speed to this: `fsubs f12, speed, K ; fsel f1, f12, K, speed` == min(speed, K).
    //
    // ⚠️ FLAGGED, NOT GUESSED. The symbol lives in BSS and READS 0.0 out of the decrypted
    // ARTIST image, and a whole-image scan of the export set finds EXACTLY ONE reference to
    // it -- this read. So no function in the image writes it, and 0.0f is the value the
    // shipped binary runs with. Effect: a car recovered through the FAILURE arm is placed
    // STATIONARY, which is also what the arm means (the AI found no on-track pose, so the car
    // is put back where it last was, at rest). If a data-driven writer is ever found, this
    // becomes a real tunable.
    const f32 KF_FAILURE_RESET_SPEED_CAP = 0.0f;

    // The console's `li r5, 0x25 ; li r6, 1` game event: type 37, payload size 1 -- i.e. an
    // EMPTY CgsModule::Event derivative (sizeof(empty struct) == 1), raised on the frame the
    // PLAYER's car is placed on track. ⚠️ The symbolic name is NOT recovered: no enum in the
    // DecFIGS dump or in this tree gives 37 a name in the race-car module's game-event space.
    // Quoted as the console's literal, with the console's size.
    const s32 KI_GAME_EVENT_PLAYER_PLACED_ON_TRACK = 37;
}

// =================================================================================================
// WriteUpdatedAIData @0x822D1FC8
//
// Console order (r28 == this, r19 == the interface, r22 == the loop index):
//   0x822D1FEC  ai = lpOutput->GetRaceCarAIInterface()          (write-locked accessor)
//   0x822D1FFC  if (mePlayerActiveRaceCarIndex == -1) return    (the whole body is inside this)
//   0x822D2020  assert(mePlayerActiveRaceCarIndex >= 0)                  (:5533)
//   0x822D2044  car = GetActiveRaceCar(mePlayerActiveRaceCarIndex)
//   0x822D2080  ai->SetPlayerActiveRaceCarData(car->GetPosition(), car->GetDirection(), index)
//   0x822D20C0  for (i = 0; i < 8; ++i)                                  (BurnoutConstants.h:39)
//   0x822D20D0    if (!GetActiveRaceCar(i)->IsActive()) continue
//   0x822D20E4    assert(IsAttached())                          (BrnActiveRaceCar.h:1096)
//   0x822D2110    assert(&mPhysicsState != NULL)                         (:5553)
//   0x822D2130    showtime = (i == mePlayerActiveRaceCarIndex) && mbIsInShowtimeMode
//   0x822D2158    inAir    = mPhysicsState.mfTimeInAir > 0.0f            (phys+0x404)
//   0x822D216C    drifting = mPhysicsState.mfTimeDrifting > 0.0f         (car +0x4E0)
//   0x822D2170    touchPlayer / touchCar / frontRayOccluded              (+0x773/+0x772/+0x530)
//   0x822D21B8    onStartLine = (meRaceStartState == 0)                  (+0x77C)
//   0x822D2230    ai->UpdateActiveRaceCarData(...)
//
// ⚠️ THE ARGUMENT ORDER OF THE EIGHT BOOLS IS THE ONE THING HERE THAT NOTHING BUT THE ASM
// PINS -- see the banner on the declaration in BrnRaceCarAIInterfaces.h. The caller's five
// stack slots are at r1+0x6F/0x77/0x7F/0x87/0x8F and IDA prints them as var_101/var_F9/var_F1/
// var_E9/var_E1, i.e. IN DESCENDING NAME ORDER. Read that list top-to-bottom as ascending and
// "touching player" lands in the on-start-line slot, silently.
// =================================================================================================
void RaceCarEntityModule::WriteUpdatedAIData( RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput )
{
    CGS_ASSERT( lpOutput != 0, "lpOutput != NULL" );
    if( lpOutput == 0 )
    {
        return;
    }

    BrnAI::AIModuleIO::RaceCarAIInterface* lpAI = lpOutput->GetRaceCarAIInterface();

    if( mePlayerActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID )
    {
        return;
    }
    CGS_ASSERT( mePlayerActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                "mePlayerActiveRaceCarIndex >= 0" );   // X360 :5533

    {
        const ActiveRaceCar* lpPlayerCar = GetActiveRaceCar( mePlayerActiveRaceCarIndex );

        // [FLAG PC bring-up] the bail is the deviation. The console reads the player slot's
        // pose unconditionally here, behind ActiveRaceCar::GetPosition's own IsAttached
        // assert. On this build mePlayerActiveRaceCarIndex is published by the junkyard
        // hand-off and the slot is attached by then -- but a PC assert BLOCKS the game until
        // END, so an unattached slot would be a hang rather than a dropped frame.
        if( !lpPlayerCar->IsAttached() )
        {
            static bool sbReportedUnattachedPlayer = false;
            if( !sbReportedUnattachedPlayer && CgsDev::Log::gpDebugPrint != 0 )
            {
                sbReportedUnattachedPlayer = true;
                *CgsDev::Log::gpDebugPrint
                    << "[resetpump] WriteUpdatedAIData: mePlayerActiveRaceCarIndex is "
                    << static_cast<s32>( mePlayerActiveRaceCarIndex )
                    << " but that slot is not attached -- skipping this frame's AI publish "
                       "(the console would assert here) [FLAG]\n";
            }
            return;
        }

        lpAI->SetPlayerActiveRaceCarData( lpPlayerCar->GetPosition(),
                                          lpPlayerCar->GetDirection(),
                                          mePlayerActiveRaceCarIndex );
    }

    for( s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar )
    {
        const EActiveRaceCarIndex leCar = static_cast<EActiveRaceCarIndex>( liCar );
        const ActiveRaceCar* lpCar = GetActiveRaceCar( leCar );

        if( !lpCar->IsActive() )
        {
            continue;
        }

        CGS_ASSERT( lpCar->IsAttached(), "IsAttached()" );   // BrnActiveRaceCar.h:1096
        if( !lpCar->IsAttached() )
        {
            continue;
        }

        const BrnPhysics::Vehicle::RaceCarState* lpPhysicsState = lpCar->GetPhysicsState();
        CGS_ASSERT( lpPhysicsState != 0, "lpPhysicsState" );   // X360 :5553

        const bool lbIsInShowtime =
            ( leCar == mePlayerActiveRaceCarIndex ) && mbIsInShowtimeMode;

        // =========================================================================================
        // [FLAG PC witness] NOT IN THE X360 BINARY. THE PRODUCER END OF THE RIVAL ROUTE PATH.
        //
        // `lpCar->GetCurrentAISection()` (ActiveRaceCar +0x73E) is the ONLY thing that tells the AI
        // which road a car is on: it becomes RaceCarAIInterface::mauSectionIndices[slot], which
        // AIModule::StoreDrivenCarData @0x827957F0 hands to AICar::UpdateInRangeData @0x82792A18 as
        // the under-car section (PPC r7), which stores it straight into AICar::muBestSectionIndex
        // @+0x1534 -- and RouteRequestManager::Update @0x82797FA8 refuses to issue ANY route request
        // for a car whose best AND default section are both 0x7FFF. So a rival stuck on 0x7FFF here
        // can never get a road, and nothing downstream would say so.
        //
        // In run6 this was only inferable from the [collision-tag] diagnostic, which is throttled to
        // one line per 512 calls and therefore sampled two of the six cars. One line per slot when
        // it first publishes, plus one whenever a slot's section crosses the valid/invalid boundary,
        // makes the claim direct and bounded. DELETE-WHEN rivals are seen driving their own routes.
        // =========================================================================================
        {
            const u16 lu16Section = lpCar->GetCurrentAISection();

            static s32 saiSectionWitness[E_ACTIVE_RACE_CAR_INDEX_COUNT] = { 0 };
            static bool sabWasValid[E_ACTIVE_RACE_CAR_INDEX_COUNT]      = { false };
            static bool sabSeen[E_ACTIVE_RACE_CAR_INDEX_COUNT]          = { false };
            static s32  siSectionWitnessTotal                           = 0;

            const bool lbValid   = ( lu16Section != BrnWorld::KI_INVALID_SECTION_INDEX );
            const bool lbFlipped = sabSeen[ liCar ] && ( lbValid != sabWasValid[ liCar ] );

            if( ( saiSectionWitness[ liCar ] < 2 || lbFlipped )
                && siSectionWitnessTotal < 48
                && CgsDev::Log::gpDebugPrint != 0 )
            {
                ++saiSectionWitness[ liCar ];
                ++siSectionWitnessTotal;
                *CgsDev::Log::gpDebugPrint
                    << "[route-src] active slot " << liCar
                    << ( ( leCar == mePlayerActiveRaceCarIndex ) ? " (PLAYER)" : " (rival)" )
                    << " publishes AI section " << static_cast<s32>( lu16Section )
                    << ( lbValid ? " VALID" : " INVALID(0x7FFF) -- this car can never be routed" )
                    << " insideSectionSystem " << ( lpCar->IsInsideAISectionSystem() ? 1 : 0 )
                    << " [FLAG PC witness]\n";
            }

            sabSeen[ liCar ]     = true;
            sabWasValid[ liCar ] = lbValid;
        }

        lpAI->UpdateActiveRaceCarData(
            leCar,
            lpPhysicsState->mTransform,                                  // phys +0x1F0
            lpPhysicsState->mLinearVelocity,                             // phys +0x330 (v1)
            lpPhysicsState->mfSpeedMPH * KF_MPH_TO_METRES_PER_SECOND,    // phys +0x3CC * 0.44704
            lpCar->GetCurrentAISection(),                                // car  +0x73E
            lpPhysicsState->mfTimeInAir    > 0.0f,                       // phys +0x404
            lpPhysicsState->mbCrashing,                                  // phys +0x44A
            lbIsInShowtime,
            lpCar->IsOnStartLine(),                                      // car  +0x77C == 0
            lpPhysicsState->mfTimeDrifting > 0.0f,                       // phys +0x400
            lpPhysicsState->mbIsFrontRayOccluded,                        // phys +0x450
            lpCar->IsTouchingAnotherRaceCar(),                           // car  +0x772
            lpCar->IsTouchingPlayer() );                                 // car  +0x773
    }
}

// =================================================================================================
// SendResetOnTrackRequests @0x822CE178
//
//   0x822CE18C  requests = lpOutput->GetAIModuleRequestInterface()
//   0x822CE1B8  for (i = 0; i < 35; ++i)                        (BurnoutConstants.h:84)
//   0x822CE1C8    assert(car->muType < E_RACE_CAR_TYPE_COUNT)   (BrnRaceCar.h:482)
//   0x822CE1EC    if (car->muType == E_RACE_CAR_TYPE_INACTIVE) continue
//   0x822CE1F8    if (!car->mbToBeResetOnTrack) continue
//   0x822CE208    ResetOnTrackRequest::Construct(&req, i, car->mfResetOnTrackSpeed(+0x84),
//                                                car->mfResetOnTrackDistance(+0x88),
//                                                car->meResetOnTrackType(+0x8C))
//   0x822CE224    requests->mResetOnTrackRequestQueue.AddEvent(req)
//
// ⚠️ THE FLAG IS **NOT** CLEARED HERE. The console leaves mbToBeResetOnTrack set and clears it
// on the RESULT side (ProcessResetOnTrackResultQueue's `stb r19, 0x90(r31)`), so a request that
// the AI never answers is re-sent every frame until it is. Reproduced as-is: clearing it here
// would silently turn a retry into a single shot.
// =================================================================================================
void RaceCarEntityModule::SendResetOnTrackRequests(
        RaceCarEntityModuleIO::OutputBuffer_PostScene* lpOutput )
{
    CGS_ASSERT( lpOutput != 0, "lpOutput != NULL" );
    if( lpOutput == 0 )
    {
        return;
    }

    BrnAI::AIModuleIO::AIModuleRequestInterface* lpRequests =
        lpOutput->GetAIModuleRequestInterface();

    for( s32 liCar = 0; liCar < E_GLOBAL_RACE_CAR_INDEX_COUNT; ++liCar )
    {
        const EGlobalRaceCarIndex leCar = static_cast<EGlobalRaceCarIndex>( liCar );
        RaceCar* lpRaceCar = GetGlobalRaceCar( leCar );

        CGS_ASSERT( lpRaceCar->GetType() < E_RACE_CAR_TYPE_COUNT,
                    "muType < E_RACE_CAR_TYPE_COUNT" );   // BrnRaceCar.h:482

        if( !lpRaceCar->IsInWorld() || !lpRaceCar->ToBeResetOnTrack() )
        {
            continue;
        }

        BrnAI::AIModuleIO::ResetOnTrackRequest lRequest;
        lRequest.Construct( leCar,
                            lpRaceCar->GetResetOnTrackSpeed(),
                            lpRaceCar->GetResetOnTrackDist(),
                            lpRaceCar->GetResetOnTrackType() );
        lpRequests->GetResetOnTrackRequestQueue()->AddEvent( lRequest );

        {
            // [DIAG resetpump] NOT IN THE X360 BINARY. The witness that separates "the crash
            // exit raised the flag" from "a request left the race-car module", which are
            // different claims and the whole reason this wave exists. One line per request.
            if( CgsDev::Log::gpDebugPrint != 0 )
            {
                *CgsDev::Log::gpDebugPrint
                    << "[resetpump] request SENT: global car " << liCar
                    << " type " << static_cast<s32>( lpRaceCar->GetResetOnTrackType() )
                    << " speed " << lpRaceCar->GetResetOnTrackSpeed()
                    << " dist " << lpRaceCar->GetResetOnTrackDist() << "\n";
            }
        }
    }
}

// =================================================================================================
// ProcessResetOnTrackResultQueue @0x822F4580 -- the CONSUMER, both rings.
//
// ---- ring 1: the reset-on-track RESULTS (0x822F45CC..0x822F4730) -------------------------------
//   results = lpInput->GetAIModuleResultInterface()          (0x822B5800, this+196656)
//   for (i = 0; i < results->mResetPosResultEventQueue.miLength; ++i)
//     r   = queue.GetEvent(i)
//     car = GetGlobalRaceCar(r.meGlobalRaceCarIndex)          (r +0x24)
//     car->mbToBeResetOnTrack = false                         (stb 0x90 -- ⭐ ALWAYS, even when
//                                                              the car is not placeable)
//     assert(car->muType < E_RACE_CAR_TYPE_COUNT)
//     if (car->muType == E_RACE_CAR_TYPE_INACTIVE || car->mpActiveRaceCar == 0) continue
//     ac = car->GetActiveRaceCar()
//     if (r.meState == E_STATE_SUCCESS)                       (r +0x20 == 0)
//         ac->RequestPlaceOnTrack(r.mResetPosition, r.mResetDirection, r.mfResetSpeed)
//     else
//         ac->GetResetCoords(&pos, &dir)                      (r4 == pos, r5 == dir)
//         ac->RequestPlaceOnTrack(pos, dir, min(r.mfResetSpeed, flt_82FAD610))
//     if (ac == GetActiveRaceCar(mePlayerActiveRaceCarIndex))
//         lpOutput->GetGameEventQueue()->AddEvent(<empty>, 37, 1)
//         lpOutput->GetPlayerResetInterface()->SetPlayerResetPos(ac->mPlaceOnTrackPosition)
//
// ⭐ THE ARGUMENT ORDER OF GetResetCoords IS LOAD-BEARING AND THE ASM IS THE ONLY WITNESS:
// `addi r5, r1, var_B0 ; addi r4, r1, var_A0` -- r4 (var_A0) is the POSITION out-parameter and
// r5 (var_B0) the DIRECTION, and the RequestPlaceOnTrack that follows loads v1 from var_A0 and
// v2 from var_B0. Swap them and the car is placed at its own facing vector, i.e. within a metre
// or two of the world origin, which is exactly the failure four earlier briefs PREDICTED for
// this arm for an unrelated (and now retracted) reason.
//
// ⭐ AND THE FAILURE ARM IS NOT AN ERROR PATH. On this build EVERY result is a FAILURE (see the
// file banner), so this is the arm that actually recovers the car.
//
// ---- ring 2: the place-on-track REQUESTS (0x822F4734..0x822F486C) ------------------------------
//   The same shape over results->mPlaceOnTrackRequestQueue (the console reaches it as
//   `interface + 0x1810`, which is exactly sizeof(EventQueue<ResetOnTrackResult,128>) ==
//   0x10 + 128*48 -- i.e. the SECOND member, by name here). No GetResetCoords arm: a
//   place-on-track request always carries its own pose. The console prints
//   "Calling RequestPlaceOnTrack\n" through the dev log on every one; kept.
// =================================================================================================
void RaceCarEntityModule::ProcessResetOnTrackResultQueue(
        const RaceCarEntityModuleIO::InputBuffer_PrePhysics* lpInput,
        RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpOutput )
{
    CGS_ASSERT( lpInput != 0, "lpInput != NULL" );   // X360 :1539
    if( lpInput == 0 || lpOutput == 0 )
    {
        return;
    }

    const BrnAI::AIModuleIO::AIModuleResultInterface* lpResults =
        lpInput->GetAIModuleResultInterface();
    if( lpResults == 0 )
    {
        return;
    }

    // ---- ring 1: reset-on-track results -----------------------------------------------------
    {
        const BrnAI::AIModuleIO::AIModuleResultInterface::ResetOnTrackResultQueue* lpQueue =
            lpResults->GetResetOnTrackResultQueue();

        for( s32 liEvent = 0; liEvent < lpQueue->GetLength(); ++liEvent )
        {
            const BrnAI::AIModuleIO::ResetOnTrackResult& lrResult = lpQueue->GetEvent( liEvent );

            RaceCar* lpRaceCar = GetGlobalRaceCar( lrResult.GetGlobalRaceCarIndex() );

            // The console clears the pending flag FIRST and unconditionally: the request has
            // been answered whether or not the car can be placed.
            lpRaceCar->ClearResetOnTrack();

            CGS_ASSERT( lpRaceCar->GetType() < E_RACE_CAR_TYPE_COUNT,
                        "muType < E_RACE_CAR_TYPE_COUNT" );   // BrnRaceCar.h:482

            if( !lpRaceCar->IsInWorld() || !lpRaceCar->HasActiveRaceCar() )
            {
                continue;
            }

            ActiveRaceCar* lpActiveRaceCar = lpRaceCar->GetActiveRaceCar();
            if( lpActiveRaceCar == 0 )
            {
                continue;
            }

            // The pose that is actually handed to RequestPlaceOnTrack, latched so the witness
            // below can print IT rather than re-deriving something adjacent to it.
            Vector3 lAppliedPosition;
            Vector3 lAppliedDirection;
            f32     lfAppliedSpeed;

            if( lrResult.GetState() == BrnAI::AIModuleIO::ResetOnTrackResult::E_STATE_SUCCESS )
            {
                lAppliedPosition  = lrResult.GetResetPosition();
                lAppliedDirection = lrResult.GetResetDirection();
                lfAppliedSpeed    = lrResult.GetResetSpeed();

                lpActiveRaceCar->RequestPlaceOnTrack( lAppliedPosition,
                                                      lAppliedDirection,
                                                      lfAppliedSpeed );
            }
            else
            {
                lpActiveRaceCar->GetResetCoords( &lAppliedPosition, &lAppliedDirection );

                lfAppliedSpeed = ( lrResult.GetResetSpeed() >= KF_FAILURE_RESET_SPEED_CAP )
                                     ? KF_FAILURE_RESET_SPEED_CAP
                                     : lrResult.GetResetSpeed();

                lpActiveRaceCar->RequestPlaceOnTrack( lAppliedPosition, lAppliedDirection,
                                                      lfAppliedSpeed );
            }

            if( CgsDev::Log::gpDebugPrint != 0 )
            {
                // [DIAG resetpump] NOT IN THE X360 BINARY -- the other end of the witness pair
                // opened by SendResetOnTrackRequests. Prints the POSE, because "a request came
                // back" and "the car was told to go somewhere sane" are different claims.
                //
                // ⚠️ CORRECTED 2026-09-03 (aiwave A11). This used to print
                // ActiveRaceCar::GetResetCoords ON BOTH ARMS -- i.e. on a SUCCESS it printed the
                // car's OWN last-good pose while announcing "SUCCESS (AI pose)", so the one line
                // that exists to tell the two apart printed the same thing either way. It could
                // not have been noticed before this wave because nothing had ever produced a
                // SUCCESS. It now prints the pose that was actually applied.
                *CgsDev::Log::gpDebugPrint
                    << "[resetpump] RESULT applied: global car "
                    << static_cast<s32>( lrResult.GetGlobalRaceCarIndex() )
                    << ( lrResult.GetState() ==
                             BrnAI::AIModuleIO::ResetOnTrackResult::E_STATE_SUCCESS
                             ? " SUCCESS (AI pose)" : " FAILURE (own reset coords)" )
                    << " -> (" << lAppliedPosition.x << "," << lAppliedPosition.y
                    << "," << lAppliedPosition.z << ") facing ("
                    << lAppliedDirection.x << "," << lAppliedDirection.y
                    << "," << lAppliedDirection.z << ") speed " << lfAppliedSpeed << "\n";
            }

            if( mePlayerActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID &&
                lpActiveRaceCar == GetActiveRaceCar( mePlayerActiveRaceCarIndex ) )
            {
                // The console's payload-less game event: `li r5, 0x25 ; li r6, 1` on a stack
                // slot it never writes (an empty CgsModule::Event derivative is one byte). A
                // zero byte is passed here rather than an uninitialised one -- the consumer
                // reads the TYPE, not the payload, and an uninitialised byte on a shared queue
                // is how a plausible wrong value gets read as data later.
                const u8 lu8EmptyEvent = 0;
                lpOutput->GetGameEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>( &lu8EmptyEvent ),
                    KI_GAME_EVENT_PLAYER_PLACED_ON_TRACK, 1 );

                lpOutput->GetPlayerResetInterface()->SetPlayerResetPos(
                    lpActiveRaceCar->GetPlaceOnTrackPosition() );
            }
        }
    }

    // ---- ring 2: place-on-track requests -----------------------------------------------------
    {
        const BrnAI::AIModuleIO::AIModuleResultInterface::PlaceOnTrackRequestQueue* lpQueue =
            lpResults->GetPlaceOnTrackRequestQueue();

        for( s32 liEvent = 0; liEvent < lpQueue->GetLength(); ++liEvent )
        {
            const BrnAI::AIModuleIO::PlaceOnTrackRequest& lrRequest = lpQueue->GetEvent( liEvent );

            RaceCar* lpRaceCar = GetGlobalRaceCar( lrRequest.GetGlobalRaceCarIndex() );

            CGS_ASSERT( lpRaceCar->GetType() < E_RACE_CAR_TYPE_COUNT,
                        "muType < E_RACE_CAR_TYPE_COUNT" );   // BrnRaceCar.h:482

            if( !lpRaceCar->IsInWorld() || !lpRaceCar->HasActiveRaceCar() )
            {
                continue;
            }

            CGS_ASSERT( lpRaceCar->GetType() < E_RACE_CAR_TYPE_COUNT,
                        "muType < E_RACE_CAR_TYPE_COUNT" );   // BrnRaceCar.h:547
            CGS_ASSERT( lpRaceCar->IsInWorld(), "IsInWorld()" );   // BrnRaceCar.h:401

            ActiveRaceCar* lpActiveRaceCar = lpRaceCar->GetActiveRaceCar();
            if( lpActiveRaceCar == 0 )
            {
                continue;
            }

            if( CgsDev::Log::gpDebugPrint != 0 )
            {
                *CgsDev::Log::gpDebugPrint << "Calling RequestPlaceOnTrack\n";
            }

            lpActiveRaceCar->RequestPlaceOnTrack( lrRequest.GetResetPosition(),
                                                  lrRequest.GetResetDirection(),
                                                  lrRequest.GetResetSpeed() );
        }
    }
}

}   // namespace BrnWorld
