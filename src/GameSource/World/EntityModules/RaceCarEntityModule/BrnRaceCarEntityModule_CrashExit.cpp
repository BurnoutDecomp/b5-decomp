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
// helpers, and when this file landed SIX of them did not exist anywhere in this tree. Five are
// bodied now -- ProcessRaceCarCrashCompleteEvents, SendResetOnTrackRequests,
// CheckForResetOnTrackConditions, (2026-09-11) UpdateTrafficAndRaceCarNearMisses and (2026-09-24,
// CHAIN-STOMPEES) ProcessLeapedAndStompedCars -- and the remaining TWO still have no body:
// ProcessPowerParking · PlaceOnTrackManager::PostSceneUpdate. This body runs every leg that IS
// reachable and logs the rest once, so it is honest about exactly what is missing.
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
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"   // KU_FLAG_AI_PERSISTENT_DAMAGE
#include "SharedClasses/Graphics/BrnGlobalColourPalette.h"                  // the re-colour legs' palette asserts
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficToRaceCarInterface.h" // GetPotentialStompees (ProcessLeapedAndStompedCars)

#include <cstdlib>   // getenv -- the [stomp] and BRN_CRASH_EXIT_DIAG witnesses

namespace BrnWorld
{

namespace
{
    // The crash exit's two place-on-track speeds (ProcessRaceCarCrashCompleteEvents 0x822F4468 /
    // 0x822F4478). Both words are BSS -- they read 0x00000000 out of the image -- and both are
    // written by CRT dynamic initialisers in the 0x82C4Bxxx bank (findinit: one writer, one reader
    // each). The NAMES are the DecFIGS DWARF's (BrnRaceCarEntityModule.cpp:232/:233): that bank
    // runs in declaration order, and 0x82C4BB10 (10 mph -> flt_82FAD610) / 0x82C4BB70 (120 mph ->
    // flt_82FAD728) bracket these two exactly as :231 KF_RESET_ON_TRACK_SPEED_FAILURE and :234
    // KF_RESET_ON_TRACK_IN_RANGE_SPEED bracket them in the declaration list.
    //   0x82C4BB30..0x82C4BB48  flt_82FAD720 = flt_82F31928 (0.44704) * flt_820138DC (50.0)
    //   0x82C4BB50..0x82C4BB68  flt_82FAD8C0 = flt_82F31928 (0.44704) * flt_82019A30 (75.0)
    const f32 KF_RESET_ON_TRACK_SPEED        = 0.44704f * 50.0f;   // flt_82FAD720, 50 mph in m/s
    const f32 KF_RESET_ON_TRACK_SPEED_ONLINE = 0.44704f * 75.0f;   // flt_82FAD8C0, 75 mph in m/s

    // DWARF BrnRaceCarEntityModule.cpp:306 KI_BLACK_CAR_COLOUR_INDEX = 6: the re-colour legs'
    // SET_OPPONENTS_TO_COPS colour (`li r17, 6` @0x822F4078, stored @0x822F420C / 0x822F4330).
    // (Also TU-local in BrnRaceCarEntityModule.cpp for SetupCarColour -- the DWARF home.)
    const s32 KI_BLACK_CAR_COLOUR_INDEX = 6;

