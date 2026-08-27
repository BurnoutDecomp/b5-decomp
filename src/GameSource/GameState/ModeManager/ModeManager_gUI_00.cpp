// b5-decomp/src/GameSource/GameState/ModeManager/ModeManager_gUI_00.cpp
//
// Partfile of the BrnGameState::ModeManager TU (owning header BrnModeManager.h).
// FOUR bodies live here: GetScoringSystem (both overloads), ConstructInterModeStateBringUp and
// PreWorldUpdateClocksBringUp.
//
// ============================================================================================
// WHY THIS PARTFILE STILL EXISTS AFTER THE WAVE-B KEYSTONE (reconciled 2026-08-26)
// ============================================================================================
// It is the ONLY ModeManager artefact mounted in tools/build/build_game_exe.bat (line 4173).
// Everything else in this directory -- BrnModeManager.cpp's sixteen committed bodies, the wave-B
// partfiles, the fourteen game-mode TUs, the ScoringSystem TUs -- is unmounted. That single fact
// decides this file's shape: a body here may call ONLY things that are already mounted or
// header-inline, because an unmounted callee from here is an unresolved external in the shipping
// link, not a compile error the gate would catch.
//
// [!] SUPERSESSION STATUS, both bring-up legs (hazards H1):
//
//   ConstructInterModeStateBringUp  <-  superseded ON PAPER by ModeManager::Construct @0x82340008,
//       which is now BODIED in BrnModeManager_Lifecycle.cpp -- and which has NO CALLER, on purpose.
//       Four of Construct's eight arguments are not reachable at GameStateModule::Construct's call
//       position (NetworkRoundManager and MugshotManager have no GameStateModule member at all;
//       RoadRulesManager is DWARF-declared but not reconstructed; mAchievementManager is an
//       AchievementManagerX360 while StuntModeScoring::AchievementManager is typedef'd to the PS3
//       sibling, so its address does not convert). The full argument-by-argument evidence is in
//       that body's banner. So this seam remains THE ONE ARMED CONSTRUCTION PATH -- the two are
//       never both armed, which is the rule conductor decision #1 set.
//       DELETE-WHEN all four land: swap the call site at BrnGameStateModule.cpp:134 to the real
//       Construct and delete this method with its declaration.
//
//   PreWorldUpdateClocksBringUp     <-  superseded ON PAPER by ModeManager::PreWorldUpdate
//       @0x823537B8 (agent 7a, this wave). Same rule: the day the full PreWorldUpdate is mounted
//       AND called from BrnGameStateModule.cpp:1268, this method and its declaration go. Until
//       then it is the only thing accumulating the three mode clocks.
//
//   GetScoringSystem (x2)           <-  NOT superseded and NOT duplicated. BrnModeManager.cpp does
//       not body them, and StuntManager::ProcessStuntElement @0x8239CDB0 asserts
//       `mpModeManager->GetScoringSystem()` before calling ScoringSystem::DealWithStunt through it,
//       so the symbol is an UNDEF external in StuntManager_gUI_00.obj. They stay here until this
//       whole TU mounts.
//
// (i) THE CONSOLE EMITS NO SYMBOL FOR GetScoringSystem -- there is no such entry in
// progress/identity.json or the export set, because the X360 inlines it everywhere: each game-mode
// body reaches the ScoringSystem the ModeManager embeds BY VALUE at ModeManager+0xDB0 as a direct
// `this + 0xDB0` pointer adjust (e.g. OnlineRaceMode::PreWorldUpdate / GetOutroTimeout). The
// declaration in BrnModeManager.h is this repo's de-inlining of that adjust; these are its bodies,
// and they are exactly the adjust and nothing more.
//
// [!] DUPLICATE-SYMBOL WATCH: if BrnModeManager.cpp is ever bodied WITH these two, delete this
// partfile (and its mount line) rather than letting both exist.
#include "GameSource/GameState/ModeManager/BrnModeManager.h"

#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"   // ScoringSystem (embedded by value)
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"      // [tut-ticker] GameMode::IsOnline (the clock leg)
#include "GameSource/GameState/BrnGameStateModule.h"                     // [tut-ticker] GameStateModule accessors (the clock leg)
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"     // [tut-ticker] TrainingManager::IsInPictureParadise
#include "GameSource/GameState/CarSelect/BrnCarSelectManager.h"          // [tut-ticker] CarSelectManager::GetJunkyardId
#include "GameSource/GameState/NetworkRoundManager/BrnNetworkRoundManager.h" // [stuntrace] the bring-up round-manager stand-in
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT

