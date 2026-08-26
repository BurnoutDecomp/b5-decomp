// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/BrnModeManager_Start.cpp
// ============================================================================
// Partfile of the BrnGameState::ModeManager TU (owning header BrnModeManager.h).
// Wave-B keystone, AGENT 2 -- the START / EXIT spine. Bodies exactly three functions:
//
//   ModeManager::StartGameMode         X360 0x8234FCE8
//   ModeManager::ExitCurrentMode       X360 0x8234FFE0
//   ModeManager::SendModeStopMessages  X360 0x8234BEC0
//
// Every store, every branch and every action id below is read off those three exports'
// ASSEMBLY, not off the Hex-Rays pseudocode. The pseudocode is wrong in five places that
// matter here and each is called out at its site:
//   (1) StartGameMode's mRaceId store renders as `*(a1 + HIDWORD(v19)) = v19` (hazards H9,
//       "local variable allocation has failed"); the asm is
//       `lis r10,0 / ori r10,r10,0x8030 / ld r11,0x2C8(r30) / stdx r11,r31,r10`, i.e.
//       this+0x8030 = lpStartGameModeParams+712.
//   (2) StartGameMode's IDA prototype carries six arguments; four of them are register
//       residue handed to HUDMessageLogic::Prepare, which the asm shows taking ONLY r3.
//   (3) SendModeStopMessages' `*(a1+3240) = -1` is really `stw <dword_820A766C>, 0x68(r30)`
//       -- a LOAD from rodata that happens to be -1 (image-cited below), not an immediate.
//   (4) SendModeStopMessages' action-7 payload and the action-39 payload are TWO distinct
//       stack records the compiler overlaid; IDA merged them into one v80.
//   (5) ExitCurrentMode's `Fram[4] = 1; *Fram = 1;` are two stores into the OutputBuffer's
//       frame-rate-type request interface, not into an array.
//
// [X] hazards H2: the 16 committed BrnModeManager.cpp bodies are CALLED, never re-implemented.
//     This file calls SetupStuntChallenge (:212) and EndStuntChallenge (:226).
//
// [X] hazards H3: meCurrentGameModeType idles at E_MODE_NONE (-1), never 0. The bound the
//     console asserts is 18 == KI_GAME_MODE_SLOTS, NOT the tree's E_MODE_COUNT (17) -- see
//     BrnModeManager.h's E_MODE_COUNT audit. The assert STRING still spells E_MODE_COUNT
//     because that is the console's own text, verbatim.
// ============================================================================

#include "GameSource/GameState/ModeManager/BrnModeManager.h"

#include <cstddef>   // offsetof (the action-record layout pins)
#include <cstring>   // std::memcpy / std::memset (the opaque frame-rate-request write; the
                     //   two tail records the console posts with un-written stack bytes)

// The scope question the owning header bans is local here: BrnModeManager.h is parsed FIRST,
// so every ModeManager declaration has already bound ::EActiveRaceCarIndex (BurnoutConstants.h)
// before BrnGameStateModuleIO.h can introduce BrnGameState::EActiveRaceCarIndex.
#include "GameSource/GameState/BrnGameStateModuleIO.h"                 // OutputBuffer, GameActionQueue
#include "GameSource/GameState/BrnGameStateModule.h"                   // GetLastActiveRaceCarInterface
#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h"
#include "GameSource/GameState/NetworkRoundManager/BrnNetworkRoundManager.h"
#include "GameSource/GameState/Progression/BrnProgressionManager.h"    // GetProfile, RequestUpdateRivals
#include "GameSource/GameState/Progression/BrnProfile.h"               // Profile::FindCar
#include "GameSource/GameState/Progression/BrnProgressionCarData.h"    // CarData::GetUnlockDeformationAmount
#include "GameSource/World/EntityModules/TriggerEntityModule/SharedIO/BrnTriggerEntityModuleInputInterface.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"       // VariableEventQueue<13312,16>::AddEvent
#include "GameShared/GameClasses/Development/Log/CgsLog.h"             // gpDebugPrint / gxMessageFilterFlags
#include "SharedClasses/Progression/BrnTrainingTypes.h"                // E_TRAINING_TYPE_QUITS_EVENT