    // [DIAG] BRN_CRASH_EXIT_DIAG -- NOT IN THE X360 BINARY. The latch every [crash-exit] /
    // [persist-damage] witness in this TU prints under. They used to print on every run, gated only
    // on gpDebugPrint (reviewer A on 65eadffe, 2026-09-24).
    bool CrashExitDiagEnabled()
    {
        static const bool sbOn = ( getenv( "BRN_CRASH_EXIT_DIAG" ) != 0 );
        return sbOn && CgsDev::Log::gpDebugPrint != 0;
    }
}

// =================================================================================================
// X360 0x822A4A38 -- GetPersistentDamageCarCount (G67-D2). Its only caller is the persistent-
// damage arm of ProcessRaceCarCrashCompleteEvents below (bl at 0x822F4168): it caps the carry-over
// at three damaged rivals.
//   for (i = 0; i < 35; ++i)                  -- cmpwi 0x23; sub_822A3628 == &maRaceCars[i]
//     assert muType < 4                       (BrnRaceCar.h:482)
//     if (muType == 3) continue               -- E_RACE_CAR_TYPE_INACTIVE
//     if (mfPersistentDamage > 0.0f) ++count  -- fcmpu flt_82001CC0 ; ble skips (NaN too)
// =================================================================================================
s32 RaceCarEntityModule::GetPersistentDamageCarCount() const
{
    s32 liCount = 0;
    for( s32 liIndex = 0; liIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT; ++liIndex )
    {
        const RaceCar& lrRaceCar = maRaceCars[liIndex];
        CGS_ASSERT( lrRaceCar.GetType() < E_RACE_CAR_TYPE_COUNT, "muType < E_RACE_CAR_TYPE_COUNT" );

        if( lrRaceCar.GetType() != E_RACE_CAR_TYPE_INACTIVE && lrRaceCar.GetPersistentDamage() > 0.0f )
        {
            ++liCount;
        }
    }
    return liCount;
}

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
// ⭐ THE PERSISTENT-DAMAGE ARM (0x822F40C8..0x822F4390) -- LANDED 2026-09-23 (crash parity
// G67-D1). The old banner parked the whole block on "there is exactly one race car -- the
// player"; that stopped being true when rivals landed, and the PARK line fired in ~20 banked
// runs (e.g. aimod_rival_damage/20260922_220959 BrnGame.log:27804, car 1). In a mode carrying
// KU_FLAG_AI_PERSISTENT_DAMAGE (Road Rage, Survivor, Marked Man) every taken-down AI rival
// carries 0.3 more damage into its respawn -- at most three rivals carry damage at once:
//   0x822F40C8  GetGameModeFlag(0x40000000) ; 0x822F40E0 lbz 0x789 (mbTakenDown)
//   0x822F40F0  IsAttached assert (:1089) ; lwz 0x6F0 ; muType < 4 assert (BrnRaceCar.h:603)
//   0x822F4140  lbz 0xA4 == 1 (E_RACE_CAR_TYPE_AI) else skip
//   0x822F4158  lfs 0xA0 > 0.0f -> increase   ||   GetPersistentDamageCarCount() < 3 -> increase
//               otherwise straight to the re-colour (0x822F4174)
//   0x822F4264  IncreasePersistentDamage (inlined) ; true -> re-colour (0x822F4298), false -> done
// The damage reaches the car on its next reset: ResetActiveRaceCar's non-player arm reads it
// (G67-D3) into ResetRaceCar's lfHowCloseToTotalled, which WriteOutVehicleStats hands to
// DeformableObject::ResetDeformation as the initial damage -- the rival comes back crumpled.
//
// ⭐ THE RE-COLOUR LEG LANDED 2026-09-24 (crash parity CHAIN-RECOLOUR): 0x822F4174..0x822F4260
// (no damage to add: damage == 0 and three rivals already carry damage) and 0x822F4298..0x822F4390
// (IncreasePersistentDamage wrapped past 1.0): `miColourIndex = GetGameModeFlag(
// KU_FLAG_SET_OPPONENTS_TO_COPS) ? 6 : GetRandomCarColour(miColourPalette, -1)` plus its
// "Invalid Colour Index" asserts (:1282/:1288, :1298/:1304). It was parked because it indexes
// maPalettes[miColourPalette] and a rival's palette was RaceCar::Reset's -1; its console writer
// SetupCarColour @0x822F5170 (OnRaceCarResourcesLoaded, with the G61-D6 default-colour leg of
// ActiveRaceCar::OnResourcesLoaded) landed with it, as did GetRandomCarColour @0x822EA088 and
// IsCarColourInUse @0x822D2E68. The damage does not depend on the colour.
// ⭐ The `mbTakenDown = false` store that FOLLOWS the block is NOT part of it: it is outside it
// on the console (0x822F4394 is the merge point of both arms) and it is real bookkeeping.
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

