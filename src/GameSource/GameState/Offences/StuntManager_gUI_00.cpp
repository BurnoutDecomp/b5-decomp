// b5-decomp/src/GameSource/GameState/Offences/StuntManager_gUI_00.cpp
//
// Partfile of the BrnGameState::StuntManager TU (owning header
// GameSource/GameState/Offences/BrnStuntManager.h; the spine lives in BrnStuntManager.cpp).
//
// ONE FUNCTION: StuntManager::ProcessStuntElement @ X360 0x8239CDB0 (449 instructions).
//
// WHY IT IS THE KEYSTONE OF THE gateui WAVE. This is the ONLY producer in the image of game
// action 58 (E_ACTION_ON_STUNT_ELEMENT_COMPLETE), and action 58 is what
// BrnGameModule::TranslateGameActionsToGuiEvents @0x823E9CE0 case 58 turns into Gui event 217
// (GuiEventStuntInfo) / 218 (GuiEventBoostBarStuntInfo), which HudMessageAnalyzer::HandleStuntInfo
// @0x8251F650 renders as the "Billboard Smashed 12/45" HUD popup. Nothing else on the smash /
// billboard path reaches the HUD.
//
// Its two callers are both in BrnStuntManager.cpp:
//   StuntManager::Update      @0x8239F8D0 -- ProcessStuntElement(queue, /*lbIsJump*/false, active)
//                                            after StuntElementTriggered() reports a latch
//   StuntManager::UpdateJumps @0x8239D460 -- ProcessStuntElement(queue, /*lbIsJump*/true,  active)
//                                            0.5 s after the player lands
//
// SIGNATURE: FOUR parameters -- the committed declaration's, and the DWARF's
// (`void ProcessStuntElement(OutputBuffer::GameActionQueue*, bool, bool)`, dwarfdump
// GameSource/GameState/Offences/BrnStuntManager.h:355). A round-1 banner here claimed three, off
// the CALLEE prologue; that reasoning was wrong (see the long correction at the declaration in
// BrnStuntManager.h -- both console call sites materialise r6 immediately before the branch).
// The callee genuinely ignores the trailing bool; this body does too, explicitly.
#include "GameSource/GameState/Offences/BrnStuntManager.h"

#include <stdlib.h>                                         // getenv ([UI-gate] diag ladder)
#include <cstring>                                          // std::memset (the action-58 record)

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CgsDev::Assert::Begin/Fire/EndAssert (verbatim X360 strings)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // CgsDev::Log::gpDebugPrint ([UI-gate] diag ladder)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // CgsModule::VariableEventQueue<13312,16>::AddEvent

#include "GameSource/GameState/BrnGameActions.h"            // WorldStuntAction / OnStuntElementCompleteAction / the three siblings
#include "GameSource/GameState/BrnGameStateModule.h"        // GameStateModule::GetDeveloperChallengeManager
#include "GameSource/GameState/DeveloperChallengeManager/BrnDeveloperChallengeManager.h" // OnCollectStunt
#include "GameSource/GameState/ModeManager/BrnModeManager.h"                 // ModeManager::GetScoringSystem / GetCurrentGameMode(Type)
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"          // GameMode::IsOnline
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"       // ScoringSystem::DealWithStunt
#include "GameSource/GameState/Progression/BrnProgressionManager.h"          // ProgressionManager (profile / counts / unlocks)
#include "GameSource/GameState/Progression/BrnProfile.h"                     // Profile::AddStuntElement / RecordPropHit / ...
#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h" // OnCollectStunt / OnCollectAllStunts

#include "SharedClasses/Trigger/BrnGenericRegion.h"         // BrnTrigger::GenericRegion::GetGroupId / GetId
#include "SharedClasses/World/BrnWorldRegion.h"             // BrnWorld::ECounty

namespace BrnGameState
{

namespace
{
    // Verbatim X360-baked source path for this TU's asserts (identical to BrnStuntManager.cpp's).
    const char* const KAC_FILE =
        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Offences/BrnStuntManager.cpp";

    // Same cast-through as BrnStuntManager.cpp: the DWARF spells the parameter GameActionQueue (an
    // incomplete alias) while the X360 AddEvent calls target the <13312,16> queue directly.
    typedef CgsModule::VariableEventQueue<13312, 16> GameActionQueueImpl;

