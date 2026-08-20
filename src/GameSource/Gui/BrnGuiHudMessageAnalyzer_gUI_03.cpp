#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"   // the class home (brings BrnGuiEventTypeDefs)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"         // GuiHudMessage / GuiChallengeEndEvent / GuiEventRoadRuleNewHighScore
#include "GameSource/Gui/BrnGuiCache.h"                 // GuiCache (friend access + GetWorldDataController/GetHudMessageDirector/IsActiveRaceCarDisconnected)
#include "GameSource/Gui/BrnGuiWorldDataController.h"   // [gateui r3] WorldDataController::GetFreeburnChallengeList (the console two-hop)
#include "GameSource/Gui/BrnGuiHudMessageDirector.h"    // HudMessageDirector::IsMessageAllowed
#include "GameSource/GameState/BrnGameStateSharedIO.h"  // GameStateModuleIO::EGameModeType (meGameModeType compare)
#include "GameSource/BurnoutConstants.h"                // BrnGameState::EChallengeStatus
#include "SharedClasses/StreetData/BrnChallengeData.h"  // BrnStreetData::ScoreType
#include "SharedClasses/DataLists/ChallengeList.h"      // BrnResource::ChallengeList::GetChallengeData
#include "SharedClasses/DataLists/ChallengeListEntry.h" // BrnResource::ChallengeListEntry::GetTitleStringID

#include "GameShared/GameClasses/Core/CgsAssert.h"      // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"          // CgsID / CgsIDCompress
#include "GameShared/GameClasses/Core/CgsStringUtils.h" // CgsCore::SPrintf
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // [gateui r3] gpDebugPrint / gxMessageFilterFlags (the drive-through + finisher dumps)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiHudMessage.h"  // CgsGui::E_HUDMESSAGEPARAMTYPES_*
#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostType.h" // [gateui r3] BrnWorld::E_BOOST_TYPE_DANGER

// ===========================================================================================
// BrnGui::HudMessageAnalyzer -- reconstructed from BURNOUT_X360_ARTIST.XEX
// (gateui wave, partfile 03. Round 2 landed the two Trigger* bodies; round 3 landed the four
// handlers round 2 had to park, and corrected one round-2 defect.)
//
//   TriggerNewRoadRulesHighScoreMessage  @0x8251ED08  (198 insns)
//   TriggerChallengeEndedMessage         @0x82520078  (162 insns)
//   HandleDriveThrough                   @0x8251D570  (rd 3)
//   HandleEventFinisher                  @0x824F2FB0  (rd 3)
//   HandleNetworkBattling                @0x8251CDC0  (rd 3)
//   HandleChainedBoost                   @0x8251EA50  (rd 3)
//   + the class static KA_CHALLENGE_END_MESSAGE_IDS (X360 .data qword_82FB58B0, recovered
//     from its dynamic initialiser @0x82C56578 -- it is ZERO in the image, never dump it)
//
// [gateui r3] ROUND-2 DEFECT FIXED: TriggerNewRoadRulesHighScoreMessage's "you now rule N
// roads" tail, copy 2, carried an extra `&& miNumRoadsNowRuled > 0` conjunct that the console
// does not have -- a dropped console branch (verify_r2_guihand1/VERDICT.md WRONG-1). Removed,
// and both copies' FLAGs rewritten to state the asymmetry the way the asm has it.
//
// [gateui r3] The four round-3 handlers were parked in round 2 on payload/accessor
// DECLARATIONS that round's ownership rules forbade. All four dependencies landed this round
// and every one of them is now a real named type/member -- no maData[] indexing, no
// raw-offset hack:
//   GuiDriveThroughEvent (366) / GuiInEventFinisher (423) / GuiNetworkPlayerBattlingEvent
//   (483) / GuiEventBoostInfo (206)   -> real field sets in BrnGuiEventTypeDefs.h
//   GuiCache::IsActiveRaceCarDisconnected @0x82443C50 + maRaceCarDisconnected[8] @+0xA0F4
//                                        -> BrnGuiCache.h
//   CgsNumeric::Random::RandomUInt()     -> bodied in CgsRandom.h next to RandomBool
//   KAPC_DRIVE_THROUGH_* (3 tables)      -> BrnGuiHudMessageAnalyzer_wB_res.cpp
// Verdicts + the round-2 evidence this file was corrected against:
// scratch/gateui_wave/verify_r2_guihand1/VERDICT.md.
// ===========================================================================================