        if( CrashExitDiagEnabled() )
        {
            *CgsDev::Log::gpDebugPrint
                << "[crash-exit] CRASH COMPLETE received for active race car "
                << static_cast<s32>( luActiveRaceCarIndex )
                << " crashing=" << ( lpActiveRaceCar->IsCrashing() ? 1 : 0 )
                << " remove=" << ( lrEvent.mbRemoveRaceCar ? 1 : 0 ) << "\n";
        }

        // 0x822F40C8..0x822F4390 -- the taken-down AI persistent-damage block. See the banner.
        if( GetGameModeFlag( BrnGameState::GameModeParams::KU_FLAG_AI_PERSISTENT_DAMAGE ) &&
            lpActiveRaceCar->IsTakenDown() )
        {
            RaceCar* lpTakenDownCar = lpActiveRaceCar->GetGlobalRaceCar();
            CGS_ASSERT( lpTakenDownCar->GetType() < E_RACE_CAR_TYPE_COUNT,
                        "muType < E_RACE_CAR_TYPE_COUNT" );

            if( lpTakenDownCar->GetType() == E_RACE_CAR_TYPE_AI )
            {
                const f32 lfDamageBefore = lpTakenDownCar->GetPersistentDamage();
                bool lbRecolour = true;
                if( lfDamageBefore > 0.0f || GetPersistentDamageCarCount() < 3 )
                {
                    lbRecolour = lpTakenDownCar->IncreasePersistentDamage();
                }

                const s32 liColourBefore = lpTakenDownCar->GetColourIndex();   // [DIAG] only

                if( lbRecolour )
                {
                    // 0x822F4174..0x822F4260 (no-damage arm, asserts :1298 / :1304) and
                    // 0x822F4298..0x822F4390 (IncreasePersistentDamage wrapped, :1282 / :1288) --
                    // the same code twice on the console:
                    //     GetGameModeFlag(1<<34) == 0 -> miColourIndex = GetRandomCarColour(
                    //                                    miColourPalette (lwz 0x98), -1)
                    //     else                        -> miColourIndex = 6 (r17)
                    //     assert miColourIndex < maPalettes[miColourPalette].miNumColours
                    //            "Invalid Colour Index: " << miColourIndex
                    if( !GetGameModeFlag( BrnGameState::GameModeParams::KU_FLAG_SET_OPPONENTS_TO_COPS ) )
                    {
                        lpTakenDownCar->SetColourIndex(
                            GetRandomCarColour( lpTakenDownCar->GetColourPalette(), -1 ) );
                    }
                    else
                    {
                        lpTakenDownCar->SetColourIndex( KI_BLACK_CAR_COLOUR_INDEX );
                    }
                    CGS_ASSERT( lpTakenDownCar->GetColourIndex() <
                                    mCarColoursResource->maPalettes[lpTakenDownCar->GetColourPalette()]
                                        .GetNumColours(),
                                "Invalid Colour Index: " );
                }

                // [DIAG] BRN_CRASH_EXIT_DIAG -- NOT IN THE X360 BINARY -- one line per credited AI
                // takedown in a persistent-damage mode: what the rival will carry into its respawn.
                if( CrashExitDiagEnabled() )
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[persist-damage] car " << static_cast<s32>( luActiveRaceCarIndex )
                        << " damage " << lfDamageBefore << " -> "
                        << lpTakenDownCar->GetPersistentDamage()
                        << " recolour " << ( lbRecolour ? 1 : 0 )
                        << " colour " << liColourBefore << " -> " << lpTakenDownCar->GetColourIndex()
                        << " palette " << lpTakenDownCar->GetColourPalette() << "\n";
                }
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

        // 0x822F4440..0x822F4478: `lwz r11, 0x768(car)` (meEngineState) == 2 ->
        //     lbzx this+0x18345 (mbIsInOnlineGameMode) ? lfs flt_82FAD8C0 : lfs flt_82FAD720
        // Both words are BSS and read 0.0 out of the image; their writers are CRT dynamic
        // initialisers (tools/re/findinit.py), so a running engine puts the car back on the road
        // at 50 mph offline / 75 mph online, not at rest. See the constants at the top of the file.
        if( lpActiveRaceCar->GetEngineState() ==
            RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_RUNNING )
        {
            lfResetSpeed = mbIsInOnlineGameMode ? KF_RESET_ON_TRACK_SPEED_ONLINE
                                                : KF_RESET_ON_TRACK_SPEED;
        }

        if( lpRaceCar->GetType() == E_RACE_CAR_TYPE_AI &&
            GetGameModeFlag( 0x80000000ull ) )
        {
            lfResetDistance = -50.0f;                                  // flt_820148B4, image-read
            leResetType     = BrnAI::E_RESET_TYPE_BEHIND_PLAYER_ROAD_RAGE;   // li r30, 3
        }

        lpRaceCar->RequestResetOnTrack( lfResetSpeed, leResetType, lfResetDistance );

        // [DIAG] BRN_CRASH_EXIT_DIAG -- NOT IN THE X360 BINARY -- one line per crash-exit reset
        // request (the CRASH COMPLETE line above is the same cadence), carrying the engine state
        // that picked the speed.
        if( CrashExitDiagEnabled() )
        {
            *CgsDev::Log::gpDebugPrint
                << "[crash-exit] reset-on-track active race car "
                << static_cast<s32>( luActiveRaceCarIndex )
                << " engineState " << static_cast<s32>( lpActiveRaceCar->GetEngineState() )
                << " online " << ( mbIsInOnlineGameMode ? 1 : 0 )
                << " speed " << lfResetSpeed << " type " << static_cast<s32>( leResetType )
                << " dist " << lfResetDistance << "\n";
        }
    }
}

