// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/BrnModeManager_IntroPlay.cpp
// ============================================================================
// Partfile of the BrnGameState::ModeManager TU (owning header BrnModeManager.h).
// Wave-B keystone, AGENT 5 -- the starting grid + the intro/play transitions. Six bodies,
// every one reconstructed store-for-store from the X360 ARTIST export's ASSEMBLY (the
// Hex-Rays pseudocode is used only as a reading aid; where the two differ the asm wins):
//
//   ModeManager::SetStartingGrid             X360 0x82328608   (THE seat-the-cars function --
//                                                               StuntAttackMode::Start @0x82331E98
//                                                               and four sibling Start bodies call it)
//   ModeManager::GetStartDataForTrafficLight X360 0x82327310
//   ModeManager::StartModeIntro              X360 0x82343018
//   ModeManager::StopModeIntro               X360 0x82343F38
//   ModeManager::StartPlayingMode            X360 0x82343340
//   ModeManager::FinishOfflineModeIntro      X360 0x823119B0   (the GUI-event-25 landing point)
//
// [X] hazards H2: the sixteen bodies already committed in BrnModeManager.cpp are CALLED here,
//     never re-implemented. This file calls exactly one of them -- GetNextLandmarkIndex
//     (BrnModeManager.cpp:64) from StartPlayingMode.
//
// ---------------------------------------------------------------------------------------------
// [!!] VTABLE BINDING (per the wave's vtable micro-check, which reported MISMATCH on the
// committed BrnGameMode.h virtual ORDER and handed the conductor the 26-slot replacement):
// every mode call below goes through the C++ virtual BY NAME -- SendEvent / GetCurrentState /
// GetIntroDurationSeconds -- never through a slot index. Two of the micro-check's findings are
// load-bearing here and are honoured:
//   * vtbl+48 (slot 12) is SendEvent(EGameModeEvent), NOT a bool-taking "finish intro" hook. The
//     argument the console passes at BOTH of this file's call sites (StartModeIntro @0x823430A4
//     and FinishOfflineModeIntro @0x82311A4C, each `li r4,1`) is the ENUM E_GME_NEXT == 1.
//     function_grouping.md:71's "calls mode vtbl+48 ... with arg true" is corrected accordingly.
//   * GetIntroDurationSeconds is slot 8 (vtbl+32) -- which is the slot StartModeIntro's
//     `lwz r11,0x20(r11)` @0x823432A4 dispatches, so the by-name call is the right one.
// ---------------------------------------------------------------------------------------------
//
// HEADER FRICTION (bodies are written faithfully anyway, per the wave rule; the exact declaration
// text for each is in this agent's report as a header_request -- NO header is edited here):
//   R1  GameStateModuleIO::StartModeIntroAction   + E_ACTION_START_MODE_INTRO == 29  (BrnGameActions.h)
//   R2  GameStateModuleIO::StopModeIntroAction    + E_ACTION_STOP_MODE_INTRO  == 30  (BrnGameActions.h)
//   R3  GameStateModuleIO::StartPlayingModeAction (E_ACTION_START_PLAYING_MODE == 34 already exists)
//   R4  GameStateModuleIO::RequestGameTrainingAction (E_ACTION_REQUEST_GAME_TRAINING == 149 exists)
//   R5  GameModeParams::AddStartLocation(Vector3, Vector3)                    (BrnGameModeParams.h)
//   R6  BrnTraffic::TrafficData::GetStartDataForTrafficLight(u32, bool) const (BrnTrafficDataResourceType.h)
//
// [!] STATUS 2026-08-26 (CLOSURE round) -- THE PREVIOUS "ALL SIX HAVE LANDED" STAMP OVERSTATED
// THE CASE AND IS REWORDED HERE. Six declarations landed, but LANDED-AS-A-DECLARATION and
// LANDED-AS-A-LINKABLE-BODY are not the same fact, and this file's gate (selfcheck.py, /c only)
// can only ever see the first. What is true on the on-disk tree:
//   R1/R2/R3/R4  FULLY LANDED (complete types; nothing further to link).  BrnGameActions.h --
//                E_ACTION_START_MODE_INTRO == 29 (:119) and E_ACTION_STOP_MODE_INTRO == 30 (:120);
//                the records StartModeIntroAction (:1321, static_assert size 604 + offsetof
//                meGameMode == 0x254), StopModeIntroAction (:1338, size 8), StartPlayingModeAction
//                (:1350, size 16 + offsetof mDestinationLandmarkID == 8) and
//                RequestGameTrainingAction (:1367, size 4).
//   R5           FULLY LANDED.  Declared BrnGameModeParams.h:252, BODIED BrnGameModeParams.cpp:176
//                (which is why the IsNormal guard is NOT restated at this file's call site -- see
//                the SetStartingGrid banner).
//   R6           FULLY LANDED 2026-08-26 (CLOSURE round). It WAS declaration-only, for the reason
//                the previous stamp gave -- the body needs BrnTraffic::JunctionLogicBox, which was
//                un-homed tree-wide. That type now has a real header home
//                (SharedClasses/Traffic/Junctions/BrnJunctionLogicBox.h, promoted out of
//                BrnTrafficHullRuntime.cpp), so the whole chain is bodied:
//                  TrafficData::GetStartDataForTrafficLight   -> BrnTrafficData.cpp (X360 0x8231CC48)
//                  Hull::GetJunctionForLightTrigger           -> BrnTrafficHull.cpp (X360 0x82752870)
//                  Hull::GetLightTriggerStartDataForJunction  -> BrnTrafficHull.cpp (X360 0x82752900)
// So: this partfile gates STATUS=pass (it did not before -- the gaps above were distinct selfcheck
// errors, which is why this file was the batch's blocker), and R1-R6 now have bodies as well as
// declarations. Check the owning TUs, never this stamp, before calling the link closed.
// The per-site banners below are KEPT because they record WHY each declaration is shaped the way
// it is; read their leading marker as historical, not as an open blocker.
// ============================================================================

