// =================================================================================================
// GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule_CrashExit.cpp
// (crash exit wave, 2026-08-25)
//
// The CONSUMER end of the crash exit:
//   RaceCarEntityModule::PostSceneUpdate                 @0x822FE3F0  (a minimal-complete SLICE)
//   RaceCarEntityModule::ProcessRaceCarCrashCompleteEvents @0x822F3FE0 (359 insns, reconstructed)
//
// DWARF home is BrnRaceCarEntityModule.cpp; split out for the same reason the crash-module bodies
// are (that TU is 5000+ lines and carries the whole entity module). DELETE-WHEN the home TU can
// absorb them.
//
// =================================================================================================
// ⭐⭐ THE NINTH GATE. The brief for this wave listed eight producer-side gates. There was a ninth,
// and it sat on the CONSUMER side where nobody was looking: RaceCarEntityModule::PostSceneUpdate
// was itself a boot gate (WorldLinkStubs.cpp:1035, "inert (body not reconstructed)"). So even with
// the crash module perfectly wired, the RaceCarCrashCompleteEvent would have arrived in
// InputBuffer_PostScene::mCrashInterface and been read by nobody, every frame, forever.
//
// ⛔ PostSceneUpdate IS LANDED AS A DELIBERATE SLICE, NOT WHOLE. Its console body calls eight
// helpers and SIX OF THEM DO NOT EXIST ANYWHERE IN THIS TREE:
//     UpdateTrafficAndRaceCarNearMisses · ProcessLeapedAndStompedCars · ProcessPowerParking ·
//     PlaceOnTrackManager::PostSceneUpdate · SendResetOnTrackRequests · CheckForResetOnTrackConditions
// Reconstructing all six is its own multi-wave slice and none of them is on the crash exit. This
// body therefore runs the two legs that ARE reachable -- the lock pair and
// ProcessRaceCarCrashCompleteEvents -- and logs the rest once. That is strictly more than the
// link stub did, and it is honest about exactly what is missing.
//
// ⚠️⚠️ THE PARK THAT MATTERS, STATED PLAINLY: SendResetOnTrackRequests is the consumer of
// RaceCar::mbToBeResetOnTrack. RaceCar::RequestResetOnTrack (BrnRaceCar.cpp:251, real and
// committed) SETS that flag and NOTHING IN THIS TREE READS IT. ⇒ the branch of
// ProcessRaceCarCrashCompleteEvents that goes through RequestResetOnTrack currently ends there.
// ⭐ That branch is NOT the one a normal crash takes: it is entered only when the car is STILL
// flagged mPhysicsState.mbCrashing at the moment the complete event lands. The other branch --
// ActiveRaceCar::ResetAfterCrash -- is fully live, and it is the one that clears the wreck state.
// Both are reproduced; which one fires is decided by the console's own test, not by this slice.
//
// ⭐ BOUNDARY MOVED 2026-08-26 (aimodule slice 1). The paragraph above still stands, but the
// REASON it stands has changed and the old reason -- "the AI module lifecycle is an inert boot
// gate" -- IS NOW FALSE. AI.dat loads, WorldMapData resolves and BrnAI::ResetOnTrackManager IS
// Constructed against a real bound road network (measured on the boot log: version 12, 7639
// sections, 136 reset pairs, 3273824 B). The remaining hole is the PUMP, in dependency order:
//   (1) SendResetOnTrackRequests @0x822CE178 (57) -- this file's own park, still absent
//   (2) the 35-entry AI-car array AIModule::Construct still parks. THIS IS THE REAL GATE ON
//       THE WHOLE PUMP, not a later polish: ResetOnTrackManager::Update @0x8279A890
//       dereferences GetAICar(mePlayerGlobalRaceCarIndex) at +2714 on its FIRST request, and
//       AIModule::Prepare passes the manager a NULL array today (flagged at that site).
//   (3) AIModule::Update @0x8279B478 + UpdateResetOnTrackManager @0x8279ABB0 -- still boot
//       gates in WorldLinkStubs.cpp
//   (4) ResetOnTrackManager::Update and its 32 siblings (~4,750 insns, one bodied)
//   (5) ProcessResetOnTrackResultQueue @0x822F4580 (192)
//
// ⭐⭐ BOUNDARY MOVED AGAIN 2026-08-26 (aicar_reset wave). (2) and the reachable half of (4) ARE
// LANDED: AIModule::maAICars[35] is a real member seeded to E_AI_CAR_STATE_INACTIVE and passed to
// the manager's Construct, and ResetOnTrackManager::{Update, ProcessResetOnTrackRequest,
// ComputeResetOnTrack, ComputeInitialCoordinatesStandard} are bodied. The manager can resolve a
// request; nothing calls it. (1), (3) and (5) -- the PLUMBING -- remain.
// ⛔ TWO BLOCKERS UNDER THE PLUMBING, MEASURED on a booted drive rather than inferred:
//   * VehicleManager::GenerateAboveGroundLineTests @0x82633990 is ABSENT, so
//     RaceCarState::mAboveGroundTestResult.mbValid is false every frame and no car ever enters
//     the AI section system ([collision-tag] aboveGroundValid=0 on every sample).
//   * RaceCarEntityModule::WriteUpdatedAIData @0x822D1FC8 is ABSENT, so
//     AIModuleIO::RaceCarAIInterface::mbPlayerDataSet is never set -- and AIModule::Update
//     @0x8279B478 wraps its WHOLE body in `if (GetRaceCarAIInterface()->mbPlayerDataSet)`.
//     Landing (3) before that would be a body that provably never executes.
// ⭐⭐ AND THE RECOVERY DOES NOT WAIT ON THE AI ROAD NETWORK. ActiveRaceCar::GetResetCoords
// (landed 2026-08-26) has an EMPTY-RING arm at 0x822BF37C that returns the car's LIVE transform,
// measured tracking the player on a drive run -- so once the pump runs, the manager's FAILURE
// result already yields a usable pose. Every banner in this tree that said that path "would place
// the car at the origin" was wrong; see BrnRaceCar.cpp::RequestResetOnTrack.
// =================================================================================================