namespace BrnGameState
{

ScoringSystem* ModeManager::GetScoringSystem()
{
    return &mScoringSystem;
}

const ScoringSystem* ModeManager::GetScoringSystem() const
{
    return &mScoringSystem;
}

// ----------------------------------------------------------------------------
// [tut-ticker] ConstructInterModeStateBringUp -- the extracted INTER-MODE SEED stores of
// ModeManager::Construct @0x82340008 (sole caller GameStateModule::Construct).
//
// It is a strict SUBSET of the real Construct, chosen by one rule: every store here must be
// self-contained. Anything in Construct that needs an unmounted callee -- the 18-slot mode table
// (GameMode::Construct, i.e. the fourteen unmounted mode TUs), ScoringSystem::Construct,
// ClearLandmarkAndFinishLineData (which calls the unmounted ResetNextLandmarks), the PerfMon
// registrations, the two debug components -- is deliberately absent, because THIS file is mounted
// and those are not. That is the whole reason the seam is not simply "call Construct's inner legs":
// a call from here into BrnModeManager_Lifecycle.cpp would be an unresolved external in the
// shipping link.
//
// Console stores covered here (asm @0x82340008, offsets verbatim):
//     *(a1 + 27992) = a2;    // mpGameStateModule   (+0x6D58)
//     *(a1 + 3480)  = 0;     // mpCurrentGameMode   (+0xD98)
//     *(a1 + 3476)  = -1;    // meCurrentGameModeType       = E_MODE_NONE
//     *(a1 + 3484)  = -1;    // mePreviousGameModeType      = E_MODE_NONE
//     *(a1 + 3488)  = -1;    // mePreviousGameModeState     = E_GMS_INVALID
//     *(a1 + 3492)  = -1;    // meLastAttemptedGameModeType = E_MODE_NONE
//     *(a1 + 3496)  = 0;     // miNumUnsucessfulGameModeAttempts
//     *(a1 + 28008) = 0;     // mpGameActionQueue   (+0x6D68)
//     *(a1 + 38168) = 0;     // miDebugFinishPosition (+0x9518)
//     <the inlined CgsNumeric::Random::Construct() at +28016>
//     *(a1 + 38172) = *(a1 + 38176) = *(a1 + 38180) = 0.0f;   // the three clocks
//
// [!] THE FIRST -1 IS THE LOAD-BEARING ONE AND WAS MISSING ON THIS BUILD: nothing ever wrote
// meCurrentGameModeType, so between modes it read 0 == E_MODE_OFFLINE_RACE instead of the console's
// -1 == E_MODE_NONE. Two measured consumers of that lie: TrainingManager::IsTipAllowedInGameMode's
// mode switch (0 lands in the mode-running arm and rejects the junkyard tips), and
// TranslateGameActionsToGuiEvents case 58 (mode 0 picks the BOOST-BAR presentation, GUI event 218,
// where the console's free-roam -1 takes the unsigned-compare default and posts the plain 217).
// The other three -1s were added in the wave-B pass for the same reason: they are the only stores in
// this leg whose value is not the zero-initialised default, so a missing one is invisible until
// something reads it and gets "offline race" instead of "none".
//
// [FLAG PC bring-up] the EXTRACTION is the deviation, not the values.
// ----------------------------------------------------------------------------
void ModeManager::ConstructInterModeStateBringUp(GameStateModule* lpGameStateModule)
{
    CGS_ASSERT(lpGameStateModule != nullptr, "lpGameStateModule");   // BrnModeManager.cpp:202

    mpGameStateModule = lpGameStateModule;                                  // +27992
    // [stuntrace 2026-08-26] +28000: the real Construct @0x82340008 wires the TQM back-pointer
    // from its lpTriggerQueryManager argument, which at the sole call site is the module's own
    // embedded manager (gsm+42320). Without it the FIRST live StartModeAtLights ->
    // GetStartDataForTrafficLight -> GetTrafficData chain dereferenced null (boot-proven AV
    // @module+0x26E48D reading 0x658 == TQM's traffic ResourcePtr through a null base).
    mpTriggerQueryManager = lpGameStateModule->GetTriggerQueryManager();    // +28000
    // [stuntrace 2026-08-26] +27996: the real Construct's lpProgressionManager argument is the
    // module's own embedded manager at the sole call site (boot-proven: the first live
    // StuntAttackMode::Start -> GetProgressionRankForGameMode chain read Profile members
    // through a null PM, AV @module+0x1B504A with this == 0x170 == the embedded-Profile
    // adjust on a null base).
    mpProgressionManager  = lpGameStateModule->GetProgressionManager();     // +27996
    // +28004: GameStateModule has no NetworkRoundManager member (measured, wave-B fix round),
    // but SetupGameMode reads GetCurrentRound() UNCONDITIONALLY on the console (boot-proven:
    // a null here AV'd @module+0x27D1B0 the first time a mode started). Bring-up stand-in: a
    // zero-initialised file-static instance -- static storage zero-init makes
    // GetCurrentRound() == miTotalRounds - miRoundsRemaining - 1 == -1, the no-round
    // sentinel, and every online read stays inert-sane. DELETE-WHEN the online wave homes the
    // real owner (NetworkRoundManager::Construct is still declared-only; do not call it).
    static BrnGameState::NetworkRoundManager sBringUpNetworkRoundManager;
    mpNetworkRoundManager = &sBringUpNetworkRoundManager;                   // +28004
    mpCurrentGameMode = nullptr;                                            // +3480

    // [stuntrace 2026-08-26] THE MODE-POINTER ARRAY + the per-mode Constructs -- the real
    // Construct @0x82340008's arg-free leg, extracted here because the first live
    // StartGameMode dispatch (`mpCurrentGameMode = mapGameModes[modeType]`) crashed on the
    // unpopulated array (boot-proven AV @module+0x271325). Slot map per the image consensus
    // (vtable_ground_truth 2026-08-26): 6 (ELIMINATOR) and 9 (TRAFFIC_ATTACK) are AUTHORED
    // NULLs; slots 12, 14 and 17 all alias the ONE OnlineStuntRunMode object. Each embedded
    // mode gets its Construct(this) exactly as the console emits after the stores.
    for (s32 liSlot = 0; liSlot < KI_GAME_MODE_SLOTS; ++liSlot)
    {
        mapGameModes[liSlot] = nullptr;
    }
    mapGameModes[0]  = &mRace;
    mapGameModes[1]  = &mFaceOff;
    mapGameModes[2]  = &mCrashMode;
    mapGameModes[3]  = &mRoadRage;
    mapGameModes[4]  = &mPursuit;
    mapGameModes[5]  = &mBurningRoute;
    mapGameModes[7]  = &mStuntAttackMode;
    mapGameModes[8]  = &mSurvivor;
    mapGameModes[10] = &mOnlineRace;
    mapGameModes[11] = &mOnlineRoadRage;
    mapGameModes[12] = &mOnlineStuntRun;
    mapGameModes[13] = &mOnlineBurningHomeRun;
    mapGameModes[14] = &mOnlineStuntRun;      // alias (ORDER NOTE 2)
    mapGameModes[15] = &mOnlineFreeBurnLobby;
    mapGameModes[16] = &mOnlineShowtime;
    mapGameModes[17] = &mOnlineStuntRun;      // alias (ORDER NOTE 2)
    for (s32 liSlot = 0; liSlot < KI_GAME_MODE_SLOTS; ++liSlot)
    {
        if (mapGameModes[liSlot] != nullptr && liSlot != 14 && liSlot != 17)
        {
            mapGameModes[liSlot]->Construct(this);   // vtable slot 0; aliases constructed once
        }
    }

    // [stuntrace 2026-08-26] THE SCORING SYSTEM CONSTRUCT -- the real ModeManager::Construct's
    // `ScoringSystem::Construct(this+3504, lpAchievementManager)` call, extracted here because
    // the first live in-air stunt WROTE THROUGH NULL (boot-proven AV @module+0x279B30 in
    // UpdateAirStunts: mRecentJumpSet's ring storage attaches in StuntModeScoring::Construct,
    // which only ScoringSystem::Construct runs). lpAchievementManager is passed NULL: the
    // module's embedded manager is an AchievementManagerX360 while the scorer's typedef names
    // the PS3 sibling (the documented no-convert). NULL IS SAFE TODAY: the scorer's one call
    // site (OnStuntRunMultiplier, BrnStuntModeScoring_Combo.cpp:150) is deliberately
    // UN-EMITTED (incomplete type), so the pointer is stored, never dereferenced.
    // DELETE the null when the manager types unify and that call re-emits.
    mScoringSystem.Construct(/*lpAchievementManager*/ nullptr);

    // [mbRecentStunt wave 2026-08-27] THE HUD-MESSAGE PUMP CONSTRUCT -- the real
    // ModeManager::Construct's `bl HUDMessageLogic::Construct` @0x82340008 leg, extracted
    // here for the same reason as every line above: THIS seam is what boot actually runs
    // (the real Construct is still uncalled -- its own banner says so), and the latch-drain
    // fix un-parked mHUDMessageLogic.PreWorldUpdate, whose bulk-Append hit the <256,16>
    // queue's "Not Constructed" tripwire EVERY pre-world tick from the first frame
    // (boot-proven, live-play log 2026-08-27: assert quartet per tick at boot). The call is
    // fully self-contained (it constructs the object's own embedded queue; no unmounted
    // callees), which is the seam's admission rule. The twin call in the real Construct
    // (BrnModeManager_Lifecycle.cpp:263) stays for the day that path takes over.
    mHUDMessageLogic.Construct();

    meCurrentGameModeType       = GameStateModuleIO::E_MODE_NONE;           // +3476  = -1
    mePreviousGameModeType      = GameStateModuleIO::E_MODE_NONE;           // +3484  = -1
    mePreviousGameModeState     = GameStateModuleIO::E_GMS_INVALID;         // +3488  = -1
    meLastAttemptedGameModeType = GameStateModuleIO::E_MODE_NONE;           // +3492  = -1

    miNumUnsucessfulGameModeAttempts = 0;                                   // +3496
    mpGameActionQueue                = nullptr;                             // +28008
    miDebugFinishPosition            = 0;                                   // +38168

    // The console inlines CgsNumeric::Random::Construct() here; the host call is entirely
    // header-inline (CgsRandom.h), so it costs this mounted TU no external symbol. Seeding it
    // matters for the same reason the -1s do: an all-zero ring is not this generator's identity --
    // muSeed == 0 makes the LCG emit the constant 1 forever.
    mRandom.Construct();                                                    // +28016

    mfTimeInFreeBurn = 0.0f;                                                // +38172
    mfTimeInMode     = 0.0f;                                                // +38176
    mfTimeInOnline   = 0.0f;                                                // +38180
}

// ----------------------------------------------------------------------------
// [tut-ticker] PreWorldUpdateClocksBringUp -- the extracted CLOCK leg of
// ModeManager::PreWorldUpdate @0x823537B8. The console, in its own if/else:
//   mode running (mpCurrentGameMode != 0):
//       *(a1+38176) += gameTimestep;                       // mfTimeInMode
//       if (mode->mbIsOnline /*+172*/) *(a1+38180) += dt;  // mfTimeInOnline
//       else                           *(a1+38180) = 0.0;
//   no mode running:
//       *(a1+38180) = 0.0;
//       v59 = gsm->mCarSelectManager.mJunkyardId /*gsm+183744, ld*/;
//       if (v59 || gsm->mTrainingManager.mbInPictureParadise /*gsm+46660*/)
//            *(a1+38172) = 0.0;                            // mfTimeInFreeBurn
//       else *(a1+38172) += gameTimestep;
// The timestep the console adds is the pre-world input buffer's timer product
// (*(a4+32) * *(a4+28) == multiplier * base); the caller hands in the same game timestep the
// sibling PreWorldUpdate legs already use. [!] Note that the full PreWorldUpdate reads that product
// off its own mTimerStatusInterface member (X360 +28092 * +28096 -- the SIM sub-status), which is
// the same number by a different route.
// [FLAG PC bring-up] the picture-paradise byte is read through the heap TrainingManager the PC
// allocates (see mpTrainingManager's FLAG in BrnGameStateModule.h); same value, named read.
// ----------------------------------------------------------------------------
void ModeManager::PreWorldUpdateClocksBringUp(f32 lfGameTimestep)
{
    CGS_ASSERT(mpGameStateModule != nullptr, "mpGameStateModule");

    if (mpCurrentGameMode != nullptr)
    {
        mfTimeInMode += lfGameTimestep;
        if (mpCurrentGameMode->IsOnline())
        {
            mfTimeInOnline += lfGameTimestep;
        }
        else
        {
            mfTimeInOnline = 0.0f;
        }
    }
    else
    {
        mfTimeInOnline = 0.0f;

        const bool lbInJunkyard =
            (mpGameStateModule->GetCarSelectManager()->GetJunkyardId() != 0);
        const bool lbInPictureParadise =
            (mpGameStateModule->GetTrainingManager() != nullptr) &&
            mpGameStateModule->GetTrainingManager()->IsInPictureParadise();

        if (lbInJunkyard || lbInPictureParadise)
        {
            mfTimeInFreeBurn = 0.0f;
        }
        else
        {
            mfTimeInFreeBurn += lfGameTimestep;
        }
    }
}

}