#include "GameSource/GameState/ModeManager/BrnModeManager.h"

#include "GameSource/GameState/BrnGameStateModule.h"                    // GameStateModule::IsOnlineGameMode (StartModeIntro)
#include "GameSource/GameState/BrnGameActions.h"                        // the GameAction<> payload records + EGameActionType
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // GameActionQueue::AddEvent, CgsModule::Event
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h" // GameModeParams / LightTriggerId / StartLocation
#include "rw/math/vpu/vector3_operation.h"                             // Vector3 operator* / operator+ (the grid maths)
#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"           // BrnTraffic::TrafficData (complete type)
#include "SharedClasses/Traffic/BrnTrafficLightTrigger.h"               // BrnTraffic::LightTriggerStartData (complete type)
#include "SharedClasses/Progression/BrnTrainingTypes.h"                 // BrnProgression::ETrainingType

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// TU-local constants. Every recovered value is image-cited; nothing is invented.
// ----------------------------------------------------------------------------

// SetStartingGrid's "push the grid forward" distance, in metres. X360 @0x82328750 loads
// flt_82020EE8 and splats it across the direction vector.
// IMAGE-CITED: scratch/postfx_step9_final/envfix/work/image.bin, offset 0x20EE8 (VA 0x82020EE8,
// big-endian) reads 42 0C 00 00 == 35.0f exactly. (Its neighbour at 0x82020EEC is 40 40 00 00 ==
// 3.0f and belongs to a different constant -- do not widen this to a pair.)
static const f32 KF_PUSH_FORWARDS_DISTANCE_METRES = 35.0f;

// The landmark "none" sentinel StartModeIntro posts in its action-44 payload. X360 @0x8234306C
// `lhz r11, word_82CDB7D4`. IMAGE-CITED: image.bin offset 0xCDB7D4 reads FF FF (its neighbour at
// 0xCDB7D8 reads FF FE == K_MULTIPLE_LANDMARKS). This is the same global BrnModeManager.h's
// landmark-block banner and ClearLandmarkAndFinishLineData already cite -- LandmarkIndex(-1).
// [!] A .bss ZERO WOULD BE A LIE HERE: 0 is a REAL landmark/region index.
static const u16 KU_INVALID_LANDMARK_INDEX = 0xFFFFu;

// StartModeIntro's online-stunt training request. X360 @0x82343318 `li r11, 0x4D` (77) into the
// 4-byte action-149 payload, posted only for the online-stunt family {12, 14, 17}.
// [!] FLAG -- DO NOT "FIX" THIS TO A NAME. In this tree's DWARF-derived BrnProgression::
// ETrainingType (SharedClasses/Progression/BrnTrainingTypes.h) 77 is E_TRAINING_TYPE_NOT_TIMED_COUNT,
// a COUNT sentinel, which no producer would ever post. That is a PS3-vs-X360 enum divergence, not a
// bug in this body: the X360 build's ETrainingType has at least one more not-timed enumerator than
// the PS3 DWARF does, so slot 77 there is a real tip (the online-stunt-run one). The literal is
// what the ROM posts, so the literal is what is written; the enum re-pin is filed in the report.
static const s32 KI_TRAINING_TYPE_ONLINE_STUNT_X360 = 77;

