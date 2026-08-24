// ============================================================================
// b5-decomp/src/GameSource/GameState/TrainingManager/BrnTrainingManager.cpp
// ============================================================================
// Bodies for BrnGameState::TrainingManager -- the Easy Drive training-tip manager.
// Layout home + member docs live in BrnTrainingManager.h. Each function below is
// reconstructed store-for-store from the X360 pseudocode/asm (addresses noted per body);
// every member access is BY NAME against the DWARF-attested, X360-offset-gated layout.
//
// Cross-TU reads (GameStateModule internal state, the embedded RaceCar output interface,
// BrnProgression::Profile training-flag accessors, ProgressionManager::GetProfile,
// GameStateModule::RequestPause) are expressed through the NAMED accessors additively
// declared on those types' minimal-slice homes (each flagged there).
//
// (2026-08-16, tutorial-ticker leg) SendTrainingTickerMessage @0x82388940 is now BODIED here
// -- it was previously declared-only, which is what stopped TriggerAnyFollowOnTrainingTips
// from linking. See its banner below; it is the producer of the bottom-of-screen tutorial
// text the game never shows today.
//
// ⭐⭐ (2026-08-24, [tut-ticker] wave) THE WHOLE MANAGER IS BODIED NOW. The old banner parked
// RequestTraining (0x82365B20) and IsTipAllowedInGameMode (0x823590A0) as "deep, un-homed
// offsets with no recoverable member name". Every one of those offsets has since been
// identified against committed, named members -- none needed fabricating:
//   gsm+42300 / gsm+42304   == mModeManager.mfTimeInFreeBurn / mfTimeInMode
//                              (ModeManager +0x951C/+0x9520; identity proven from the
//                              accumulate/reset writers in ModeManager::PreWorldUpdate
//                              @0x823537B8 -- see BrnModeManager.h)
//   gsm+183744              == mCarSelectManager.mJunkyardId (CarSelectManager +0x20)
//   gsm+232288              == miSimPauseFlags (IsTrainingPauseSuppressed, already named)
//   gsm+7604 / gsm+7608     == mModeManager.meCurrentGameModeType / mpCurrentGameMode
//   gsm+245952              == (the one true gap) a byte with NO writer anywhere in the
//                              30,084-function export set; see ShouldAllowTimedTutorialTips
//   iface+10328/10332/10336 == mePlayerActiveRaceCarIndex / mePlayerEngineState /
//                              mbIsPlayerCarActive (the interface's named trio)
//   profile+108             == mfInCarTimePlayed        profile+42512 == muMedalCountFromTheStart
//   profile+117948          == meCurrentCarType         profile+117952 == maHasPlayerSeenTraining
// Newly bodied this wave: Construct (PS3 0x241DE0), Update (0x823937D0), RequestTraining,
// IsTipAllowedInGameMode, PlayNewAtomikaFreeburnVO (0x82365FC8), and the DriveThruManager
// accessor quartet (IsTipPending / GetProfile / GetTimeSinceLastTip / RequestTip).

#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"

#include "GameSource/GameState/ModeManager/BrnModeManager.h"       // the two mode clocks + current mode
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h" // GameMode::GetCurrentState (the boost-tip gate)
#include "GameSource/GameState/CarSelect/BrnCarSelectManager.h"    // CarSelectManager::GetJunkyardId
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::VariableEventQueue<13312,16>::AddEvent

#include <cstring>   // std::memset (the 48-byte junkyard-drive-thru record)

// The XDK signin probe (PC stub in BrnBaselineLinkStubs.cpp returns 0 == signed out).
extern "C" u32 XUserGetSigninState(u32 luUserIndex);

namespace BrnGameState
{
namespace
{
    // The GameActionQueue concrete type (forward-declared in BrnGameStateModuleIO.h) is the
    // OutputBuffer's variable-size event queue. The X360 invokes
    // VariableEventQueue<13312,16>::AddEvent straight on the GameActionQueue pointer the manager
    // is handed; bridge the opaque queue type to the concrete VEQ (same pattern as MugshotManager).
    inline CgsModule::VariableEventQueue<13312, 16>* AsVeq(GameStateModuleIO::GameActionQueue* lpQueue)
    {
        return reinterpret_cast<CgsModule::VariableEventQueue<13312, 16>*>(lpQueue);
    }

    // ---- GameAction payload type ids the X360 passes to AddEvent (the liType immediates) ----
    // These select which GameAction the gui/sound layer runs when it drains the queue. The records
    // the TrainingManager pushes are all single-byte payloads (the X360 AddEvent liSize immediate
    // is 1) -- a flag/discriminator byte. The full GameAction payload structs live in BrnGameActions.h;
    // a 1-byte stack record matches the X360 image exactly for these queue writes.
    const s32 KI_GAME_ACTION_TRAINING_UNPAUSE = 151;  // ForceUnpause                       (0x97)
    const s32 KI_GAME_ACTION_SHOW_SATNAV      = 190;   // TriggerAnyFollowOnTrainingTips     (0xBE)
    const s32 KI_GAME_ACTION_TRAINING_PAUSE   = 150;   // TriggerAnyFollowOnTrainingTips     (0x96)