namespace BrnGameState
{
namespace
{

// ============================================================================
// [x] TU-LOCAL ACTION IDS RETIRED 2026-08-26 (stuntrace waveB CLOSURE round).
// ============================================================================
// The eight ids this file posts now have enumerators in GameStateModuleIO::EGameActionType and
// are used BY NAME below. The old block carried them as `KI_ACTION_*` numerics with a
// DWARF-name-plus-measured-shift derivation; that derivation, plus everything the closure round
// added to it, moved into BrnGameActions.h with the values:
//   30 E_ACTION_STOP_MODE_INTRO           25 E_ACTION_STOP_MODE_OFFLINE
//   26 E_ACTION_FINISH_MODE_ONLINE        27 E_ACTION_FINISH_MODE_FINAL_ONLINE
//   40 E_ACTION_QUIT_MODE_OFFLINE         41 E_ACTION_QUIT_MODE_ONLINE
//   43 E_ACTION_IMPACT_TIME_END            7 E_ACTION_SET_PLAYER_CAR_DRIVER
// Two of them (43 and 7) were ALSO being carried by BrnModeManager_Finish.cpp -- one value, two
// TU-local definitions, which is precisely the drift hazard this retirement closes.
// [x] THE SET_UP_ALL_DRIVE_THRUS COLLISION THIS FILE REPORTED IS SETTLED, IN THIS FILE'S FAVOUR:
// E_ACTION_SET_UP_ALL_DRIVE_THRUS moved 40 -> 45, and it is no longer an inference --
// GameStateModule::SendSetUpAllDriveThrusMessage posts `li r5,0x2D` (45) + `li r6,0x458` (1112)
// @0x82381CC0, the producer symbol naming the enumerator and 1112 being the DriveThruInfo[46]
// table's own size. 40 is this file's one-byte offline quit.

// ============================================================================
// TU-LOCAL ACTION PAYLOAD RECORDS.
// Each is the exact byte image the console builds on the stack, sized to the literal
// `li r6, <size>` at its AddEvent. FLAG: PROVISIONAL local mirrors -- header_request filed to
// home them in BrnGameActions.h next to StopModeAction (which this file DOES use by name).
// ============================================================================

// [x] StopModeIntroActionRecord DELETED 2026-08-26 (stuntrace waveB CLOSURE round). It was a
// byte-identical second copy of GameStateModuleIO::StopModeIntroAction (BrnGameActions.h), which
// ModeManager::StopModeIntro already posts -- this file's own banner said "it must end up shared,
// not duplicated". ExitCurrentMode @0x823501C8/@0x8234020C posts the same 8-byte image
// (word 0 = meCurrentGameModeType, byte 4 = IsOnlineModeWithInstantIntro()), so the shared record
// is used by name at the call site below; its members are meGameMode / mbMovingBetweenLobbyModes.

// 1 byte. `stb <NRM+0x130>, var_130` @0x8234C670.
struct QuitModeOfflineActionRecord
{
    u8 mbStartingGameDueToPlayerJoin;   // +0x00
};

// 4 bytes. `stw 0x2F, var_128` @0x8234C688 -- the ETrainingType, posted through the
// already-named E_ACTION_REQUEST_GAME_TRAINING (149).
struct RequestGameTrainingActionRecord
{
    s32 meTrainingType;                 // +0x00
};

// 8 bytes, ONLINE arm. word 0 = meCurrentGameModeType, byte 4 = "quit because the mode timed
// out", byte 5 = "the player is sitting in a junkyard" (gsm+0x2CDC0 != kCGSID_NULL).
struct QuitModeOnlineActionRecord
{
    BrnGameState::GameStateModuleIO::EGameModeType meGameModeType;  // +0x00
    u8                                            mbQuitByTimeout;  // +0x04
    u8                                            mbInJunkyard;     // +0x05
    u8                                            maPad06[2];       // +0x06
};

// 8 bytes, ONLINE arm. @0x8234C634: word 0 = meCurrentGameModeType, word 4 = leNextGameModeType.
struct FinishModeOnlineActionRecord
{
    BrnGameState::GameStateModuleIO::EGameModeType meGameModeType;      // +0x00
    BrnGameState::GameStateModuleIO::EGameModeType meNextGameModeType;  // +0x04
};

// 4 bytes, ONLINE arm. @0x8234C624: word 0 = meCurrentGameModeType.
struct FinishModeFinalOnlineActionRecord
{
    BrnGameState::GameStateModuleIO::EGameModeType meGameModeType;      // +0x00
};

// 48 bytes. The console writes EXACTLY TWO of them -- `stw 1, +0x00` @0x8234C6F8 and
// `stb 0, +0x2C` @0x8234C700 -- and posts the record with the other 42 bytes holding whatever
// the overlaid action-39 record left on the stack (var_110 is the same slot). Reproduced as a
// separate local with the two named fields; the unwritten span is explicit padding so the
// record is byte-exact at the attested 48 without inventing members.
struct SetPlayerCarDriverActionRecord
{
    s32 miField00;        // +0x00  console: 1
    u8  maUnread04[0x28]; // +0x04..+0x2B never written by this producer
    u8  mu8Field2C;       // +0x2C  console: 0
    u8  maPad2D[3];       // +0x2D tail pad to 48
};

// 1 byte, posted last. The console reuses the SAME 1-byte stack slot the offline quit arm used
// (var_130), so when that arm did not run the byte is stack residue.
struct ImpactTimeEndActionRecord
{
    u8 mu8Payload;        // +0x00
};

// ---- the action sizes the console literally posts (`li r6, <n>`) ------------------------
// Kept as static_asserts rather than magic numbers at the call sites: these are WIRE FORMAT
// (the records cross into the shared 13312-byte VariableEventQueue, hazards H5).
// Note StopModeAction already carries its own == 24 assert in BrnGameActions.h.
// (Placed at namespace scope, not in a function, so they are checked even if a leg is parked.)
// ----------------------------------------------------------------------------------------
// X360 0x82350214  `li r6,8`   -- ExitCurrentMode's action-30 post. The record is now the
// shared GameStateModuleIO::StopModeIntroAction, which carries its own == 8 assert in
// BrnGameActions.h; asserting it a second time here would just duplicate that pin.
// X360 0x8234C654  `li r6,1`   -- the offline quit post
static_assert(sizeof(QuitModeOfflineActionRecord) == 1,
              "X360 SendModeStopMessages posts action 40 with size 1");
// X360 0x8234C67C  `li r6,4`   -- the training-tip request
static_assert(sizeof(RequestGameTrainingActionRecord) == 4,
              "X360 SendModeStopMessages posts action 149 with size 4");
// X360 0x8234C57C  `li r6,8`   -- the online quit post (parked arm)
static_assert(sizeof(QuitModeOnlineActionRecord) == 8,
              "X360 SendModeStopMessages posts action 41 with size 8");
// X360 0x8234C638  `li r6,8`   -- the online finish post (parked arm)
static_assert(sizeof(FinishModeOnlineActionRecord) == 8,
              "X360 SendModeStopMessages posts action 26 with size 8");
// X360 0x8234C620  `li r6,4`   -- the online final-round finish post (parked arm)
static_assert(sizeof(FinishModeFinalOnlineActionRecord) == 4,
              "X360 SendModeStopMessages posts action 27 with size 4");
// X360 0x8234C6F0  `li r6,0x30` -- the 48-byte tail post
static_assert(sizeof(SetPlayerCarDriverActionRecord) == 48,
              "X360 SendModeStopMessages posts action 7 with size 48");
static_assert(offsetof(SetPlayerCarDriverActionRecord, mu8Field2C) == 0x2C,
              "the console's second write into the action-7 record is at +0x2C");
// X360 0x8234C714  `li r6,1`   -- the final 1-byte post
static_assert(sizeof(ImpactTimeEndActionRecord) == 1,
              "X360 SendModeStopMessages posts actions 25 and 43 with size 1");

// ============================================================================
// THE NETWORK-ROUND "ROUNDS REMAINING" READ -- de-inlined through the declared accessors.
//
// SendModeStopMessages reads NetworkRoundManager+296 (miRoundsRemaining, private) at three
// sites and NetworkRoundManager+300 - +296 - 1 at a fourth.
// [x] SETTLED 2026-08-26 (stuntrace waveB CLOSURE round). The header_request this banner filed
// LANDED: NetworkRoundManager now publishes GetRoundsRemaining() alongside GetCurrentRound() /
// GetTotalRounds(), all three bodied in BrnNetworkRoundManager.cpp, and that TU's own ruling
// fixes the identity that used to be the FLAG here -- GetCurrentRound() IS the ZERO-BASED
// current round index (miTotalRounds - miRoundsRemaining - 1), pinned by the committed
// BrnGuiCache body that displays GetCurrentRound() + 1.
// So the old expression `GetTotalRounds() - GetCurrentRound() - 1` algebraically WAS
// miRoundsRemaining -- the value was never wrong -- but it is now a redundant round trip
// through two accessors, and it reads as if it were computing an index. One call instead.
// (The genuine round-INDEX site in this file calls GetCurrentRound() directly and is correct.)
// ============================================================================
s32 GetNetworkRoundsRemaining(const NetworkRoundManager* lpNetworkRoundManager)
{
    return lpNetworkRoundManager->GetRoundsRemaining();
}

}  // anonymous namespace

// ================================================================================================
// ModeManager::StartGameMode -- X360 0x8234FCE8
// ================================================================================================
// ProcessGameEvents case 20 -> here. Tears down whatever mode is running, latches the new mode
// type, resolves it through the 18-slot pointer array, hands the mode a FRESH stack GameModeParams
// to fill in, then runs the four setup passes over what the mode wrote and forwards the filled
// params to SetupGameMode.
//
// [!] THE GameModeParams IS A LOCAL, NOT mCurrentGameModeParams. The console builds it at
// sp+0x50 (2160 bytes of stack) and every callee below takes its address; only SetupGameMode
// copies it into the member. The 20 lines of `-1` stores the pseudocode shows before
// HUDMessageLogic::Prepare are that local's IMPLICIT CONSTRUCTION inlined (the CgsArray
// "used before Construct" sentinel in each embedded Array member) -- reproduced here by simply
// declaring the local, which is what the console source said.
// ================================================================================================
void ModeManager::StartGameMode(GameStateModuleIO::OutputBuffer* lpOutputBuffer,
                                StartGameModeParams*             lpStartGameModeParams)
{
    GameModeParams lGameModeParams;

    // [X] PARKED LEG -- HUDMessageLogic. Console: `addi r3, r31, 0x6B00; bl HUDMessageLogic::Prepare`
    // @0x8234FD28 (ONE argument: the embedded logic at this+27392; the pseudocode's extra five
    // arguments are register residue). PARKED PER CONDUCTOR DECISION #4 -- the HUD-message
    // lifecycle (Construct / Prepare / PreWorldUpdate / PostWorldUpdate) belongs to the event-GUI
    // wave and BrnHUDMessageLogic.h declares no Prepare at all (only the six
    // GenerateOnlineStuntRun* generators). The MEMBER exists; only this call is parked.
    // header_request #2. Behaviour cost: online stunt-run HUD messages are not re-armed at mode
    // start -- offline stunt races post no HUD messages through this path.
    //     mHUDMessageLogic.Prepare();

    // The old mode is torn down "as a quit" unless we are leaving the online free-burn lobby or
    // online showtime, which hand over rather than quit.
    //   asm 0x8234FD38..0x8234FD70: r11 = (meCurrentGameModeType == 15 || == 16);
    //   r5 = (r11 == 0) via cntlzw/extrwi -- i.e. the argument is the NEGATION.
    const bool lbHandingOverFromOnlineLobbyOrShowtime =
        (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY) ||
        (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME);

    ExitCurrentMode(lpOutputBuffer,
                    !lbHandingOverFromOnlineLobbyOrShowtime,
                    lpStartGameModeParams->GetGameModeType());

    // asm 0x8234FD74..0x8234FDAC: the store happens FIRST, then the range assert reads it back.
    meCurrentGameModeType = lpStartGameModeParams->GetGameModeType();
    CGS_ASSERT(meCurrentGameModeType > GameStateModuleIO::E_MODE_NONE &&
                   static_cast<s32>(meCurrentGameModeType) < KI_GAME_MODE_SLOTS,
               "( meCurrentGameModeType > GameStateModuleIO::E_MODE_NONE ) && ( meCurrentGameModeType < GameStateModuleIO::E_MODE_COUNT )");

    // hazards H3: slots 6 (E_MODE_ELIMINATOR) and 9 (E_MODE_TRAFFIC_ATTACK) are AUTHORED NULLS.
    // Dispatching into them is a null call and THIS is the assert the console fires. Keep it.
    mpCurrentGameMode = mapGameModes[meCurrentGameModeType];
    CGS_ASSERT(mpCurrentGameMode != NULL, "mpCurrentGameMode");

    // asm 0x8234FDE4..0x8234FDF4. The console's argument is
    // OutputBuffer::GetTriggerManagementInputInterface() (0x8231D758, write-locked, returns
    // this+0x9050); the tree's TQM signature takes the InRemoveTriggerEvent queue that interface
    // embeds at +131088, which is the queue the callee actually appends to.
    mpTriggerQueryManager->ClearLandmarkIndexesForGameMode(
        lpOutputBuffer->GetTriggerManagementInputInterface()->GetRemoveTriggerEventQueue());

    // vtbl+20 == slot 5 (vtable micro-check, 2026-08-26): Start(startParams, gameModeParams,
    // scoringSystem). THE stunt-race seat: StuntAttackMode::Start @0x82331E98.
    mpCurrentGameMode->Start(lpStartGameModeParams, &lGameModeParams, &mScoringSystem);

    // `lbz r11, 0xAC(mode)` == GameMode::mbIsOnline (+172), i.e. IsOnline().
    if (!mpCurrentGameMode->IsOnline())
    {
        SetUpCheckPointsForGameMode(lpStartGameModeParams, &lGameModeParams, &mScoringSystem);
    }

    SetupPathfinding(lpStartGameModeParams, &lGameModeParams);
    SetupOpponentData(lpStartGameModeParams, &lGameModeParams);
    SetupCheckpointDistricts(&lGameModeParams);

    // [x] UN-PARKED 2026-08-26 (fix round). Both blockers this leg was parked on have landed:
    // BrnGameMode.h now carries the console 26-slot order with
    // `virtual CgsSystem::EFrameRateManagerType GetFrameRateType() const;` at slot 7 (:230), and
    // the enum's real home already exists at CgsFrameRate.h:27 (the earlier "does not exist
    // anywhere in the tree" claim is refuted -- it is also what
    // CgsFrameRateTypeRequestInterface.h:27-34 types its own member with).
    // Console @0x8234FE6C..0x8234FEA8, store order preserved (`stw` FIRST, then `stb` -- note this
    // is the OPPOSITE order from ExitCurrentMode's write of the same two fields):
    //     lwz r3, 0xD98(r31) / lwz r11, 0(r3) / lwz r11, 0x1C(r11) / bctrl   ; vtbl+28 == slot 7
    //     mr  r29, r3
    //     bl  OutputBuffer::GetFrameRateTypeRequestInterface                 ; -> r11
    //     stw r29, 0(r11)          ; meRequestedFrameRateType
    //     stb r7(1), 4(r11)        ; mbRequestValid = 1
    // Cross-check on the value: all EIGHT offline mode vtables return 1 from slot 7 (folded leaf
    // 0x82C296C8 = `li r3,1; blr`) and all SEVEN online ones return 2 (0x827DF718) -- so for a
    // stunt race this requests type 1, matching ExitCurrentMode's hard-coded 1 on the way out.
    // [!] FLAG -- OPAQUE PLACEHOLDER WRITE, same as ExitCurrentMode's below:
    // GameStateModuleIO::OutputBufferFrameRateTypeReqInterface is still
    // `struct { u8 maOpaque[12]; }` (BrnGameStateModuleIO.h:274, "Swap for the real
    // FrameRateTypeRequestInterface"), so the two console offsets (+0 word, +4 byte) are written
    // through that storage rather than through named members. header_request #6 still stands; when
    // it lands both of this file's blocks become plain assignments.
    {
        const s32 liRequestedFrameRateType =
            static_cast<s32>(mpCurrentGameMode->GetFrameRateType());

        GameStateModuleIO::OutputBufferFrameRateTypeReqInterface* lpFrameRateRequest =
            lpOutputBuffer->GetFrameRateTypeRequestInterface();

        std::memcpy(&lpFrameRateRequest->maOpaque[0], &liRequestedFrameRateType, sizeof(s32));
        lpFrameRateRequest->maOpaque[4] = 1;
    }

    // this+0x6A90 == mScoringSystem + 0x5CE0. PINNED, not guessed: BrnScoringSystem_Lookup.cpp:532
    // already annotates the identical store as `SetCheckPointDistancesToFinishReady(false)
    // // ASM stb r30,0x5CE0`.
    mScoringSystem.SetCheckPointDistancesToFinishReady(false);

    // [stuntrace waveB fix round] renamed from mbFinishedOnlineLobbyMode -- same byte (+38150),
    // now named from its proven reader/writer TransmitCheckPointDistancesToFinishLine @0x82341FF8.
    mbWayPointDistancesToFinishSent = false;                      // `stbx r27, r31, 0x9506`

    // `lfs f0, var_844(sp)` == the local GameModeParams + 0x6C. The mode's Start() has just
    // filled it in; the burning-route medal times sit at the three words below it (+96/+100/+104).
    mfModeTimeLimit = lGameModeParams.mfModeTimeLimit;            // `stfsx f0, r31, 0x8024`

    // [!] THE SUSPECT-OFFSET RESOLUTION the frozen header asked agent 2 for (BrnModeManager.h's
    // mRaceId banner). PINNED FROM THE ASSEMBLY, not the pseudocode:
    //     0x8234FE94  lis r10, 0
    //     0x8234FEA4  ori r10, r10, 0x8030            <- 0x8030 == 32816
    //     0x8234FEB0  ld  r11, 0x2C8(r30)             <- startParams + 712 (8-byte CgsID)
    //     0x8234FED8  stdx r11, r31, r10              <- this + 32816, 8-byte store
    // So mRaceId IS at console +32816 (0x8030) and its source IS StartGameModeParams+712. That
    // also settles BrnGameModeParams.h's own "miRaceId // [177] +708" comment: 708 is the word
    // INDEX times four, but a CgsID is 8-aligned, so the member sits at 712 -- which is exactly
    // what this `ld` reads and what meGameModeType@+720 (`lwz r6,0x2D0`) brackets. The header's
    // SUSPECT flag can be cleared.
    mRaceId = lpStartGameModeParams->GetRaceId();

    SetupGameMode(lpOutputBuffer, &lGameModeParams, lpStartGameModeParams);

    // [X] PARKED LEG -- ACCESS, not scope. Console @0x8234FEE0..0x8234FEF8:
    //     for (i = 0; i < 8; ++i) *(s32*)(this + 0x677C + 24*i) = 0;
    // this+0x677C == mScoringSystem + 0x59CC, and 0x59CC is
    // maRaceCarPositioningData[0].miFinishPosition (the array follows maCarData[8], stride 24 ==
    // sizeof(RaceCarPositioningData), and +0x10 is miFinishPosition in the committed member run).
    // The array is PRIVATE in BrnScoringSystem.h with no accessor. header_request #5.
    // Behaviour cost: stale finish positions survive into the next event's race-position table.
    //     for (s32 liCar = 0; liCar < 8; ++liCar)
    //         mScoringSystem.GetRaceCarPositioningData(liCar)->miFinishPosition = 0;

    // ---- road-rage / marked-man: spend one point of the player's crash budget if the car is
    // already carrying unlock deformation. asm 0x8234FF00..0x8234FFAC.
    if (meCurrentGameModeType == GameStateModuleIO::E_MODE_ROAD_RAGE ||
        meCurrentGameModeType == GameStateModuleIO::E_MODE_MARKED_MAN)
    {
        // `lwz r11,0x6D58(this); addis r30,r11,4; addi r30,r30,-0x6820` == mpGameStateModule +
        // 235488 (0x397E0) == GameStateModule::mLastActiveRaceCarInterface -- the module's
        // END-OF-LAST-WORLD-UPDATE snapshot, NOT the live interface (BrnModeManager.h accessor
        // note 2b: the two are never collapsed).
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpLastActive =
            mpGameStateModule->GetLastActiveRaceCarInterface();

        // The console inlines RCEntityActiveRaceCarOutputInterface::GetPlayerActiveRaceCarIndex
        // (0x82277BF8) here -- ITS assert is the "Player car index hasn't been set" the export
        // shows, fired from BrnRaceCarEntityModuleOutputInterface.h:980. De-inlined to the real
        // call, so the assert is not duplicated.
        const ::EActiveRaceCarIndex lePlayerActiveRaceCarIndex =
            lpLastActive->GetPlayerActiveRaceCarIndex();

        const CgsID lPlayerCarModelId = lpLastActive->GetCarModelId(lePlayerActiveRaceCarIndex);

        // `addi r3, mpProgressionManager, 0x170` == &ProgressionManager::mProfile (GetProfile()
        // inlined).
        BrnProgression::CarData* lpPlayerCarData =
            mpProgressionManager->GetProfile()->FindCar(lPlayerCarModelId);

        // `lfs f0, 0xC(r3); fcmpu f0, f31(=flt_82001CC0)` -- and flt_82001CC0 IS 0.0f:
        // image.bin offset 0x1CC0 (VA 0x82001CC0) reads 00 00 00 00 big-endian, dumped this
        // session. CarData+0x0C is mfUnlockDeformedAmount (BrnProgressionCarData.h:78).
        if (lpPlayerCarData != NULL && lpPlayerCarData->GetUnlockDeformationAmount() > 0.0f)
        {
            // DE-INLINED, not parked: the console does
            //     r11 = mScoringSystem.miMaximumPlayerCrashedNumber - 1;   // ss + 0x4B58
            //     mScoringSystem.miMaximumPlayerCrashedNumber = r11;
            //     CGS_ASSERT(r11 > 0, "miMaximumPlayerCrashedNumber > 0");
            //         // ...gamestate\modemanager\Scoring/BrnScoringSystem.h line 3561
            // which is exactly ScoringSystem::ReducePlayerDurability() (DWARF
            // BrnScoringSystem.h:1192, declared public in the committed header, body not yet
            // landed). The member is private and this is its only caller, so calling the named
            // method is the correct de-inlining; the two lines above are recorded verbatim so
            // whoever bodies it has the source. FLAG: name match + semantics match, not an
            // address match (the console emitted no call here).
            mScoringSystem.ReducePlayerDurability();
        }
    }

    mfTimeInMode = 0.0f;                                          // `stfsx f31(=0.0f), r31, 0x9520`

    // hazards H2: SetupStuntChallenge @0x8231EB00 is ALREADY BODIED (BrnModeManager.cpp:212).
    if (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY)
    {
        SetupStuntChallenge();
    }
}

// ================================================================================================
// ModeManager::ExitCurrentMode -- X360 0x8234FFE0
// ================================================================================================
// Two behaviours in one function:
//   (1) THE ONLINE-SHOWTIME HANDOVER. If an ONLINE showtime/crash mode is ending normally with no
//       successor named, do not exit at all -- restart straight into the online free-burn lobby.
//       This is a tail-recursive call back into StartGameMode.
//   (2) Otherwise: tell the mode to reset, drain the stop messages, clear the latches.
// The whole body is a no-op when mpCurrentGameMode is already NULL (idle free-burn), which is why
// StartGameMode can call it unconditionally.
// ================================================================================================
void ModeManager::ExitCurrentMode(GameStateModuleIO::OutputBuffer* lpOutputBuffer,
                                  bool                             lbTimedOut,
                                  GameStateModuleIO::EGameModeType leNextGameModeType)
{
    GameMode* lpCurrentGameMode = mpCurrentGameMode;

    // `lwz r3,0xD98; if (r3) r11 = r3->+0xAC else r11 = 0` -- the null-safe IsOnline().
    const bool lbCurrentModeIsOnline =
        (lpCurrentGameMode != NULL) && lpCurrentGameMode->IsOnline();

    if (lbCurrentModeIsOnline)
    {
        const bool lbShowtime =
            (meCurrentGameModeType == GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME) ||
            (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME);

        // `cmpwi r31,-1 / cmpwi r31,0x12` -- E_MODE_NONE or the 18 "no next mode" sentinel
        // UpdateCurrentMode passes (hazards H3: 18 == KI_GAME_MODE_SLOTS, one past the last slot).
        const bool lbNoSuccessorNamed =
            (leNextGameModeType == GameStateModuleIO::E_MODE_NONE) ||
            (static_cast<s32>(leNextGameModeType) == KI_GAME_MODE_SLOTS);

        if (lbShowtime && !lbTimedOut && lbNoSuccessorNamed)
        {
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "\n\nONLINE SHOWTIME: NORMAL EXIT BACK TO ONLINE FREE BURN LOBBY\n\n";
            }

            // The 16 x 44-byte `-1` fill + the `-1` count word the pseudocode shows before the
            // Construct call are this local's IMPLICIT construction (the embedded
            // Array<CheckpointData,16>'s "used before Construct" sentinels) -- declaring the
            // local reproduces them.
            StartGameModeParams lStartGameModeParams;

            // `li r4,0xF; li r5,0; vspltisw v1,0` -- the Vector3 player position travels in a
            // vector register (BrnGameModeParams.cpp:170 records the same `vmr128 v127,v1` on the
            // callee side) and it is ZERO here.
            Vector3 lZeroPlayerPosition;
            lZeroPlayerPosition.SetZero();

            lStartGameModeParams.Construct(GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY,
                                           lZeroPlayerPosition,
                                           E_GAMEMODESTARTMECHANISM_DEFAULT);

            StartGameMode(lpOutputBuffer, &lStartGameModeParams);
            return;
        }
    }