// ----------------------------------------------------------------------------
// LightTriggerId::IsValid() -- X360-INLINED, de-inlined here as a file-local predicate.
//
// The console packs the handle as { hull index = bits 8..23, light-trigger index = bits 0..7 } and
// BOTH halves carry an all-ones "none" sentinel. Two independent sites agree instruction for
// instruction, and the second one NAMES the predicate:
//   ModeManager::SetStartingGrid @0x82328678: `rlwinm r9,r11,0,8,23` (id & 0x00FFFF00) compared
//       against 0x00FFFF00; then `clrlwi r11,r11,24` (id & 0xFF) compared against 0xFF. Either
//       match skips the whole grid loop.
//   TrafficData::GetStartDataForTrafficLight @0x8231CC48: the SAME two tests, and on failure it
//       fires the assert string "lTriggerId.IsValid()" (..\..\..\SharedClasses\Traffic/
//       BrnTrafficData.h:265) -- which also proves the two halves' roles, because the very next
//       asserts there are "luHull < muNumHulls" against (id >> 8) and
//       "luLightTrigger < lpHull->muNumLightTriggers" against (id & 0xFF).
//
// The tree models LightTriggerId as a bare `typedef u32` stub (BrnGameModeParams.h:85), so the
// predicate has no class to live on. File-local here; the real home is filed as a header_request.
// ----------------------------------------------------------------------------
static bool IsLightTriggerIdValid(LightTriggerId lTriggerId)
{
    const u32 luId = static_cast<u32>(lTriggerId);
    if ((luId & 0x00FFFF00u) == 0x00FFFF00u)   // hull half == 0xFFFF
    {
        return false;
    }
    if ((luId & 0x000000FFu) == 0x000000FFu)   // light-trigger half == 0xFF
    {
        return false;
    }
    return true;
}

// ============================================================================
// ModeManager::GetStartDataForTrafficLight -- X360 0x82327310
// ============================================================================
// A three-instruction wrapper: resolve the mode manager's TrafficData resource, assert it, and
// forward to TrafficData::GetStartDataForTrafficLight with the "alternate start data" flag FALSE.
//
// [!] THE HOLDER IS THE TriggerQueryManager, AND IT IS +0x640, NOT +0x620. The asm reads
// `lwz r11, 0x6D60(r3)` (== mpTriggerQueryManager) and then calls
// ResourcePtr<TrafficData>::GetMemory() on `r11 + 0x640` (1600). header_grow_spec section 6.5
// documents TQM+0x620 (1568) -- that is the TRIGGER-data ResourcePtr, a DIFFERENT resource.
// The TrafficData one is +0x640. Both live on the TQM; agent 9's GetTrafficData() must read the
// +0x640 slot, and GetTriggerData()/GetCheckpointTriggerData() the +0x620 slot. Reported.
//
// The assert's file/line is BrnModeManager.h:1631 -- i.e. it belongs to the INLINED accessor
// GetTrafficData(), not to this function's own source.
// [!] FIX ROUND 2026-08-26 -- CROSS-AGENT DUPLICATE RESOLVED, this end dropped. The console fires
// "lpTrafficData" EXACTLY ONCE for this call: BeginAssert @0x82327340, line 0x65F == 1631, inside
// the inlined GetTrafficData() (the whole export is 0x82327310..0x82327384 and contains that one
// Begin/Fire/End triple). ModeManager::GetTrafficData is now bodied with that same assert at
// BrnModeManager_Accessors.cpp:278-284, so firing it again at this call site would double it and
// poison the H10 assert storm, whose whole value is that each line names ONE missing wire.
// ============================================================================
const BrnTraffic::LightTriggerStartData* ModeManager::GetStartDataForTrafficLight(u32 luLightTriggerId) const
{
    // X360: BrnTraffic::ResourcePtr<TrafficData>::GetMemory(mpTriggerQueryManager + 0x640), then the
    // one "lpTrafficData" assert -- both of which live inside GetTrafficData().
    const BrnTraffic::TrafficData* lpTrafficData = GetTrafficData();

    // X360 @0x82327360: `li r5, 0` -- the second argument is FALSE.
    // [x] header_request R6 -- DECLARED *AND* BODIED as of the 2026-08-26 CLOSURE round; the
    // frontier this banner used to record is closed. TrafficData::GetStartDataForTrafficLight is
    // a real standalone X360 export (0x8231CC48, also called by
    // GameStateModule::SendSetUpAllEventStartsMessage) that walks
    // Hull::GetJunctionForLightTrigger @0x82752870 -> Hull::GetLightTriggerStartDataForJunction
    // @0x82752900. Both of those need BrnTraffic::JunctionLogicBox (288-byte stride, the two
    // start-data indices at +0x3C/+0x40), which was the un-homed type that blocked the whole
    // chain; it now lives in SharedClasses/Traffic/Junctions/BrnJunctionLogicBox.h and all three
    // functions are bodied in their owning traffic TUs. Still emphatically not ModeManager work --
    // this call site is a pass-through and always was.
    return lpTrafficData->GetStartDataForTrafficLight(luLightTriggerId, false);
}