    // ⭐ THE TICKER ACTION. SendTrainingTickerMessage's AddEvent immediates are `li r5,0x94;
    // li r6,4` -- type 148, size 4 -- and unlike the three flag actions above its payload is
    // NOT a stack flag byte: it is the 4-byte ETrainingType itself (the asm stores
    // *(this+4) into the stack slot it hands to AddEvent). The consumer is
    // BrnGameModule::TranslateGameActionsToGuiEvents @0x823E9CE0 case 148, which reads that
    // s32, maps it through BrnGame::ConvertTrainingTypeToStringId, and -- when the id is
    // non-NULL -- publishes GUI event 537 (BrnGui::GuiEventTickerCustomMessage) carrying the
    // string id with param type 2.
    const s32 KI_GAME_ACTION_TRAINING_TICKER  = 148;   // SendTrainingTickerMessage          (0x94)

    // The X360 reason bitflag passed to GameStateModule::RequestPause for a training-driven pause.
    const s32 KI_PAUSE_REASON_TRAINING = 64;           // (0x40)

    // [tut-ticker] Update posts the 48-byte "junkyard drive-thru" flag record (same id + size
    // as CarSelectManager::UpdateExitState's post: dword[0] = the flag, rest of the record
    // uninitialised on the console stack; zeroed here). Case 4 (waiting on LEAVES_JUNKYARD)
    // posts it with 0 every tick; finishing the START_ENGINE tip (type 2) posts it with 1.
    const s32 KI_GAME_ACTION_JUNKYARD_DRIVE_THRU = 7;  // size 48

    // ------------------------------------------------------------------------
    // ⭐ [tut-ticker] Update's rodata, READ FROM THE IMAGE (x360 id1 reader, 2026-08-24;
    // range-0 placement verified byte-exact against the model -- see the wave log):
    //   dword_8202AE40: the four ambient timed-tip ids
    //   flt_8202AE50  : their in-car-time thresholds (seconds)
    //   dword_8202AE60: their alternative medal-count triggers
    //   dword_82032308: the per-progression-rank licence tips (rank 1..6 -> TRAFFIC_*_LICENSE)
    // ------------------------------------------------------------------------
    const s32 KAI_TIMED_TIP_TYPES[4]      = { 38, 41, 42, 43 };            // dword_8202AE40
    const f32 KAF_TIMED_TIP_TIME[4]       = { 600.0f, 2700.0f, 5400.0f, 10800.0f }; // flt_8202AE50
    const s32 KAI_TIMED_TIP_MEDALS[4]     = { 1, 8, 20, 40 };              // dword_8202AE60
    const s32 KAI_RANK_LICENCE_TIPS[6]    = { 71, 72, 73, 74, 75, 76 };    // dword_82032308