    if (lpCurrentGameMode == NULL)
    {
        return;
    }

    // vtbl+48 == slot 12 (vtable micro-check): SendEvent(EGameModeEvent), argument 0.
    // [!] The argument is the ENUM E_GME_RESTART, not a bool -- the same slot the three other
    // ModeManager sites drive with E_GME_NEXT.
    lpCurrentGameMode->SendEvent(E_GME_RESTART);

    SendModeStopMessages(lpOutputBuffer->GetGameActionQueue(), lbTimedOut, leNextGameModeType);

    // asm 0x8234C11C..0x82350128. The console asks for frame-rate-manager type 1 -- which is
    // exactly what all EIGHT offline mode vtables return from slot 7 (folded leaf 0x82C296C8,
    // `li r3,1; blr`), i.e. "back to the offline/free-roam frame-rate manager".
    // [!] FLAG -- OPAQUE PLACEHOLDER WRITE: GameStateModuleIO::OutputBufferFrameRateTypeReqInterface
    // is still `struct { u8 maOpaque[12]; }` in BrnGameStateModuleIO.h:274 ("Swap for the real
    // FrameRateTypeRequestInterface when those are homed"). The two console offsets (+0 word,
    // +4 byte) are written through that opaque storage rather than through named members;
    // header_request #6 asks for the two members so this becomes an assignment.
    {
        GameStateModuleIO::OutputBufferFrameRateTypeReqInterface* lpFrameRateRequest =
            lpOutputBuffer->GetFrameRateTypeRequestInterface();
        const s32 liRequestedFrameRateType = 1;
        // `stb r28(1), 4(r3)` then `stw r28(1), 0(r3)` -- in that order on the console.
        lpFrameRateRequest->maOpaque[4] = 1;
        std::memcpy(&lpFrameRateRequest->maOpaque[0], &liRequestedFrameRateType, sizeof(s32));
    }