// ============================================================================
// ModeManager::SetStartingGrid -- X360 0x82328608
// ============================================================================
// Seats liCarCount cars on the start grid of the event's traffic-light junction. This is THE
// stunt-attack seat-the-car function: StuntAttackMode::Start @0x82331E98 calls it, as do
// RaceMode::Start @0x82330018, RoadRageMode::Start @0x82330678, BurningRouteMode::Start
// @0x82331A80 and SurvivorMode::Start @0x823322B8.
//
// WHOLE, not sliced (hazards H7 lists SetStartingGrid under "no slicing"). There is no online
// fork in this body at all -- SetupOnlineStartingGrid is a separate function (agent 9).
//
// STUNT NOTE: when the event's GameModeParams carries no traffic-light trigger (an invalid
// LightTriggerId) the console does NOTHING -- no assert, no default grid. That is the authored
// path for a mode started away from lights, and it is why the id test comes BEFORE the
// "lpStartData != NULL" assert rather than after it.
// ============================================================================
void ModeManager::SetStartingGrid(GameModeParams* lpGameModeParams, s32 liCarCount, bool lbPushForwards) const
{
    CGS_ASSERT(lpGameModeParams != 0, "lpGameModeParams != NULL");   // BrnModeManager.cpp:4147

    // X360 @0x82328674 `lwz r11, 0x40(r30)`. +0x40 == 64 == GameModeParams::mTrafficLightTriggerId
    // on the console layout (miNumRivals/miNumNetworkPlayers 0..1, mfProgressionRankAsRatio +4,
    // maNetworkPlayerID[8] +8, mSpecialEventCarId +40, the three traffic floats +48/+52/+56,
    // meStartMechanism +60 -> mTrafficLightTriggerId +64). Reached BY NAME here.
    const LightTriggerId lTriggerId = lpGameModeParams->mTrafficLightTriggerId;

    if (IsLightTriggerIdValid(lTriggerId))
    {
        const BrnTraffic::LightTriggerStartData* lpStartData = GetStartDataForTrafficLight(lTriggerId);
        CGS_ASSERT(lpStartData != 0, "lpStartData != NULL");   // :4155

        // X360 @0x823286D8 `lbz r11, 0x190(r29)` -- an UNSIGNED byte compare against liCarCount.
        CGS_ASSERT(lpStartData->GetNumStartPositions() >= static_cast<u32>(liCarCount),
                   "lpStartData->GetNumStartPositions() >= static_cast<uint32_t>( liCarCount )");   // :4156

        for (s32 liIndex = 0; liIndex < liCarCount; ++liIndex)
        {
            // X360 order matters: the DIRECTION is fetched first (v127) because the push-forward
            // offset is derived from it before the position is read.
            const Vector3 lDirection = lpStartData->GetStartDirection(static_cast<u32>(liIndex));

            // `vspltisw128 v125, 0` is hoisted out of the loop and copied into v126 every
            // iteration -- i.e. the offset starts at the ALL-FOUR-LANE zero vector each time
            // round (SetZero() clears w too, which is what vspltisw does).
            Vector3 lOffset;
            lOffset.SetZero();
            if (lbPushForwards)
            {
                // `vspltw v0,v0,0` then `vmulfp128 v126, v127, v0`: the scalar 35.0f is splatted
                // and multiplied component-wise, i.e. a plain scale of the unit direction.
                lOffset = lDirection * KF_PUSH_FORWARDS_DISTANCE_METRES;
            }

            const Vector3 lPosition =
                lpStartData->GetStartPosition(static_cast<u32>(liIndex)) + lOffset;

            // [!] FIX ROUND 2026-08-26 (CLOSURE) -- CROSS-FILE DUPLICATE RESOLVED, this end
            // dropped. The console fires "BrnMath::IsNormal( lDirection )" exactly ONCE per grid
            // slot (BeginAssert @0x823287B0, line 0x490 == 1168, file BrnGameModeParams.h), and
            // that line belongs to AddStartLocation, not to SetStartingGrid -- it is here only
            // because the console inlined the callee. AddStartLocation now has a real body
            // (BrnGameModeParams.cpp) carrying that same guard verbatim, so restating it at this
            // call site would fire it twice per car and poison the H10 assert storm, whose whole
            // value is that each line names ONE missing wire. Same ruling, same reason, as the
            // "lpTrafficData" de-duplication at :150-155 above.
            // [x] header_request R5 (LANDED -- see the STATUS stamp at the top of this file). The console inlines GameModeParams::AddStartLocation
            // (DWARF BrnGameModeParams.h:577, `void AddStartLocation(Vector3, Vector3)`) down to
            // "build a 32-byte StartLocation{position, direction} on the stack and call
            // Array<StartLocation,8>::Append @0x82317C58 on this+0x150". maStartLocations is
            // PRIVATE in this tree's GameModeParams and there is no public mutator, so the
            // de-inlined named call is what is written -- reaching past the accessor with raw
            // offset math would be exactly the fabrication the wave rules forbid.
            lpGameModeParams->AddStartLocation(lPosition, lDirection);
        }
    }
}