    // Single-byte GameAction record. The X360 builds a 1-byte stack buffer (the leading byte set to
    // 1 == "active/enable") and hands its address + size 1 to AddEvent.
    struct TrainingFlagGameAction
    {
        u8 mbFlag;   // +0x00 (set to 1 by the producer)
    };
}

// ---------------------------------------------------------------------------
// OnTogglePictureParadise -- X360 0x82359010.
// Latches the "in Picture Paradise" flag (the X360 stores the bool arg at this+0x14).
// ---------------------------------------------------------------------------
void TrainingManager::OnTogglePictureParadise(bool lbActive)
{
    mbInPictureParadise = lbActive;
}

// ---------------------------------------------------------------------------
// OnEnableTrainingTips -- X360 0x82359018.
// Latches the global "training tips enabled" flag (the X360 stores the bool arg at this+0x16).
// ---------------------------------------------------------------------------
void TrainingManager::OnEnableTrainingTips(bool lbActive)
{
    mbTipsEnabled = lbActive;
}

// ---------------------------------------------------------------------------
// OnVoiceoverFinished -- X360 0x82359020.
// The sound layer's "training voiceover finished" hook. Only acts if a message has been
// playing for more than 1 second (mfStateTime > 1.0) and the FSM is mid-message; then asserts
// the FSM is in a playing/waiting-for-unpause state and marks the voiceover finished so the
// next Update can advance the FSM.
// ---------------------------------------------------------------------------
void TrainingManager::OnVoiceoverFinished()
{
    if (mfStateTime > 1.0f)
    {
        if (meTrainingState != E_TRAINING_STATE_INACTIVE)
        {
            CGS_ASSERT(meTrainingState == E_TRAINING_STATE_PLAYING_MESSAGE ||
                       meTrainingState == E_TRAINING_STATE_WAITINGFORUNPAUSE,
                       "meTrainingState == E_TRAINING_STATE_PLAYING_MESSAGE || "
                       "meTrainingState == E_TRAINING_STATE_WAITINGFORUNPAUSE");
            mbVoiceoverFinishedLastFrame = true;
        }
    }
}

// ---------------------------------------------------------------------------
// DoesTrainingPauseGame -- X360 0x823593C0.
// Never pauses in an online game mode; otherwise a fixed allow-list of "intro / mode
// explanation" tip types pause the world, the rest do not. (The X360 lowers the allow-list to
// a jump table over leTrainingType; reproduced here as an explicit case set.)
// ---------------------------------------------------------------------------
bool TrainingManager::DoesTrainingPauseGame(BrnProgression::ETrainingType leTrainingType)
{
    if (mpGameStateModule->IsOnlineGameMode())
        return false;

    switch (leTrainingType)
    {
        case BrnProgression::E_TRAINING_TYPE_LEAVES_JUNKYARD:        // 0
        case BrnProgression::E_TRAINING_TYPE_MAP_APPEARS:           // 1
        case BrnProgression::E_TRAINING_TYPE_START_ENGINE:          // 2
        case BrnProgression::E_TRAINING_TYPE_DISCOVERS_EVENT:       // 8
        case BrnProgression::E_TRAINING_TYPE_POINT_TO_POINT_EVENT:  // 9
        case BrnProgression::E_TRAINING_TYPE_OTHER_DRIVER:          // 16
        case BrnProgression::E_TRAINING_TYPE_CORRECT_CAR_FOR_CHALLENGE: // 24
        case BrnProgression::E_TRAINING_TYPE_WRONG_CAR_FOR_CHALLENGE:   // 25
        case BrnProgression::E_TRAINING_TYPE_ROAD_RULES_ON:         // 26
        case BrnProgression::E_TRAINING_TYPE_ROAD_RULES_FORCE_ON:   // 27
        case BrnProgression::E_TRAINING_TYPE_CRASH_ROAD_RULES_ON:   // 28
        case BrnProgression::E_TRAINING_TYPE_MAP_INFORMATION:       // 38
        case BrnProgression::E_TRAINING_TYPE_TAKEDOWN:              // 40
        case BrnProgression::E_TRAINING_TYPE_INTRO_TO_ONLINE_1:     // 41
            return true;
        default:
            return false;
    }
}

// ---------------------------------------------------------------------------
// ForceUnpause -- X360 0x823888F0.
// If the current training type would have paused the game, push the "training unpause"
// GameAction onto the queue. (The X360 builds a 1-byte stack record -- left uninitialised in the
// pseudocode -- and AddEvents it with type 151 / size 1.)
// ---------------------------------------------------------------------------
void TrainingManager::ForceUnpause(GameStateModuleIO::GameActionQueue* lpGameActionQueue)
{
    if (DoesTrainingPauseGame(meCurrentTrainingType))
    {
        TrainingFlagGameAction lAction;
        AsVeq(lpGameActionQueue)->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lAction),
            KI_GAME_ACTION_TRAINING_UNPAUSE, (s32)sizeof(TrainingFlagGameAction));
    }
}

// ---------------------------------------------------------------------------
// TriggerAnyFollowOnTrainingTips -- X360 0x823889C8.
// When a tip finishes, fire any follow-on tip it chains to:
//   * after LEAVES_JUNKYARD (0): request MAP_APPEARS (1), then push a "show sat-nav" GameAction.
//   * after MAP_APPEARS (1):     request START_ENGINE (2).
// Then, if a tip is now pending (meTrainingState == PENDING_MESSAGE), send its ticker message,
// advance the FSM to PLAYING_MESSAGE, and -- unless a pause is suppressed -- if the newly-latched
// tip pauses the game while the just-finished one did NOT, request the pause and push the
// "training pause" GameAction.
// ---------------------------------------------------------------------------
void TrainingManager::TriggerAnyFollowOnTrainingTips(
    BrnProgression::ETrainingType leFinishedTrainingType,
    GameStateModuleIO::GameActionQueue* lpGameActionQueue)
{
    if (leFinishedTrainingType == BrnProgression::E_TRAINING_TYPE_LEAVES_JUNKYARD)
    {
        RequestTraining(BrnProgression::E_TRAINING_TYPE_MAP_APPEARS);

        TrainingFlagGameAction lShowSatNavAction;
        lShowSatNavAction.mbFlag = 1;
        AsVeq(lpGameActionQueue)->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lShowSatNavAction),
            KI_GAME_ACTION_SHOW_SATNAV, (s32)sizeof(TrainingFlagGameAction));
    }
    else if (leFinishedTrainingType == BrnProgression::E_TRAINING_TYPE_MAP_APPEARS)
    {
        RequestTraining(BrnProgression::E_TRAINING_TYPE_START_ENGINE);
    }

    if (meTrainingState == E_TRAINING_STATE_PENDING_MESSAGE)
    {
        SendTrainingTickerMessage(lpGameActionQueue);
        meTrainingState = E_TRAINING_STATE_PLAYING_MESSAGE;

        if (!mpGameStateModule->IsTrainingPauseSuppressed())
        {
            if (DoesTrainingPauseGame(meCurrentTrainingType) &&
                !DoesTrainingPauseGame(leFinishedTrainingType))
            {
                mpGameStateModule->RequestPause(KI_PAUSE_REASON_TRAINING, lpGameActionQueue, 0, 0);

                TrainingFlagGameAction lAction;
                AsVeq(lpGameActionQueue)->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lAction),
                    KI_GAME_ACTION_TRAINING_PAUSE, (s32)sizeof(TrainingFlagGameAction));
            }
        }
    }
}