namespace BrnGui
{

// -------------------------------------------------------------------------------------------
// X360 .data qword_82FB58B0 -- the challenge-end HUD message ids, indexed by
// BrnGameState::EChallengeStatus (TriggerChallengeEndedMessage does `slwi r9,r9,3; ldx`,
// unchecked). ZERO in the image; filled by the TU's dynamic initialiser sub_82C56578,
// which the asm gives verbatim (0x82C5658C..0x82C565E0):
//     CgsIDCompress("FBChalComp")  -> qword_82FB58B8   (== [1])
//     CgsIDCompress("FBChalAbort") -> qword_82FB58C0   (== [2])
//     CgsIDCompress("FBChalAbort") -> qword_82FB58C8   (== [3], a SECOND compress of the
//                                                       same string, not an alias store)
//     0                            -> qword_82FB58D0   (== [4])
//     0                            -> qword_82FB58D8   (== [5])
//     CgsIDCompress("FBChalFail")  -> qword_82FB58E0   (== [6])
// [0] is never written by the initialiser -- it stays the zero the image has.
//
// The zero slots are load-bearing behaviour, not gaps: a zero id makes the trigger clear
// mbChallengeEnded and fire nothing (ONGOING / ABORTED_BEFORE_STARTING / RESET_IF_NEEDED).
// Size == E_CHALLENGE_STATUS_COUNT (7): the header declares the array with an incomplete
// bound, so this definition completes it.
//
// (Same recovery class as KA_STUNT_INFO_MESSAGES in _wB_res.cpp: walk the dyn-init thunk,
// never read the zeroed .data.)
const CgsID HudMessageAnalyzer::KA_CHALLENGE_END_MESSAGE_IDS[BrnGameState::E_CHALLENGE_STATUS_COUNT] =
{
    0,                              // [0] E_CHALLENGE_STATUS_ONGOING                     (no message)
    CgsIDCompress("FBChalComp"),    // [1] E_CHALLENGE_STATUS_SUCCESS
    CgsIDCompress("FBChalAbort"),   // [2] E_CHALLENGE_STATUS_ABORTED
    CgsIDCompress("FBChalAbort"),   // [3] E_CHALLENGE_STATUS_ABORTED_DUE_TO_PLAYER_LEAVE
    0,                              // [4] E_CHALLENGE_STATUS_ABORTED_BEFORE_STARTING     (no message)
    0,                              // [5] E_CHALLENGE_STATUS_RESET_IF_NEEDED             (no message)
    CgsIDCompress("FBChalFail"),    // [6] E_CHALLENGE_STATUS_FAILURE
};

// @ 0x8251ED08
// The deferred road-rules high-score announce: Update parks the 48-byte event in
// mRoadRuleHighScoreData + starts mfRoadRulesMessageTimeout (HandleNewRoadRulesHighScore
// @0x824F32A0), and this fires when the timer expires.
//
// Two completely separate shapes share the body, picked by mbMultipleScores:
//   * multi-score summary  -- "you lost N roads" (online/lobby only) + "you now rule N roads"
//   * single-score line    -- "<time|crash> record on <road>", parameterised by the road id
//     PRINTED AS DECIMAL (`CgsCore::SPrintf(buf, 12, "%llu", mRoadId)`) and handed to the
//     message as a STRINGID -- the localisation key for a road IS its compressed id.
//
// [gateui r3] RESOLVED -- the shared-header layout request this body was written against has
// landed. GuiEventRoadRuleNewHighScore in BrnGuiEventTypeDefs.h now carries the X360 member
// order (mRoadId@0x00, meScoreType@0x08, miNumScoresLost@0x0C, miNumRoadsNowRuled@0x10,
// mPlayerName@0x14, the five bools @0x24..0x28), pinned by offsetof, on the two witnesses this
// FLAG cited: this function's own reads (`ld r6, 0x480(r31)` feeding SPrintf("%llu") from
// record+0x00; `addi r30, r31, 0x494` passing record+0x14 as a STRING param, with
// mRoadRuleHighScoreData at analyzer+0x480) and the producer TranslateGameActionsToGuiEvents
// case 281 @0x823EC7C4. sizeof was 48 in BOTH orders, which is why the existing static_assert
// never caught it. The body below reads BY NAME and needed no change.
// (This was faithfulness debt, not a live corruption: BrnGuiHudMessageAnalyzer_wB_07.cpp fills
// the latch with a typed struct copy and nothing in the tree reads the record at raw X360
// offsets -- re-verified this round.)
void HudMessageAnalyzer::TriggerNewRoadRulesHighScoreMessage()
{
    if (mRoadRuleHighScoreData.mbMultipleScores)
    {
        // ---- multi-score summary -------------------------------------------------------
        if (mRoadRuleHighScoreData.miNumScoresLost > 0)
        {
            // The "roads you lost" half shows ONLY when no game mode is running or the
            // player is sitting in the online freeburn lobby: the console tests
            // meGameModeType (cache +0x9E58) against exactly those two values and skips
            // the whole message otherwise (0x8251ED48 cmpwi -1 / 0x8251ED50 cmpwi 0xF).
            const bool lbAnnounceLostRoads =
                (mpGuiCache->meGameModeType == BrnGameState::GameStateModuleIO::E_MODE_NONE) ||
                (mpGuiCache->meGameModeType == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY);

            if (lbAnnounceLostRoads)
            {
                GuiHudMessage lMessage;
                if (mRoadRuleHighScoreData.miNumScoresLost == 1)
                {
                    lMessage.Construct("RRLost1Road");
                }
                else
                {
                    lMessage.Construct("RRLostXRoads");
                    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 0,
                                      mRoadRuleHighScoreData.miNumScoresLost);
                }
                TriggerMessage(&lMessage);
            }
        }
        else
        {
            GuiHudMessage lMessage;
            lMessage.Construct("RRNewScores");
            TriggerMessage(&lMessage);
        }

        // ---- the shared "you now rule N roads" tail (copy 1 of 2) --------------------
        // The X360 emitted the MESSAGE-BUILDING block twice -- an inlined private helper --
        // but the two call sites GATE it differently (see the corrected FLAG on copy 2).
        // Here the gate is the count (`0x8251EDC4 lwz 0x490 ; cmpwi 0 ; ble -> exit`), with
        // no mbWasRulePlayersBefore test; this leg falls straight into it.
        // AGENTS.md would have the shared block de-inlined into a private helper
        // (`TriggerRoadsRuledMessage()`, the declaration requested of this header's owner);
        // until that lands it is reproduced at both sites. When it does land, the GATES stay
        // at the call sites -- they are not part of the helper.
        if (mRoadRuleHighScoreData.miNumRoadsNowRuled > 0)
        {
            GuiHudMessage lRuledMessage;
            if (mRoadRuleHighScoreData.miNumRoadsNowRuled == 1)
            {
                lRuledMessage.Construct("RRYouRule1");
            }
            else
            {
                lRuledMessage.Construct("RRYouRuleX");
                lRuledMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 0,
                                       mRoadRuleHighScoreData.miNumRoadsNowRuled);
            }
            TriggerMessage(&lRuledMessage);
        }
        return;
    }

    // ---- single-score line -------------------------------------------------------------
    // The road's localisation key is its compressed id in decimal. The buffer is 16 bytes on
    // the stack but SPrintf is given 12 and the terminator is written at [12] by hand
    // (X360 `li r4, 0xC` @0x8251EDF4 then `stb r10, var_A04` @0x8251EE10 -- var_A10 + 12).
    char lacRoadStringId[16];
    CgsCore::SPrintf(lacRoadStringId, 12, "%llu", mRoadRuleHighScoreData.mRoadId);
    lacRoadStringId[12] = 0;

    if (mRoadRuleHighScoreData.mbIsLocalPlayer)
    {
        // "You set a new <time|crash> record" -- the ...Off variants are the online-loss /
        // offline-win case (the score only stands offline).
        const char* lpcMessageId = NULL;
        if (mRoadRuleHighScoreData.mbOnlineLossButOfflineWin)
        {
            if (mRoadRuleHighScoreData.meScoreType == BrnStreetData::E_SCORE_TYPE_TIME)
                lpcMessageId = "RRTimeYouOff";
            else if (mRoadRuleHighScoreData.meScoreType == BrnStreetData::E_SCORE_TYPE_CRASH)
                lpcMessageId = "RRCrshYouOff";
            else
                return;   // faithful: any other score type fires nothing at all
        }
        else
        {
            if (mRoadRuleHighScoreData.meScoreType == BrnStreetData::E_SCORE_TYPE_TIME)
                lpcMessageId = "RRTimeYou";
            else if (mRoadRuleHighScoreData.meScoreType == BrnStreetData::E_SCORE_TYPE_CRASH)
                lpcMessageId = "RRCrashYou";
            else
                return;
        }

        GuiHudMessage lMessage;
        lMessage.Construct(lpcMessageId);
        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 1, lacRoadStringId);
        TriggerMessage(&lMessage);

        if (mRoadRuleHighScoreData.mbIsWholeRoadOwned)
        {
            GuiHudMessage lRoadMessage;
            lRoadMessage.Construct("RRRoadYou");
            lRoadMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 1, lacRoadStringId);
            TriggerMessage(&lRoadMessage);
        }
        return;
    }

    // Somebody else took the record. The "Beat" variants are used when the road was ruled by
    // the local player before (i.e. the player just LOST it).
    const char* lpcMessageId = NULL;
    if (mRoadRuleHighScoreData.mbWasRulePlayersBefore)
    {
        if (mRoadRuleHighScoreData.meScoreType == BrnStreetData::E_SCORE_TYPE_TIME)
            lpcMessageId = "RRTimeBeat";
        else if (mRoadRuleHighScoreData.meScoreType == BrnStreetData::E_SCORE_TYPE_CRASH)
            lpcMessageId = "RRCrashBeat";
        else
            return;
    }
    else
    {
        if (mRoadRuleHighScoreData.meScoreType == BrnStreetData::E_SCORE_TYPE_TIME)
            lpcMessageId = "RRTimeX";
        else if (mRoadRuleHighScoreData.meScoreType == BrnStreetData::E_SCORE_TYPE_CRASH)
            lpcMessageId = "RRCrashX";
        else
            return;
    }

    {
        GuiHudMessage lMessage;
        lMessage.Construct(lpcMessageId);
        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1,
                          mRoadRuleHighScoreData.mPlayerName.macName);
        lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 2, lacRoadStringId);
        TriggerMessage(&lMessage);
    }

    if (mRoadRuleHighScoreData.mbIsWholeRoadOwned)
    {
        GuiHudMessage lRoadMessage;
        lRoadMessage.Construct("RRRoadX");
        lRoadMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 1,
                              mRoadRuleHighScoreData.mPlayerName.macName);
        lRoadMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 2, lacRoadStringId);
        TriggerMessage(&lRoadMessage);
    }

    // ---- the shared "you now rule N roads" tail (copy 2 of 2) --------------------------
    // FLAG (console asymmetry, reproduced -- CORRECTED [gateui r3]). The two copies of this
    // tail gate DIFFERENTLY, and the difference is the opposite way round from what the
    // round-2 banner claimed. From the asm, verbatim:
    //   copy 1 (multi-score leg): reached by FALLTHROUGH from 0x8251EDC0 -- no
    //     mbWasRulePlayersBefore test at all -- and gated on the count:
    //     `0x8251EDC4 lwz r11,0x490 ; 0x8251EDC8 cmpwi r11,0 ; 0x8251EDCC ble -> exit`.
    //   copy 2 (this one, single-score / not-local leg): gated on the FLAG and NOT on the
    //     count: `0x8251EFB0 lbz r11,0x4A6 ; 0x8251EFB8 beq -> exit`, then
    //     `0x8251EFBC lwz r11,0x490` is loaded only to pick the singular/plural line
    //     (`0x8251EFC4 cmpwi r11,1`) -- there is no `> 0` branch anywhere on this path.
    // So a count of 0 (or negative) reaching here fires "RRYouRuleX" with the parameter 0:
    // another player took the local player's LAST ruled road. Round 2 carried an extra
    // `&& miNumRoadsNowRuled > 0` conjunct here, which silently DROPPED that console
    // branch; it is removed. Do not re-add it, and if this tail is ever de-inlined into the
    // requested TriggerRoadsRuledMessage() helper, the helper must NOT carry the `> 0` test
    // -- it belongs to copy 1's call site only.
    if (mRoadRuleHighScoreData.mbWasRulePlayersBefore)
    {
        GuiHudMessage lRuledMessage;
        if (mRoadRuleHighScoreData.miNumRoadsNowRuled == 1)
        {
            lRuledMessage.Construct("RRYouRule1");
        }
        else
        {
            lRuledMessage.Construct("RRYouRuleX");
            lRuledMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 0,
                                   mRoadRuleHighScoreData.miNumRoadsNowRuled);
        }
        TriggerMessage(&lRuledMessage);
    }
}