// ============================================================================
// ModeManager::StartModeIntro -- X360 0x82343018
// ============================================================================
// Fired ONCE per event by UpdateCurrentMode's intro latch (hazards H6: !mbModeIntroStarted &&
// AreAllRaceCarsSetup() && mbDistanceToFinishLineTransmitted). It clears the GUI's
// "in a start region" state, walks the mode state machine into E_GMS_INTRO, builds the 604-byte
// intro action (flyby roster + intro duration) and posts it.
//
// ONE ARM DEFERRED (hazards H7 names StartModeIntro's online leg as MAY-SLICE) -- see the banner
// at the flyby fetch. The remote-player-disconnected replay loop is NOT deferred: it is written
// out in full below, because it needs nothing this tree lacks.
// ============================================================================
void ModeManager::StartModeIntro(GameStateModuleIO::GameActionQueue* lpGameActionQueue)
{
    CGS_ASSERT(lpGameActionQueue != 0, "lpGameActionQueue != NULL");   // :3069

    // ---- action 44, size 4: "the player is no longer in a mode-start region" ----------------
    // X360 @0x82343058..0x8234307C. The console writes ONLY the half-word at +0 and the byte at
    // +2 (`sth word_82CDB7D4` / `stb r23`); +3 is left as whatever the stack held, and the queue
    // copies all four bytes.
    //
    // [!] DELIBERATE, NARROW DIVERGENCE (2026-08-26, stuntrace waveB CLOSURE round) -- the pad
    // byte at +0x03 IS zeroed here, reversing this banner's earlier "do not 'helpfully' zero the
    // pad" instruction. Same ruling, same reason, same shape as the one
    // BrnModeManager_Start.cpp:791-797 already applies to the action-25 var_130 byte: posting an
    // INDETERMINATE stack byte into the shared 13312-byte queue is not reproducible on the host,
    // so it is a source of run-to-run divergence in every wire dump and every queue image, for no
    // information gain. Cost of the divergence: exactly zero observable behaviour. The consumer
    // arm @0x823EA948 reads only +0x00 (the u16 id) and +0x02 (mbInStartRegion) before posting
    // GuiEventEnterEventStartLocation(166) -- nothing anywhere reads +0x03 (grep-verified: the
    // record's only other producer is this function). Faithfulness is preserved where it can be
    // observed; determinism wins where it cannot.
    {
        GameStateModuleIO::SetInModeStartRegionAction lStartRegionAction;
        lStartRegionAction.mu16StartLocationId = KU_INVALID_LANDMARK_INDEX;
        lStartRegionAction.mbInStartRegion     = 0;
        lStartRegionAction.maPad03[0]          = 0;   // see the divergence banner above
        lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lStartRegionAction),
                                    GameStateModuleIO::E_ACTION_SET_IN_MODE_START_REGION,
                                    sizeof(lStartRegionAction));   // X360 li r5,0x2C; li r6,4
    }

    // ---- drive the mode state machine into the intro state ------------------------------------
    // X360 @0x82343080..0x823430A4: `lwz 0xD98(this)` -> mpCurrentGameMode, `lwz 0x28(mode)` ->
    // meCurrentState, compared against 1 (E_GMS_INTRO); if it is not already there, dispatch
    // vtbl+0x30 (slot 12 == SendEvent) with `li r4,1` == E_GME_NEXT.
    if (mpCurrentGameMode->GetCurrentState() != GameStateModuleIO::E_GMS_INTRO)
    {
        mpCurrentGameMode->SendEvent(E_GME_NEXT);
    }
    CGS_ASSERT(mpCurrentGameMode->GetCurrentState() == GameStateModuleIO::E_GMS_INTRO,
               "mpCurrentGameMode->GetCurrentState() == GameStateModuleIO::E_GMS_INTRO");   // :3089

    // ---- the online-event fork ----------------------------------------------------------------
    // X360 @0x823430D4..0x82343214. TWO conditions, and the compiler duplicates the second one:
    //   (a) GameStateModule::IsOnlineGameMode() -- the MODULE's flag, not this manager's
    //       IsOnlineGameMode() (which reads the current mode). The asm calls
    //       BrnGameState__GameStateModule__IsOnlineGameMode on `lwz 0x6D58(this)`.
    //   (b) the current mode is NOT one of the two "instant intro" online modes {15, 16}.
    // Note this is the BARE mode test, WITHOUT the +0x9508 flag -- it is NOT
    // IsOnlineModeWithInstantIntro() (which StopModeIntro below does use). Do not merge them.
    const bool lbLobbyOrShowtimeMode =
        (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY ||
         meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME);
    const bool lbOnlineEventIntro = mpGameStateModule->IsOnlineGameMode() && !lbLobbyOrShowtimeMode;

    // [x] header_request R1 (LANDED -- see the STATUS stamp at the top of this file): GameStateModuleIO::StartModeIntroAction (id 29, 604 bytes).
    // The console builds it on the stack from var_2B0 (the f32) through var_56, and posts
    // &var_2B0 with size 0x25C. Field identity is DWARF-exact (BrnGameActions.h:1170-1179).
    GameStateModuleIO::StartModeIntroAction lIntroAction;

    if (lbOnlineEventIntro)
    {
        // ---- replay every already-disconnected remote player to the new mode ------------------
        // X360 @0x82343130..0x823431EC. The loop counter is an EActiveRaceCarIndex walked with the
        // project post-increment operator, whose range guard IS the console's
        // "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT" assert (BurnoutConstants.h:39) -- so
        // writing the loop this way reproduces that assert without restating it.
        for (EActiveRaceCarIndex leRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
             leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT;
             leRaceCarIndex++)
        {
            // sub_82326878 == ScoringSystem::GetPlayerDisconnected(EActiveRaceCarIndex): its own
            // assert is BrnScoringSystem.h:1900 and its body is
            // `GetCarData(i) ? GetCarData(i)->mbDisconnected(+0x69) : 0` -- exactly the declared
            // accessor (DWARF :907). FLAG: the standalone body at 0x8231DA00 that func_index.tsv
            // resolves to that NAME is the NetworkPlayerID overload (assert :1927); 0x82326878 is
            // the active-index one, folded because it is a header inline.
            if (mScoringSystem.GetPlayerDisconnected(leRaceCarIndex))
            {
                CGS_ASSERT(mScoringSystem.GetCarData(leRaceCarIndex) != 0,
                           "mScoringSystem.GetCarData(leRaceCarIndex)");   // :3101

                // The console re-calls GetCarData a THIRD time after the assert and re-tests it --
                // the assert is non-fatal, so the null guard is real control flow, not a duplicate.
                const CarData* lpCarData = mScoringSystem.GetCarData(leRaceCarIndex);
                if (lpCarData != 0)
                {
                    GameStateModuleIO::RemotePlayerDisconnectedAction lDisconnectedAction;
                    // X360 `lwz r4, 0x148(carData)` == CarData::mNetworkPlayerID.
                    lDisconnectedAction.SetNetworkPlayerID(lpCarData->GetNetworkPlayerID());
                    lDisconnectedAction.SetActiveRaceCarIndex(leRaceCarIndex);
                    lpGameActionQueue->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lDisconnectedAction),
                        GameStateModuleIO::E_ACTION_REMOTE_PLAYER_DISCONNECTED,
                        sizeof(lDisconnectedAction));   // X360 li r5,0xB; li r6,8
                }
            }
        }

        // ⚠️ [stuntrace] ONLINE ARM DEFERRED -- the live flyby roster.
        // CONSOLE (asm 0x82343220..0x82343264): picks one of TWO flyby managers embedded in the
        // GameStateModule -- `addis r3,gsm,3` then `addi r3,r3,-0x2720` (gsm+0x2D8E0 == 186592,
        // the ONLINE one) when GameStateModule::IsOnlineGameMode(), else `addi r3,r3,-0x29D0`
        // (gsm+0x2D630 == 185904, the offline one) -- calls its vtable slot 0 (==
        // FlybyManager::GetFlybyData) and memcpy's 592 bytes (== sizeof(FlybyData)) into the
        // action's mFlybyData.
        // WHY DEFERRED: GameStateModule declares NO flyby-manager member or accessor in this tree
        // (grep "flyby" over BrnGameStateModule.h finds only E_PREPARESTAGE_FLYBYMANAGER), and the
        // only header that models FlybyManager -- FlybyManager/BrnGameStateOnlineFlybyManager.h --
        // re-declares its OWN minimal `class GameStateModule` in namespace BrnGameState, so it
        // cannot be included in the same TU as the real BrnGameStateModule.h. Reaching gsm+0x2D8E0
        // by raw offset is exactly the fabrication the wave rules forbid.
        // BEHAVIOUR HELD: the action still leaves here with a VALID, EMPTY roster (Prepare() is the
        // same call the offline path makes) instead of an uninitialised 592-byte stack blob.
        // Consequence: online events show no rival cards in the pre-race flyby. OFFLINE EVENTS --
        // including every stunt race -- are UNAFFECTED; they never take this branch.
        // RE-WIRE WHEN: GameStateModule grows GetFlybyManager()/GetOnlineFlybyManager() (filed).
        lIntroAction.mFlybyData.Prepare();
    }
    else
    {
        // X360 @0x8234326C/0x82343270: FlybyData::Prepare @0x82363A08 on the action's own field.
        lIntroAction.mFlybyData.Prepare();
    }

    // ---- fill in the rest of the record and post it -------------------------------------------
    // X360 @0x82343274..0x823432E4.
    lIntroAction.meGameMode = meCurrentGameModeType;   // `lwz 0xD94` -> var_5C

    // [!!] BOOL-BLOCK EVIDENCE, hazards H4 -- READ BEFORE RENAMING EITHER BYTE.
    // The console copies ModeManager+38151 (0x9507) into this record's mbFinishedOnlineEvent and
    // ModeManager+38152 (0x9508) into its mbFinishedOnlineLobbyMode (`lbzx r26,0x9507 -> var_58`,
    // `lbzx r26,0x9508 -> var_57`, and the DWARF field order at BrnGameActions.h:1177/1178 fixes
    // which is which). That is a NAMING signal for two bytes the frozen header currently spells
    // mbFinishedOnlineEvent (renamed from muUnkByte_0x9507 by the fix round) and
    // mbInstantIntroSplash (PINNED-semantics), and it COLLIDES with
    // the header's provisional mbFinishedOnlineLobbyMode at +38150. The frozen header's names are
    // used verbatim below -- renaming is the conductor's call, not this agent's -- and the
    // collision is filed for the verifier's cross-agent bool-block pass.
    lIntroAction.mbFinishedOnlineEvent     = mbFinishedOnlineEvent;     // +38151 / 0x9507
    lIntroAction.mbFinishedOnlineLobbyMode = mbInstantIntroSplash;      // +38152 / 0x9508

    // Mode vtbl+0x20 == slot 8 == GetIntroDurationSeconds (micro-check slot table). The console
    // keeps the result in f1, stores it to var_2B0 and then compares it against flt_82001CC0.
    // IMAGE-CITED: image.bin offset 0x1CC0 reads 00 00 00 00 == 0.0f, so the test is `> 0.0f`
    // (`fcmpu` + `bgt`, i.e. strictly greater -- a zero-length intro sets mbDoIntro FALSE).
    lIntroAction.mfDurationSeconds = mpCurrentGameMode->GetIntroDurationSeconds();
    lIntroAction.mbDoIntro         = (lIntroAction.mfDurationSeconds > 0.0f);

    lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lIntroAction),
                                GameStateModuleIO::E_ACTION_START_MODE_INTRO,
                                sizeof(lIntroAction));   // X360 li r5,0x1D (29); li r6,0x25C (604)

    // ---- the online-stunt-family training tip -------------------------------------------------
    // X360 @0x823432E8..0x82343330. Tested in THIS order: 14, then 12, then 17 -- the three slots
    // that all alias the ONE OnlineStuntRunMode object (BrnModeManager.h ORDER NOTE 2). 17 is
    // spelled E_MODE_ONLINE_MODE_END in this tree's enum (it aliases E_MODE_COUNT; see the
    // KI_GAME_MODE_SLOTS banner for why the enum is deliberately left at 17).
    if (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_FREE_BURN ||
        meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_FUGITIVE ||
        meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_MODE_END)
    {
        // [x] header_request R4 (LANDED -- see the STATUS stamp at the top of this file): GameStateModuleIO::RequestGameTrainingAction (DWARF
        // BrnGameActions.h:3523; one member, the ETrainingType). The id already exists
        // (E_ACTION_REQUEST_GAME_TRAINING == 149) and the mounted consumer
        // (BrnRaceCarEntityModule.cpp:2469) reads the payload as a bare s32, so the wire format is
        // already pinned at both ends -- only the producer-side record is missing.
        GameStateModuleIO::RequestGameTrainingAction lTrainingAction;
        lTrainingAction.meTrainingType =
            static_cast<BrnProgression::ETrainingType>(KI_TRAINING_TYPE_ONLINE_STUNT_X360);
        lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lTrainingAction),
                                    GameStateModuleIO::E_ACTION_REQUEST_GAME_TRAINING,
                                    sizeof(lTrainingAction));   // X360 li r5,0x95 (149); li r6,4
    }
}