// =================================================================================================
// ProcessLeapedAndStompedCars @ 0x822BD5B8   (crash parity CHAIN-STOMPEES c / G67-D7, 2026-09-24)
//
// The Showtime leap/stomp target assist's module leg. Had no body, and PostSceneUpdate skipped its
// slot, so miStoredStompeeCount stayed 0 and ProcessPlayerVehicleInput's AddTargetAssist loop never
// ran. The console body, whole:
//   lbzx this+0x1823D          mCrashPlayManager (+0x180F0) +0x14D == IsInShowtime()  0x822BD5DC
//   GetActiveRaceCar(mePlayerActiveRaceCarIndex (+0x182F8))
//   lfs +0x4E4 > 0.0f (flt_82001CC0)   mPhysicsState (+0xE0) .mfTimeInAir (+0x404)    0x822BD5FC
//   lpInput->GetTrafficToRaceCarInterface_PreScene()                                 bl @0x822BD624
//   the inlined DWARF :122 GetPotentialStompees(&miStoredStompeeCount):
//       lwz 0x208 -> stw this+0x185F0 (miStoredStompeeCount) ; addi 0x40 (mPotentialStompees)
//   for (i = 0; i < miStoredStompeeCount [re-read each pass]; ++i)
//       lvx128 record+0 -> stvx this+0x184F0 + 32*i   mStoredStompees[i].mPosition
//       lwz record+0x10 -> stw this+0x18500 + 32*i    mStoredStompees[i].mEntityId
// Outside Showtime or on the ground nothing is written: the list keeps its last contents and
// count (the console has no clear here either).
// =================================================================================================
void RaceCarEntityModule::ProcessLeapedAndStompedCars(
    const RaceCarEntityModuleIO::InputBuffer_PostScene* lpInput,
    RaceCarEntityModuleIO::OutputBuffer_PostScene* lpOutput )
{
    (void)lpOutput;   // passed in r5 by the console caller, never read

    if( mCrashPlayManager.IsInShowtime()
        && GetActiveRaceCar( mePlayerActiveRaceCarIndex )->GetPhysicsState()->mfTimeInAir > 0.0f )
    {
        const BrnTraffic::BrnTrafficIO::VehicleStompingData* lpPotentialStompees =
            lpInput->GetTrafficToRaceCarInterface_PreScene()->GetPotentialStompees( &miStoredStompeeCount );

        for( s32 liStompee = 0; liStompee < miStoredStompeeCount; ++liStompee )
        {
            mStoredStompees[liStompee].mPosition = lpPotentialStompees[liStompee].mStompeePosition;
            mStoredStompees[liStompee].mEntityId = lpPotentialStompees[liStompee].mStompeeEntityId;
        }

        // [DIAG] BRN_STOMP_DIAG -- NOT IN THE X360 BINARY. First-N capped proof the leg was
        // DISPATCHED with a non-empty traffic list (Showtime + airborne + candidates).
        static const bool sbStompDiag = ( getenv( "BRN_STOMP_DIAG" ) != 0 );
        static u32 suStompDiagLines = 0u;
        if( sbStompDiag && miStoredStompeeCount > 0 && suStompDiagLines < 64u
            && CgsDev::Log::gpDebugPrint != 0 )
        {
            ++suStompDiagLines;
            *CgsDev::Log::gpDebugPrint
                << "[stomp] ProcessLeapedAndStompedCars stored " << miStoredStompeeCount
                << " stompee(s); first entity " << mStoredStompees[0].mEntityId.muValue
                << " at (" << mStoredStompees[0].mPosition.x << ", " << mStoredStompees[0].mPosition.y
                << ", " << mStoredStompees[0].mPosition.z << ")\n";
        }
    }
}

