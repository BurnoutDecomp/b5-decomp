// =================================================================================================
// GameSource/World/CrashModule/BrnCrashModule_RaceCarCrashes.cpp   (crash exit wave, 2026-08-25)
//
// ⭐⭐⭐ THIS IS THE FILE THAT MAKES A CRASH END. Everything upstream of it has worked for weeks --
// driving into traffic fires SetRaceCarCrashing, the physics module publishes a RaceCarCrashEvent,
// and the world bridges carry it. Nothing consumed it, so the car froze at the impact point
// forever. These six bodies are the consumer: they open a crash RECORD, run its countdown, and
// then say "this one is over" into the ring the race-car module already reads.
//
// X360 ARTIST spine:
//   CrashModule::PreSceneUpdate            @0x827D3A60   (86 insns)
//   CrashModule::PostPhysicsUpdate         @0x827D3BB8   (83 insns)
//   CrashModule::ProcessCrashedRaceCarEvents @0x827CAAB8 (370 insns)  -- allocates the record
//   CrashModule::TickCrashes               @0x827C6490  (212 insns)  -- runs the countdown
//   CrashModule::ClearupCrashes            @0x827CDE98  (475 insns)  -- retires expired records
//   CrashModule::ResetRaceCarFromCrashIndex@0x827C6C40  (133 insns)  -- POSTS the complete event
//
// DWARF home is World/CrashModule/BrnCrashModule.cpp; same file-split rationale as
// BrnCrashModule_Lifecycle.cpp. DELETE-WHEN the home TU becomes mountable whole.
//
// =================================================================================================
// ⛔ WHAT IS DELIBERATELY PARKED HERE, AND WHY EACH PARK IS SAFE ON THIS BUILD
// =================================================================================================
// PreSceneUpdate and PostPhysicsUpdate each call helpers this slice does not reconstruct. Every
// one is TRAFFIC- or NETWORK-side; not one is on the race-car crash-exit path. They are parked
// with a one-shot log apiece rather than dropped, and each park is justified by a live invariant:
//
//   ClearUpRecycledTraffic      54  -- walks mRecycledTrafficQueue and drops the matching
//                                      TrafficCrash. mTrafficCrashes is only ever filled by
//                                      HandleNewCrashingTraffic/AddCrashingTrafficVehicle, both
//                                      parked, so it is permanently empty and this is a no-op loop.
//   HandleNetworkCrashingTraffic 1268 / ResetCrashedNetworkRaceCars 193
//                                   -- BOTH already gated behind mbIsOnlineGameMode, which
//                                      Construct sets false and only an online game mode raises.
//   ProcessSlammedTrafficEvents 449 / HandleNewCrashingTraffic 37 /
//   HandleRecoveredSlammedTraffic 46 / HandleCleanedUpTrafficEvents 228 /
//   GenerateOwnedTrafficUpdates 957 -- the crashing-TRAFFIC bookkeeping. Traffic cars already
//                                      crash and recover through the physical-traffic manager on
//                                      this build; the crash module's traffic ledger is a separate
//                                      slice and is inert today either way.
//
// The recycled-traffic queue and traffic bitmask copies remain parked with their consumers.
// The crash-ending game event42 is published through the canonical typed output queue.
// =================================================================================================

#include "GameSource/World/CrashModule/BrnCrashModule.h"
#include "rw/math/vpu/vector3_operation.h"
#include "GameSource/GameState/BrnGameEvents.h"
#include <cstdlib>
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                   // CgsDev::Log::gpDebugPrint
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"              // IOBuffer lock/unlock
#include "GameSource/BurnoutConstants.h"                            // E_ACTIVE_RACE_CAR_INDEX_*
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"