// ============================================================================
// ModeManager::StopModeIntro -- X360 0x82343F38
// ============================================================================
// WHOLE (hazards H7). Called from UpdateCurrentMode's mbIntroJustFinished arm (GameMode +176).
// One 8-byte action; no state is touched.
// ============================================================================
void ModeManager::StopModeIntro(GameStateModuleIO::GameActionQueue* lpGameActionQueue)
{
    CGS_ASSERT(lpGameActionQueue != 0, "lpGameActionQueue != NULL");   // :3958

    // [x] header_request R2 (LANDED -- see the STATUS stamp at the top of this file): GameStateModuleIO::StopModeIntroAction (id 30, 8 bytes; DWARF
    // BrnGameActions.h:1191-1194).
    GameStateModuleIO::StopModeIntroAction lAction;

    // X360 @0x82343F7C `lwz r11, 0xD94(r31)` -> var_20.
    lAction.meGameMode = meCurrentGameModeType;

    // X360 @0x82343F80..0x82343FC4 is the (mode == 15 || mode == 16) && *(this + 0x9508)
    // composite -- byte for byte the same expression the frozen header documents behind
    // IsOnlineModeWithInstantIntro(). De-inlined to that accessor rather than restated, so the
    // two can never drift apart. (Agent 9 bodies it; the header declares it.)
    lAction.mbMovingBetweenLobbyModes = IsOnlineModeWithInstantIntro();

    lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAction),
                                GameStateModuleIO::E_ACTION_STOP_MODE_INTRO,
                                sizeof(lAction));   // X360 li r5,0x1E (30); li r6,8
}