#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleRaceCarIOInterfaces.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameSource/World/AI/BrnAISharedConstants.h"   // BrnAI::EResetType

namespace BrnWorld
{

// =================================================================================================
// ProcessRaceCarCrashCompleteEvents @ 0x822F3FE0   (359 insns)
//
// ⭐ THE CRASH EXIT, CONSUMER SIDE. Walk the ring the crash module filled and give each finished
// wreck its car back.
//
//   0x822F3FFC  lpInput->GetCrashInterface()  (the RaceCarOutputInterface)
//   0x822F400C  lwz r11, 8(queue)                      -- the queue's miLength
//   0x822F4084  bl EventQueue<RaceCarCrashCompleteEvent,10>::GetEvent(queue, i)
//   0x822F4088  ld r10, 0(event) ; ld r9, 8(event) ; copied to a 16-byte stack record
//   0x822F409C  srdi 32 ; extrwi 14,8                  -- the active-race-car slot out of the id
//   0x822F40A8  mulli 0x1CD0 ; addi 0x1A60             -- &maActiveRaceCars[slot]
//   0x822F40B8  if (!IsAttached()) continue
//   0x822F40C8  if (GetGameModeFlag(0x40000000) && car->mbTakenDown) { the AI RE-COLOUR block }
//   0x822F4394  car->mbTakenDown = false                             (stb 0, 0x789)
//   0x822F43C8  if (!car->mPhysicsState.mbCrashing) -> ResetAfterCrash(car, false)   (lbz 0x52A)
//   0x822F4428  else if (raceCar->GetType() == 2 && !event.mbRemoveRaceCar)
//                                                     -> ResetAfterCrash(car, false)
//   0x822F4440  else -> RequestResetOnTrack(raceCar, speed, type, distance) with
//                       speed    = 0.0f, or flt_82FAD8C0/flt_82FAD720 when meEngineState == 2
//                       type     = 1, or 3 when the car is an AI (type 1) and game-mode flag
//                                  0x80000000 is set
//                       distance = 0.0f, or -50.0f (flt_820148B4, IMAGE-READ) on that same arm
//
// ⚠️ THE ARGUMENT ORDER IS A PPC-ABI TRAP. The call site is
//   0x822F452C  mr r5, r30 ; lwz r3, 0x6F0 ; fmr f2, f30 ; fmr f1, f31 ; bl RequestResetOnTrack
// -- r4 IS NEVER SET. That is not a dropped argument: on this ABI a float argument consumes BOTH
// an FPR and the matching GPR slot, so `(f32 lfSpeed, EResetType leType, f32 lfDistance)` puts
// lfSpeed in f1 (burning r4), leType in r5, and lfDistance in f2 (burning r6). The tree's
// committed RaceCar::RequestResetOnTrack signature matches exactly, and Hex-Rays' rendering of
// this call (five positional args, one of them the uninitialised `v31`) does not.
//
// ⛔ THE AI RE-COLOUR BLOCK IS PARKED. It is ~200 of the 359 instructions and it re-rolls a
// TAKEN-DOWN AI car's paint from the global colour palette (GetPersistentDamageCarCount,
// GetRandomCarColour, GlobalColourPalette, the four "Invalid Colour Index" asserts). It is gated
// on GetGameModeFlag(0x40000000) AND ActiveRaceCar::mbTakenDown, and on this build there is
// exactly one race car -- the player -- who is never an AI (muType == 1) and never taken down.
// ⭐ The `mbTakenDown = false` store that FOLLOWS the block is NOT parked: it is outside it on the
// console (0x822F4394 is the merge point of both arms) and it is real bookkeeping.
// =================================================================================================
void RaceCarEntityModule::ProcessRaceCarCrashCompleteEvents(
    const RaceCarEntityModuleIO::InputBuffer_PostScene* lpInput )
{
    const CrashIO::RaceCarOutputInterface* lpCrashInterface = lpInput->GetCrashInterface();
    const CrashIO::RaceCarOutputInterface::RaceCarCrashCompleteEventQueue* lpQueue =
        lpCrashInterface->GetRaceCarCrashCompleteEventQueue();

    const s32 liEventCount = lpQueue->GetLength();
    for( s32 liEvent = 0; liEvent < liEventCount; ++liEvent )
    {
        const CrashIO::RaceCarCrashCompleteEvent& lrEvent = lpQueue->GetEvent( liEvent );

        const u32 luActiveRaceCarIndex =
            lrEvent.mRaceCarVolumeInstanceId.GetEntityIDEntityIndex();
        if( luActiveRaceCarIndex >= static_cast<u32>( E_ACTIVE_RACE_CAR_INDEX_COUNT ) )
        {
            continue;
        }

        ActiveRaceCar* lpActiveRaceCar =
            GetActiveRaceCar( static_cast<EActiveRaceCarIndex>( luActiveRaceCarIndex ) );

        // 0x822F40B8 -- a detached slot is skipped entirely.
        if( !lpActiveRaceCar->IsAttached() )
        {
            continue;
        }

        if( CgsDev::Log::gpDebugPrint != 0 )
        {
            *CgsDev::Log::gpDebugPrint
                << "[crash-exit] CRASH COMPLETE received for active race car "
                << static_cast<s32>( luActiveRaceCarIndex )
                << " crashing=" << ( lpActiveRaceCar->IsCrashing() ? 1 : 0 )
                << " remove=" << ( lrEvent.mbRemoveRaceCar ? 1 : 0 ) << "\n";
        }

        // 0x822F40C8..0x822F4390 -- the taken-down AI re-colour block. PARKED, see the banner.
        if( GetGameModeFlag( 0x40000000ull ) && lpActiveRaceCar->IsTakenDown() )
        {
            static bool sbLoggedRecolourPark = false;
            if( !sbLoggedRecolourPark && CgsDev::Log::gpDebugPrint != 0 )
            {
                sbLoggedRecolourPark = true;
                *CgsDev::Log::gpDebugPrint
                    << "[crash-exit] ProcessRaceCarCrashCompleteEvents PARK: the taken-down AI"
                       " re-colour block (GetPersistentDamageCarCount / GetRandomCarColour /"
                       " GlobalColourPalette) is not reconstructed [FLAG]\n";
            }
        }

        // 0x822F4394 -- the merge point of both arms; always cleared.
        lpActiveRaceCar->SetTakenDown( false );

        // 0x822F43C8 -- the car is no longer flagged crashing: just re-seat the bookkeeping.
        if( !lpActiveRaceCar->IsCrashing() )
        {
            lpActiveRaceCar->ResetAfterCrash( false );
            continue;
        }

        RaceCar* lpRaceCar = lpActiveRaceCar->GetGlobalRaceCar();
        CGS_ASSERT( lpRaceCar->GetType() < E_RACE_CAR_TYPE_COUNT, "muType < E_RACE_CAR_TYPE_COUNT" );

        // 0x822F4428 -- a REMOTE (network) car that is not being removed also just re-seats.
        if( lpRaceCar->GetType() == E_RACE_CAR_TYPE_NETWORK && !lrEvent.mbRemoveRaceCar )
        {
            lpActiveRaceCar->ResetAfterCrash( false );
            continue;
        }

        // 0x822F4440..0x822F453C -- the place-on-track request.
        f32 lfResetSpeed    = 0.0f;   // flt_82001CC0
        f32 lfResetDistance = 0.0f;
        BrnAI::EResetType leResetType = BrnAI::E_RESET_TYPE_STANDARD;                 // li r30, 1

        if( lpActiveRaceCar->GetEngineState() ==
            RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_RUNNING )
        {
            // ⚠️ flt_82FAD8C0 / flt_82FAD720 BOTH READ 0.0 FROM THE IMAGE and both live in BSS
            // (their whole 0x60-byte neighbourhoods are zero), i.e. they are runtime-written
            // tunables this slice has not traced. 0.0f is what the console itself reads before
            // any writer runs, and it is the identity for this parameter (RequestResetOnTrack
            // asserts `mfResetOnTrackSpeed >= 0.0f` and a zero means "replace at rest"), so it is
            // reproduced as the symbol's static value -- FLAGGED, not guessed.
            lfResetSpeed = 0.0f;
        }

        if( lpRaceCar->GetType() == E_RACE_CAR_TYPE_AI &&
            GetGameModeFlag( 0x80000000ull ) )
        {
            lfResetDistance = -50.0f;                                  // flt_820148B4, image-read
            leResetType     = BrnAI::E_RESET_TYPE_BEHIND_PLAYER_ROAD_RAGE;   // li r30, 3
        }

        lpRaceCar->RequestResetOnTrack( lfResetSpeed, leResetType, lfResetDistance );
    }
}

// =================================================================================================
// PostSceneUpdate @ 0x822FE3F0   -- MINIMAL-COMPLETE SLICE (see the file banner)
//
// Console order:
//   PerfMon start · assert lpInput/lpOutput · LockForRead(in) · LockForWrite(out)
//   if (!(lUpdateSet & 1)) UpdateTrafficAndRaceCarNearMisses          [ABSENT]
//   ProcessRaceCarCrashCompleteEvents                                 ⭐ REPRODUCED
//   ProcessLeapedAndStompedCars                                       [ABSENT]
//   the showtime traffic-density publish into the output interface    [ABSENT helper]
//   ProcessPowerParking · PlaceOnTrackManager::PostSceneUpdate ·
//   SendResetOnTrackRequests · CheckForResetOnTrackConditions         [ABSENT]
//   UnlockForRead(in) · UnlockForWrite(out) · PerfMon stop
// =================================================================================================
void RaceCarEntityModule::PostSceneUpdate(
    RaceCarEntityModuleIO::InputBuffer_PostScene* lpInput,
    RaceCarEntityModuleIO::OutputBuffer_PostScene* lpOutput,
    BrnUpdateSet lUpdateSet )
{
    CGS_ASSERT( lpInput  != 0, "lpInput != NULL" );    // BrnRaceCarEntityModule.cpp:1179
    CGS_ASSERT( lpOutput != 0, "lpOutput != NULL" );   // BrnRaceCarEntityModule.cpp:1180

    (void)lUpdateSet;

    lpInput->LockForRead();
    lpOutput->LockForWrite();

    ProcessRaceCarCrashCompleteEvents( lpInput );

    {
        static bool sbLoggedPostScenePark = false;
        if( !sbLoggedPostScenePark && CgsDev::Log::gpDebugPrint != 0 )
        {
            sbLoggedPostScenePark = true;
            *CgsDev::Log::gpDebugPrint
                << "[crash-exit] RaceCarEntityModule::PostSceneUpdate SLICE: only"
                   " ProcessRaceCarCrashCompleteEvents is reconstructed. Six console helpers have"
                   " no body anywhere in this tree (UpdateTrafficAndRaceCarNearMisses,"
                   " ProcessLeapedAndStompedCars, ProcessPowerParking,"
                   " PlaceOnTrackManager::PostSceneUpdate, SendResetOnTrackRequests,"
                   " CheckForResetOnTrackConditions) [FLAG]\n"
                   "[crash-exit] ... and SendResetOnTrackRequests @0x822CE178 (57) is the ONLY"
                   " reader of RaceCar::mbToBeResetOnTrack, which the crash exit sets."
                   " BOUNDARY UPDATED 2026-08-26 (aimodule slice 1): the AI MODULE LIFECYCLE IS"
                   " NO LONGER THE BLOCKER -- AIModule::Construct/Prepare/LoadMapData are real,"
                   " AI.dat loads, WorldMapData resolves (version 12, 7639 sections) and"
                   " BrnAI::ResetOnTrackManager IS Constructed with a bound road network."
                   " What is still missing is the REQUEST/RESULT PUMP above it. UPDATED"
                   " 2026-08-26 (aicar_reset): the 35-entry AI-car array and"
                   " ResetOnTrackManager::{Update, ProcessResetOnTrackRequest,"
                   " ComputeResetOnTrack} are now BODIED -- the manager can resolve a request."
                   " What remains is the PLUMBING that would call it: (1) this"
                   " SendResetOnTrackRequests, (2) the two AI bridges, (3) AIModule::Update,"
                   " (4) ProcessResetOnTrackResultQueue -- plus two MEASURED blockers under"
                   " them: VehicleManager::GenerateAboveGroundLineTests is absent (no car ever"
                   " enters the AI section system) and RCEM::WriteUpdatedAIData is absent"
                   " (RaceCarAIInterface::mbPlayerDataSet never set, so AIModule::Update would"
                   " skip its whole body). So a heavy crash STILL pins the car and"
                   " crash ENTRY stays off the public path (BRN_ENABLE_CRASH_ENTRY)."
                   " See BrnRaceCar.cpp::RequestResetOnTrack [FLAG]\n";
        }
    }

    lpInput->UnlockForRead();
    lpOutput->UnlockForWrite();
}

}   // namespace BrnWorld