// @ 0x82520078
// The deferred freeburn-challenge-end announce (Update parks the 24-byte event in
// mChallengedEndedData via HandleChallengeEnded @0x824F33E0 and raises mbChallengeEnded;
// this fires it and lowers the flag).
//
// The message id comes from KA_CHALLENGE_END_MESSAGE_IDS keyed on the status, and a ZERO
// entry is a real outcome, not an error: the flag is cleared and nothing is shown. The
// availability pre-check is genuine console behaviour -- the analyzer asks the director
// IsMessageAllowed BEFORE building the message and, if the answer is no, returns WITHOUT
// clearing mbChallengeEnded, so the announce retries on the next Update (0x82520178 branches
// past the `stb 0, 0x458`). Do not hoist the clear.
void HudMessageAnalyzer::TriggerChallengeEndedMessage()
{
    CGS_ASSERT(mbChallengeEnded == true, "mbChallengeEnded == true");   // cpp:5552

    // [gateui r3] THE CONSOLE TWO-HOP, RESTORED. The X360 inlines
    //     mpGuiCache->GetWorldDataController()->GetFreeburnChallengeList()
    // -- the BrnGuiCache.h:2324 "mpWorldDataController" assert (`lwz r11, 0x4064`) followed
    // by the WorldDataController+0x4A8 read. Round 2 could not write it because
    // BrnGuiWorldDataController.h typed that accessor's return as `const BrnGui::ChallengeList*`,
    // a phantom defined nowhere in the tree, and routed through GuiCache::
    // GetFreeburnChallengeList @0x8240F018 instead (the same two hops plus one extra
    // non-gating null assert). That header now names the real BrnResource::ChallengeList, so
    // the console's own shape is back -- and it links out of two MOUNTED TUs (BrnGuiCache.cpp,
    // BrnGuiWorldDataController.cpp) rather than the unmounted BrnGuiCache_wB_01.cpp.
    const BrnResource::ChallengeList* lpChallengeList =
        mpGuiCache->GetWorldDataController()->GetFreeburnChallengeList();
    CGS_ASSERT(lpChallengeList != NULL, "lpChallengeList");            // cpp:5558

    const CgsID lMessageId =
        KA_CHALLENGE_END_MESSAGE_IDS[mChallengedEndedData.meChallengeStatus];

    if (lMessageId == 0)
    {
        mbChallengeEnded = false;
        return;
    }

    if (!mpGuiCache->GetHudMessageDirector()->IsMessageAllowed(lMessageId))
        return;   // still pending -- mbChallengeEnded deliberately NOT cleared

    GuiHudMessage lMessage;
    lMessage.Construct(lMessageId);

    switch (mChallengedEndedData.meChallengeStatus)
    {
        case BrnGameState::E_CHALLENGE_STATUS_SUCCESS:
            // "Challenge complete <done>/<total>" -- BOTH counts on display string 1
            // (`li r5,1` at 0x825201D0 and 0x825201E4), the same two-params-one-string idiom
            // HandleStuntInfo uses.
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 1,
                              mChallengedEndedData.miNumChallengesComplete);
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 1,
                              mChallengedEndedData.miTotalNumChallenges);
            break;

        case BrnGameState::E_CHALLENGE_STATUS_ABORTED:
            // FLAG (console oddity, reproduced verbatim): the abort line takes an EMPTY
            // string id -- the X360 loads the shared empty rodata byte unk_820046A7 and
            // still calls AddParam (0x82520220). The reason slot is simply left blank here.
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 1, "");
            break;

        case BrnGameState::E_CHALLENGE_STATUS_ABORTED_DUE_TO_PLAYER_LEAVE:
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 1,
                              "CHALLENGE_ABORT_PLAYER_LEFT");
            break;

        case BrnGameState::E_CHALLENGE_STATUS_FAILURE:
            // The failure line names the challenge itself.
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRINGID, 1,
                              lpChallengeList->GetChallengeData(mChallengedEndedData.mChallengeID)
                                  ->GetTitleStringID());
            break;

        default:
            // Reachable only for a status whose table slot is non-zero, i.e. a value past the
            // 7-entry table (the three in-range zero slots are filtered above). The console
            // streams the offending status into the assert buffer and, crucially, returns
            // WITHOUT firing and WITHOUT clearing the flag.
            CGS_ASSERT(false, "Flag should not be set to trigger this in this state");  // cpp:5599
            return;
    }

    // Non-gating tripwires re-evaluated by the console on EVERY arm, not just the failure one
    // (0x82520238 / 0x82520268 call GetChallengeData again after the switch); the second is
    // the compiler's `GetTitleStringID() != NULL` rendered as `entry + 0xB0 != 0`.
    CGS_ASSERT(lpChallengeList->GetChallengeData(mChallengedEndedData.mChallengeID) != NULL,
               "lpChallengeList->GetChallengeData( mChallengedEndedData.mChallengeID )");   // cpp:5604
    CGS_ASSERT(lpChallengeList->GetChallengeData(mChallengedEndedData.mChallengeID)
                   ->GetTitleStringID() != NULL,
               "lpChallengeList->GetChallengeData( mChallengedEndedData.mChallengeID )->GetTitleStringID()");  // cpp:5605

    TriggerMessage(&lMessage);
    mbChallengeEnded = false;
}

