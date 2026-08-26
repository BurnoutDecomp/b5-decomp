// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/BrnModeManager_Lifecycle.cpp
// ============================================================================
// Partfile of the BrnGameState::ModeManager TU (owning header BrnModeManager.h).
// Wave-B keystone, agent 1. Bodies the four lifecycle/dispatch functions:
//
//   ModeManager::Construct                      X360 0x82340008   (THE layout oracle)
//   ModeManager::Prepare                        X360 0x823407A0
//   ModeManager::ClearLandmarkAndFinishLineData X360 0x82328590
//   ModeManager::ProcessEvent                   X360 0x82340AB8
//
// Every store below is reconstructed from that export's ASSEMBLY, not from the Hex-Rays
// pseudocode: Construct's IDA prototype is "local variable allocation has failed" garbage
// (28 int arguments -- hazards H9), it merges unrelated 32-bit stores into fake 64-bit
// constants, and it renders the inlined CgsNumeric::Random::Construct() as forty lines of
// __int128 shuffling. The real shapes are noted at each site.
//
// [X] DO NOT re-implement anything from hazards H2's list of 16 committed bodies -- call them.

#include "GameSource/GameState/ModeManager/BrnModeManager.h"

#include "GameSource/GameState/BrnGameStateModule.h"                 // GameStateModule::GetPlayerActiveRaceCarIndex (ProcessEvent)
#include "GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoring.h" // StuntModeScoring::DealWithInProgressStunt
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"  // CgsDev::PerfMonCpu::AddMonitor

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// TU-local constants, all image- or asm-cited. No invented values.
// ----------------------------------------------------------------------------

// The landmark "none" sentinel. ClearLandmarkAndFinishLineData loads it as a half-word from the
// global at X360 0x82CDB7D4 and stores it into mPlayerCurrentLandmark and every maLandmarkIndices
// slot. DUMPED FROM THE IMAGE THIS SESSION (scratch/postfx_step9_final/envfix/work/image.bin,
// offset 0xCDB7D4 == VA - 0x82000000, big-endian): the half-word reads FF FF, i.e. -1 as s16.
// That is the canonical BrnGameState::K_INVALID_LANDMARK the DecFIGS BrnGameStateTypes.h banner
// names but this tree has not homed yet. (Its neighbour at 0x82CDB7D8 reads FF FE == -2, which is
// K_MULTIPLE_LANDMARKS -- recorded here so nobody has to re-dump it.)
// [!] A .bss ZERO WOULD HAVE BEEN A LIE HERE: clearing these to 0 makes landmark/region 0 look like
// a live checkpoint. The image says -1.
static const s32 KI_INVALID_LANDMARK = -1;

// The CgsID "none" sentinel ClearLandmarkAndFinishLineData stores into maLandmarkCgsIDs: the asm is
// `li r7, -1` followed by `std r7, 0(r9)`, i.e. a sign-extended 64-bit all-ones, NOT zero.
static const CgsID KU_INVALID_CGS_ID = 0xFFFFFFFFFFFFFFFFull;

// ----------------------------------------------------------------------------
// The two PerfMon handles Construct registers.
//
// On console these are FILE STATICS of BrnModeManager.cpp (dword_82CDB6F4 / dword_82CDB6F8; the
// DecFIGS dump lists them as `extern int32_t miPreWorldUpdatePM / miPostWorldUpdatePM` precisely
// because they are not members). The host TU is split across partfiles, so they sit at namespace
// scope here and agent 7a's PreWorldUpdate partfile reaches them with:
//     namespace BrnGameState { extern s32 miPreWorldUpdatePM; extern s32 miPostWorldUpdatePM; }
//
// [!] INITIALISER DIVERGENCE, DELIBERATE AND NARROW: the console's .bss starts both at 0 and
// AddMonitor overwrites them during Construct. Here they start at -1, which is exactly what
// PerfMonCpu::AddMonitor returns for "no monitor" (CgsPerfMonCpu.h:96), because on this build
// Construct is NOT YET CALLED (see its banner) -- a 0 would make the first PreWorldUpdate bracket
// monitor handle 0, i.e. silently attribute ModeManager time to whatever registered first.
// Restore 0 only if Construct becomes the armed path AND that risk is gone.
// ----------------------------------------------------------------------------
s32 miPreWorldUpdatePM  = -1;
s32 miPostWorldUpdatePM = -1;

