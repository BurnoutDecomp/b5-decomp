// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeGameStateToX_TrainingStringIds.cpp
//
// PER-FUNCTION SIBLING of GameSource/Game/GameBridgeGameStateToX.cpp, split out on
// 2026-08-16 (tutorial-ticker leg). Homes exactly one X360 function plus the two rodata
// tables it indexes:
//
//   BrnGame::ConvertTrainingTypeToStringId   0x823AA3B8
//
// ⭐ WHAT THIS IS FOR. It is the one step that turns a training-tip enum into the GUI
// string id the bottom-of-screen tutorial ticker looks up in the language data. The full
// console chain the game shows the "how to start the car" text through is:
//
//   TrainingManager::Update @0x823937D0 (state 1)
//     -> TrainingManager::SendTrainingTickerMessage @0x82388940
//          -> GameAction 148, payload = the 4-byte BrnProgression::ETrainingType
//   BrnGameModule::BridgeGameStateToGui @0x823EE880
//     -> TranslateGameActionsToGuiEvents @0x823E9CE0, case 148
//          -> **ConvertTrainingTypeToStringId (THIS FUNCTION)**
//          -> GuiEventTickerCustomMessage::AddString(id, 2) -> GUI event 537
//   BrnGui::CustomRendererManager::RecvEvent -> case 537
//     -> BrnGui::InGameMessageRenderer (the Im2d ticker) + BrnGui::BlackBarRenderer
//
// ⛔ ONLY THIS STEP IS LIVE. Nothing on this build produces GameAction 148 (the
// TrainingManager is neither constructed nor updated) and nothing consumes GUI event 537
// (InGameMessageRenderer has 15 X360 functions and none is reconstructed; its draw path
// needs CgsGraphics::TextRenderer, which has no reconstructed home in this tree at all).
// This TU is deliberately mounted anyway: it is correct, complete, costs zero unresolved
// externals, and it is the piece that was previously blocked on data nobody had.
//
// WHY IT IS A SEPARATE TU: the owning GameBridgeGameStateToX.cpp does not compile and did
// not before this leg either (measured against HEAD's own copy: an ODR fork on
// BrnGui::GuiTakedownEvent plus a stale mpCgsGuiModule reference, all inside
// TranslateTakedownsToGuiEvents). The MOVED-OUT block left behind there records the four
// exact errors. Fold this file back in when they are repaired -- that is a delete, not a
// duplicate-symbol hunt, because the function was MOVED and not copied.
// ============================================================================

#include "GameSource/Game/GameBridgeGameStateToX.h"        // BrnGame::ConvertTrainingTypeToStringId decl

#include "GameShared/GameClasses/Core/CgsAssert.h"         // CGS_ASSERT
#include "SharedClasses/Progression/BrnTrainingTypes.h"    // BrnProgression::ETrainingType