// ===========================================================================================
// [gateui r3] The four handlers round 2 parked. Each is dispatched by the committed fan-out
// in BrnGuiHudMessageAnalyzer_wB_12.cpp :: Update, so each was an LNK2019 standing between
// this tree and a mountable HudMessageAnalyzer.
// ===========================================================================================

// @ 0x8251D570
// "You drove through a <drive-thru>" -- one line per GuiDriveThroughEvent::DriveThroughType,
// picked out of three tables:
//   * mbEffective == false                -> KPAC_DRIVE_THROUGH_INEFFECTIVE_MESSAGES
//   * effective, type not CAR_WASH/PAINT  -> KAPC_DRIVE_THROUGH_MESSAGES
//   * effective CAR_WASH or PAINT_SHOP    -> a 1-in-KU_FREQUENCY_OF_DRIVETHROUGH_MAGIC_MESSAGES
//                                            draw between the MAGIC table and the basic one.
// The console's branch shape is `if (type != 0 && type != 2) { … } else { … }` with the
// mbEffective test duplicated inside both arms (0x8251D66C..0x8251D6AC); it is re-expressed
// here as the three cases above, which is the same predicate lattice with the duplication
// removed. CAR_WASH(0) and PAINT_SHOP(2) are exactly the two types with a MAGIC entry.
//
// INLINING REVERSAL (the draw): the console inlines the whole thing --
//     0x8251D6D8  ld    r11, 0x30(r22)     ; mRandom.muSeed, the OLD value
//     0x8251D6DC  srdi  r8, r11, 32        ; the draw is the PRE-step high word
//     0x8251D6E0  mulld r11, r11, r10      ; * 0x5851F42D4C957F2D
//     0x8251D6E4  addi  r11, r11, 1
//     0x8251D6F0  std   r11, 0x30(r22)
//     0x8251D6EC  mulhwu r9, r10, 0xCCCCCCCD ; srwi 2 ; slwi/add/subf  -> an UNSIGNED % 5
// which is `CgsNumeric::Random::RandomInt(0, 4)` with both bounds constant-folded: RandomInt
// (bodied in the mounted GameShared/GameClasses/Numeric/CgsRandom.cpp) is
// `liMin + (u32)(RandomUInt() % (u32)(liMax - liMin + 1))` -- pre-step draw, unsigned
// reduction, no ring-buffer touch -- and at (0, 4) its two baked asserts are constant-true
// and its `divwu` strength-reduces to exactly the magic multiply above. The PS3 DWARF
// (BrnGuiHudMessageAnalyzer.cpp:3178) confirms the source shape: a local `int32_t
// liRandomNumber` filled by a RandomInt call. The local keeps the DWARF's name; the bound is
// written as the existing class constant so the 1-in-N reads where the N lives.
//
// FLAG (console defect, reproduced): three MAGIC slots and three INEFFECTIVE slots are NULL in
// the X360 image and the Construct is NOT guarded against a null id. Every NULL slot is
// unreachable through this function's own branch structure (see the table definitions in
// BrnGuiHudMessageAnalyzer_wB_res.cpp), so adding a guard would add behaviour the binary does
// not have. Left ungated deliberately.
void HudMessageAnalyzer::HandleDriveThrough(const GuiDriveThroughEvent* lpEvent)
{
    // Non-gating tripwires (cpp:3301 / cpp:3302; the first message text really does say
    // "impact" -- copied from the sibling impact handler in the original).
    CGS_ASSERT(lpEvent != NULL, "Invalid impact message");
    CGS_ASSERT(lpEvent->meDriveThroughType < GuiDriveThroughEvent::E_DRIVE_THROUGH_TYPE_COUNT,
               "Drive Through type invalid");

    // The two types that have a "magic" variant, i.e. the two the console excludes from the
    // straight table lookup (`cmpwi r11,0` / `cmpwi r11,2` @0x8251D670/0x8251D678).
    const bool lbTypeHasMagicVariant =
        (lpEvent->meDriveThroughType == GuiDriveThroughEvent::E_DRIVE_THROUGH_TYPE_CAR_WASH) ||
        (lpEvent->meDriveThroughType == GuiDriveThroughEvent::E_DRIVE_THROUGH_TYPE_PAINT_SHOP);

    const char* lpcMessageId = NULL;

    if (!lpEvent->mbEffective)
    {
        lpcMessageId = KPAC_DRIVE_THROUGH_INEFFECTIVE_MESSAGES[lpEvent->meDriveThroughType];
    }
    else if (!lbTypeHasMagicVariant)
    {
        lpcMessageId = KAPC_DRIVE_THROUGH_MESSAGES[lpEvent->meDriveThroughType];
    }
    else
    {
        const s32 liRandomNumber =
            mRandom.RandomInt(0, static_cast<s32>(KU_FREQUENCY_OF_DRIVETHROUGH_MAGIC_MESSAGES) - 1);

        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "Drive through number : " << liRandomNumber << "\n";

        if (liRandomNumber != 0)
        {
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "Basic Drive Through\n";
            lpcMessageId = KAPC_DRIVE_THROUGH_MESSAGES[lpEvent->meDriveThroughType];
        }
        else
        {
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint << "Special Drive Through\n";
            lpcMessageId = KAPC_DRIVE_THROUGH_MAGIC_MESSAGES[lpEvent->meDriveThroughType];
        }
    }

    GuiHudMessage lMessage;
    lMessage.Construct(lpcMessageId);
    TriggerMessage(&lMessage);
}

