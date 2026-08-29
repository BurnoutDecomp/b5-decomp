// ============================================================================
// THE SHOWTIME SLICE of the GameState -> Gui bridge.
//
//   BrnGame::BrnGameModule::TranslateShowtimeActionToGuiEvent  @ X360 0x823E1988
//
// DWARF home GameSource/Game/GameBridgeGameStateToX.cpp (the console's asserts bake
// "d:\p4\b5_main\burnout\main\code\gamesource\unity\../Game/GameBridgeGameStateToX.cpp");
// split into its own TU here for the same reason the event-flow and event-status slices are --
// GameBridgeGameStateToX_StuntGuiEvents.cpp cannot include BrnGuiDemangledEventTypes.h (the
// pre-existing GuiEventNetworkPlayerImage C2011 fork documented on that file), and FOUR of the
// eight arms below post records that live in exactly that header.
//
// ============================================================================
// ⭐⭐⭐ WHY THIS FUNCTION IS THE SHOWTIME SCORE
// ============================================================================
// Showtime's entire score presentation reaches the GUI through here and nowhere else. Every
// score-bearing showtime GUI event in the image -- GuiShowtimeScoreUpdate (396),
// GuiHitVehicleEvent (394), GuiHUDMessageShowtimeMultiplier (399), GuiHUDMessageCrushCombo
// (401), GuiHUDMessageSignSmashed (400), GuiLeaptVehicleEvent (393), GuiShowtimeModeSwitch
// (397), GuiShowtimeJustBounced (402) -- has this function as its ONLY producer. Their
// consumers were already live and mounted before this wave (HudMessageAnalyzer's
// HandleShowtimeMultiplierMessage / HandleShowtimeModeSwitch / HandleCrushComboMessage /
// HandleSignSmashMessage, BoostMessageManager::RecvEvent case 394), and the drain loop that
// reaches this function -- TranslateGameActionsToGuiEvents -- has been running EVERY FRAME
// since the stunt wave. So the eight arms were the one missing link, and while they were
// missing the showtime HUD could only ever show a static tally.
//
// ⚠️ The caller-side mode gate is NOT here, it is at the call site (see the dispatch arm added
// to GameBridgeGameStateToX_StuntGuiEvents.cpp). Reproducing it here as well would double it.
// ============================================================================
// ⚠️⚠️ THREE HEX-RAYS ARTEFACTS IN ONE FUNCTION -- ALL BIG-ENDIAN, ALL SILENT
// ============================================================================
// This body's pseudocode is wrong in three places that a little-endian host would inherit as
// real bugs. Each is corrected against the asm and marked at its arm:
//   (1) case 143: `HIBYTE(v12) = *(a3 + 12)` -- the asm is `lbz r11, 0xC(r31)` + `stb r11,
//       ev+0x0C`, a plain byte at +0x0C. "HIBYTE of the word at +0x0C" is the same address only
//       on big-endian.
//   (2) case 144: `HIWORD(v9) = *(a3 + 33)` -- the asm is TWO independent `lbz`/`stb` pairs
//       (+0x21 -> ev+0x00, +0x22 -> ev+0x01).
//   (3) case 140: the pseudocode's `v12 = a3[3]; v11 = a3[4]` looks like a transcription slip;
//       it is not -- the console really does cross the two, ev+0x08 <- action+0x10 and
//       ev+0x0C <- action+0x0C. Preserved as written.
// ============================================================================
#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Game/GameBridgeGameStateToX.h"                // PushGuiEvent<T>

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                    // GuiHUDMessageSignSmashed / CrushCombo / ShowtimeMultiplier / ShowtimeModeSwitch
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"              // GuiHitVehicleEvent / GuiLeaptVehicleEvent / GuiShowtimeScoreUpdate / GuiShowtimeJustBounced
#include "GameSource/GameState/BrnGameActions.h"                   // the six showtime action payload homes
#include "GameSource/GameState/BrnGameStateModule.h"               // GameStateModule::GetModeManager()
#include "GameSource/GameState/ModeManager/BrnModeManager.h"       // ModeManager::GetScoringSystem()
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"  // ScoringSystem::GetCrashScorer()
#include "GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoringRecentCrash.h" // CrashModeScoring::DealWithShowtimeStunt
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"             // InputBuffer::GetGuiEvents()
#include "GameShared/GameClasses/Development/Log/CgsLog.h"         // CgsDev::Log::gpDebugPrint
#include <stdlib.h>                                                // getenv (the diag guard)