    // [DIAG] NOT IN THE X360 BINARY. Same env guard + logger as the `[prop-diag] BREAK` rung this
    // ladder hangs off (PropEntityModule_wQ_04.cpp). Evaluated once per process.
    bool UIGateDiagOn()
    {
        static const bool sbDiag = (getenv("BRN_PROP_DIAG") != 0);
        return sbDiag && CgsDev::Log::gpDebugPrint != 0;
    }

    // [DIAG] NOT IN THE X360 BINARY. First-N latch (the wQ_04 pattern) so a multi-element burst
    // cannot flood the log. Mirrors the same helper in BrnStuntManager.cpp; each TU keeps its own
    // counter, exactly as the wQ partfiles do.
    const s32 KI_UI_GATE_DIAG_FIRST_N = 16;

    bool UIGateDiagFirstN(s32* lpiCounter)
    {
        if (!UIGateDiagOn())
            return false;
        if (*lpiCounter >= KI_UI_GATE_DIAG_FIRST_N)
            return false;
        ++(*lpiCounter);
        return true;
    }
}

// ----------------------------------------------------------------------------
// ProcessStuntElement @ 0x8239CDB0
//
// One collectible stunt element (Super Jump / Super Smash / Billboard) has just been COMPLETED.
// Post the five game actions the rest of the game listens for, and run the progression fan-out.
//
// Body order, statement for statement off the X360:
//   1. Pick the element + type from the right latch: lbIsJump -> mpLastJumpElement (+0x608) with
//      the type FORCED to JUMP(0); otherwise mpLastStuntOrSmashElement (+0x5FC) with
//      meLastStuntElementType (+0x600). The jump arm asserts the latch is non-null (line 618).
//   2. The element KEY is `region->GetGroupId()`, or `region->GetId()` when the group id is zero
//      -- sign-extended to 64 bits (`extsw r11`). Every one of this body's SEVEN key reads
//      recomputes it the same way; recomputed once here.
//   3. POST ACTION 127 (E_ACTION_WORLD_STUNT_PERFORMED, 16 bytes) { key@+0, type@+8 }.
//   4. ScoringSystem::DealWithStunt(type, key, isOnline) -- asserting mpModeManager (636) and
//      its scoring system (637). `isOnline` is the CURRENT GameMode's mbIsOnline, or false when
//      no mode is running (`v10 = mpCurrentGameMode; v11 = v10 ? v10->mbIsOnline : 0`).
//   5. The training-tip arm, keyed on the type. [PARKED -- see the FLAG at the site.]
//   6. ModeManager::HandleWorldStunt(type, key)  [PARKED -- see the FLAG at the site.]
//   7. When this was NOT a jump: Profile::RecordPropHit(muLastZoneId, muLastPropId).
//   8. lbAlreadyDone = the key is already in the player's completed-set FOR THIS TYPE
//      (Set<s64,512>::Find over mpProgressionManager + 4104*type + 30568).
//   9. POST ACTION 61 (E_ACTION_STUNT_ELEMENT_BOOST, 4 bytes) { type } when
//      `!lbAlreadyDone || type == BILLBOARD` -- billboards re-award boost every time.
//  10. County-classify the element (FindTriggersCounty) and tell the DeveloperChallengeManager
//      (asserting mpGameStateModule (694) + its challenge manager (695)).
//  11. Everything below is inside `if (!lbAlreadyDone)` -- the FIRST-completion block:
//        Profile::AddStuntElement(type, key, county)   (asserts mpProgressionManager, 706)
//        AchievementManagerBase::OnCollectStunt(type)  (asserts the achievement manager, 710)
//        ProgressionManager::CheckForSpecialCarUnlocks() + SendGameCompletionResults(queue)
//        build the 24-byte OnStuntElementCompleteAction { key, type, current, total, gameMode },
//        CheckForTrophyUnlocks(&it), then POST ACTION 58.
//        if (per-county count >= per-county total) POST ACTION 59 { type, county } (8 bytes).
//        if (total count >= type total)            POST ACTION 60 { type } (4 bytes)
//                                                  + AchievementManagerBase::OnCollectAllStunts.
// ----------------------------------------------------------------------------
void StuntManager::ProcessStuntElement(GameStateModuleIO::GameActionQueue* lpActionQueue,
                                       bool lbIsJump,
                                       bool lbIsAGameModeActive)
{
    // The console's callee never reads its fourth argument (the X360 prologue keeps only
    // r3/r4/r5), even though both call sites pass it -- the compiler optimised its single use
    // away. Reproduced literally: accepted, unread.
    (void)lbIsAGameModeActive;

    GameActionQueueImpl* lpActionQueueImpl = reinterpret_cast<GameActionQueueImpl*>(lpActionQueue);

    // ---- 1) pick the latch ---------------------------------------------------------------
    const BrnTrigger::GenericRegion* lpElement;
    StuntElementType                 leElementType;

    if (lbIsJump)
    {
        if (!mpLastJumpElement)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("mpLastJumpElement != NULL", KAC_FILE, 618);
            CgsDev::Assert::EndAssert();
        }
        lpElement     = mpLastJumpElement;                  // X360 lwz r22, 0x608(r31)
        leElementType = E_STUNT_ELEMENT_TYPE_JUMP;          // X360 li r24, 0 -- forced, not read
    }
    else
    {
        lpElement     = mpLastStuntOrSmashElement;          // X360 lwz r22, 0x5FC(r31)
        leElementType = meLastStuntElementType;             // X360 lwz r24, 0x600(r31)
    }