// @ 0x824F2FB0
// Online-event finisher: latch the WINNER's gamer tag so the HUD can announce it. Nothing is
// triggered from here -- the message itself is fired later off mbOnlineWinnerWaiting.
// Three gates, in the console's order:
//   * the whole body is inside `if (GuiCache +0x4B4C)` -- the "this is an online event"
//     discriminator the tree spells IsOnlineStartInProgress()
//   * only the winner counts (miFinishPosition == 1)
//   * a car that has dropped out is not announced (GuiCache::IsActiveRaceCarDisconnected)
// The name resolve seeds its out-flag TRUE and the flag gates ONLY the waiting latch -- the
// PlayerName is constructed from whatever GetOnlineName returned either way
// (`bl PlayerName::Construct` @0x824F3280 precedes the `lbz`/`beq` on the flag). Reproduced.
void HudMessageAnalyzer::HandleEventFinisher(const GuiInEventFinisher* lpEvent)
{
    CGS_ASSERT(lpEvent != NULL, "lpEvent");   // cpp:3892

    if (!mpGuiCache->IsOnlineStartInProgress())
        return;

    // The console's debug dump of the whole disconnected table, unrolled eight times in the
    // image (`ori r10, r10, 0xA0F4` .. `0xA0FB`, "," between and "\n\n" after the last),
    // re-rolled here. Read through the accessor rather than the raw byte the console loads.
    if (CgsDev::Message::gxMessageFilterFlags & 1)
    {
        *CgsDev::Log::gpDebugPrint << "Handling Event Finisher in hud message analyzer ::\n";
        *CgsDev::Log::gpDebugPrint << "Disconnected :";

        for (s32 liIndex = 0; liIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liIndex)
        {
            const EActiveRaceCarIndex leIndex = static_cast<EActiveRaceCarIndex>(liIndex);
            *CgsDev::Log::gpDebugPrint
                << " "
                << static_cast<s32>(mpGuiCache->IsActiveRaceCarDisconnected(leIndex) ? 1 : 0)
                << ((liIndex == (E_ACTIVE_RACE_CAR_INDEX_COUNT - 1)) ? "\n\n" : ",");
        }
    }

    if (lpEvent->miFinishPosition != 1)
        return;

    if (mpGuiCache->IsActiveRaceCarDisconnected(lpEvent->meActiveRaceCarIndex))
        return;

    bool lbNameValid = true;
    const char* lpcOnlineName = GetOnlineName(lpEvent->meActiveRaceCarIndex, &lbNameValid);
    mOnlineWinnerName.Construct(lpcOnlineName);

    if (lbNameValid)
        mbOnlineWinnerWaiting = true;
}