namespace BrnGame
{
namespace
{
    // The console's baked assert path for this body (the three FireAssert sites at
    // 0x823E1A38 / 0x823E1A98 / 0x823E1C18 all load the same literal).
    const char* const KPC_ASSERT_FILE =
        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/GameBridgeGameStateToX.cpp";
}

// ============================================================================
//  BrnGameModule::TranslateShowtimeActionToGuiEvent  @ X360 0x823E1988
//
//  Signature from the asm prologue (@0x823E1994..0x823E19A0): r3 = this (r30), r4 = the action
//  type (biased by -0x7F for the 18-slot jump table), r5 = the action record (r31), r6 = the GUI
//  input buffer (r29). The two base addresses the console materialises are
//      addis 0x67 / addi -0x4D10  == this + 6730480  == &mCrashModeScoring
//      addis 0x6F / addi -0x55E0  == this + 7252512  == the embedded CgsGui::GuiModule
//  The first is reached by name here through ModeManager::GetScoringSystem()->GetCrashScorer()
//  (ScoringSystem + 0x20, the offset this tree already records); the second is not used at all,
//  because the shared PushGuiEvent writes the input buffer's queue directly -- see its banner
//  in GameBridgeGameStateToX.h for why, and for the DELETE-WHEN.
// ============================================================================
void BrnGameModule::TranslateShowtimeActionToGuiEvent(
        s32 liActionType,
        const CgsModule::Event* lpAction,
        CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInput)
{
    using namespace BrnGameState::GameStateModuleIO;

    // [DIAG] NOT IN THE X360 BINARY. Env-guarded + first-N latched, the same shape as the
    // sibling slices' ladders. It exists because "the showtime score is now published" is a
    // claim about a drain loop, and a drain loop that never sees an action looks identical to
    // one whose arms are wrong.
    static const bool sbShowtimeDiag  = ( getenv( "BRN_SHOWTIME_SCORE_DIAG" ) != 0 );
    static s32        siDiagLinesLeft = 24;

    switch (liActionType)
    {
    // ---- 127 E_ACTION_WORLD_STUNT_PERFORMED (16 bytes) -- jump-table case 0 ---------------
    // @0x823E1A0C. The ONLY arm that posts no GUI event: it feeds the crash scorer's
    // recent-stunt de-dupe window and returns. (StuntModeScoring gets the same action by a
    // different route; this is the showtime-side copy.)
    case E_ACTION_WORLD_STUNT_PERFORMED:
    {
        const WorldStuntAction* const lpStuntAction =
            reinterpret_cast<const WorldStuntAction*>(lpAction);
        mGameStateModule.GetModeManager()->GetScoringSystem()->GetCrashScorer()
            ->DealWithShowtimeStunt(lpStuntAction);
        break;
    }

    // ---- 128 E_ACTION_OVERHEAD_SIGN_HIT (8 bytes) -- jump-table case 1 --------------------
    // @0x823E1BE8: `lwz r11, 0(r31)` -> the single word of GuiHUDMessageSignSmashed. The
    // producer stores the literal 10000 there right after CrashModeScoring::
    // DealWithHitOverheadSign, so this is the overhead-sign score popup.
    case E_ACTION_OVERHEAD_SIGN_HIT:
    {
        const OverheadSignHitAction* const lpSignAction =
            reinterpret_cast<const OverheadSignHitAction*>(lpAction);

        BrnGui::GuiHUDMessageSignSmashed lEvent;                 // id 400, 4 bytes
        lEvent.miPointsAwarded = lpSignAction->miPointsAwarded;
        PushGuiEvent(lEvent, lpGuiInput);
        break;
    }

    // ---- 139 E_ACTION_VEHICLE_LEAPT (1 byte) -- jump-table case 12 ------------------------
    // @0x823E1AE4. The console hands AddGuiEvent a 1-byte stack slot it NEVER WRITES (there is
    // no store between `addi r4, r1, var_40` and the call), because GuiLeaptVehicleEvent is a
    // payload-less tag record (DWARF: `struct GuiLeaptVehicleEvent : public GuiEvent<388> {}`).
    // Zeroed here rather than left uninitialised -- the host must not post stack garbage -- and
    // no consumer reads the byte. Same accommodation as PreRaceFlyByEndWire164 in the
    // event-flow slice, and it is an accommodation, not a behaviour change.
    case E_ACTION_VEHICLE_LEAPT:
    {
        BrnGui::GuiLeaptVehicleEvent lEvent;                     // id 393, 1 byte
        lEvent.maData[0] = 0;
        PushGuiEvent(lEvent, lpGuiInput);
        break;
    }

    // ---- 140 E_ACTION_VEHICLE_HIT (36 bytes) -- jump-table case 13 ------------------------
    // ⭐⭐ THE SHOWTIME PER-CAR SCORE ARM. @0x823E1B00..0x823E1BB0. Up to THREE GUI events out
    // of one action, in this order:
    //   (a) always            GuiHitVehicleEvent (394, 24)
    //   (b) every tenth car   GuiHUDMessageCrushCombo (401, 4)
    //   (c) when a multiplier GuiHUDMessageShowtimeMultiplier (399, 8)
    //       was actually earned
    case E_ACTION_VEHICLE_HIT:
    {
        const VehicleHitAction* const lpHitAction =
            reinterpret_cast<const VehicleHitAction*>(lpAction);

        // (a) @0x823E1B00..0x823E1B44. The six stores, in the console's own (scattered) order.
        // ⚠️ The +0x08/+0x0C pair really is crossed relative to the action -- `lwz r11, 0xC` ->
        // ev+0x0C and `lwz r11, 0x10` -> ev+0x08. That is not a slip: the GUI record orders
        // category before base score, the action orders base score before category.
        BrnGui::GuiHitVehicleEvent lHitEvent;                    // id 394, 24 bytes
        lHitEvent.meVehicleClass         = lpHitAction->meVehicleClass;          // <- action+0x00
        lHitEvent.miVehicleClassTotalHit = lpHitAction->miVehicleTypeCrashed;    // <- action+0x04
        lHitEvent.meVehicleScoreCategory = lpHitAction->meVehicleScoreCategory;  // <- action+0x10
        lHitEvent.miVehicleBaseScore     = lpHitAction->miVehicleBaseScore;      // <- action+0x0C
        lHitEvent.miVehicleChainBonus    = lpHitAction->miComboBonusEarned;      // <- action+0x1C
        lHitEvent.muVehicleIndex         = lpHitAction->muTrafficEntityIndex;    // <- action+0x20 (lhz/sth)
        lHitEvent.maPad16                = 0;   // the console leaves ev+0x16 unwritten; see (b)'s note
        PushGuiEvent(lHitEvent, lpGuiInput);

        // (b) @0x823E1B48..0x823E1B88. The console does NOT emit a modulo: it magic-multiplies
        // by 0x66666667, `srawi 2`, adds the sign bit and remultiplies by 10 to recover the
        // remainder -- the standard signed-divide-by-10 strength reduction. De-optimised back to
        // `% 10` per the reconstruction rules. The dividend is the action's TOTAL cars crashed
        // (maiNumCarsCrashed[0..3] summed by the producer), so this fires on every tenth car --
        // INCLUDING the zeroth, which is the console's behaviour and not an off-by-one here.
        if ((lpHitAction->miTotalVehiclesCrashed % 10) == 0)
        {
            BrnGui::GuiHUDMessageCrushCombo lComboEvent;         // id 401, 4 bytes
            lComboEvent.miCrushComboCount = lpHitAction->miTotalVehiclesCrashed;
            PushGuiEvent(lComboEvent, lpGuiInput);
        }

        // (c) @0x823E1B8C..0x823E1BB0. Gated on the EARNED multiplier being strictly positive
        // (`cmpwi r11, 0 / ble` -- a SIGNED test), and the record carries the new running total
        // first and the earned delta second, which is the order the analyzer's latch expects.
        if (lpHitAction->miScoreMultiplierEarned > 0)
        {
            BrnGui::GuiHUDMessageShowtimeMultiplier lMultEvent;  // id 399, 8 bytes
            lMultEvent.miNewMultiplier    = lpHitAction->miTotalScoreMultiplier;   // <- action+0x18
            lMultEvent.miMultiplierEarned = lpHitAction->miScoreMultiplierEarned;  // <- action+0x14
            PushGuiEvent(lMultEvent, lpGuiInput);
        }

        if ( sbShowtimeDiag && siDiagLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0 )
        {
            --siDiagLinesLeft;
            *CgsDev::Log::gpDebugPrint
                << "[showtime-score] action 140 VEHICLE_HIT -> gui 394 base="
                << lpHitAction->miVehicleBaseScore
                << " chain=" << lpHitAction->miComboBonusEarned
                << " carsCrashed=" << lpHitAction->miTotalVehiclesCrashed
                << " multTotal=" << lpHitAction->miTotalScoreMultiplier
                << " multEarned=" << lpHitAction->miScoreMultiplierEarned << "\n";
        }
        break;
    }

    // ---- 141 E_ACTION_ENTER_NEW_ROAD (1 byte) -- jump-table case 14 -----------------------
    // @0x823E1C2C IS the function epilogue. The console recognises 141 and deliberately posts
    // nothing: the arm exists purely to keep the action off the "Unknown Showtime action."
    // default. Reproduced as an explicit empty case rather than folded into the default, which
    // is what makes the difference visible.
    case E_ACTION_ENTER_NEW_ROAD:
        break;

    // ---- 142 E_ACTION_SHOWTIME_UPDATE (12 bytes) -- jump-table case 15 --------------------
    // ⭐⭐ THE SHOWTIME SCORE UPDATE. @0x823E1A24..0x823E1A74. Three words straight across into
    // GuiShowtimeScoreUpdate. The console null-asserts the action first
    // (GameBridgeGameStateToX.cpp:3976) and then dereferences it anyway on the assert-disabled
    // path -- reproduced as written, no invented early-out.
    case E_ACTION_SHOWTIME_UPDATE:
    {
        CGS_ASSERT(lpAction != 0, "lpShowtimeUpdateAction");     // :3976 (li r5, 0xF88)
        (void)KPC_ASSERT_FILE;

        const ShowtimeUpdateAction* const lpUpdateAction =
            reinterpret_cast<const ShowtimeUpdateAction*>(lpAction);

        BrnGui::GuiShowtimeScoreUpdate lEvent;                   // id 396, 12 bytes
        lEvent.mNetworkPlayerID     = lpUpdateAction->mNetworkPlayerID;
        lEvent.meActiveRaceCarIndex = lpUpdateAction->meActiveRaceCarIndex;
        lEvent.miShowtimeScore      = lpUpdateAction->miShowtimeScore;
        PushGuiEvent(lEvent, lpGuiInput);

        if ( sbShowtimeDiag && siDiagLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0 )
        {
            --siDiagLinesLeft;
            *CgsDev::Log::gpDebugPrint
                << "[showtime-score] action 142 SHOWTIME_UPDATE -> gui 396 score="
                << lEvent.miShowtimeScore
                << " car=" << static_cast<s32>(lEvent.meActiveRaceCarIndex) << "\n";
        }
        break;
    }

    // ---- 143 E_ACTION_SHOWTIME_MODE_SWITCH (16 bytes) -- jump-table case 16 ---------------
    // @0x823E1A80..0x823E1AD8. Three words plus ONE BYTE at +0x0C.
    // ⚠️ Hex-Rays spells that byte `HIBYTE(v12) = *(a3 + 12)`; the asm is `lbz r11, 0xC(r31)` /
    // `stb r11, ev+0x0C`. On a little-endian host the HIBYTE spelling would write ev+0x0F.
    case E_ACTION_SHOWTIME_MODE_SWITCH:
    {
        CGS_ASSERT(lpAction != 0, "lpShowtimeModeSwitchAction"); // :3992 (li r5, 0xF98)

        const ShowtimeModeSwitchAction* const lpSwitchAction =
            reinterpret_cast<const ShowtimeModeSwitchAction*>(lpAction);

        BrnGui::GuiShowtimeModeSwitch lEvent;                    // id 397, 16 bytes
        lEvent.mNetworkPlayerID     = static_cast<s32>(lpSwitchAction->mNetworkPlayerID);
        lEvent.meActiveRaceCarIndex = lpSwitchAction->meActiveRaceCarIndex;
        lEvent.miFinalShowtimeScore = lpSwitchAction->miFinalShowtimeScore;
        lEvent.mbEnteringShowtime   = lpSwitchAction->mbEnteringShowtime;
        PushGuiEvent(lEvent, lpGuiInput);
        break;
    }

    // ---- 144 E_ACTION_JUST_BOUNCED (48 bytes) -- jump-table case 17 -----------------------
    // @0x823E1BBC..0x823E1BDC. Two byte copies, +0x21 -> ev+0x00 and +0x22 -> ev+0x01.
    // ⚠️ Hex-Rays collapses them into `HIWORD(v9) = *(a3 + 33)`, which is a 16-bit big-endian
    // artefact: taken literally the host would write ev+0x02/+0x03 of a 2-byte record.
    case E_ACTION_JUST_BOUNCED:
    {
        const JustBouncedAction* const lpBounceAction =
            reinterpret_cast<const JustBouncedAction*>(lpAction);

        BrnGui::GuiShowtimeJustBounced lEvent;                   // id 402, 2 bytes
        lEvent.mbOnCar         = lpBounceAction->mbOnCar;          // <- action+0x21
        lEvent.mbBoostedBounce = lpBounceAction->mbBoostedBounce;  // <- action+0x22
        PushGuiEvent(lEvent, lpGuiInput);
        break;
    }

    // ---- default: jump-table cases 2-11 (actions 129-138) ---------------------------------
    // @0x823E1C0C, GameBridgeGameStateToX.cpp:4075. The console fires the assert and returns;
    // the biased jump table means anything outside 127..144 never reaches this function at all
    // (the caller's switch selects the arm), so this really is the 129-138 hole.
    default:
        CGS_ASSERT(false, "Unknown Showtime action.");            // :4075 (li r5, 0xFEB)
        break;
    }
}

} // namespace BrnGame