    // Online free-burn lobby / online showtime only: re-post the "stop the mode intro" action so
    // the GUI drops the intro splash on the way out.
    if ((meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY ||
         meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME) &&
        mbHasCrashedOut)   // `lbzx r11, r29, 0x94FC` -- see the FLAG below
    {
        CGS_ASSERT(lpOutputBuffer != NULL, "lpOutput");
        CGS_ASSERT(lpOutputBuffer->GetGameActionQueue() != NULL, "lpOutput->GetGameActionQueue()");

        GameStateModuleIO::StopModeIntroAction lStopModeIntro;
        lStopModeIntro.meGameMode = meCurrentGameModeType;
        // `(mode == 15 || mode == 16) && *(this+0x9508)` -- that composite IS
        // ModeManager::IsOnlineModeWithInstantIntro() (agent 9 bodies it; the frozen header
        // spells the same composite in its declaration comment).
        lStopModeIntro.mbMovingBetweenLobbyModes = IsOnlineModeWithInstantIntro() ? 1 : 0;
        lStopModeIntro.maPad05[0] = 0;
        lStopModeIntro.maPad05[1] = 0;
        lStopModeIntro.maPad05[2] = 0;

        lpOutputBuffer->GetGameActionQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lStopModeIntro),
            GameStateModuleIO::E_ACTION_STOP_MODE_INTRO,
            static_cast<s32>(sizeof(GameStateModuleIO::StopModeIntroAction)));
    }

    // The console RE-READS mpCurrentGameMode here rather than reusing lpCurrentGameMode
    // (`lwz r11, 0xD98(r29)` @0x82350224) -- SendModeStopMessages could in principle have moved
    // it. Reproduced literally.
    const bool lbExitingModeIsOnline =
        (mpCurrentGameMode != NULL) && mpCurrentGameMode->IsOnline();

    if (!lbExitingModeIsOnline)
    {
        // `lis r10,2; ori r10,r10,0x971; stbx 1, mpProgressionManager, r10` == the byte at
        // ProgressionManager+133489, which BrnProgressionManager.h:291 already names:
        // RequestUpdateRivals() (mbUpdateRivalsRequested). Leaving an OFFLINE event dirties the
        // rival set.
        mpProgressionManager->RequestUpdateRivals();
    }

    mpCurrentGameMode              = NULL;
    meCurrentGameModeType          = GameStateModuleIO::E_MODE_NONE;   // `li r11,-1; stw 0xD94`
    mbFinishCurrentModeNextUpdate  = false;                            // `stbx r26(0), r29, 0x94F7`

    ClearLandmarkAndFinishLineData();
}