// ============================================================================
// ModeManager::Construct -- X360 0x82340008
// ============================================================================
// [!!] THIS BODY HAS NO CALLER ON THIS BUILD, AND THAT IS THE DECIDED STATE (conductor decision #1,
// branch (b)). The investigation it asked for, done against GameStateModule::Construct's call
// position (BrnGameStateModule.cpp:134, where ConstructInterModeStateBringUp is called today):
//
//   arg                                        reachable there?
//   1 GameStateModule*                         YES -- `this`
//   2 BrnProgression::ProgressionManager*      YES -- &mProgressionManager (BrnGameStateModule.h:842)
//   3 TriggerQueryManager*                     YES -- &mTriggerQueryManager (:965)
//   4 NetworkRoundManager*                     NO  -- GameStateModule has no such member
//   5 StreetManager*                           YES -- &mStreetManager (:868)
//   6 MugshotManager*                          NO  -- GameStateModule has no such member
//   7 const RoadRulesManager*                  NO  -- DWARF declares mRoadRulesManager between
//                                                     mAchievementManager and mStreetManager, but it
//                                                     is NOT reconstructed (see the note at :856)
//   8 StuntModeScoring::AchievementManager*    NO (TYPE) -- the member exists (mAchievementManager,
//                                                     :854) but it is an AchievementManagerX360,
//                                                     while StuntModeScoring::AchievementManager is
//                                                     typedef'd to AchievementManagerPS3
//                                                     (BrnStuntModeScoring.h:194). Those two are
//                                                     SIBLINGS under AchievementManagerBase, so the
//                                                     address does not convert.
//
// FOUR OF EIGHT ARE UNREACHABLE, so wiring this in would mean passing nulls through four asserts the
// console fires on exactly those pointers. The bring-up seam therefore stays, and it stays the ONLY
// ARMED PATH -- both paths are never armed at once.
// DELETE-WHEN, precisely: (a) GameStateModule grows a NetworkRoundManager member/accessor, (b) it
// grows a MugshotManager one, (c) RoadRulesManager is reconstructed and embedded, and (d) the
// StuntModeScoring::AchievementManager typedef is re-pointed at the base (or at the X360 flavour) so
// &mAchievementManager converts. On that day: replace the ConstructInterModeStateBringUp call site
// with this Construct and DELETE the seam and its two bring-up methods.
// ============================================================================
void ModeManager::Construct(GameStateModule*                      lpGameStateModule,
                            BrnProgression::ProgressionManager*   lpProgressionManager,
                            TriggerQueryManager*                  lpTriggerQueryManager,
                            NetworkRoundManager*                  lpNetworkRoundManager,
                            StreetManager*                        lpStreetManager,
                            MugshotManager*                       lpMugshotManager,
                            const RoadRulesManager*               lpRoadRulesManager,
                            StuntModeScoring::AchievementManager* lpAchievementManager)
{
    // ------------------------------------------------------------------------
    // [X] PARKED LEG 1 -- the ModeManagerDebugComponent. Console (inlined
    // ModeManagerDebugComponent::Construct(this) followed by Register):
    //     v30 = this + 28112;
    //     *(v30 + 12) = this;   // mpModeManager
    //     *(v30 + 16) = 0;      // mbShowModeInfo
    //     *(v30 + 17) = 0;      // mbInfiniteLives
    //     *(v30 + 20) = 1;      // miFinishPosition  (matches SetRange(&miFinishPosition, 1, 8))
    //     CgsDev::DebugComponent::Register(v30);
    // The MEMBER is not declared (include cycle -- see BrnModeManager.h's DIVERGENCE banner at
    // console +28112) and ModeManagerDebugComponent declares no Construct, so there is nothing to
    // call. Registering it half-wired would be worse than not registering it: FinshMode
    // dereferences mpModeManager. DELETE-WHEN the two-line include-cycle fix lands and the member
    // is re-declared; this leg is written and ready.
    // ------------------------------------------------------------------------

    // ------------------------------------------------------------------------
    // [X] PARKED LEG 2 -- the ScoringSystemDebugComponent's own Construct. Console:
    //     if (this + 3504 == NULL) assert "lpScoringSystem"   (BrnScoringSystemDebugComponent.cpp:99)
    //     *(this + 28148) = this + 3504;   // comp+12  mpScoringSystem
    //     *(this + 28152) = 0;             // comp+16  mbShowChainableStunts
    //     CgsDev::DebugComponent::Register(this + 28136);
    // The MEMBER exists here (mScoringSystemDebugComponent), but BrnScoringSystemDebugComponent.h
    // declares no Construct and both fields are private, so the two stores cannot be made by name
    // and DebugComponent::Register alone would register a component whose
    // DebugRenderChainableStunts asserts on a null mpScoringSystem. The assert itself IS kept --
    // it is the console's, verbatim, and it is the one part of this leg that costs nothing.
    // DELETE-WHEN ScoringSystemDebugComponent grows `void Construct(ScoringSystem*)` (filed as a
    // header_request).
    // ------------------------------------------------------------------------
    ScoringSystem* lpScoringSystem = GetScoringSystem();
    CGS_ASSERT(lpScoringSystem != nullptr, "lpScoringSystem");

    // Console orders this store BEFORE the ChallengeManager call; kept in place.
    miDebugFinishPosition = 0;                                          // +38168 (0x9518)

    // ------------------------------------------------------------------------
    // [X][X] DIVERGENCE: ChallengeManager NOT embedded/mounted (29 TUs, ~35 unresolved externals;
    // freeburn challenges are off the offline-event path).
    // Console: BrnGameState::ChallengeManager::Construct(this + 28160, lpGameStateModule, this,
    //                                                    lpProgressionManager, lpRoadRulesManager,
    //                                                    lpTriggerQueryManager);
    // Re-wire when the ChallengeManager mount lands. lpRoadRulesManager exists ONLY to be forwarded
    // here -- it is consumed nowhere else in this body, which is why it is void-ed below rather than
    // stored.
    // ------------------------------------------------------------------------
    (void)lpRoadRulesManager;

    CGS_ASSERT(lpGameStateModule != nullptr, "lpGameStateModule");      // BrnModeManager.cpp:202
    mpGameStateModule = lpGameStateModule;                              // +27992

    CGS_ASSERT(lpProgressionManager != nullptr, "lpProgressionManager");// BrnModeManager.cpp:205
    mpProgressionManager = lpProgressionManager;                        // +27996

    mpTriggerQueryManager = lpTriggerQueryManager;                      // +28000 (no assert on console)

    CGS_ASSERT(lpNetworkRoundManager != nullptr, "lpNetworkRoundManager"); // BrnModeManager.cpp:210
    mpNetworkRoundManager = lpNetworkRoundManager;                      // +28004

    // ------------------------------------------------------------------------
    // The mode-pointer table. Console: an 18-iteration `*v38++ = 0` loop over this+0, then SIXTEEN
    // stores (slots 6 and 9 are never written -- ORDER NOTE 3, authored NULLs), then an 18-iteration
    // loop calling vtable slot 0 on every non-null slot.
    // ------------------------------------------------------------------------
    for (s32 liSlot = 0; liSlot < KI_GAME_MODE_SLOTS; ++liSlot)
    {
        mapGameModes[liSlot] = nullptr;
    }

    mapGameModes[0]  = &mRace;                   // console *(this+0)  = this+72
    mapGameModes[1]  = &mFaceOff;                //         *(this+4)  = this+268
    mapGameModes[2]  = &mCrashMode;              //         *(this+8)  = this+464
    mapGameModes[3]  = &mRoadRage;               //         *(this+12) = this+652
    mapGameModes[4]  = &mPursuit;                //         *(this+16) = this+908
    mapGameModes[5]  = &mBurningRoute;           //         *(this+20) = this+1096
    // slot 6  == E_MODE_ELIMINATOR      -- AUTHORED NULL, never stored by the console.
    mapGameModes[7]  = &mStuntAttackMode;        //         *(this+28) = this+1328
    mapGameModes[8]  = &mSurvivor;               //         *(this+32) = this+1552
    // slot 9  == E_MODE_TRAFFIC_ATTACK  -- AUTHORED NULL, never stored by the console.
    mapGameModes[10] = &mOnlineRace;             //         *(this+40) = this+1776
    mapGameModes[11] = &mOnlineRoadRage;         //         *(this+44) = this+2016
    mapGameModes[12] = &mOnlineStuntRun;         //         *(this+48) = this+2256  ] ORDER NOTE 2:
    mapGameModes[13] = &mOnlineBurningHomeRun;   //         *(this+52) = this+2512  ] ONE object,
    mapGameModes[14] = &mOnlineStuntRun;         //         *(this+56) = this+2256  ] THREE slots.
    mapGameModes[15] = &mOnlineFreeBurnLobby;    //         *(this+60) = this+2952  ] Its Construct
    mapGameModes[16] = &mOnlineShowtime;         //         *(this+64) = this+3296  ] therefore runs
    mapGameModes[17] = &mOnlineStuntRun;         //         *(this+68) = this+2256  ] three times.

    // Console: `do { if (*v40) (***v40)(*v40, this); ... } while (--v41);` over all 18 slots --
    // vtable slot 0 == GameMode::Construct(ModeManager*). The three aliased slots invoke it three
    // times on the same OnlineStuntRunMode; that is idempotent on console and MUST stay that way.
    for (s32 liSlot = 0; liSlot < KI_GAME_MODE_SLOTS; ++liSlot)
    {
        if (mapGameModes[liSlot] != nullptr)
        {
            mapGameModes[liSlot]->Construct(this);
        }
    }

    // ------------------------------------------------------------------------
    // The free-burn-lobby manager pointers. The console fires TWO asserts per argument, because BOTH
    // inlined setters are inlined here: BrnOnlineFreeBurnLobbyMode.h:175 AND BrnBurnoutSkillzManager.h:265
    // for the street manager, :190 / :280 for the mugshot manager. Only one CGS_ASSERT is written per
    // argument (a duplicate assert on the same condition is noise, not fidelity) -- the pair is
    // recorded here instead.
    // [X] PARKED STORES: the console then does `*(this + 3252) = lpStreetManager;` and
    // `*(this + 3256) = lpMugshotManager;`, which land INSIDE mOnlineFreeBurnLobby (+2952) -- at
    // +300 / +304 within it, in the embedded BurnoutSkillzManager region that starts at +3136.
    // BrnOnlineFreeBurnLobbyMode declares no members at all on this tree, so there is nothing to
    // store into. DELETE-WHEN OnlineFreeBurnLobbyMode/BurnoutSkillzManager grow their
    // SetStreetManager / SetMugshotManager setters (filed as a header_request).
    // ------------------------------------------------------------------------
    CGS_ASSERT(lpStreetManager != nullptr, "lpStreetManager");
    CGS_ASSERT(lpMugshotManager != nullptr, "lpMugshotManager");
    (void)lpStreetManager;
    (void)lpMugshotManager;

    // ------------------------------------------------------------------------
    // The scalar / sentinel seeds, in console store order.
    // [!] THE -1s ARE LOAD-BEARING: meCurrentGameModeType idling at 0 instead of -1 is
    // E_MODE_OFFLINE_RACE, which is the tut-ticker bug (hazards H3).
    // ------------------------------------------------------------------------
    miNumUnsucessfulGameModeAttempts = 0;                                // +3496
    mpCurrentGameMode                = nullptr;                          // +3480
    mbFinishedOnlineEvent            = false;                            // +38151
    mbInstantIntroSplash             = false;                            // +38152
    meCurrentGameModeType            = GameStateModuleIO::E_MODE_NONE;   // +3476  = -1
    mePreviousGameModeType           = GameStateModuleIO::E_MODE_NONE;   // +3484  = -1
    meLastAttemptedGameModeType      = GameStateModuleIO::E_MODE_NONE;   // +3492  = -1
    mePreviousGameModeState          = GameStateModuleIO::E_GMS_INVALID; // +3488  = -1
    mbEventJustFinished              = false;                            // +38153
    miFramesUntilModeSwitchSend      = 0;                                // +38160
    muUnkByte_0x950B                 = 0;                                // +38155
    mbPlayerCrashedLastFrame         = false;                            // +38154
    mbIsInTimeUpOutro                = false;                            // +38156
    mfTimeUpStateTimer               = 0.0f;                             // +38164

    // The embedded ScoringSystem. Console: ScoringSystem::Construct(this + 3504, lpAchievementManager).
    mScoringSystem.Construct(lpAchievementManager);

    // ------------------------------------------------------------------------
    // [X] PARKED LEG 3 -- HUDMessageLogic. Console: BrnGameState::HUDMessageLogic::Construct(this + 27392).
    // PARKED PER CONDUCTOR DECISION #4: the HUD-message lifecycle (Construct / Prepare /
    // PreWorldUpdate / PostWorldUpdate) belongs to the event-GUI wave; Hud/BrnHUDMessageLogic.cpp
    // bodies only the six GenerateOnlineStuntRun* generators and declares none of the four. The
    // MEMBER exists (mHUDMessageLogic) so the layout and the offsets behind it are right; only the
    // seeding is deferred. Consequence while parked: the stunt-run message edge trackers start at
    // whatever the allocation holds rather than at their -1 / 0 seeds.
    // ------------------------------------------------------------------------

    mbFinishCurrentModeNextUpdate = false;                               // +38135 (0x94F7)
    mbOnlineFinalStandingsShown   = false;                               // +38136 (0x94F8)

    // ------------------------------------------------------------------------
    // The random generator. The console INLINES CgsNumeric::Random::Construct() here, which is why
    // Hex-Rays renders ~55 lines of __int128 shuffling: seed <- KU_RANDOM_DEFAULT_SEED
    // (the pseudocode's `0x3F8000001AD0891B` is IDA merging the ring[0] = 0x3F800000 == 1.0f store
    // with the seed's low half 0x1AD0891B), ring[0] <- 1.0f, then seven AddRandomFloatToBuffer steps
    // (each `seed = seed * 0x5851F42D4C957F2D + 1`, `(seed >> 32) >> 9 | 0x3F800000` into the next
    // ring slot -- the recurring 1284865837 in the pseudocode is that multiplier's low word), then
    // `muOldestBufferIndex = (muOldestBufferIndex + 1) & 7`. That is exactly Random::Construct().
    // ------------------------------------------------------------------------
    mRandom.Construct();                                                 // +28016

    ClearLandmarkAndFinishLineData();

    mfPlayerTotalledTime               = 0.0f;                           // +32808
    mbHasAborted                       = false;                          // +38137
    mfPFMSecondPhaseTimer              = 0.0f;                           // +38128
    mbModeDataIsLoading                = false;                          // +38145
    mbIsModePrepared                   = false;                          // +38147
    mbIsWaitingForSecondPFM            = false;                          // +38132
    mbModeStartFromRegionEnabled       = false;                          // +38144
    mbStuntChallengeActive             = false;                          // +38157
    mbInModeStartRegion                = false;                          // +38133
    mbLastInModeStartRegion            = false;                          // +38134
    mbHasCrashedOut                    = false;                          // +38140
    mbPlayerFinishedTimedOut                   = false;                          // +38141
    mbDistanceToFinishLineTransmitted  = false;                          // +38148
    mbModeIntroStarted                 = false;                          // +38149
    mbReadyForModeIntro                = false;                          // +38146
    mbHasTimedOut                      = false;                          // +38139
    mbHasPlayerFinished                = false;                          // +38143
    mbPlayerFinishedCarDestroyed        = false;                          // +38142

    // Console: `std r30, 0(this+0x8BF0)` / `std r30, 0(this+0x8BF8)` with r30 == 0 -- two 8-byte
    // zero stores. (Hex-Rays shows `*(a1+35824) = v29` with a bogus 64-bit v29 whose high half it
    // filled from an unrelated address register; the asm has no such value.)
    // (BitArray<35>::UnSetAll and ::Prepare are byte-identical whole-field zero loops and the asm
    //  cannot tell them apart -- one `std` of zero either way. UnSetAll is used here because this is
    //  a plain clear, not a per-mode prepare.)
    mRaceCarReachedCheckpoint.UnSetAll();                                // +35824
    mRaceCarReachedFinish.UnSetAll();                                    // +35832

    mfTimeInFreeBurn = 0.0f;                                             // +38172
    mfTimeInMode     = 0.0f;                                             // +38176
    mpGameActionQueue = nullptr;                                         // +28008
    mfTimeInOnline   = 0.0f;                                             // +38180

    mbNeedToSendNextRequest            = false;                          // +35852
    mbIsCalculatingCheckpointDistances = false;                          // +35853
    muNextDistanceRequestCheckpoint    = 0;                              // +35848

    // ------------------------------------------------------------------------
    // The two PerfMon monitors. Console:
    //   r3 = "ModeManager PreWorld" / "ModeManager PostWorld", r4 = 5, r5 = 0,
    //   f1 = flt_82001C98, r7 = 1  -- r6 is NEVER WRITTEN.
    // r6 is the integer slot the f32 argument consumes on the PPC ABI; Hex-Rays sees the hole and
    // invents a sixth argument for it (the "38176" and "v47" in the pseudocode are leftover address
    // registers, not arguments). CgsPerfMonCpu.h already documents this trap in full.
    // flt_82001C98 == 1.0f, dumped from the image at offset 0x1C98 this session.
    // [OK] PAGE 5 NOW HAS A NAMED ENUMERATOR: the header_request was applied 2026-08-26 by the
    // wave-B fix round (E_PMP_5 = 5 in CgsPerfMonCpu.h), after re-reading the console's `li r4, 5`
    // at 0x823406BC. The value-cast is gone; the page is named.
    // ------------------------------------------------------------------------
    miPreWorldUpdatePM  = CgsDev::PerfMonCpu::AddMonitor(
        "ModeManager PreWorld",  CgsDev::E_PMP_5, false, 1.0f, true);
    miPostWorldUpdatePM = CgsDev::PerfMonCpu::AddMonitor(
        "ModeManager PostWorld", CgsDev::E_PMP_5, false, 1.0f, true);
}