    // ---- 2) the element key ---------------------------------------------------------------
    // X360 `lwz r11, 0x2C(r22)` (GenericRegion::mGroupId) / `lwz r11, 0x24(r22)` (::mId) with an
    // `extsw` sign extension to 64 bits. Read by name; the group-id-or-own-id fallback is the
    // console's, repeated verbatim at each of its seven read sites.
    const CgsID lElementGroupId = lpElement->GetGroupId();
    const CgsID lElementKey     = (lElementGroupId != 0) ? lElementGroupId : lpElement->GetId();

    // ---- 3) action 127: "a world stunt was performed" -------------------------------------
    // 16-byte record; the layout (key@+0x00, type@+0x08) is the asm-attested one -- see the
    // long correction note on WorldStuntAction in BrnGameActions.h.
    GameStateModuleIO::WorldStuntAction lWorldStuntAction;
    lWorldStuntAction.mId                = lElementKey;
    lWorldStuntAction.meStuntElementType = leElementType;
    lpActionQueueImpl->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lWorldStuntAction),
                                GameStateModuleIO::E_ACTION_WORLD_STUNT_PERFORMED, 16);

    // ---- 4) the scorer --------------------------------------------------------------------
    if (!mpModeManager)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mpModeManager", KAC_FILE, 636);
        CgsDev::Assert::EndAssert();
    }
    if (!mpModeManager->GetScoringSystem())
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mpModeManager->GetScoringSystem()", KAC_FILE, 637);
        CgsDev::Assert::EndAssert();
    }

    // X360: `v10 = *(modeManager + 3480)`  (mpCurrentGameMode)
    //       `v11 = v10 ? *(v10 + 172) : 0` (GameMode::mbIsOnline @+0xAC, `lbz r6, 0xAC(r11)`)
    const GameMode* lpCurrentGameMode = mpModeManager->GetCurrentGameMode();
    const bool      lbIsOnline        = (lpCurrentGameMode != 0) && lpCurrentGameMode->IsOnline();

    // ⚠️⚠️ [gateui] ROUND-3 PARK (verify_r2_fixgsm F3b / verify_r2_deps N4). The console call is
    //     mpModeManager->GetScoringSystem()->DealWithStunt(&lWorldStuntAction, lbIsOnline);
    // (X360 ScoringSystem::DealWithStunt @0x823384F0 -- the ABI splits the small WorldStuntAction
    // across r4/r5; the tree models it by pointer in the scorer's home, so the record built above
    // would be reused.)
    //
    // ⛔ CORRECTION TO THE BRIEF/N4 FIRST: this symbol is NOT bodiless. `ScoringSystem::DealWithStunt`
    // has a REAL, complete body at `ModeManager/Scoring/BrnScoringSystem_UpdateB.cpp ::
    // ScoringSystem::DealWithStunt`, and its callee `StuntModeScoring::DealWithStunt` @0x8232CEB0
    // is equally real at `Scoring/BrnStuntModeScoring_Register.cpp`. What is missing is a LINK:
    // neither TU is mounted, and mounting them is not a two-line change. MEASURED this round with
    // `cl /c` + `dumpbin /SYMBOLS` over the whole `GameState/ModeManager/Scoring/**` directory:
    //   * the minimal transitive set (UpdateB + StuntModeScoring _Register/_Combo/_Queries/
    //     _UpdatePass + ScoringSystem _Lookup/_Standings/_Queries) still leaves 16 game-side
    //     externals open and pulls in _Lifecycle, _Timer, _StuntTypes, CrashModeScoring,
    //     BurnoutSkillzData and BaseOnlineModeScoring behind them;
    //   * the FULL directory (all 22 .cpp) does not even compile -- 5 hard errors in
    //     BrnStuntModeScoringOnline.cpp (C2664/C2440, the BrnGameState::EActiveRaceCarIndex vs
    //     BrnWorld::EActiveRaceCarIndex fork at :401/:405/:523/:527/:537) -- and still leaves
    //     ~40 symbols with NO body anywhere in the tree (every OnlineRace/OnlineStuntRun/
    //     OnlineBurningHomeRun Construct/Prepare/Update/Release, StuntModeScoring::Prepare,
    //     CrashModeScoring's five getters, RoadRageModeScoring::SetTakeDownTarget -- that last one
    //     is a direct callee of THIS function's own TU).
    // So the honest choice is not "body it or fabricate it" but "park it or block the wave":
    // keeping the call means the whole gsm mount LNK2019s and not one `[UI-gate]` line prints.
    //
    // ⓘ COST OF THE PARK, stated plainly: completed stunt elements stop feeding the STUNT-RUN /
    // CRASH-MODE scorer. On the path this wave proves it costs nothing measurable -- the freeburn
    // drive test runs with `mbStuntModeActive == false`, and `StuntModeScoring::DealWithStunt`'s
    // first statement is `if (!mbStuntModeActive) return;`. It also writes NONE of the five fields
    // of the action-58 record built further down (those come from the progression set count, this
    // class's own census table and ModeManager::meCurrentGameModeType), so the HUD popup is
    // unaffected. Same treatment as ModeManager::HandleWorldStunt below.
    // RESTORE-WHEN the Scoring subsystem mounts (see the mount-cost measurement above); the call is
    // one line and the record it needs is already built.
    (void)lbIsOnline;
    (void)lWorldStuntAction;

    // ---- 5) the training-tip arm ----------------------------------------------------------
    // The X360 emits an inlined TrainingManager gauntlet THREE times here, once per stunt type
    // (JUMP -> training type 12, SMASH -> 14, BILLBOARD -> 13; MEASURED off the three
    // `IsTipAllowedInGameMode(..., 0xC/0xE/0xD)` pairs -- NOT 12/13/14 in enum order):
    //     if (!meTrainingState && !mbInPictureParadise && IsTipAllowedInGameMode(type))
    //     { Profile* p = GetProfile();          // asserted non-null, BrnTrainingManager.cpp:382
    //       if (!p->HasPlayerSeenTrainingType(type)
    //           && (p->GetTime() - mfLastMessageFinishedTime) >= 5.0f)
    //       { meTrainingState = 1; meCurrentTrainingType = type; } }
    //
    // ⚠️⚠️ [gateui] PARKED, NOT FABRICATED -- the same treatment as HandleWorldStunt below, and
    // for the same measured reason. De-inlining it names SIX symbols that have NO BODY ANYWHERE
    // in this tree and no link stub standing in:
    //     TrainingManager::IsTipPending            (BrnTrainingManager.h:108)
    //     TrainingManager::IsTipAllowedInGameMode  (:123)
    //     TrainingManager::GetProfile              (:112)
    //     TrainingManager::GetTimeSinceLastTip     (:115)
    //     TrainingManager::RequestTip              (:118)
    //     Profile::HasPlayerSeenTrainingType       (BrnProfile.h:468)
    // -- and TrainingManager::GetProfile() would additionally null-deref, because nothing in the
    // tree ever calls TrainingManager::Construct (the only writer of its mpProgressionManager
    // back-pointer; the console's GameStateModule::Construct @0x82380388 does not call it either).
    // The tip is a TUTORIAL POPUP: it has no bearing on the collectible bookkeeping, on any of the
    // five actions this function posts, or on the HUD rung this wave proves. Landing it would cost
    // the entire mount for that. Lands the moment those six bodies do.
    //
    // The type-range assert IS kept -- it is StuntManager's own (BrnStuntManager.cpp:665), it
    // names nothing foreign, and it is the console's guard against a corrupt latch.
    if (static_cast<u32>(leElementType) >= 3u)
    {
        // X360: `if (type == 3) assert(...)` then jump PAST the whole tip arm. Values above 3
        // fall through the same way without asserting.
        if (leElementType == E_STUNT_ELEMENT_TYPE_COUNT)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("leElementType != E_STUNT_ELEMENT_TYPE_COUNT", KAC_FILE, 665);
            CgsDev::Assert::EndAssert();
        }
    }

    // ---- 6) the mode manager's own hook ---------------------------------------------------
    if (!mpModeManager)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mpModeManager", KAC_FILE, 671);
        CgsDev::Assert::EndAssert();
    }
    // ⚠️ [gateui] PARKED, NOT FABRICATED. The console call here is
    //     BrnGameState::ModeManager::HandleWorldStunt(mpModeManager, leElementType, lElementKey)
    //         @0x82337B68, which is a pure forwarder:
    //         `return ChallengeManager::HandleWorldStunt(modeManager + 28160, key);`
    // NEITHER end has a body in this tree: ModeManager::HandleWorldStunt is not declared at all,
    // and ChallengeManager::HandleWorldStunt is declaration-only (BrnChallengeManager.h:310, marked
    // "not in this TU's X360 ledger"). Calling it would add a link symbol with NO home anywhere,
    // which would block the whole mount for a freeburn-challenge side effect that has no bearing on
    // the HUD path this wave proves. Landed the moment either body does; recorded as a park.
    (void)lElementKey;

    // ---- 7) the per-prop census (smash / billboard only) ----------------------------------
    if (!lbIsJump)
    {
        // X360 `Profile::RecordPropHit(mpProgressionManager + 368, muLastZoneId, muLastPropId)` --
        // the +368 is the ProgressionManager's embedded Profile, reached by name here.
        mpProgressionManager->GetProfile()->RecordPropHit(static_cast<s32>(muLastZoneId),
                                                          static_cast<s32>(muLastPropId));
    }

    // ---- 8) has the player already collected this element? --------------------------------
    // X360: `Set<__int64,512>::Find(mpProgressionManager + 4104*type + 30568, &key)`, whose
    // "not found" answer is -1; the `cntlzw(-1 - result) & 0x20` dance is the compiler's sign
    // test, so `lbAlreadyDone` means FOUND and the first-completion block below is its negation.
    // ⛔ [gateui] THE QUERY IS PER TYPE (`4104 * HIDWORD(v8)`), and round 1 dropped that
    // discriminant onto a type-less facade with no body. Re-pointed at the DWARF shape
    // (BrnProgressionManager.h:447 `IsStuntElementDone(StuntElementType, CgsID) const`), whose
    // byte-exact sibling Profile::IsStuntElementDone(type, id) is already bodied in BrnProfile.cpp.
    const bool lbAlreadyDone = mpProgressionManager->IsStuntElementDone(leElementType, lElementKey);

    // ---- 9) action 61: the boost award ----------------------------------------------------
    // Billboards re-award boost on EVERY smash; jumps and super smashes only the first time.
    const bool lbPostedBoostAction =
        (!lbAlreadyDone || leElementType == E_STUNT_ELEMENT_TYPE_BILLBOARD);
    if (lbPostedBoostAction)
    {
        GameStateModuleIO::StuntElementBoostAction lBoostAction;
        lBoostAction.meStuntElementType = leElementType;
        lpActionQueueImpl->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lBoostAction),
                                    GameStateModuleIO::E_ACTION_STUNT_ELEMENT_BOOST, 4);
    }

    // ---- 10) county classify + the developer-challenge census ------------------------------
    const BrnWorld::ECounty leCounty = FindTriggersCounty(lpElement);

    if (!mpGameStateModule)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mpGameStateModule", KAC_FILE, 694);
        CgsDev::Assert::EndAssert();
    }
    if (!mpGameStateModule->GetDeveloperChallengeManager())
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mpGameStateModule->GetDeveloperChallengeManager()", KAC_FILE, 695);
        CgsDev::Assert::EndAssert();
    }
    // ⚠️ [gateui] ROUND-3 PARK (verify_r2_fixgsm F4 sweep). The console runs, UNCONDITIONALLY here
    // (outside the !lbAlreadyDone gate below):
    //     mpGameStateModule->GetDeveloperChallengeManager()->OnCollectStunt(type, lElementKey);
    // `DeveloperChallengeManager::OnCollectStunt` @0x82373028 HAS a real body
    // (`DeveloperChallengeManager/BrnDeveloperChallengeManager.cpp :: OnCollectStunt`) -- the park
    // is a LINK park, not a missing-body park. MEASURED (`cl /c` + `dumpbin /SYMBOLS`): that TU is
    // unmounted and mounting it opens SEVEN externals with no body anywhere in b5-decomp/src --
    //     StuntModeScoring::GetBestStuntScore, ScoringSystem::GetCarCount,
    //     CarData::GetFinishScore, OutputBuffer::GetGuiOutputQueue,
    //     GameStateModule::IsActiveRaceCarStillPresent, Profile::IsDeveloperChallengeComplete
    // -- plus `ScoringSystem::GetCarData`, which drags the whole unmountable Scoring subsystem
    // measured at the DealWithStunt park in step 4 above.
    // ⓘ COST: the developer-challenge billboard census (the Array<CollectedBillboard,5> +
    // UpdateBufferedCollectedBillboards @0x82372F80 arm) stops counting. The wave scout classes
    // this consumer as OPTIONAL (scout.md §C, "OPTIONAL counting -- fire and forget, no effect on
    // the HUD"), and it writes none of the action-58 record's five fields.
    // The two asserts above are KEPT -- they are StuntManager's own (lines 694/695) and name
    // nothing foreign. RESTORE-WHEN those seven bodies land.
    (void)lElementKey;

    // ---- 11) the first-completion block ----------------------------------------------------
    if (lbAlreadyDone)
    {
        static s32 siRepeatDiagCount = 0;
        if (UIGateDiagFirstN(&siRepeatDiagCount))
        {
            *CgsDev::Log::gpDebugPrint
                << "[UI-gate] stunt-element type=" << static_cast<s32>(leElementType)
                << " action=127" << (lbPostedBoostAction ? "+61" : "")
                << " (repeat -- already collected)\n";
        }
        return;
    }

    if (!mpProgressionManager)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mpProgressionManager", KAC_FILE, 706);
        CgsDev::Assert::EndAssert();
    }
    mpProgressionManager->GetProfile()->AddStuntElement(leElementType, lElementKey, leCounty);

    if (!mpProgressionManager->GetAchievementManager())
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mpProgressionManager->GetAchievementManager()", KAC_FILE, 710);
        CgsDev::Assert::EndAssert();
    }
    // ⚠️ [gateui] ROUND-3 PARK (verify_r2_fixgsm F4 sweep). The console call here is
    //     mpProgressionManager->GetAchievementManager()->OnCollectStunt(leElementType);
    // (X360 AchievementManagerBase::OnCollectStunt @0x82366EB8). Again a LINK park: the body is
    // real, at `AchievementManager/BrnGameStateAchievementManagerBase.cpp :: OnCollectStunt`.
    // That TU is deliberately NOT mounted -- `tools/build/build_game_exe.bat` carries the standing
    // conductor decision and the reason at its achievement-manager leg ("mounting it costs EIGHT
    // unresolved externals that have no definition anywhere in the tree"), with the warning that
    // /OPT:REF does NOT save an unreferenced COMDAT that calls an undefined symbol.
    // ⓘ RE-MEASURED THIS ROUND: it is now SEVEN, not eight -- owner `deps` bodied
    // `ProgressionManager::GetCollectedStuntElementCount` this wave. The remaining seven are
    //     ScoringSystem::GetPlayerScore / GetPlayerModeCrashes / GetPlayerModeTakedowns /
    //     GetNewlyWreckedCarCount, ProgressionManager::GetCarChallengeWinCount /
    //     GetProfileTotalTakedowns
    // (`ScoringSystem::GetNumberOfTakedownsAgainst` is bodied but in the unmountable
    //  Scoring/BrnScoringSystem_Queries.cpp -- see the DealWithStunt park in step 4).
    // ⓘ COST: stunt-collectible ACHIEVEMENTS stop incrementing. Off the HUD path -- the scout
    // classes it OPTIONAL, and it writes none of the action-58 record's five fields.
    // The assert above is KEPT (StuntManager's own, line 710). RESTORE-WHEN the seven land.
    (void)leElementType;

    // ⚠️ [gateui] PARKED CALLS, NOT FABRICATED -- the HandleWorldStunt treatment (step 6 above).
    // The console runs, next, in this order:
    //     mpProgressionManager->CheckForSpecialCarUnlocks();                    // 0x82396058
    //     mpProgressionManager->SendGameCompletionResults(lpActionQueueImpl);   // 0x82395C28
    // Owner `deps` PARKED both this wave with the measured blocker list now carried in
    // BrnProgressionManager.h's declarations: CheckForSpecialCarUnlocks needs
    // ComputeCompletionPercentage @0x8238A198 (320 insns) + UnlockSpecialCars @0x8237AF38
    // (106 insns); SendGameCompletionResults needs the same percentage plus an mpModeManager
    // back-pointer member (X360 +133436) that nothing in the tree models or installs. Neither
    // has a body anywhere in b5-decomp/src and no link stub stands in -> calling them is a hard
    // LNK2019 that blocks the whole gsm mount.
    // ⓘ Neither is on the HUD path: BOTH run BEFORE `lCompleteAction` is built below and neither
    // writes any of its five fields (X360 0x8239CDB0 posts `AddEvent(a2,&v51,58,24)` only after
    // these calls, and v51 is untouched by them). Parking them costs the popup nothing.
    // A fabricated SendGameCompletionResults would additionally post a WRONG "game complete"
    // (action 208) onto the very queue this function is writing. Land the calls when the bodies do.
    // (`lpActionQueueImpl` is NOT (void)'d here -- it is still the live sink for actions 58/59/60
    //  below; only the two calls are parked.)

    // ---- the action-58 record: THE HUD POPUP ----------------------------------------------
    // Both counts are read AFTER AddStuntElement above, so `current` already includes this
    // element -- that is what makes the popup read "12/45" and not "11/45".
    //   miCurrentCount  X360 `*(progressionManager + 4104*type + 30568 + 4096)` == the completed
    //                   Set's element count for this type == GetCollectedStuntElementCount(type)
    //                   (its own body carries the console's "Set used before Construct/Clear was
    //                    called" sentinel check, CgsSet.h:227).
    //   miTotalCount    X360 `*(this + 2*(type + 738))` == this + 1476 + 2*type ==
    //                   maiTotalStuntElementCounts[type] (Prepare's census).
    //   meCurrentGameMode  X360 `*(modeManager + 3476)` == ModeManager::meCurrentGameModeType --
    //                   the discriminant TranslateGameActionsToGuiEvents case 58 switches on to
    //                   choose GuiEventBoostBarStuntInfo(218) vs GuiEventStuntInfo(217).
    GameStateModuleIO::OnStuntElementCompleteAction lCompleteAction;
    std::memset(&lCompleteAction, 0, sizeof(lCompleteAction));
    lCompleteAction.mID                = lElementKey;
    lCompleteAction.meStuntElementType = leElementType;
    lCompleteAction.miCurrentCount     = mpProgressionManager->GetCollectedStuntElementCount(leElementType);
    lCompleteAction.miTotalCount       = maiTotalStuntElementCounts[leElementType];
    lCompleteAction.meCurrentGameMode  = mpModeManager->GetCurrentGameModeType();

    CheckForTrophyUnlocks(&lCompleteAction);

    // ⚠️ POSTED AT THE CONSOLE'S LITERAL 24 BYTES, not sizeof(). That is NOT a console-literal
    // transcription bug of the kind this tree keeps hitting: the five DWARF members occupy
    // +0x00..+0x17 == exactly 24 bytes on the x64 host too (the leading CgsID is 8 bytes and
    // 8-aligned on both), and everything past +0x18 is the X360-only convoy block that belongs to
    // a DIFFERENT action's consumer (StuntModeScoring::DealWithInProgressStunt). Posting sizeof()
    // here would copy 0x68 bytes of zeroed convoy padding the case-58 consumer never reads.
    lpActionQueueImpl->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lCompleteAction),
                                GameStateModuleIO::E_ACTION_ON_STUNT_ELEMENT_COMPLETE, 24);

    static s32 siCompleteDiagCount = 0;
    if (UIGateDiagFirstN(&siCompleteDiagCount))
    {
        *CgsDev::Log::gpDebugPrint
            << "[UI-gate] stunt-element type=" << static_cast<s32>(leElementType)
            << " action=58 count=" << lCompleteAction.miCurrentCount
            << "/" << lCompleteAction.miTotalCount
            << " county=" << static_cast<s32>(leCounty)
            << " mode=" << static_cast<s32>(lCompleteAction.meCurrentGameMode) << "\n";
    }

    // ---- action 59: every element of this type in this county is collected -----------------
    // X360 total: `*(this + 2*(5*type + county + 741))` == this + 1482 + 2*(5*type + county) ==
    // maaiTotalStuntElementCountsPerCounty[type][county].
    //
    // ⛔ [gateui] OFF-MAP GUARD (not in the X360). FindTriggersCounty answers
    // BrnWorld::E_COUNTY_INVALID (== E_COUNTY_VALID_COUNT == 5) for a region whose world position
    // samples off the district map -- and with the map's world rect only recovered this wave, an
    // off-map answer is a live possibility, not a theoretical one. The console then indexes
    // maaiTotalStuntElementCountsPerCounty[type][5] (an s16[3][5]) OUT OF BOUNDS and calls
    // Profile::GetStuntElementCountByCounty(type, 5), which asserts
    // `leCounty < E_COUNTY_VALID_COUNT` (BrnProfile.cpp :: GetStuntElementCountByCounty) and then
    // reads OOB itself. Guarded the way THIS FILE'S OWN SIBLINGS already guard it: Prepare's
    // census skips an invalid county (`if (leCounty != E_COUNTY_INVALID)`) and
    // Profile::AddStuntElement guards too -- the asymmetry was only in this body.
    if (leCounty != BrnWorld::E_COUNTY_INVALID)
    {
        if (mpProgressionManager->GetProfile()->GetStuntElementCountByCounty(leElementType, leCounty)
            >= maaiTotalStuntElementCountsPerCounty[leElementType][leCounty])
        {
            GameStateModuleIO::OnStuntElementCompleteForCountyAction lCountyAction;
            lCountyAction.meStuntElementType = leElementType;
            lCountyAction.meCounty           = leCounty;
            lpActionQueueImpl->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lCountyAction),
                GameStateModuleIO::E_ACTION_ON_STUNT_ELEMENT_COMPLETE_FOR_COUNTY, 8);

            static s32 siCountyDiagCount = 0;
            if (UIGateDiagFirstN(&siCountyDiagCount))
            {
                *CgsDev::Log::gpDebugPrint
                    << "[UI-gate] stunt-element type=" << static_cast<s32>(leElementType)
                    << " action=59 county=" << static_cast<s32>(leCounty) << "\n";
            }
        }
    }

    // ---- action 60: every element of this type in the whole city is collected --------------
    if (mpProgressionManager->GetCollectedStuntElementCount(leElementType)
        >= maiTotalStuntElementCounts[leElementType])
    {
        GameStateModuleIO::OnStuntElementCompleteByTypeAction lAllAction;
        lAllAction.meStuntElementType = leElementType;
        lpActionQueueImpl->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lAllAction),
            GameStateModuleIO::E_ACTION_ON_STUNT_ELEMENT_COMPLETE_BY_TYPE, 4);

        // ⚠️ [gateui] ROUND-3 PARK -- the console's
        //     mpProgressionManager->GetAchievementManager()->OnCollectAllStunts(leElementType);
        // (X360 AchievementManagerBase::OnCollectAllStunts @0x8235AF38). SAME symbol, SAME reason,
        // SAME restore condition as the OnCollectStunt park in the first-completion block above:
        // the body is real in BrnGameStateAchievementManagerBase.cpp, but that TU is deliberately
        // unmounted and mounting it opens SEVEN externals with no body anywhere in the tree.
        // The action-60 post ABOVE is the HUD-visible half (-> GuiEventStuntAllComplete(220)) and
        // it is LIVE; only the achievement side effect is parked.

        static s32 siAllDiagCount = 0;
        if (UIGateDiagFirstN(&siAllDiagCount))
        {
            *CgsDev::Log::gpDebugPrint
                << "[UI-gate] stunt-element type=" << static_cast<s32>(leElementType)
                << " action=60 (all collected)\n";
        }
    }
}

}