// ---------------------------------------------------------------------------
// SendTrainingTickerMessage -- X360 0x82388940 (BrnTrainingManager.cpp:794).
//
// ⭐ THIS IS THE FUNCTION THAT ASKS FOR THE BOTTOM-OF-SCREEN TUTORIAL TEXT. It was
// declared-only in the header ("body lands when SendTrainingTickerMessage is
// reconstructed"), so TriggerAnyFollowOnTrainingTips called a symbol no TU defined and this
// whole TU could never be mounted. Bodied here store-for-store; it reaches nothing this TU
// does not already own.
//
// The X360, in order:
//   1. copies meCurrentTrainingType (*(this+4)) into a 4-byte stack slot and AddEvents it
//      onto the GameAction queue as type 148 / size 4,
//   2. forms the Profile as mpProgressionManager + 368 (GetProfile inlined, the same
//      `+368` DEBUG_ClearTrainingFlags reaches through the named accessor) and asserts it
//      is non-NULL (the `v3 == -368` test is the compiler's null check on the base),
//   3. marks the tip already-seen: Profile::SetTrainingAlreadySeen(meCurrentTrainingType).
//
// ⚠️ THE MARK-AS-SEEN IS PART OF THE SEND, NOT OF THE FSM. It happens here, before the
// message has been shown, so a tip that is queued once is never queued again even if the
// ticker drops it. Keep them together.
// ---------------------------------------------------------------------------
void TrainingManager::SendTrainingTickerMessage(GameStateModuleIO::GameActionQueue* lpGameActionQueue)
{
    // The payload is the training type itself (4 bytes), not a flag record.
    BrnProgression::ETrainingType leTrainingType = meCurrentTrainingType;
    AsVeq(lpGameActionQueue)->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&leTrainingType),
        KI_GAME_ACTION_TRAINING_TICKER, (s32)sizeof(leTrainingType));

    BrnProgression::Profile* lpProfile = mpProgressionManager->GetProfile();
    CGS_ASSERT(lpProfile, "lpProfile");   // BrnTrainingManager.cpp:794
    lpProfile->SetTrainingAlreadySeen(meCurrentTrainingType);
}

// ---------------------------------------------------------------------------
// DEBUG_ClearTrainingFlags -- X360 0x82366050.
// DEBUG-only: clear the player's persisted training flags via the Profile. The X360 reaches the
// Profile through the ProgressionManager back-pointer (asserting it is non-null) and zeroes the
// training-flag bitfield region; that region's exact layout belongs to the Profile TU, so this
// body calls the named Profile::ClearTrainingFlags accessor.
// ---------------------------------------------------------------------------
void TrainingManager::DEBUG_ClearTrainingFlags()
{
    BrnProgression::Profile* lpProfile = mpProgressionManager->GetProfile();
    CGS_ASSERT(lpProfile, "lpProfile");
    lpProfile->ClearTrainingFlags();
}

// ---------------------------------------------------------------------------
// ⭐ Construct -- PS3 DecFIGS 0x241DE0 (the X360 inlines it; no export exists). Store-for-store:
//   +16 mfStateTime = 0            +4  meCurrentTrainingType = -1     +22 mbTipsEnabled = 1
//   +8  mpProgressionManager       +12 mpGameStateModule              +28 mfLastBoostMessagePlayTime = -600
//   +36 mbGotAirBefore = 0         +0  meTrainingState = 0            +20 mbInPictureParadise = 0
//   +21 mbIsOnlinePossible = 0     +23 mbVoiceoverFinishedLastFrame = 0
//   +24 mfLastMessageFinishedTime = 0                 +32 miNextAtomikaFreeburnVoIndex = 0
// (the -600 seed lets the first boost tip pass its 600-second spacing gate immediately.)
// ---------------------------------------------------------------------------
void TrainingManager::Construct(BrnProgression::ProgressionManager* lpProgressionManager,
                                GameStateModule* lpGameStateModule)
{
    mfStateTime                  = 0.0f;
    meCurrentTrainingType        = static_cast<BrnProgression::ETrainingType>(-1);
    mbTipsEnabled                = true;
    mpProgressionManager         = lpProgressionManager;
    mpGameStateModule            = lpGameStateModule;
    mfLastBoostMessagePlayTime   = -600.0f;
    mbGotAirBefore               = false;
    meTrainingState              = E_TRAINING_STATE_INACTIVE;
    mbInPictureParadise          = false;
    mbIsOnlinePossible           = false;
    mbVoiceoverFinishedLastFrame = false;
    mfLastMessageFinishedTime    = 0.0f;
    miNextAtomikaFreeburnVoIndex = 0;
}