// ============================================================================
// ModeManager::StartPlayingMode -- X360 0x82343340
// ============================================================================
// WHOLE (hazards H7). The handover from the countdown to live play: tell the GUI which landmark
// the player is driving to, and clear the time-up outro latch pair.
// ============================================================================
void ModeManager::StartPlayingMode(GameStateModuleIO::GameActionQueue* lpGameActionQueue)
{
    CGS_ASSERT(lpGameActionQueue != 0, "lpGameActionQueue != NULL");   // :3167

    // [x] header_request R3 (LANDED -- see the STATUS stamp at the top of this file): GameStateModuleIO::StartPlayingModeAction (id 34 -- already in the
    // enum -- 16 bytes; DWARF BrnGameActions.h:1330-1333).
    GameStateModuleIO::StartPlayingModeAction lAction;

    // X360 @0x82343384..0x82343394: `lwz r10, 0xD94` -> var_40 and `std r27(=0), var_38`, i.e. the
    // destination id is zeroed FIRST and only overwritten when the mode has landmarks. A stunt
    // race with no landmark list therefore posts id 0, and that is authored, not a hole.
    lAction.meGameMode             = meCurrentGameModeType;
    lAction.mDestinationLandmarkID = static_cast<CgsID>(0);

    // `lwzx r11, r31, 0x801C` (muNumLandmarks) then `cmplwi` + `ble` -- an UNSIGNED "== 0 skips".
    if (muNumLandmarks > 0)
    {
        CGS_ASSERT(mePlayerGlobalRaceCarIndex > E_GLOBAL_RACE_CAR_INDEX_INVALID &&
                       mePlayerGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT,
                   "( mePlayerGlobalRaceCarIndex > E_GLOBAL_RACE_CAR_INDEX_INVALID ) && "
                   "( mePlayerGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT )");   // :3175

        // X360 @0x823433DC..0x823433F8: GetNextLandmarkIndex (the COMMITTED body,
        // BrnModeManager.cpp:64 -- called, never re-implemented), its result masked to a byte
        // (`clrlwi r11,r3,24`, which that body already does), then `(idx + 0xFC8) * 8` off `this`.
        // 4040 * 8 == 32320 == maLandmarkCgsIDs, so this is the CgsID array indexed by name.
        const s32 liNextLandmark = GetNextLandmarkIndex(mePlayerGlobalRaceCarIndex);
        lAction.mDestinationLandmarkID = maLandmarkCgsIDs[liNextLandmark];
    }

    lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAction),
                                GameStateModuleIO::E_ACTION_START_PLAYING_MODE,
                                sizeof(lAction));   // X360 li r5,0x22 (34); li r6,0x10 (16)

    // X360 @0x82343410..0x8234342C, AFTER the post: `stbx r27(=0), r31, 0x950C` and
    // `stfsx flt_82001CC0, r31, 0x9514`. IMAGE-CITED: image.bin offset 0x1CC0 == 00 00 00 00,
    // so the timer is set to 0.0f, not to some sentinel.
    mbIsInTimeUpOutro  = false;    // +38156 / 0x950C
    mfTimeUpStateTimer = 0.0f;     // +38164 / 0x9514
}