// @ 0x8251CDC0
// Online "X is battling Y" line: one message, two gamer tags, both on display string 0. The
// message is ABANDONED the moment a name fails to resolve -- the console re-reads the shared
// out-flag after each GetOnlineName and branches straight to the epilogue (0x8251D024 and
// 0x8251D054), so a half-filled message is never triggered. The first AddParam still runs
// with whatever the failed resolve returned; only the second call and the trigger are skipped.
void HudMessageAnalyzer::HandleNetworkBattling(const GuiNetworkPlayerBattlingEvent* lpEvent) const
{
    // Five non-gating tripwires, cpp:3045..3049. The two range pairs share a message text
    // each, exactly as the console does.
    CGS_ASSERT(lpEvent != NULL, "Invalid network battling message");
    CGS_ASSERT(0 <= lpEvent->meAggressorActiveRaceCarIndex, "Battling aggressor index invalid");
    CGS_ASSERT(lpEvent->meAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "Battling aggressor index invalid");
    CGS_ASSERT(0 <= lpEvent->meVictimActiveRaceCarIndex, "Battling victim invalid");
    CGS_ASSERT(lpEvent->meVictimActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "Battling victim invalid");

    bool lbNameValid = true;

    GuiHudMessage lMessage;
    lMessage.Construct("AggDrFghting");

    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0,
                      GetOnlineName(lpEvent->meAggressorActiveRaceCarIndex, &lbNameValid));
    if (!lbNameValid)
        return;

    lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0,
                      GetOnlineName(lpEvent->meVictimActiveRaceCarIndex, &lbNameValid));
    if (!lbNameValid)
        return;

    TriggerMessage(&lMessage);
}