// ---------------------------------------------------------------------------
// ⭐⭐ Update -- X360 0x823937D0. The training FSM tick; every arm transcribed from the
// pseudocode/asm with the raw reads resolved to the named members listed in the file banner.
// ---------------------------------------------------------------------------
void TrainingManager::Update(GameStateModuleIO::GameActionQueue* lpGameActionQueue,
                             const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
                                 lpcActiveRaceCarInterface,
                             f32 lfGameTimestep, bool lbAllowTimedTips)
{
    // The console's interface parameter (r6 == gsm+235488) is carried but not read by this
    // body's own arms (the reads go through mpGameStateModule); kept for the console signature.
    (void)lpcActiveRaceCarInterface;

    // `v12 = a4 + *(this+16); *(this+16) = v12` -- accumulate first, and the case-2 test uses
    // the ALREADY-ACCUMULATED value.
    mfStateTime += lfGameTimestep;

    switch (meTrainingState)
    {
        case E_TRAINING_STATE_INACTIVE:   // 0 -- the ambient timed-tip scan
        {
            BrnProgression::Profile* lpProfile = mpProgressionManager->GetProfile();
            CGS_ASSERT(lpProfile, "lpProfile");   // BrnTrainingManager.cpp:184

            // [FLAG PC deviation, named in the header] the console reads the controller
            // interface's user index off the PreWorldInputBuffer; the PC probes user 0
            // against the stubbed XDK (0 == signed out) -- mbIsOnlinePossible stays false.
            const u32 luSigninState = XUserGetSigninState(0);
            mbIsOnlinePossible = (luSigninState == 2);

            // `v17 = ((mbInPictureParadise == 0) & a7) == 0; if (!v17) { ... }`
            if (!mbInPictureParadise && lbAllowTimedTips)
            {
                for (u32 luIndex = 0; luIndex < 4; ++luIndex)
                {
                    const BrnProgression::ETrainingType leTimedType =
                        static_cast<BrnProgression::ETrainingType>(KAI_TIMED_TIP_TYPES[luIndex]);
                    if (!lpProfile->HasPlayerSeenTrainingType(leTimedType) &&
                        (lpProfile->GetInCarTimePlayed() > KAF_TIMED_TIP_TIME[luIndex] ||
                         lpProfile->GetMedalCountFromTheStart() ==
                             static_cast<u32>(KAI_TIMED_TIP_MEDALS[luIndex])))
                    {
                        RequestTraining(leTimedType);
                    }
                }

                if ((lpProfile->GetInCarTimePlayed() - mfLastMessageFinishedTime) > 600.0f &&
                    mpGameStateModule->GetModeManager()->GetTimeInFreeBurn() > 5.0f)
                {
                    PlayNewAtomikaFreeburnVO();
                }
            }

            // The per-rank licence tip (rank 1..6 -> TRAFFIC_D.._ELITE_LICENSE).
            const s32 liRank = mpProgressionManager->GetProgressionRank();
            if (liRank > 0)
            {
                const BrnProgression::ETrainingType leRankTip =
                    static_cast<BrnProgression::ETrainingType>(KAI_RANK_LICENCE_TIPS[liRank - 1]);
                if (!lpProfile->HasPlayerSeenTrainingType(leRankTip) &&
                    mpGameStateModule->GetModeManager()->GetTimeInFreeBurn() > 5.0f)
                {
                    RequestTraining(leRankTip);
                }
            }
            break;
        }

        case E_TRAINING_STATE_PENDING_MESSAGE:   // 1 -- send the ticker (or swallow the tip)
        {
            // The junkyard/licence intro tips force through even with tips disabled:
            // jpt cases {0,1,2,0x46..0x4C} -> forced.
            bool lbShowMessage = mbTipsEnabled;
            switch (meCurrentTrainingType)
            {
                case 0: case 1: case 2:
                case 0x46: case 0x47: case 0x48: case 0x49: case 0x4A: case 0x4B: case 0x4C:
                    lbShowMessage = true;
                    break;
                default:
                    break;
            }

            if (lbShowMessage)
            {
                SendTrainingTickerMessage(lpGameActionQueue);
                meTrainingState = E_TRAINING_STATE_PLAYING_MESSAGE;
                mfStateTime     = 0.0f;

                if (!mpGameStateModule->IsTrainingPauseSuppressed())
                {
                    if (DoesTrainingPauseGame(meCurrentTrainingType))
                    {
                        TrainingFlagGameAction lAction;   // 1 byte, console-uninitialised
                        AsVeq(lpGameActionQueue)->AddEvent(
                            reinterpret_cast<const CgsModule::Event*>(&lAction),
                            KI_GAME_ACTION_TRAINING_PAUSE, (s32)sizeof(TrainingFlagGameAction));
                    }
                }
            }
            else
            {
                BrnProgression::Profile* lpProfile = mpProgressionManager->GetProfile();
                CGS_ASSERT(lpProfile, "lpProfile");   // :283
                lpProfile->SetTrainingAlreadySeen(meCurrentTrainingType);
                mfStateTime     = 0.0f;
                meTrainingState = E_TRAINING_STATE_INACTIVE;
            }
            break;
        }

        case E_TRAINING_STATE_PLAYING_MESSAGE:   // 2 -- wait for the voiceover / the 16s cap
        {
            if (mfStateTime > 16.0f || mbVoiceoverFinishedLastFrame)
            {
                const BrnProgression::ETrainingType leFinishedType = meCurrentTrainingType;
                meTrainingState       = E_TRAINING_STATE_INACTIVE;
                meCurrentTrainingType = static_cast<BrnProgression::ETrainingType>(-1);

                TriggerAnyFollowOnTrainingTips(leFinishedType, lpGameActionQueue);

                // Unpause only when the finished tip paused and the tip the follow-on may have
                // just latched does NOT pause (the console re-reads this+4 after the call).
                if (DoesTrainingPauseGame(leFinishedType) &&
                    !DoesTrainingPauseGame(meCurrentTrainingType))
                {
                    TrainingFlagGameAction lAction;
                    AsVeq(lpGameActionQueue)->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lAction),
                        KI_GAME_ACTION_TRAINING_UNPAUSE, (s32)sizeof(TrainingFlagGameAction));
                }

                // Finishing the START_ENGINE tip closes the junkyard-drive-thru flag record.
                if (leFinishedType == BrnProgression::E_TRAINING_TYPE_START_ENGINE)
                {
                    u8 lacDriveThru[48];
                    std::memset(lacDriveThru, 0, sizeof(lacDriveThru));
                    *reinterpret_cast<s32*>(lacDriveThru) = 1;   // v28[0] = 1 (v29/byte 44 = 0)
                    AsVeq(lpGameActionQueue)->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(lacDriveThru),
                        KI_GAME_ACTION_JUNKYARD_DRIVE_THRU, 48);
                }

                // *(this+24) = *(mpProgressionManager + 476) == the Profile's in-car time.
                mfLastMessageFinishedTime = mpProgressionManager->GetProfile()->GetInCarTimePlayed();
                mfStateTime = 0.0f;
            }
            mbVoiceoverFinishedLastFrame = false;   // `*(v7+23) = 0`, every tick in this state
            break;
        }

        case E_TRAINING_STATE_WAIT_FOR_MESSAGE:  // 4 -- the LEAVES_JUNKYARD settle wait
        {
            if (meCurrentTrainingType == BrnProgression::E_TRAINING_TYPE_LEAVES_JUNKYARD)
            {
                u8 lacDriveThru[48];
                std::memset(lacDriveThru, 0, sizeof(lacDriveThru));   // v28[0] = 0, v29 = 0
                AsVeq(lpGameActionQueue)->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(lacDriveThru),
                    KI_GAME_ACTION_JUNKYARD_DRIVE_THRU, 48);

                if (mpGameStateModule->GetModeManager()->GetTimeInFreeBurn() > 3.0f)
                {
                    meTrainingState = E_TRAINING_STATE_PENDING_MESSAGE;
                }
            }
            break;
        }

        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// ⭐ RequestTraining -- X360 0x82365B20, transcribed whole (the boost arm included). See the