// ================================================================================================
// ModeManager::SendModeStopMessages -- X360 0x8234BEC0
// ================================================================================================
// The mode-teardown broadcast. Builds the 24-byte action-39 summary record, then fans out one of
// six terminal actions depending on online/offline x quit/stop/last-round, clears the scoring
// system, tells the GameStateModule the mode ended, and stops the mode clocks.
//
// SLICING (hazards H7 -- SendModeStopMessages is on the MAY-SLICE list): TWO online-only blocks
// carry `ONLINE ARM DEFERRED` banners naming every console call they omit. Everything reachable
// with mpCurrentGameMode->IsOnline() == false is WHOLE.
// ================================================================================================
void ModeManager::SendModeStopMessages(GameStateModuleIO::GameActionQueue* lpGameActionQueue,
                                       bool                                lbTimedOut,
                                       GameStateModuleIO::EGameModeType    leNextGameModeType)
{
    CGS_ASSERT(mpNetworkRoundManager != NULL, "mpNetworkRoundManager");   // BrnModeManager.cpp:3695

    // ---- the 24-byte action-39 summary (StopModeAction, BrnGameActions.h:1114) --------------
    // Field identities below are new this wave: BrnGameActions.h currently marks +0x08 / +0x10 /
    // +0x12 / +0x13 / +0x14 / +0x15 FLAG. Reported so those comments can be tightened.
    GameStateModuleIO::StopModeAction lStopModeAction;
    lStopModeAction.miField04      = static_cast<s32>(leNextGameModeType);   // `stw r14, var_10C`
    lStopModeAction.mu8Field11     = lbTimedOut ? 1u : 0u;                   // `stb r15, var_FF`
    lStopModeAction.meGameModeType = meCurrentGameModeType;                  // `stw r11, var_110`
    lStopModeAction.miField0C      = miNumUnsucessfulGameModeAttempts;       // `stw r9,  var_104`
    lStopModeAction.mu8Field15 =
        mpNetworkRoundManager->GetStartingFreeburnLobbyDueToPlayerJoin() ? 1u : 0u;  // NRM+0x130

    // hazards H2: EndStuntChallenge @0x823120E8 is ALREADY BODIED (BrnModeManager.cpp:226).
    if (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY)
    {
        EndStuntChallenge();
    }

    const bool lbCurrentModeIsOnline =
        (mpCurrentGameMode != NULL) && mpCurrentGameMode->IsOnline();

    if (lbCurrentModeIsOnline)
    {
        lStopModeAction.mu8Field10 = 1u;
        lStopModeAction.miField08  = mpNetworkRoundManager->GetCurrentRound();  // +300 - +296 - 1
    }
    else
    {
        lStopModeAction.mu8Field10 = 0u;
        lStopModeAction.miField08  = 0;
    }

    lStopModeAction.mu8Field14 =
        (leNextGameModeType == GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME ||
         leNextGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME) ? 1u : 0u;

    if (meCurrentGameModeType == GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME ||
        meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME)
    {
        // [X] PARKED ARM -- the SHOWTIME mode-switch post. Console @0x8234BFDC..0x8234C044:
        //     record[0] = *(mpGameStateModule + 232296);        // mLocalPlayerNetworkID
        //     record[4] = mePlayerActiveRaceCarIndex;           // this + 0x8038
        //     record[8] = (100 * (s32)(*(f32*)(this+0x10D8) * 1.0936133f)
        //                       + *(s32*)(this+0x10B4)) * *(s32*)(this+0x10AC);
        //     *(u8*)(record + 0x0C) = 0;                        // stb r20, var_114 @0x8234BFF0
        //     AddEvent(lpGameActionQueue, record, 143, 16);
        // [!] STORE ADDED TO THIS BANNER 2026-08-26 (fix round): the +0x0C byte was previously
        // omitted from the park description, which would have left a 16-byte wire record one field
        // short when it is re-armed. Re-derived against the record base -- the AddEvent payload
        // pointer is `addi r4, r1, var_120` @0x8234C004, and var_120 - var_114 == 0x0C -- and r20
        // is this body's zero register. So the record is
        // { u32 @+0x00 network id, u32 @+0x04 active race-car index, s32 @+0x08 score, u8 @+0x0C 0 }
        // with +0x0D..+0x0F padding to the posted size of 16.
        // Action 143 == DWARF 135 E_ACTION_SHOWTIME_MODE_SWITCH under the +8 shift measured above
        // -- and the ModeManager member that drives it is literally miFramesUntilModeSwitchSend,
        // which PrepareForMode arms only for modes 2 / 16. The three scalars are
        // mScoringSystem + 0x328 / +0x304 / +0x2FC, i.e. CrashModeScoring internals (the crash
        // score, its bonus and its multiplier); 1.0936133f is the metres->yards constant,
        // IMAGE-CITED from flt_820DB5A8 (image.bin offset 0xDB5A8 reads 3F 8B FB 85 big-endian
        // == 1.0936132669448853f).
        // BLOCKED ON FOUR ABSENT DECLARATIONS, filed as header_requests #7..#9:
        //   * GameStateModule::GetLocalPlayerNetworkID() -- the member at gsm+232296 is named
        //     (BrnGameStateFlybyManager.cpp:131/151 identifies it) but BrnGameStateModule.h
        //     declares neither member nor accessor;
        //   * three CrashModeScoring accessors for +0x2FC / +0x304 / +0x328 (hazards H9 forbids
        //     reaching ScoringSystem internals by raw offset);
        //   * the 16-byte action-143 record.
        // Behaviour cost: SHOWTIME (crash mode) does not publish its end-of-mode switch summary.
        // OFF THE STUNT-RACE PATH ENTIRELY (modes 2 and 16 only).
    }

    // this + 0x9500 == mbModeStartFromRegionEnabled. Forced true for a sub-two-player non-showtime
    // mode, i.e. "there was nobody to race, treat it as a region start".
    lStopModeAction.mu8Field13 = mbModeStartFromRegionEnabled ? 1u : 0u;
    if (meCurrentGameModeType != GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY &&
        meCurrentGameModeType != GameStateModuleIO::E_MODE_ONLINE_SHOWTIME)
    {
        if (meCurrentGameModeType != GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME &&
            meCurrentGameModeType != GameStateModuleIO::E_MODE_ONLINE_SHOWTIME &&
            mScoringSystem.GetNumberOfNonDisconnectedPlayers() < 2)
        {
            lStopModeAction.mu8Field13 = 1u;
        }
    }

    const s32  liRoundsRemaining     = GetNetworkRoundsRemaining(mpNetworkRoundManager);
    const bool lbNoRoundsRemaining   = (liRoundsRemaining == 0);
    lStopModeAction.mu8Field12       = lbNoRoundsRemaining ? 1u : 0u;   // `cntlzw/extrwi` of +296

    lStopModeAction.maPad16[0] = 0;
    lStopModeAction.maPad16[1] = 0;

    lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lStopModeAction),
                                GameStateModuleIO::E_ACTION_STOP_MODE,
                                static_cast<s32>(sizeof(GameStateModuleIO::StopModeAction)));

    // The console re-loads var_FD after the post and keeps it live to the end of the function.
    const bool lbModeStartedFromRegion = (lStopModeAction.mu8Field13 != 0u);

    // ---- the two bool-block latches this function OWNS ---------------------------------------
    // +38151 (mbFinishedOnlineEvent, renamed from muUnkByte_0x9507 by the wave-B fix round):
    // "an ONLINE, non-lobby, non-showtime mode ended cleanly (not timed out, not a sub-two-player
    // region start)". Stated from THIS writer -- and the two readers agree: PrepareForMode copies
    // it into PrepareForModeAction::mbFinishedOnlineEvent (action +0x8D1) and StartModeIntro into
    // StartModeIntroAction::mbFinishedOnlineEvent.
    mbFinishedOnlineEvent =
        (lbCurrentModeIsOnline &&
         meCurrentGameModeType != GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY &&
         meCurrentGameModeType != GameStateModuleIO::E_MODE_ONLINE_SHOWTIME &&
         !lbTimedOut && !lbModeStartedFromRegion);

    // +38152 (mbInstantIntroSplash): set when the LOBBY/SHOWTIME pair ended cleanly -- which is
    // what makes the next lobby entry skip its timed intro. Consistent with the frozen header's
    // reader-side pinning (StopModeIntro's payload byte + IsOnlineModeWithInstantIntro).
    mbInstantIntroSplash =
        ((meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY ||
          meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME) &&
         !lbTimedOut && !lbModeStartedFromRegion);

    // "we are handing the online lobby/showtime pair straight on to another one of the pair" --
    // the console keeps this in r19 and uses it three times at the tail.
    const bool lbOnlineLobbyHandover =
        mbInstantIntroSplash &&
        (leNextGameModeType == GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY ||
         leNextGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME);

    if (lbCurrentModeIsOnline)
    {
        // ================================================================================
        // [!] ONLINE ARM DEFERRED -- the network results / round-save block.
        // ================================================================================
        // Console @0x8234C210..0x8234C4CC, gated on `mpCurrentGameMode->IsOnline()`:
        //   v39 = mpGameStateModule->GetLocalPlayerNetworkID();          // gsm + 232296
        //   if (v39 == -1) { mScoringSystem.ClearCumulativeData(); }     // 0x8231F140 (DEF)
        //   else {
        //     if (!lbTimedOut && meCurrentGameModeType == E_MODE_ONLINE_RACE)
        //       mpProgressionManager->OnOnlineRaceComplete(                // 0x82366B98 (ABSENT)
        //           mScoringSystem.GetNumberOfNetworkPlayers(),            // 0x82311020
        //           mScoringSystem.GetLead() == GetCarData(v39)->+0x144);  // 0x82310DA0 / 0x8231DD88
        //     if (meCurrentGameModeType != E_MODE_ONLINE_SHOWTIME)
        //       mScoringSystem.SaveNetworkRoundData(v39, &mTimerStatusInterface-sim-time,
        //           mpGameStateModule->GetPlayerActiveRaceCarIndex(), meCurrentGameModeType);
        //                                                                 // 0x8232BC78
        //     if (mode is not the lobby/showtime pair) {
        //       OnlineRoundResults lResults;  lResults.Construct();        // declared-only
        //       for (car = 0; car < 8; ++car) { ... GetOnlineFinishPosition / SetPosition
        //                                       ... Array<int,8>::Append with the
        //                                       "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID"
        //                                       assert (BrnGameActions.h:6641) ... }
        //       AddEvent(lpGameActionQueue, &lResults, 230, 68);
        //     }
        //     if (lbTimedOut) GetCarData(v39)->+0x134 = mScoringSystem + 0x4DFC;
        //     if (mode is the pair || !roundsRemaining || lbTimedOut) {
        //       SendGameResultsToNetwork(lpGameActionQueue);               // 0x82343E88 (agent 9)
        //       for (car = 0; car < 8; ++car) { maCarData[car].+0x134 = 0; .+0x138 = -1; }
        //       mScoringSystem.mOnlineGameResults.Clear();                 // 0x8230F178
        //     }
        //   }
        // DEFERRED because it needs, at minimum: GetLocalPlayerNetworkID (ABSENT),
        // OnlineRoundResults::Construct (declared-only), the two unnamed ScoringSystem CarData
        // helpers sub_8231DD88 / sub_8231DCD0 / sub_82326878, two private CarData fields
        // (+0x134 / +0x138) with no accessors, ProgressionManager::OnOnlineRaceComplete (ABSENT),
        // and the E_ACTION_ONLINE_ROUND_RESULT value correction below.
        // [!!] VALUE CORRECTION TO REPORT: BrnGameActions.h spells
        // `E_ACTION_ONLINE_ROUND_RESULT = 222` (the PS3 value). The X360 posts THIS record --
        // sizeof(OnlineRoundResults) == 68, matching the literal `li r6,0x44` -- with
        // `li r5,0xE6` == 230, i.e. DWARF 222 + 8, the same shift the freeburn-challenge block
        // already records. Filed as a header_request.
        // OFFLINE IMPACT: none. Every statement above is inside the IsOnline() gate.
    }

    // `if (mode is not the lobby/showtime pair && (regionStart || noRoundsRemaining || timedOut))`
    if (meCurrentGameModeType != GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY &&
        meCurrentGameModeType != GameStateModuleIO::E_MODE_ONLINE_SHOWTIME &&
        (lbModeStartedFromRegion || lbNoRoundsRemaining || lbTimedOut))
    {
        mScoringSystem.ClearDisconnectedPlayers();
    }

    // ---- the terminal action: one of six, chosen by online/offline x quit/stop/last-round ----
    if (lbCurrentModeIsOnline)
    {
        // ================================================================================
        // [!] ONLINE ARM DEFERRED -- the online half of the terminal-action switch.
        // ================================================================================
        // Console @0x8234C548..0x8234C64C:
        //   if (lbTimedOut)                 -> action 41 (QUIT_MODE_ONLINE), size 8,
        //                                      {meCurrentGameModeType, 1, junkyard != null}
        //   else if (lbModeStartedFromRegion)-> action 41, size 8,
        //                                      {meCurrentGameModeType, 0, junkyard != null}
        //   else if (!lbOnlineLobbyHandover && !roundsRemaining && mode is not showtime)
        //                                   -> action 27 (FINISH_MODE_FINAL_ONLINE), size 4,
        //                                      {meCurrentGameModeType}
        //   else                            -> action 26 (FINISH_MODE_ONLINE), size 8,
        //                                      {meCurrentGameModeType, leNextGameModeType}
        // The two action-41 arms read `ld r11, (mpGameStateModule + 0x2CDC0)` -- the
        // CarSelectManager junkyard id (BrnGameStateModule.h:411 already annotates that exact
        // load as `mCarSelectManager.mJunkyardId != kCGSID_NULL`) -- and BrnGameStateModule.h
        // exposes no accessor for it. header_request #10 (plus the three payload records above).
        // OFFLINE IMPACT: none.
    }
    else
    {
        if (lbTimedOut)
        {
            QuitModeOfflineActionRecord lQuitModeOffline;
            lQuitModeOffline.mbStartingGameDueToPlayerJoin =
                mpNetworkRoundManager->GetStartingFreeburnLobbyDueToPlayerJoin() ? 1u : 0u;

            lpGameActionQueue->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lQuitModeOffline),
                GameStateModuleIO::E_ACTION_QUIT_MODE_OFFLINE,
                static_cast<s32>(sizeof(QuitModeOfflineActionRecord)));

            // `li r11, 0x2F` == 47 == BrnProgression::E_TRAINING_TYPE_QUITS_EVENT -- the tip that
            // fires when the player bails out of an event. (That the literal lands exactly on the
            // QUITS_EVENT enumerator is the independent check on the action-40 identification
            // above: both halves of this arm say "the player quit".)
            RequestGameTrainingActionRecord lTrainingRequest;
            lTrainingRequest.meTrainingType =
                static_cast<s32>(BrnProgression::E_TRAINING_TYPE_QUITS_EVENT);

            lpGameActionQueue->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lTrainingRequest),
                GameStateModuleIO::E_ACTION_REQUEST_GAME_TRAINING,
                static_cast<s32>(sizeof(RequestGameTrainingActionRecord)));
        }
        else
        {
            // [!] DELIBERATE, NARROW DIVERGENCE: the console posts the 1-byte var_130 slot here
            // WITHOUT writing it -- it holds whatever the quit arm (which did not run) left on the
            // stack. Posting indeterminate bytes into the shared 13312-byte queue is not
            // reproducible on the host, so the byte is zeroed. No consumer of action 25 reads it
            // (the record's size is 1 and TranslateGameActionsToGuiEvents' arm takes no payload).
            ImpactTimeEndActionRecord lStopModeOffline;
            lStopModeOffline.mu8Payload = 0;

            lpGameActionQueue->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lStopModeOffline),
                GameStateModuleIO::E_ACTION_STOP_MODE_OFFLINE,
                static_cast<s32>(sizeof(ImpactTimeEndActionRecord)));
        }
    }

    // [x] UN-PARKED 2026-08-26 (fix round) -- THE HIGHEST-VALUE ITEM IN THE BATCH-1 VERDICT IS NOW
    // LIVE. Both halves are in the tree:
    //   ACCESS  -- BrnScoringSystem.h moved `void ClearData(bool lbFull);` into the public block
    //              this same fix round (its own banner records the C2248 measurement).
    //   BODY    -- BrnScoringSystem_Lookup.cpp:474 `void ScoringSystem::ClearData(bool
    //              lbResetCarData)` is a full semantic-parity reconstruction of 0x8232A4A8
    //              (ClearModeTimer / ClearTimeLimit, the six sub-scorer resets, the FLT_MAX
    //              distance seeds). [x] BrnScoringSystem.h's "has NO body anywhere in src/" note
    //              -- the one factual error in the batch-1 escalation -- was CORRECTED at the
    //              declaration this same closure round; both halves now agree with the tree.
    //              The call below compiles and links.
    // Console @0x8234C6A0..0x8234C6B0:
    //     addi   r3, r28, 0xDB0          ; &mScoringSystem
    //     cntlzw r11, r31 / extrwi r29, r11, 1,26   ; r29 = !lbOnlineLobbyHandover
    //     mr     r4, r29
    //     bl     BrnGameState__ScoringSystem__ClearData      ; 0x8232A4A8
    // The argument is the LOGICAL NEGATION of lbOnlineLobbyHandover: `cntlzw` + `extrwi ...,1,26`
    // on r31 is the standard PPC "is-zero -> 1" idiom, not a copy.
    //
    // WHAT WAS AT STAKE: this is the offline end-of-mode scoring reset. While it was parked, every
    // event's scores, timers and finish flags leaked into the next event -- which would have read on
    // the campaign's own boot oracle as a scoring bug rather than as a missing wire.
    mScoringSystem.ClearData(!lbOnlineLobbyHandover);   // X360 0x8232A4A8 @0x8234C6B0

    if (!lbOnlineLobbyHandover)
    {
        // [X] PARKED LEG -- the BurnoutSkillzManager buffered-road-score reset. Console
        // @0x8234C6B4..0x8234C6D8, with r30 == this + 0xC40 == mOnlineFreeBurnLobby + 184 (the
        // embedded BurnoutSkillzManager region the frozen header's mOnlineFreeBurnLobby banner
        // names, and which BrnModeManager_Lifecycle.cpp's PARKED STORES leg already parks the
        // StreetManager/MugshotManager half of):
        //     mBufferedChallengeScore.Construct();               // 0x8267D7E8, skillz + 0x40
        //     meBufferedScoreType          = 2;                  // skillz + 0x6C
        //     mBufferedScoreChallengeIndex = dword_820A766C;     // skillz + 0x68
        // dword_820A766C IS -1, IMAGE-CITED: image.bin offset 0xA766C (VA 0x820A766C) reads
        // FF FF FF FF big-endian. [!] The pseudocode's `*(a1+3240) = -1` hides that this is a
        // LOAD from rodata, not an immediate -- worth keeping, because a placeholder-zero sweep
        // that "fixed" a literal -1 would be wrong for the same reason a .bss zero is not a
        // console zero.
        // BLOCKED: BrnOnlineFreeBurnLobbyMode has NO BurnoutSkillzManager member on host (the
        // class declares only GetName + Start), and BurnoutSkillzManager's three fields are
        // private with no accessor. header_request #11.
        // Behaviour cost: a road score buffered when the event started stays buffered.
    }

    // [X] PARKED LEG -- GameStateModule::OnModeEnd. Console @0x8234C6E4:
    //     mpGameStateModule->OnModeEnd(!lbOnlineLobbyHandover);   // X360 0x823767E0
    // PARKED PER CONDUCTOR DECISION #6: GameStateModule::OnModeFinish / OnModeEnd /
    // WaitForStreaming belong to the DETECTION / START-DRIVER wave, not this one, and none of
    // the three exists on BrnGameStateModule.h today. Re-wire there.
    //     mpGameStateModule->OnModeEnd(!lbOnlineLobbyHandover);

    // RE-ARMED 2026-08-26 (mode-tick verify): the controller-state reset. Console @0x8234C70C:
    //     *(s32*)(mpGameStateModule + 232292) = 0;   // meControllerState = E_CONTROLLERSTATE_NOT_IN_GAME
    // The park's own condition fired: UpdateCurrentMode's countdown arm now sets state 3
    // (BrnModeManager_UpdateMode.cpp:379), so without this reset the state would never leave
    // ACTIVE_GAME_MODE_STATE after a mode ends. header_request #12's setter landed
    // (BrnGameStateModule.h:748), so the store is made BY NAME.
    mpGameStateModule->SetControllerState(GameStateModule::E_CONTROLLERSTATE_NOT_IN_GAME);

    // ---- the two unconditional tail posts -----------------------------------------------------
    // [!] These are TWO SEPARATE 48-byte / 1-byte stack records that the console overlaid on the
    // action-39 and quit-arm slots (var_110 and var_130). IDA merged the first with the action-39
    // record, which is why the pseudocode appears to re-post a mutated StopModeAction.
    SetPlayerCarDriverActionRecord lSetPlayerCarDriver;
    // The console writes ONLY these two fields and posts the rest as stack residue; zeroed here
    // for the same reproducibility reason as the action-25 payload above.
    std::memset(&lSetPlayerCarDriver, 0, sizeof(lSetPlayerCarDriver));
    lSetPlayerCarDriver.miField00 = 1;    // `stw r16(1), var_110`
    lSetPlayerCarDriver.mu8Field2C = 0;   // `stb r20(0), var_E4`  (var_110 + 0x2C)

    lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSetPlayerCarDriver),
                                GameStateModuleIO::E_ACTION_SET_PLAYER_CAR_DRIVER,
                                static_cast<s32>(sizeof(SetPlayerCarDriverActionRecord)));

    ImpactTimeEndActionRecord lImpactTimeEnd;
    lImpactTimeEnd.mu8Payload = 0;        // console: the same un-written 1-byte slot
    lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lImpactTimeEnd),
                                GameStateModuleIO::E_ACTION_IMPACT_TIME_END,
                                static_cast<s32>(sizeof(ImpactTimeEndActionRecord)));

    // `lfs f0, flt_82001CC0` -- IMAGE-CITED 0.0f (image.bin offset 0x1CC0 reads 00 00 00 00).
    // Both stores are `stfsx`, i.e. the two mode clocks at +0x951C and +0x9520. mfTimeInOnline
    // (+0x9524) is deliberately NOT touched here.
    mfTimeInFreeBurn = 0.0f;
    mfTimeInMode     = 0.0f;
}

}  // namespace BrnGameState