namespace BrnWorld
{

namespace
{
    // One-shot park logger. Each parked helper gets its own static bool at its call site.
    void LogCrashPark(bool& lrbAlreadyLogged, const char* lpcText)
    {
        if (lrbAlreadyLogged || CgsDev::Log::gpDebugPrint == 0)
        {
            return;
        }
        lrbAlreadyLogged = true;
        *CgsDev::Log::gpDebugPrint << lpcText;
    }
}

// =================================================================================================
// ProcessCrashedRaceCarEvents @ 0x827CAAB8   (370 insns)
//
// Drain the physics module's RaceCarCrashEvent ring and open one RaceCarCrash record per newly
// crashing race car. This is where a crash becomes a TRACKED crash.
//
//   0x827CAC7C  bl  EventQueue<RaceCarCrashEvent,8>::GetEvent(queue, i)
//   0x827CAC84  ld  r11, 0(event) ; srdi ; rlwinm 8,24,31 -- owner byte of the volume instance id,
//               asserted == E_ENTITYTYPE_RACECAR (BrnCrashModule.cpp:893)
//   0x827CAC98  extrwi r28, r10, 14, 8                    -- the active-race-car slot
//               asserted >= 0 (:894) and < 8 (:895, with the FastBitArray range message)
//   0x827CAE18  the mCrashingRaceCars bit test -- ALREADY-CRASHING CARS ARE SKIPPED ENTIRELY
//   0x827CAE6C  CGS_ASSERT(FindCrashForRaceCar(idx) == KU_INVALID_CRASH)   (:900)
//   0x827CAE90  the crash-duration select (see below)
//   0x827CAF04  bl  Array<RaceCarCrash,8>::Grow ; bl RaceCarCrash::Construct(id, seconds)
//   0x827CB038  set this slot's bit in mCrashingRaceCars
//
// THE DURATION SELECT -- every constant READ FROM THE IMAGE, none guessed:
//   event.mbCarIsAI      -> mbFastCrashesForAI ? 2.0f (0x82001D9C) : 4.5f (0x820CA5B4)
//   event.mbCarIsNetwork -> 20.0f (0x820CA5A8)
//   mbIsShowtimeGameMode -> flt_8300E9B0
//   mbIsOnlineGameMode   -> 5.0f (0x8200426C)
//   otherwise            -> mfPlayerCrashTime  (Construct: 4.0f)   <-- THE PLAYER'S PATH
// ⚠️ flt_8300E9B0 reads 0.0 from the image, but its whole 0x60-byte neighbourhood is zero: it is
// BSS, i.e. written at runtime by something this slice has not traced. It is behind
// mbIsShowtimeGameMode, which is false everywhere on this build, so it is reproduced as the
// symbol's static value with this flag rather than invented. [[placeholder-identity-element]]
// does NOT bite here: it is a straight assignment, not an identity-element folded into a lerp.
// =================================================================================================
void CrashModule::ProcessCrashedRaceCarEvents( const CrashIO::InputBuffer_PostPhysics* lpInput,
                                               CrashIO::OutputBuffer_PostPhysics* lpOutput )
{
    CGS_ASSERT( lpInput  != 0, "lpInput" );    // BrnCrashModule.cpp:877
    CGS_ASSERT( lpOutput != 0, "lpOutput" );   // BrnCrashModule.cpp:878

    const BrnPhysics::Vehicle::VehicleManagerOutputInterface* lpVehicleManagerOutputInterface =
        lpInput->GetVehicleManagerOutputInterface();
    const BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleInterface =
        lpInput->GetVehicleOutputInterface();

    CGS_ASSERT( lpVehicleManagerOutputInterface != 0, "lpVehicleManagerOutputInterface" );   // :882
    CGS_ASSERT( lpVehicleInterface != 0, "lpVehicleInterface" );                             // :883

    const BrnPhysics::Vehicle::VehicleManagerOutputInterface::RaceCarCrashEventQueue*
        lpRaceCarCrashEvents = lpVehicleManagerOutputInterface->GetRaceCarCrashEventQueue();
    CGS_ASSERT( lpRaceCarCrashEvents != 0, "lpRaceCarCrashEvents" );                         // :886

    const s32 liEventCount = lpRaceCarCrashEvents->GetLength();
    for( s32 liEvent = 0; liEvent < liEventCount; ++liEvent )
    {
        const BrnPhysics::Vehicle::RaceCarCrashEvent& lrEvent =
            lpRaceCarCrashEvents->GetEvent( liEvent );

        CGS_ASSERT( lrEvent.mRaceCarVolumeInstanceID.GetEntityIDOwner() == 1u,
                    "lEvent.mRaceCarVolumeInstanceID.GetEntityIDOwner() == E_ENTITYTYPE_RACECAR" );

        const u32 luActiveRaceCarIndex = lrEvent.mRaceCarVolumeInstanceID.GetEntityIDEntityIndex();
        CGS_ASSERT( luActiveRaceCarIndex < static_cast<u32>( E_ACTIVE_RACE_CAR_INDEX_COUNT ),
                    "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT" );
        if( luActiveRaceCarIndex >= static_cast<u32>( E_ACTIVE_RACE_CAR_INDEX_COUNT ) )
        {
            continue;   // the console's FastBitArray guard would fire below; do not corrupt memory
        }

        // 0x827CAE18..0x827CAE5C -- a car already tracked as crashing does NOT get a second record.
        if( mCrashingRaceCars.IsBitSet( luActiveRaceCarIndex ) )
        {
            continue;
        }

        CGS_ASSERT( FindCrashForRaceCar( static_cast<EActiveRaceCarIndex>( luActiveRaceCarIndex ) )
                        == KU_INVALID_CRASH,
                    "FindCrashForRaceCar( leActiveRaceCarIndex ) == KU_INVALID_CRASH" );   // :900

        f32 lfSecondsBeforeCleanup;
        if( lrEvent.mbCarIsAI )
        {
            lfSecondsBeforeCleanup = mbFastCrashesForAI ? 2.0f : 4.5f;
        }
        else if( lrEvent.mbCarIsNetwork )
        {
            lfSecondsBeforeCleanup = 20.0f;
        }
        else if( mbIsShowtimeGameMode )
        {
            // flt_8300E9B0 -- BSS, reads 0.0f; unreachable on this build. See the banner.
            lfSecondsBeforeCleanup = 0.0f;
        }
        else if( mbIsOnlineGameMode )
        {
            lfSecondsBeforeCleanup = 5.0f;
        }
        else
        {
            lfSecondsBeforeCleanup = mfPlayerCrashTime;
        }

        RaceCarCrash* lpCrash = mRaceCarCrashes.Grow();
        lpCrash->Construct( lrEvent.mRaceCarVolumeInstanceID, lfSecondsBeforeCleanup );

        mCrashingRaceCars.SetBit( luActiveRaceCarIndex );

        if( CgsDev::Log::gpDebugPrint != 0 )
        {
            *CgsDev::Log::gpDebugPrint << "[crash-exit] OPENED crash record for active race car "
                                       << static_cast<s32>( luActiveRaceCarIndex )
                                       << " seconds=" << lfSecondsBeforeCleanup << "\n";
        }
    }
}

// =================================================================================================
// TickCrashes @ 0x827C6490   (212 insns)
//
//   0x827C64F8  CGS_ASSERT(lpInput)          (:1687)
//   0x827C6510  CGS_ASSERT(mbClearUpEnabled) (:1688)   <- the flag CrashModule::Construct sets
//   0x827C6528  lpInput->GetTimerStatusInterface(): the step is `[8] * [7]` == the SIM status'
//               mfTimeStepMultiplier * mfBaseTimeStep == GetSimTimerStatus()->GetCurrentTimeStep()
//   0x827C6690  `addi r11, owner, 0x13C0 ; slwi 1 ; lbzx ; clrlwi 31` -- the HIGH byte of
//               maxRaceCarFlags[owner], bit 0 of that byte == bit 8 of the word ==
//               E_RACE_CAR_OUTPUT_FLAG_IN_SHOWTIME. ⭐ A SHOWTIME WRECK IS NOT TICKED AT ALL, and
//               if it is the player's, the pending ending message is RETRACTED.
//   0x827C66A8  r6 = lpInput->mbPlayerPressingBoost && !mbIsShowtimeGameMode
//   0x827C6714  bl RaceCarCrash::Tick(...)   -- see BrnRaceCarCrash.cpp for the 8-argument map
//   then the TrafficCrash tail loop (kept: it needs no parked helper).
// =================================================================================================
void CrashModule::TickCrashes( const CrashIO::InputBuffer_PreScene* lpInput )
{
    CGS_ASSERT( lpInput != 0, "lpInput" );                    // :1687
    CGS_ASSERT( mbClearUpEnabled, "mbClearUpEnabled" );       // :1688

    const f32 lfTimeStep =
        lpInput->GetTimerStatusInterface()->GetSimTimerStatus()->GetCurrentTimeStep();

    const bool lbIsOfflineGameMode = !mbIsOnlineGameMode;

    const RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface =
        lpInput->GetActiveRaceCarInterface();

    for( u32 luCrash = 0; luCrash < mRaceCarCrashes.GetLength(); ++luCrash )
    {
        const EActiveRaceCarIndex leOwner = static_cast<EActiveRaceCarIndex>(
            mRaceCarCrashes.GetItem( luCrash ).GetOwner() );

        const bool lbIsPlayerCrash =
            ( lpActiveRaceCarInterface->GetPlayerActiveRaceCarIndex() == leOwner );

        if( lpActiveRaceCarInterface->IsCarInShowtime( leOwner ) )
        {
            // 0x827C6728 -- a showtime wreck never times out; retract any pending ending message.
            if( lbIsPlayerCrash )
            {
                mbNeedToSendEndingMessage = false;
            }
            continue;
        }

        const bool lbPlayerPressingBoostOutsideShowtime =
            lpInput->GetPlayerPressingBoost() && !mbIsShowtimeGameMode;

        mRaceCarCrashes.GetItem( luCrash ).Tick( lfTimeStep,
                                                 lpActiveRaceCarInterface,
                                                 lbPlayerPressingBoostOutsideShowtime,
                                                 static_cast<s32>( miNumCrashExtensions ),
                                                 lbIsOfflineGameMode,
                                                 lbIsPlayerCrash,
                                                 mbIsInAGameMode,
                                                 &mbNeedToSendEndingMessage );
    }

    // ARTIST827C6748..827C67CC: the same simulation step ticks each traffic record.
    for (u32 luCrash = 0; luCrash < mTrafficCrashes.GetLength(); ++luCrash)
        mTrafficCrashes.GetItem(luCrash).Tick(lfTimeStep);

}

// =================================================================================================
// ClearupCrashes @ 0x827CDE98   (475 insns) -- the RACE-CAR arm
//
//   0x827CDF3C  the "is there a player at all" pair: mePlayerActiveRaceCarIndex != -1 AND the
//               interface's mbIsPlayerCarActive byte. ⚠️ THE WHOLE LOOP IS SKIPPED WITHOUT A
//               PLAYER -- wrecks are only retired while there is a player car in the world.
//   0x827CDF88  the loop over mRaceCarCrashes
//   0x827CDFB6  `lfs f, 0xC(item)` < 0.0f  -- EXPIRED (strictly negative, not <= 0)
//   0x827CDFC4  if (!mbIsInAGameMode) -> clean up
//               else if (!IsRaceCarRival(owner)) -> clean up
//               else if (mfSecondsBeforeCleanup <= -20.0f) -> clean up (the hard backstop)
//               else the two VMX tests: keep the wreck alive only while it is BOTH within a
//               40-unit radius AND ahead of the plane 5 units behind the player.
//   0x827CE0xx  ResetRaceCarFromCrashIndex(lpOutput, luCrash--, IsRaceCarNetwork(owner))
//
// Rival hold constants are initialized by ARTIST startup thunks, not zero defaults:
// 0x82C6AC60 splats 5.0 (8200426C) to 8300E9C0; 0x82C6AC88 splats 40.0
// (82004D0C) to 8300EA30; 0x82C6ACB0 squares it into 8300F3B0 (1600.0).
// The unnormalized forward projection must exceed -5.0, and distance squared
// must be strictly below 1600.0. The hold ends at the -20-second backstop.
// =================================================================================================
void CrashModule::ClearupCrashes( const CrashIO::InputBuffer_PreScene* lpInput,
                                  CrashIO::OutputBuffer_PreScene* lpOutput )
{
    CGS_ASSERT( lpInput  != 0, "lpInput" );
    CGS_ASSERT( lpOutput != 0, "lpOutput" );
    CGS_ASSERT( mbClearUpEnabled, "mbClearUpEnabled" );

    const RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface =
        lpInput->GetActiveRaceCarInterface();

    if( lpActiveRaceCarInterface->IsPlayerCarActive() )
    {
        const Vector3 lPlayerPosition = lpActiveRaceCarInterface->GetPlayerPosition();
        const Vector3 lPlayerDirection = lpActiveRaceCarInterface->GetPlayerDirection();
        for( u32 luCrash = 0; luCrash < mRaceCarCrashes.GetLength(); ++luCrash )
        {
            RaceCarCrash& lrCrash = mRaceCarCrashes.GetItem( luCrash );
            if( lrCrash.GetSecondsBeforeCleanup() >= 0.0f )
            {
                continue;
            }

            bool lbClearUp = true;

            if( mbIsInAGameMode )
            {
                const EActiveRaceCarIndex leOwner =
                    static_cast<EActiveRaceCarIndex>( lrCrash.GetOwner() );

                if( lpActiveRaceCarInterface->IsRaceCarRival( leOwner ) &&
                    lrCrash.GetSecondsBeforeCleanup() > -20.0f )
                {
                    const Vector3 lSeparation =
                        lpActiveRaceCarInterface->GetRaceCarState(leOwner)->mTransform.Pos() - lPlayerPosition;
                    lbClearUp = !(rw::math::vpu::Dot(lSeparation, lSeparation) < 1600.0f &&
                                  rw::math::vpu::Dot(lSeparation, lPlayerDirection) > -5.0f);
                }
            }

            if( lbClearUp )
            {
                const u32 luOwnerIndex =
                    lrCrash.GetVolumeInstanceId().GetEntityIDEntityIndex();
                CGS_ASSERT( luOwnerIndex < static_cast<u32>( E_ACTIVE_RACE_CAR_INDEX_COUNT ),
                            "mRaceCarVolumeInstanceId.GetEntityIDEntityIndex() < E_ACTIVE_RACE_CAR_INDEX_COUNT" );

                const bool lbIsNetwork = lpActiveRaceCarInterface->IsRaceCarNetwork(
                    static_cast<EActiveRaceCarIndex>( luOwnerIndex ) );

                ResetRaceCarFromCrashIndex( lpOutput, luCrash, lbIsNetwork );
                --luCrash;   // 0x827CE0xx `v29--` -- EraseFast swapped a new element into this slot
            }
        }
    }

    // The TRAFFIC arm of ClearupCrashes (the second half of the console body) is parked with the
    // rest of the traffic ledger -- see the file banner. mTrafficCrashes is empty on this build.
}

// =================================================================================================
// ResetRaceCarFromCrashIndex @ 0x827C6C40   (133 insns)
//
// ⭐ THE PRODUCER OF THE CRASH EXIT. Three statements, and the middle one is the whole point.
//
//   0x827C6CB0  bl Array<RaceCarCrash,8>::GetItem(this + 0x230, luCrashIndex)
//   0x827C6CB8  ld r30, 0(item) ; std into the 16-byte stack record  -- the VolumeInstanceId
//   0x827C6CC4  bl OutputBuffer_PreScene::GetRaceCarOutputInterface()   (the WRITE-lock overload)
//   0x827C6CCC  stb a4, 8(record)                                       -- mbRemoveRaceCar
//   0x827C6CD0  bl EventQueue<RaceCarCrashCompleteEvent,10>::AddEvent   <-- "THIS CRASH IS OVER"
//   0x827C6D5C  clear this slot's bit in mCrashingRaceCars
//   0x827C6D6C  bl Array<RaceCarCrash,8>::EraseFast(this + 0x230, luCrashIndex)
//
// The event travels: OutputBuffer_PreScene::mRaceCarOutputInterface
//   -> WorldModule::BridgeCrashModuleToRaceCarModule_PostScene  (landed c3655e4a)
//   -> RaceCarEntityModuleIO::InputBuffer_PostScene::mCrashInterface
//   -> RaceCarEntityModule::ProcessRaceCarCrashCompleteEvents
//   -> ActiveRaceCar::ResetAfterCrash / RaceCar::RequestResetOnTrack.
// =================================================================================================
void CrashModule::ResetRaceCarFromCrashIndex( CrashIO::OutputBuffer_PreScene* lpOutput,
                                              u32 luCrashIndex, bool lbRemoveRaceCar )
{
    CGS_ASSERT( lpOutput != 0, "lpOutput" );                                     // :2083
    CGS_ASSERT( luCrashIndex < mRaceCarCrashes.GetLength(),
                "luCrashIndex < mRaceCarCrashes.GetLength()" );                  // :2084

    const RaceCarCrash& lrCrash = mRaceCarCrashes.GetItem( luCrashIndex );

    CrashIO::RaceCarCrashCompleteEvent lEvent;
    lEvent.mRaceCarVolumeInstanceId = lrCrash.GetVolumeInstanceId();
    lEvent.mbRemoveRaceCar          = lbRemoveRaceCar;

    lpOutput->GetRaceCarOutputInterface()->GetRaceCarCrashCompleteEventQueue()->AddEvent( lEvent );

    const u32 luOwnerIndex = lEvent.mRaceCarVolumeInstanceId.GetEntityIDEntityIndex();
    CGS_ASSERT( luOwnerIndex < static_cast<u32>( E_ACTIVE_RACE_CAR_INDEX_COUNT ),
                "mRaceCarVolumeInstanceId.GetEntityIDEntityIndex() < E_ACTIVE_RACE_CAR_INDEX_COUNT" );

    if( CgsDev::Log::gpDebugPrint != 0 )
    {
        *CgsDev::Log::gpDebugPrint << "[crash-exit] CRASH COMPLETE posted for active race car "
                                   << static_cast<s32>( luOwnerIndex )
                                   << " remove=" << ( lbRemoveRaceCar ? 1 : 0 ) << "\n";
    }

    mCrashingRaceCars.UnSetBit( luOwnerIndex );
    mRaceCarCrashes.EraseFast( luCrashIndex );
}

// =================================================================================================
// PreSceneUpdate @ 0x827D3A60   (86 insns)
//
//   0x827D3A80  LockForWrite(lpOutput) ; LockForRead(lpInput)         -- in THAT order
//   0x827D3A90  lpInput->GetActiveRaceCarInterface(): latch the player's slot into
//               meLocalActiveRaceCarIndex, but ONLY when the interface says a player car is active
//               (0x827D3AD8 `lbz r11, 0x2860` == mbIsPlayerCarActive; :967/:980 tripwires)
//   0x827D3B30  HandleGameActions                                     [LIVE]
//   0x827D3B44  if (!(lUpdateSet & 1)) {
//   0x827D3B48      ClearUpRecycledTraffic                            [PARKED]
//   0x827D3B4C      if (mbIsOnlineGameMode) { HandleNetworkCrashingTraffic ;
//                                             ResetCrashedNetworkRaceCars }   [PARKED, unreachable]
//   0x827D3B78      if (mbClearUpEnabled)  { TickCrashes ; ClearupCrashes }   <-- THE EXIT
//               }
//   0x827D3BA4  UnlockForRead(lpInput) ; UnlockForWrite(lpOutput)
//
// ⚠️ `lUpdateSet & 1` is the console's own skip bit (the same `a6 & 1` every module's update
// carries). Reproduced, not dropped.
// =================================================================================================
void CrashModule::PreSceneUpdate( CgsModule::IOBufferStack* /*lpInputBufferStack*/,
                                  CgsModule::IOBufferStack* /*lpOutputBufferStack*/,
                                  const CrashIO::InputBuffer_PreScene* lpInput,
                                  CrashIO::OutputBuffer_PreScene* lpOutput,
                                  BrnUpdateSet lUpdateSet )
{
    lpOutput->LockForWrite();
    lpInput->LockForRead();

    {
        const RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface =
            lpInput->GetActiveRaceCarInterface();

        if( lpActiveRaceCarInterface->IsPlayerCarActive() )
        {
            meLocalActiveRaceCarIndex = lpActiveRaceCarInterface->GetPlayerActiveRaceCarIndex();
        }
    }

    HandleGameActions(lpInput, lpOutput);

    if( ( lUpdateSet & 1 ) == 0 )
    {
        {
            static bool sbLoggedRecycledTrafficPark = false;
            LogCrashPark( sbLoggedRecycledTrafficPark,
                          "[crash-exit] CrashModule::ClearUpRecycledTraffic PARK: the crashing-"
                          "TRAFFIC ledger is a separate slice; mTrafficCrashes is permanently"
                          " empty on this build [FLAG]\n" );
        }

        if( mbIsOnlineGameMode )
        {
            static bool sbLoggedNetworkPark = false;
            LogCrashPark( sbLoggedNetworkPark,
                          "[crash-exit] CrashModule network arms PARK: HandleNetworkCrashingTraffic"
                          " / ResetCrashedNetworkRaceCars are not reconstructed and this build never"
                          " enters an online game mode [FLAG]\n" );
        }

        if( mbClearUpEnabled )
        {
            TickCrashes( lpInput );
            ClearupCrashes( lpInput, lpOutput );
        }
    }

    lpInput->UnlockForRead();
    lpOutput->UnlockForWrite();
}

// =================================================================================================
// PostPhysicsUpdate @ 0x827D3BB8   (83 insns)
//
//   0x827D3BC8  LockForWrite(lpOutput) ; LockForRead(lpInput)
//   if (!(lUpdateSet & 1)) {
//     0x827D3BDC  ProcessCrashedRaceCarEvents                          <-- THE ENTRY
//     0x827D3BE8  mRecycledTrafficQueue.Clear() then .Append(vmOut->mRemovedTrafficEventQueue)
//                 and the two 80-byte traffic bitmask copies from the traffic input interface
//                                                                       [PARKED -- feeds only
//                                                                        parked helpers]
//     0x827D3C40  ProcessSlammedTrafficEvents / HandleNewCrashingTraffic /
//                 HandleRecoveredSlammedTraffic / HandleCleanedUpTrafficEvents  [PARKED]
//     0x827D3C58  if (mbIsOnlineGameMode) GenerateOwnedTrafficUpdates            [PARKED]
//     0x827D3C68  if (mbNeedToSendEndingMessage) { VariableEventQueue<1536,16>::AddEvent(
//                     lpOutput->GetGameEventQueue(), &record, 42, 1);
//                   mbNeedToSendEndingMessage = false; }                [LIVE]
//   }
//   0x827D3C8C  UnlockForRead ; UnlockForWrite
// =================================================================================================
void CrashModule::PostPhysicsUpdate( CgsModule::IOBufferStack* /*lpInputBufferStack*/,
                                     CgsModule::IOBufferStack* /*lpOutputBufferStack*/,
                                     const CrashIO::InputBuffer_PostPhysics* lpInput,
                                     CrashIO::OutputBuffer_PostPhysics* lpOutput,
                                     BrnUpdateSet lUpdateSet )
{
    lpOutput->LockForWrite();
    lpInput->LockForRead();

    if( ( lUpdateSet & 1 ) == 0 )
    {
        ProcessCrashedRaceCarEvents( lpInput, lpOutput );

        {
            static bool sbLoggedTrafficMirrorPark = false;
            LogCrashPark( sbLoggedTrafficMirrorPark,
                          "[crash-exit] CrashModule::PostPhysicsUpdate PARK: the recycled-traffic"
                          " mirror, the two traffic bitmask copies and the five crashing-traffic"
                          " handlers are the traffic ledger slice [FLAG]\n" );
        }

        if( mbNeedToSendEndingMessage )
        {
            // ARTIST 827D3CD0..827D3CE8: publish the empty signal, then consume the latch.
            const BrnGameState::GameStateModuleIO::PlayerCrashEndingEvent lEvent{};
            lpOutput->GetGameEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent),
                BrnGameState::GameStateModuleIO::E_EVENT_PLAYER_CRASH_ENDING, sizeof(lEvent));
            if (std::getenv("BRN_CRASH_ACTION_DIAG") && CgsDev::Log::gpDebugPrint)
                *CgsDev::Log::gpDebugPrint << "[crash-ending] event 42 posted\n";
            mbNeedToSendEndingMessage = false;
        }
    }

    lpInput->UnlockForRead();
    lpOutput->UnlockForWrite();
}

}   // namespace BrnWorld