// file banner for the offset->member identities.
// ---------------------------------------------------------------------------
void TrainingManager::RequestTraining(BrnProgression::ETrainingType leTrainingType)
{
    const u32 luType = static_cast<u32>(leTrainingType);
    if (luType >= 0x100u)
    {
        CGS_ASSERT(false,
                   "leTrainingType >= 0 && leTrainingType < BrnProgression::E_TRAINING_TYPE_COUNT"); // :355
    }

    // `if (!*v3 && !*(v3+20) && (v2 != 8 || *(gsm+42300) >= 30.0))`
    if (meTrainingState != E_TRAINING_STATE_INACTIVE || mbInPictureParadise)
    {
        return;
    }
    if (luType == BrnProgression::E_TRAINING_TYPE_DISCOVERS_EVENT &&
        mpGameStateModule->GetModeManager()->GetTimeInFreeBurn() < 30.0f)
    {
        return;
    }

    if (!IsTipAllowedInGameMode(leTrainingType))
    {
        return;
    }

    BrnProgression::Profile* lpProfile = mpProgressionManager->GetProfile();
    CGS_ASSERT(lpProfile, "lpProfile");   // :382

    u32 luResolvedType = luType;
    if (luType == 50u)
    {
        // ---- the boost-tip eligibility gauntlet (all reads named; see the banner) ----
        if ((lpProfile->GetInCarTimePlayed() - mfLastBoostMessagePlayTime) < 600.0f)
        {
            return;
        }
        // `v6 = *(gsm+183744); if (v6) fail` -- a junkyard flow is active.
        if (mpGameStateModule->GetCarSelectManager()->GetJunkyardId() != 0)
        {
            return;
        }
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpcIface =
            mpGameStateModule->GetLastActiveRaceCarInterface();
        if (!lpcIface->IsPlayerCarActive())
        {
            return;
        }
        // `if (*(gsm+245952) || *(gsm+232288)) fail` -- the first read is the no-writer byte
        // (see ShouldAllowTimedTutorialTips' FLAG; reads as 0), the second is the pause mask.
        if (mpGameStateModule->IsTrainingPauseSuppressed())
        {
            return;
        }
        // `v8 = *(gsm+7608); v9 = v8 && *(v8+40) == 2; if (!v9 ...) fail` -- a game mode must be
        // RUNNING (GameMode::meCurrentState @+0x28 == 2).
        const GameMode* lpcMode = mpGameStateModule->GetModeManager()->GetCurrentGameMode();
        if (lpcMode == 0 || lpcMode->GetCurrentState() != 2)
        {
            return;
        }
        if (mpGameStateModule->GetModeManager()->GetTimeInMode() < 30.0f)
        {
            return;
        }
        if (lpProfile->GetInCarTimePlayed() > 2400.0f)
        {
            return;
        }
        // `if (!*(profile+117948)) { boost must be FULL }` -- only the Burnout-2-boost
        // (E_CARTYPE_DANGER == 0) cars gate on a full bar.
        if (lpProfile->GetCurrentCarType() == 0)
        {
            const BrnWorld::RaceCarEntityModuleIO::BoostOutputInfo*
                lpcBoostInfo = lpcIface->GetBoostOutputInfoN(lpcIface->GetPlayerActiveRaceCarIndex());
            CGS_ASSERT(lpcBoostInfo, "lpBoostInfo");   // :412
            if (!lpcBoostInfo->mbBoostIsFull)          // `*(v11+28)`
            {
                return;
            }
        }
        // Seen 50 already? walk 51..52 for the next unseen flavour.
        if (lpProfile->HasPlayerSeenTrainingType(static_cast<BrnProgression::ETrainingType>(50)))
        {
            u32 luNext = 50u;
            for (;;)
            {
                if (++luNext > 52u)
                {
                    return;
                }
                if (!lpProfile->HasPlayerSeenTrainingType(
                        static_cast<BrnProgression::ETrainingType>(luNext)))
                {
                    break;
                }
            }
            luResolvedType = luNext;
        }
    }

    const BrnProgression::ETrainingType leResolvedType =
        static_cast<BrnProgression::ETrainingType>(luResolvedType);
    if (lpProfile->HasPlayerSeenTrainingType(leResolvedType))
    {
        return;
    }

    // The 5-second spacing gate, bypassed for the intro/licence family {0,1,2,7,17,33,39}.
    switch (luResolvedType)
    {
        case 0: case 1: case 2: case 7: case 17: case 33: case 39:
            break;
        default:
            if ((lpProfile->GetInCarTimePlayed() - mfLastMessageFinishedTime) < 5.0f)
            {
                return;
            }
            break;
    }

    // Per-type prerequisite gates, expressed through the seen-bit they test:
    //   type 4  : `(*(profile+117956) & 1) == 0`      == !seen(0)  -> fail (junkyard drive-thru
    //             tip only after the LEAVES_JUNKYARD intro tip)
    //   41/42/43: `v12 & ror(1,5)`  == seen(27) -> fail; 41 also `v12 & 2` == seen(1) -> fail
    if (luResolvedType == 4u)
    {
        if (!lpProfile->HasPlayerSeenTrainingType(
                BrnProgression::E_TRAINING_TYPE_LEAVES_JUNKYARD))
        {
            return;
        }
    }
    else if (luResolvedType == 41u || luResolvedType == 42u || luResolvedType == 43u)
    {
        if (lpProfile->HasPlayerSeenTrainingType(
                BrnProgression::E_TRAINING_TYPE_ROAD_RULES_FORCE_ON))   // bit 27
        {
            return;
        }
        if (luResolvedType == 41u &&
            lpProfile->HasPlayerSeenTrainingType(
                BrnProgression::E_TRAINING_TYPE_MAP_APPEARS))           // bit 1
        {
            return;
        }
    }

    meCurrentTrainingType = leResolvedType;

    if (luResolvedType != 0u)
    {
        // type 58 (TRY_A_FLAT_SPIN): the FIRST air is swallowed (and swallowed for good once
        // the autorepair tip -- seen-bit 3 -- has played): `(*(profile+117956) & 8) || !gotAir`.
        if (luResolvedType == 58u &&
            (lpProfile->HasPlayerSeenTrainingType(
                 BrnProgression::E_TRAINING_TYPE_FIRST_USE_AUTOREPAIR) ||
             !mbGotAirBefore))
        {
            mbGotAirBefore = true;
        }
        else
        {
            if (luResolvedType >= 50u && luResolvedType <= 54u)
            {
                mfLastBoostMessagePlayTime = lpProfile->GetInCarTimePlayed();
            }
            meTrainingState = E_TRAINING_STATE_PENDING_MESSAGE;
        }
    }
    else
    {
        meTrainingState = E_TRAINING_STATE_WAIT_FOR_MESSAGE;   // LEAVES_JUNKYARD settles first
    }
}