// @ 0x8251EA50
// Two unrelated jobs share this handler, split on mbWasChainJustCompleted (+0x17):
//
//   * the chain just ENDED -> announce the burnout chain, keyed on muNumChained (+0x00):
//       1        -> "BBBurnout1"  + an EMPTY string parameter (the console loads the shared
//                   empty rodata byte unk_820046A7 and still calls AddParam @0x8251EAD0)
//       2..99    -> "BBBurnouts"  + the count as an INT parameter
//       >= 100   -> "BBBurnoutWow", no parameters
//     (`cmplwi` at both compares -- muNumChained is unsigned.)
//
//   * otherwise -> the "your boost bar is full, you can boost now" one-shot tutorial line,
//     latched on mbBoostOkMessageJustTriggered (analyzer +0x456) so it fires once per fill:
//       meBoostType must be the danger/blue bar (`cmpwi r11,0` @0x8251EB3C -- any other bar
//       returns immediately); then, while the bar reads full and the player is NOT boosting
//       and has no chain running, fire "BBBoostOK" and raise the latch. The latch is lowered
//       again only on the not-full + boosting path (0x8251EBA0), i.e. when the player spends
//       the bar. Note the console RETURNS out of the whole mbBoostIsFull arm whether or not
//       the inner test passed (`cmplwi r11,0; bne` @0x8251EB98 re-tests the same byte it
//       already branched on), so a full bar can never fall through to the lowering path.
void HudMessageAnalyzer::HandleChainedBoost(const GuiEventBoostInfo* lpEvent)
{
    CGS_ASSERT(lpEvent != NULL, "lpEvent");   // cpp:4519

    if (lpEvent->mbWasChainJustCompleted)
    {
        GuiHudMessage lMessage;
        if (lpEvent->muNumChained == 1u)
        {
            lMessage.Construct("BBBurnout1");
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_STRING, 0, "");
        }
        else if (lpEvent->muNumChained >= 100u)
        {
            lMessage.Construct("BBBurnoutWow");
        }
        else
        {
            lMessage.Construct("BBBurnouts");
            lMessage.AddParam(CgsGui::E_HUDMESSAGEPARAMTYPES_INT, 0,
                              static_cast<s32>(lpEvent->muNumChained));
        }
        TriggerMessage(&lMessage);
        return;
    }

    if (lpEvent->meBoostType != BrnWorld::E_BOOST_TYPE_DANGER)
        return;

    if (lpEvent->mbBoostIsFull)
    {
        if (!lpEvent->mbIsBoosting &&
            (lpEvent->muNumChained == 0u) &&
            !mbBoostOkMessageJustTriggered)
        {
            GuiHudMessage lMessage;
            lMessage.Construct("BBBoostOK");
            TriggerMessage(&lMessage);
            mbBoostOkMessageJustTriggered = true;
        }
        return;
    }

    if (!lpEvent->mbIsBoosting)
        return;

    mbBoostOkMessageJustTriggered = false;
}

} // namespace BrnGui