namespace BrnGame
{
    // ------------------------------------------------------------------------
    // Two file-scope string-ID lookup tables (X360 rodata off_82CDBF40 /
    // dword_82FAE290). Both are const char*[] with a 4-byte element stride
    // (asm: slwi rX, rX, 2 ; lwzx).
    //   KAC_SPECIFIC_TRAINING_TEXT : const char*[77]   (@0x82CDBF40)
    //   KAC_GENERAL_TRAINING_TEXT  : const char*[128]  (@0x82FAE290)
    //
    // ⭐ RECOVERED FROM THE IMAGE (2026-08-16, tutorial-ticker leg). The previous banner
    // said the contents were "UNRECOVERABLE from this TU's dossier" and left both arrays
    // `extern`-only -- which made this TU unlinkable and left ConvertTrainingTypeToStringId
    // with no data. They are not unrecoverable: the IDA function-only export set has no
    // .rdata, but the UNPACKED .i64 image bytes are readable, and both tables are plain
    // arrays of big-endian pointers into the string blob.
    //
    // ⚠️ NOT TRUSTED ON THE READER'S WORD -- three independent controls, all passed:
    //   1. Element 0 is "TRAINING_LEAVES_JUNKYARD", the ONE entry the old banner already
    //      attested from the dossier. The recovery reproduces it exactly.
    //   2. MEASURED, not eyeballed: 53 of the 54 non-null entries spell the matching
    //      BrnProgression::ETrainingType enumerator (index 2 == TRAINING_START_ENGINE ==
    //      E_TRAINING_TYPE_START_ENGINE, 5/6/7 == the three DRIVES_*_CAR tips, 41/42/43 ==
    //      INTRO_TO_ONLINE_1..3, 71..76 == the six TRAFFIC_*_LICENSE tips). The single
    //      non-match is index 34, "TRAINING_FOUND_CAR_PARK" against
    //      E_TRAINING_TYPE_FINDS_CAR_PARK -- a tense difference between the authored string
    //      id and the enumerator, not a misalignment (both sit at 34, and every neighbour
    //      matches). That enum was derived independently (DWARF + the RequestTraining /
    //      DoesTrainingPauseGame switch arms), so the two agreeing is a real cross-check,
    //      not a restatement.
    //   3. Every recovered id resolves in the SHIPPED language data: CRC-32(id) is present
    //      exactly once in LANGUAGE/0002.bundle (already loaded at boot), and the string it
    //      points at is the tutorial line -- e.g. TRAINING_START_ENGINE resolves to
    //      "Okay, let's just check this thing still starts. Hold the accelerator (right
    //      trigger) to fire up the engine."
    //
    // The NULL slots are null IN THE IMAGE (types with no ticker string: 4, 9, 16..19, 32,
    // 33, 39, 48, 56, 57, 59..69). ConvertTrainingTypeToStringId returns them verbatim and
    // its caller (TranslateGameActionsToGuiEvents case 148) tests the result for NULL before
    // building the ticker event, so a null is a live, load-bearing value -- not a hole.
    static const char* const KAC_SPECIFIC_TRAINING_TEXT[77] =
    {
        /*  0 */ "TRAINING_LEAVES_JUNKYARD",
        /*  1 */ "TRAINING_MAP_APPEARS",
        /*  2 */ "TRAINING_START_ENGINE",
        /*  3 */ "TRAINING_FIRST_USE_AUTOREPAIR",
        /*  4 */ 0,
        /*  5 */ "TRAINING_DRIVES_STUNT_CAR",
        /*  6 */ "TRAINING_DRIVES_DANGER_CAR",
        /*  7 */ "TRAINING_DRIVES_AGGRESSION_CAR",
        /*  8 */ "TRAINING_DISCOVERS_EVENT",
        /*  9 */ 0,
        /* 10 */ "TRAINING_USES_GAS_STATION",
        /* 11 */ "TRAINING_USES_BODY_SHOP",
        /* 12 */ "TRAINING_SUPER_JUMP",
        /* 13 */ "TRAINING_BILLBOARD",
        /* 14 */ "TRAINING_SMASH",
        /* 15 */ "TRAINING_BARREL_ROLL",
        /* 16 */ 0,
        /* 17 */ 0,
        /* 18 */ 0,
        /* 19 */ 0,
        /* 20 */ "TRAINING_DANGER_BOOST_FULL",
        /* 21 */ "TRAINING_BURNOUT",
        /* 22 */ "TRAINING_AGGRESSION_TAKEDOWN",
        /* 23 */ "TRAINING_AGGRESSION_LOST_BOOST_CHUNK",
        /* 24 */ "TRAINING_CORRECT_CAR_FOR_CHALLENGE",
        /* 25 */ "TRAINING_WRONG_CAR_FOR_CHALLENGE",
        /* 26 */ "TRAINING_ROAD_RULES_ON",
        /* 27 */ "TRAINING_ROAD_RULES_FORCE_ON",
        /* 28 */ "TRAINING_CRASH_ROAD_RULES_ON",
        /* 29 */ "TRAINING_WON_ROAD_RULE_TIME",
        /* 30 */ "TRAINING_WON_ROAD_RULE_CRASH",
        /* 31 */ "TRAINING_WON_ROAD",
        /* 32 */ 0,
        /* 33 */ 0,
        /* 34 */ "TRAINING_FOUND_CAR_PARK",
        /* 35 */ "TRAINING_USES_E_BRAKE",
        /* 36 */ "TRAINING_FLAT_SPIN",
        /* 37 */ "TRAINING_POWER_PARK",
        /* 38 */ "TRAINING_MAP_INFORMATION",
        /* 39 */ 0,
        /* 40 */ "TRAINING_TAKEDOWN",
        /* 41 */ "TRAINING_INTRO_TO_ONLINE_1",
        /* 42 */ "TRAINING_INTRO_TO_ONLINE_2",
        /* 43 */ "TRAINING_INTRO_TO_ONLINE_3",
        /* 44 */ "TRAINING_WON_ROAD_RULE_ONLINE",
        /* 45 */ "TRAINING_LOST_ROAD_RULE_ONLINE",
        /* 46 */ "TRAINING_USES_PAINT_SHOP",
        /* 47 */ "TRAINING_QUITS_EVENT",
        /* 48 */ 0,
        /* 49 */ "TRAINING_BARREL_ROLL_FAILED",
        /* 50 */ "TRAINING_BOOST",
        /* 51 */ "TRAINING_BOOST",
        /* 52 */ "TRAINING_BOOST",
        /* 53 */ "TRAINING_BOOST",
        /* 54 */ "TRAINING_BOOST",
        /* 55 */ "TRAINING_2ND_WRECK_THROUGH_AUTOREPAIR",
        /* 56 */ 0,
        /* 57 */ 0,
        /* 58 */ "TRAINING_TRY_A_FLAT_SPIN",
        /* 59 */ 0,
        /* 60 */ 0,
        /* 61 */ 0,
        /* 62 */ 0,
        /* 63 */ 0,
        /* 64 */ 0,
        /* 65 */ 0,
        /* 66 */ 0,
        /* 67 */ 0,
        /* 68 */ 0,
        /* 69 */ 0,
        /* 70 */ "ONLINE_WIN_CAR",
        /* 71 */ "TRAFFIC_D_LICENSE",
        /* 72 */ "TRAFFIC_C_LICENSE",
        /* 73 */ "TRAFFIC_B_LICENSE",
        /* 74 */ "TRAFFIC_A_LICENSE",
        /* 75 */ "TRAFFIC_BURN_LICENSE",
        /* 76 */ "TRAFFIC_ELITE_LICENSE",
    };