// =================================================================================================
// PostSceneUpdate @ 0x822FE3F0   -- MINIMAL-COMPLETE SLICE (see the file banner)
//
// Console order:
//   PerfMon start · assert lpInput/lpOutput · LockForRead(in) · LockForWrite(out)
//   if (!(lUpdateSet & 1)) UpdateTrafficAndRaceCarNearMisses          ⭐ REPRODUCED (2026-09-11)
//   ProcessRaceCarCrashCompleteEvents                                 ⭐ REPRODUCED
//   ProcessLeapedAndStompedCars                                       ⭐ REPRODUCED (2026-09-24)
//   the Showtime traffic publish into the race-car -> traffic interface ⭐ REPRODUCED (2026-09-24)
//   ProcessPowerParking · PlaceOnTrackManager::PostSceneUpdate         [ABSENT]
//   SendResetOnTrackRequests                                          ⭐ REPRODUCED
//   CheckForResetOnTrackConditions                                    ⭐ REPRODUCED (2026-09-05)
//   UnlockForRead(in) · UnlockForWrite(out) · PerfMon stop
// =================================================================================================
void RaceCarEntityModule::PostSceneUpdate(
    RaceCarEntityModuleIO::InputBuffer_PostScene* lpInput,
    RaceCarEntityModuleIO::OutputBuffer_PostScene* lpOutput,
    BrnUpdateSet lUpdateSet )
{
    CGS_ASSERT( lpInput  != 0, "lpInput != NULL" );    // BrnRaceCarEntityModule.cpp:1179
    CGS_ASSERT( lpOutput != 0, "lpOutput != NULL" );   // BrnRaceCarEntityModule.cpp:1180

    lpInput->LockForRead();
    lpOutput->LockForWrite();

    // ⭐ THE FIRST OF THE EIGHT CALLEES, at the console's own slot (near-miss producer wave
    // 2026-09-11). It is the only writer of the near-miss manager's two near lists, so the
    // near-miss tick that PostPhysicsUpdate runs could not fire without it. Bit 0 of the update
    // set is the network-catchup skip every module's update carries.
    if( ( lUpdateSet & 1 ) == 0 )
    {
        UpdateTrafficAndRaceCarNearMisses( lpInput );
    }

    ProcessRaceCarCrashCompleteEvents( lpInput );

    // 0x822FE4AC..0x822FE4B8 -- unconditional (CHAIN-STOMPEES c, 2026-09-24).
    ProcessLeapedAndStompedCars( lpInput, lpOutput );

    // 0x822FE4BC..0x822FE554 -- THE SHOWTIME TRAFFIC PUBLISH (crash parity FX-RCEM4, 2026-09-24).
    //   r27 = this + 0x180F0 (mCrashPlayManager); the inlined IsPlayerInShowtimeOnGround()
    //   (0x822FE4C4..0x822FE510) -> r29 ; bl GetRaceCarToTrafficInterface (0x822B56B0) ;
    //   flag word +0x6A0: `ori 2` / `rlwinm ..,0,31,29` ; stw        == SetFlag(bit 1, r29)
    //   bl CrashPlayManager::GetShowtimeTrafficDensityScale (0x822A8088) -> f31 ;
    //   bl GetRaceCarToTrafficInterface ; stfs f31, 0x6A4             == SetShowtimeTrafficDensityScale
    // Unconditional. Its reader is TrafficEntityModule::PostSceneUpdate (+464870 / +464932), which
    // gates the Showtime on-ground traffic and scales the Showtime spawn density. Until this landed
    // the flag was never set and the scale sat at RaceCarToTrafficInterface::Construct's 1.0f.
    {
        RaceCarEntityModuleIO::RaceCarToTrafficInterface* lpRaceCarToTraffic =
            lpOutput->GetRaceCarToTrafficInterface();
        lpRaceCarToTraffic->SetFlag(
            RaceCarEntityModuleIO::RaceCarToTrafficInterface::E_FLAG_PLAYER_IS_IN_SHOWTIME_ON_GROUND,
            mCrashPlayManager.IsPlayerInShowtimeOnGround() );
        lpRaceCarToTraffic->SetShowtimeTrafficDensityScale(
            mCrashPlayManager.GetShowtimeTrafficDensityScale() );

        // [DIAG] BRN_SHOWTIME_TRAFFIC_DIAG -- NOT IN THE X360 BINARY. One capped line per change
        // of the published pair, so a run proves the publish is DISPATCHED and shows its values.
        static const bool sbShowtimeTrafficDiag = ( getenv( "BRN_SHOWTIME_TRAFFIC_DIAG" ) != 0 );
        static s32 siLastOnGround = -1;
        static f32 sfLastScale = -1.0f;
        static u32 suShowtimeTrafficLines = 0u;
        const s32 liOnGround = lpRaceCarToTraffic->IsFlagSet(
            RaceCarEntityModuleIO::RaceCarToTrafficInterface::E_FLAG_PLAYER_IS_IN_SHOWTIME_ON_GROUND ) ? 1 : 0;
        const f32 lfScale = lpRaceCarToTraffic->GetShowtimeTrafficDensityScale();
        if( sbShowtimeTrafficDiag && CgsDev::Log::gpDebugPrint != 0 && suShowtimeTrafficLines < 32u
            && ( liOnGround != siLastOnGround || lfScale != sfLastScale ) )
        {
            siLastOnGround = liOnGround;
            sfLastScale    = lfScale;
            ++suShowtimeTrafficLines;
            *CgsDev::Log::gpDebugPrint << "[showtime-traffic] PostSceneUpdate publish onGround "
                                       << liOnGround << " densityScale " << lfScale
                                       << " (in showtime " << ( mCrashPlayManager.IsInShowtime() ? 1 : 0 )
                                       << ")\n";
        }
    }

    // ⭐⭐⭐ THE PRODUCER END OF THE RESET-ON-TRACK PUMP (resetpump wave 2026-08-26), at the
    // console's own slot -- SendResetOnTrackRequests is the fifth of PostSceneUpdate's eight
    // callees and it is the ONLY reader of RaceCar::mbToBeResetOnTrack, which
    // ProcessRaceCarCrashCompleteEvents (two lines up) is what SETS.
    SendResetOnTrackRequests( lpOutput );

    // ⭐ THE EIGHTH CALLEE, at the console's own slot -- the stuck / off-road / drowned
    // watchdog, landed 2026-09-05 (roll-frequency wave). It is the OTHER producer of
    // mbToBeResetOnTrack, so it feeds the very pump the line above drains; the console runs it
    // immediately after, which is why it goes here and not earlier. It takes no buffer: every
    // condition it tests is on the ActiveRaceCar / RaceCarState the module already owns.
    CheckForResetOnTrackConditions();

    {
        static bool sbLoggedPostScenePark = false;
        if( !sbLoggedPostScenePark && CrashExitDiagEnabled() )
        {
            sbLoggedPostScenePark = true;
            *CgsDev::Log::gpDebugPrint
                << "[crash-exit] RaceCarEntityModule::PostSceneUpdate SLICE: FIVE of the eight"
                   " console callees are reconstructed -- ProcessRaceCarCrashCompleteEvents,"
                   " (resetpump wave 2026-08-26) SendResetOnTrackRequests, (roll-frequency"
                   " wave 2026-09-05) CheckForResetOnTrackConditions, (near-miss producer"
                   " wave 2026-09-11) UpdateTrafficAndRaceCarNearMisses and (2026-09-24)"
                   " ProcessLeapedAndStompedCars. TWO still have no body anywhere in this tree"
                   " (ProcessPowerParking, PlaceOnTrackManager::PostSceneUpdate) [FLAG]\n"
                   "[crash-exit] ... and the RESET-ON-TRACK PUMP HAS BOTH PRODUCERS: the crash"
                   " leg (ProcessRaceCarCrashCompleteEvents -> RequestResetOnTrack) and, as of"
                   " 2026-09-05, the WATCHDOG leg (CheckForResetOnTrackConditions @0x822CE9E0 --"
                   " drowned / super-fatal surface / wedged / force-reset / fell out of the world"
                   " / airborne over 10 s). Both drain through WriteUpdatedAIData (PreScene) ->"
                   " the two AI bridges -> AIModule::Update slice -> ResetOnTrackManager ->"
                   " BridgeAIToEntityModules_PrePhysics -> ProcessResetOnTrackResultQueue"
                   " (PrePhysics) -> ActiveRaceCar::RequestPlaceOnTrack."
                   " See BrnRaceCarEntityModule_ResetPump.cpp for the whole round trip\n";
        }
    }

    lpInput->UnlockForRead();
    lpOutput->UnlockForWrite();
}