// ============================================================================
// ModeManager::Prepare -- X360 0x823407A0
// ============================================================================
// Console, verbatim (the whole body):
//     if (ChallengeManager::Prepare(this + 0x6E00 /* +28160 */, lpFreeburnChallengeList))
//         return ScoringSystem::Prepare(this + 0xDB0 /* +3504 */, lpHeapMalloc);
//     return false;
// (IDA shows ChallengeManager::Prepare taking one argument because of its own prototype; the asm
//  leaves r4 == lpFreeburnChallengeList untouched across the call, so the list IS forwarded.)
bool ModeManager::Prepare(const BrnResource::ChallengeList* lpFreeburnChallengeList,
                          CgsMemory::HeapMalloc*            lpHeapMalloc)
{
    // ------------------------------------------------------------------------
    // [X][X] DIVERGENCE: ChallengeManager NOT embedded/mounted.
    // Console: `if (!BrnGameState::ChallengeManager::Prepare(this + 28160, lpFreeburnChallengeList))
    //            return false;`  -- i.e. a FAILED challenge-list prepare short-circuits the whole
    // method and the ScoringSystem is left un-prepared. With no ChallengeManager the gate is treated
    // as having succeeded, which is the console's normal-path behaviour (Prepare fails only when the
    // challenge list is unusable). Re-wire when the ChallengeManager mount lands.
    // ------------------------------------------------------------------------
    (void)lpFreeburnChallengeList;

    return mScoringSystem.Prepare(lpHeapMalloc);
}