// ---------------------------------------------------------------------------
// ⭐ IsTipAllowedInGameMode -- X360 0x823590A0, transcribed whole.
// ---------------------------------------------------------------------------
bool TrainingManager::IsTipAllowedInGameMode(BrnProgression::ETrainingType leTrainingType) const
{
    const s32 liType = static_cast<s32>(leTrainingType);

    // `if (a2 != 41 && a2 != 42 && a2 != 43 || *(this+21))` -- the online-intro tips need
    // online to be possible at all.
    if ((liType == 41 || liType == 42 || liType == 43) && !mbIsOnlinePossible)
    {
        return false;
    }

    // Type 38 (MAP_INFORMATION) additionally needs 5s of free-burn and the player's engine
    // RUNNING (iface+10328 != -1 && iface+10332 == 2 == E_ACTIVE_RACE_CAR_ENGINE_STATE_RUNNING).
    if (liType == 38)
    {
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpcIface =
            mpGameStateModule->GetLastActiveRaceCarInterface();
        // `*(iface+10328) != -1 && *(iface+10332) == 2` -- the player slot is set and its
        // engine state is RUNNING (the IsRaceCarEngineOn inline collapses to exactly that
        // when handed the player's own index).
        const bool lbEngineRunning =
            (lpcIface->GetPlayerActiveRaceCarIndex() != ::E_ACTIVE_RACE_CAR_INDEX_INVALID) &&
            lpcIface->IsRaceCarEngineOn(lpcIface->GetPlayerActiveRaceCarIndex());
        if (!(mpGameStateModule->GetModeManager()->GetTimeInFreeBurn() >= 5.0f &&
              lbEngineRunning))
        {
            return false;
        }
    }

    // The mode-type dispatch (`v5 = *(gsm+7604)`).
    const s32 liModeType = static_cast<s32>(
        mpGameStateModule->GetModeManager()->GetCurrentGameModeType());
    switch (liModeType)
    {
        case -1:   // E_MODE_NONE (free roam)
        case 15:   // the free-burn lobby
        {
            if (liModeType == 15 ||
                mpGameStateModule->GetModeManager()->GetTimeInFreeBurn() < 5.0f)
            {
                // The "noob window": the challenge/online-intro tips are held back.
                switch (liType)
                {
                    case 8: case 24: case 25: case 41: case 42: case 43:
                        return false;
                    default:
                        return liType != 50;
                }
            }
            return liType != 50;
        }

        case 2:    // the two Showtime modes: only the mode-relevant tip set
        case 16:
        {
            if (liType == 20)
            {
                return false;
            }
            if (liType == 28)
            {
                return true;
            }
            if (liType == 50)
            {
                // The console's `if (a2 != 50) goto LABEL_17; break;` -- the break lands past
                // the whole dispatch on the trailing `return 0`, so the boost tip is NOT
                // allowed in Showtime even though 50 sits in the shared list below.
                return false;
            }
            break;   // -> LABEL_17
        }

        default:
            break;     // -> LABEL_17
    }

    // LABEL_17 -- the shared "allowed while a mode is running" list.
    switch (liType)
    {
        case 9: case 10: case 11: case 17: case 18: case 19: case 22: case 23:
        case 30: case 31: case 39: case 40: case 46: case 48: case 50: case 56:
        case 57: case 62: case 77:
            return true;
        default:
            return false;
    }
}