// =================================================================================================
// ProcessRaceCarCrashEvents_PostPhysics @ 0x822BD8B0   (127 insns)   -- crash wave 2026-09-02
// DWARF BrnRaceCarEntityModule.h:668  void (const InputBuffer_PostPhysics*, OutputBuffer_PostPhysics*)
//
// The crash ENTRY consumer: drains the physics module's RaceCarCrashEvent queue (the input
// buffer's VehicleManagerOutputInterface image, queue @+0x3A0) once per post-physics tick.
//   0x822BD8CC  RaceCarEntityModuleIO::Input(lpInput) ; addi r26, r3, 0x3A0   -> the crash queue
//   0x822BD940  EventQueue<RaceCarCrashEvent,8>::GetEvent(i)  (the truncated `BrnPhysics::Vehicl`)
//   0x822BD948  ld 0(event) ; srdi 32 ; extrwi 14,8                 -> the victim's active index
//   0x822BD954  asserts :5251 / :5252 (index in [0,8)) and :5255 (mePlayerActiveRaceCarIndex >= 0)
//   0x822BD9BC  index == mePlayerActiveRaceCarIndex ->
//     0x822BD9D4    CGS_ASSERT(IsAttached())                             BrnActiveRaceCar.h:1418
//     0x822BD9FC    lbz 0x52A(player) -> mPhysicsState.mbCrashing: only while NOT yet crashing:
//     0x822BDA20      stdx event.mRaceCarVolumeInstanceID -> this+0x18108
//                       == mCrashPlayManager (+0x180F0) + 0x18 == mPlayerCarVolumeInstanceID
//     0x822BDA18      meGameModeType (+0x18368) in {3 ROAD_RAGE, 8, 10} and event +0x38
//                     (mbIsPrimaryCrash) set -> player ActiveRaceCar::mbIsWrecked = true (stb r14=1, 0x782)
//   0x822BDA5C  GetActiveRaceCar(index)->OnCrash(event.mbIsPrimaryCrash, event,
//                 lpOutput->GetSceneInputInterface() [sub_822B63E0: +8224, "Not locked for
//                 writing" assert IO.h:576], event.mbRemoveHandlingVolumeFromScene (lbz 0x39),
//                 f1 = this+0x183A0 == mfSimTime)
//   0x822BDA90  re-reads miLength every iteration (`lwz r11, 8(r26)`), reproduced by the loop test.
// =================================================================================================
void RaceCarEntityModule::ProcessRaceCarCrashEvents_PostPhysics(
    const RaceCarEntityModuleIO::InputBuffer_PostPhysics* lpInput,
    RaceCarEntityModuleIO::OutputBuffer_PostPhysics* lpOutput )
{
    typedef BrnPhysics::Vehicle::VehicleManagerOutputInterface::RaceCarCrashEventQueue CrashQueue;

    const CrashQueue* lpQueue = lpInput->GetVehicleManagerOutputInterface()->GetRaceCarCrashEventQueue();

    for( s32 liEvent = 0; liEvent < lpQueue->GetLength(); ++liEvent )
    {
        const BrnPhysics::Vehicle::RaceCarCrashEvent& lrEvent = lpQueue->GetEvent( liEvent );

        const u32 luEntityWord = static_cast<u32>( lrEvent.mRaceCarVolumeInstanceID.muId >> 32 );
        const s32 liActiveRaceCarIndex = static_cast<s32>( ( luEntityWord >> 10 ) & 0x3FFFu );

        CGS_ASSERT( liActiveRaceCarIndex >= 0,
                    "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0" );                  // :5251
        CGS_ASSERT( liActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                    "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT" );               // :5252
        CGS_ASSERT( static_cast<s32>( mePlayerActiveRaceCarIndex ) >= 0,
                    "mePlayerActiveRaceCarIndex >= 0" );                                    // :5255

        if( static_cast<s32>( mePlayerActiveRaceCarIndex ) == liActiveRaceCarIndex )
        {
            ActiveRaceCar* lpPlayerCar = GetActiveRaceCar( mePlayerActiveRaceCarIndex );
            CGS_ASSERT( lpPlayerCar->IsAttached(), "IsAttached()" );                       // BrnActiveRaceCar.h:1418

            if( !lpPlayerCar->GetPhysicsState()->mbCrashing )                              // lbz 0x52A
            {
                // The crash-play manager's player id -- the value its Update was starving on.
                mCrashPlayManager.mPlayerCarVolumeInstanceID = lrEvent.mRaceCarVolumeInstanceID;   // stdx -> +0x18108

                if( meGameModeType == BrnGameState::GameStateModuleIO::E_MODE_ROAD_RAGE
                    || static_cast<s32>( meGameModeType ) == 8
                    || static_cast<s32>( meGameModeType ) == 10 )
                {
                    if( lrEvent.mbIsPrimaryCrash )                                         // lbz 0x38(event)
                    {
                        // The console's bare `stb r14(1), 0x782(player)`; there is no setter in
                        // the export set, so the module writes the member by name (friend grant
                        // in BrnActiveRaceCar.h, reasoned there).
                        WreckLatchWitness( "ProcessRaceCarCrashEvents_PostPhysics@0x822BDA18",
                                           static_cast<s32>( mePlayerActiveRaceCarIndex ), true );
                        lpPlayerCar->mbIsWrecked = true;
                    }
                }
            }
        }

        ActiveRaceCar* lpActiveRaceCar = GetActiveRaceCar( static_cast<EActiveRaceCarIndex>( liActiveRaceCarIndex ) );
        lpActiveRaceCar->OnCrash( lrEvent.mbIsPrimaryCrash,
                                  lrEvent,
                                  lpOutput->GetSceneInputInterface(),
                                  lrEvent.mbRemoveHandlingVolumeFromScene,
                                  mfSimTime );
    }
}

}   // namespace BrnWorld