// ============================================================================
// ModeManager::ClearLandmarkAndFinishLineData -- X360 0x82328590
// ============================================================================
// Called by Construct and by ExitCurrentMode. Asm shape:
//     *(this + 0x801C) = 0;                       // muNumLandmarks  (stwx of r29 == 0)
//     *(this + 0x8020) = word_82CDB7D4;           // mPlayerCurrentLandmark (sthx)
//     ResetNextLandmarks(this, 1);
//     r11 = this + 0x7EC0; r9 = this + 0x7E40; r7 = -1; 16 iterations:
//         sth r8(word_82CDB7D4), -0xA0(r11)       // maLandmarkIndices[i]        (0x7EC0-0xA0 == 0x7E20)
//         std r7,               0(r9); r9 += 8    // maLandmarkCgsIDs[i] = -1
//         sth r29(0),           0(r11); r11 += 2  // mauLandmarkSectionIndices[i] = 0
void ModeManager::ClearLandmarkAndFinishLineData()
{
    muNumLandmarks         = 0;                                          // +32796
    mPlayerCurrentLandmark = LandmarkIndex(KI_INVALID_LANDMARK);         // +32800

    ResetNextLandmarks(true);

    for (s32 liLandmark = 0; liLandmark < GameStateModuleIO::KI_MAX_LANDMARKS_IN_MODE; ++liLandmark)
    {
        maLandmarkIndices[liLandmark]           = static_cast<u16>(KI_INVALID_LANDMARK); // 0xFFFF, not 0
        maLandmarkCgsIDs[liLandmark]            = KU_INVALID_CGS_ID;
        mauLandmarkSectionIndices[liLandmark]   = 0;
    }
}