// ---------------------------------------------------------------------------
// PlayNewAtomikaFreeburnVO -- X360 0x82365FC8. Walk the 108 Atomika free-burn VO tips
// (ids 128 + miNextAtomikaFreeburnVoIndex ..) for the next unseen one and request it.
// ---------------------------------------------------------------------------
void TrainingManager::PlayNewAtomikaFreeburnVO()
{
    BrnProgression::Profile* lpProfile = mpProgressionManager->GetProfile();

    if (miNextAtomikaFreeburnVoIndex < 108)
    {
        for (;;)
        {
            const s32 liVoTip = miNextAtomikaFreeburnVoIndex + 128;
            if (!lpProfile->HasPlayerSeenTrainingType(
                    static_cast<BrnProgression::ETrainingType>(liVoTip)))
            {
                RequestTraining(static_cast<BrnProgression::ETrainingType>(liVoTip));
                ++miNextAtomikaFreeburnVoIndex;
                return;
            }
            if (++miNextAtomikaFreeburnVoIndex >= 108)
            {
                return;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// [tut-ticker] the DriveThruManager accessor quartet (the header's additive-grow block said
// "bodies land with this TU" -- this is the TU landing). Each is the X360-inlined raw read
// its comment in the header names.
// ---------------------------------------------------------------------------
bool TrainingManager::IsTipPending() const
{
    return meTrainingState != E_TRAINING_STATE_INACTIVE;   // `*(this+0)` non-zero
}

BrnProgression::Profile* TrainingManager::GetProfile()
{
    return mpProgressionManager->GetProfile();             // `*(this+8) + 368`
}

f32 TrainingManager::GetTimeSinceLastTip() const
{
    // `(Profile+108) - (this+24)`
    return mpProgressionManager->GetProfile()->GetInCarTimePlayed() - mfLastMessageFinishedTime;
}

void TrainingManager::RequestTip(BrnProgression::ETrainingType leType)
{
    meTrainingState       = E_TRAINING_STATE_PENDING_MESSAGE;  // `*(this+0) = 1`
    meCurrentTrainingType = leType;                            // `*(this+4) = type`
}
}