// ============================================================================
// ModeManager::FinishOfflineModeIntro -- X360 0x823119B0
// ============================================================================
// WHOLE (hazards H7). The landing point for GUI event 25 -- the player (or the intro timer)
// dismissing an OFFLINE event intro. Its only caller is GameStateModule::ProcessGameEvents
// @0x823A0A18, and the BridgeGuiToGameState 163 -> 25 relay that reaches it is already live.
//
// [!] function_grouping.md:71 CORRECTED (per the wave's vtable micro-check): the tail is NOT a
// bool-taking hook. `(*(**(this+3480)+48))(mode, 1)` is vtbl+0x30 == slot 12 == SendEvent, and
// the argument is the ENUM E_GME_NEXT (== 1), which GameMode::SendEvent @0x8232FDA0 maps to the
// per-state advance table. Written by name so the slot cannot be mis-bound.
// ============================================================================
void ModeManager::FinishOfflineModeIntro()
{
    // Both asserts are the INLINED predicates, not field reads: the console emits
    // `lwz 0xD98` + null-test (== IsInGameMode()) and then the
    // `mode ? mode->mbIsOnline(+0xAC) : 0` ladder (== IsOnlineGameMode(), the COMMITTED body at
    // BrnModeManager.cpp:37). De-inlined to both accessors.
    CGS_ASSERT(IsInGameMode(), "IsInGameMode()");             // :2521
    CGS_ASSERT(!IsOnlineGameMode(), "!IsOnlineGameMode()");   // :2522

    mpCurrentGameMode->SendEvent(E_GME_NEXT);
}

}