// ============================================================================
// ModeManager::ProcessEvent -- X360 0x82340AB8
// ============================================================================
// The whole body is the ChallengeManager forward plus ONE event case. Asm shape:
//     r3 = this + 0x6E00; bl ChallengeManager::ProcessEvent   (r4/r5/f1 untouched -> all forwarded)
//     cmpwi r31(leEventType), 0x78 (== 120); bne out
//     r11 = mpCurrentGameMode (0xD98); r11 = r11 ? *(r11 + 0xAC) : 0     // GameMode::mbIsOnline
//     r11 = this + 0xDB0 (mScoringSystem)
//     r31 = r11 + 0x2620  (online stunt scorer)      if that byte is set
//     r31 = r11 + 0x350   (offline stunt scorer)     otherwise
//     r3 = mpGameStateModule (0x6D58); bl GameStateModule::GetPlayerActiveRaceCarIndex
//     r6 = result; f1 = the incoming delta; r4 = the incoming Event*; r3 = r31
//     bl StuntModeScoring::DealWithInProgressStunt
// [!] r5 IS NEVER WRITTEN -- it is the integer slot the f32 argument consumes on the PPC ABI, which
// is why Hex-Rays invents an uninitialised `v13` fourth argument. Three real arguments.
void ModeManager::ProcessEvent(GameStateModuleIO::EGameEventType leEventType,
                               const CgsModule::Event*           lpEvent,
                               f32                               lfDelta)
{
    // ------------------------------------------------------------------------
    // [X][X] DIVERGENCE: ChallengeManager NOT embedded/mounted.
    // Console, UNCONDITIONALLY and as the FIRST statement:
    //     BrnGameState::ChallengeManager::ProcessEvent(this + 28160, leEventType, lpEvent);
    // [!] THIS IS THE ARM THAT CHANGES BEHAVIOUR, not just layout: every freeburn-challenge event
    // (start / trigger / cancel / success / status) reaches the ChallengeManager through here and
    // through here only. While this is parked, freeburn challenges receive NOTHING. Re-wire when the
    // ChallengeManager mount lands.
    // ------------------------------------------------------------------------

    if (leEventType != GameStateModuleIO::E_EVENT_INPROGRESS_STUNT)      // console `cmpwi r31, 0x78` (120)
    {
        return;
    }

    // The scorer choice is the CURRENT MODE's online flag, not the module's: a null current mode
    // reads as offline (the console's `beq` arm loads a literal 0 rather than dereferencing).
    const bool lbOnlineMode = (mpCurrentGameMode != nullptr) ? mpCurrentGameMode->IsOnline() : false;

    // this+13264 == mScoringSystem(+3504) + 0x2620 == the ONLINE stunt scorer;
    // this+4352  == mScoringSystem(+3504) + 0x350  == the OFFLINE stunt scorer.
    StuntModeScoring* lpStuntModeScoring =
        lbOnlineMode ? mScoringSystem.GetOnlineStuntScorer() : mScoringSystem.GetStuntScorer();
    CGS_ASSERT(lpStuntModeScoring != nullptr, "lpStuntModeScoring");

    CGS_ASSERT(mpGameStateModule != nullptr, "mpGameStateModule");
    const s32 liPlayerActiveRaceCarIndex =
        static_cast<s32>(mpGameStateModule->GetPlayerActiveRaceCarIndex());

    // The console hands r5 (the raw Event*) straight through: for E_EVENT_INPROGRESS_STUNT the
    // queued payload IS an OnStuntElementCompleteAction (the record DealWithInProgressStunt walks at
    // +0x24 / +0x44 / +0x64 -- see the reconciliation note at BrnStuntModeScoring.h:355). The cast is
    // reinterpret_ rather than static_ because GameAction<T> is an empty tag base
    // (BrnGameActions.h:234) and is deliberately NOT derived from CgsModule::Event, so the two
    // hierarchies are unrelated on the host even though the console has one pointer.
    lpStuntModeScoring->DealWithInProgressStunt(
        reinterpret_cast<const GameStateModuleIO::OnStuntElementCompleteAction*>(lpEvent),
        lfDelta,
        liPlayerActiveRaceCarIndex);
}

} // namespace BrnGameState