    // The "general" (timed-tip) table lives in the MUTABLE data segment (dword_, not off_)
    // and is ALL ZERO in the shipped image -- verified over the whole 0x82FAD000..0x82FAF800
    // window, which is uniformly zero, so this is a genuinely blank table and not a reader
    // that missed the segment. That agrees with BrnTrainingTypes.h, where the timed-tip span
    // is empty (E_TRAINING_TYPE_TIMED_TIP_1 == E_TRAINING_TYPE_TIMED_TIP_END == 128, count 0).
    // ⚠️ A zero in the image is not proof nothing writes it at run time; no writer was found,
    // but the search was not exhaustive. Reproduced as the zero-initialised array the image
    // holds -- ConvertTrainingTypeToStringId only indexes it for types >= 128, and the caller
    // NULL-checks, so an unwritten entry is a no-ticker, exactly as on the console.
    static const char* const KAC_GENERAL_TRAINING_TEXT[128] = { 0 };

    // ------------------------------------------------------------------------
    // ConvertTrainingTypeToStringId @0x823AA3B8
    //
    // Maps a training-tip enum to its GUI string-ID. Indices 0..76 select from
    // the "specific" (untimed) table; indices 128..255 select from the
    // "general" (timed-tip) table at offset (index - 128). The gap 77..127 and
    // the unused specific slots return the error sentinel.
    // ------------------------------------------------------------------------
    const char* ConvertTrainingTypeToStringId(BrnProgression::ETrainingType leTrainingType)
    {
        const int liType = static_cast<int>(leTrainingType);

        CGS_ASSERT(liType >= 0 && liType < BrnProgression::E_TRAINING_TYPE_COUNT,
                   "leTrainingType >= 0 && leTrainingType < BrnProgression::E_TRAINING_TYPE_COUNT");

        if (liType < 128)
        {
            CGS_ASSERT(liType < 77, "leTrainingType < ciNumSpecificTrainingStringIDs");

            if (liType > 76)
            {
                return "ERROR - UNKNOWN TRAINING TYPE";
            }
            return KAC_SPECIFIC_TRAINING_TEXT[liType];
        }

        const int liIndex = liType - 128;
        if (liIndex >= 128)
        {
            CGS_ASSERT(liIndex < 128, "liIndex < ciNumGeneralTrainingStringIDs");
            return "ERROR - UNKNOWN TRAINING TYPE";
        }
        return KAC_GENERAL_TRAINING_TEXT[liIndex];
    }
} // namespace BrnGame
