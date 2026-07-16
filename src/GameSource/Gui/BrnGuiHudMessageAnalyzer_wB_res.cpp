#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"

#include "GameShared/GameClasses/Core/CgsID.h"   // CgsIDCompress (skillz-id table init)

// BrnGui::HudMessageAnalyzer -- the TU's data spine (wave-B keystone partfile).
// Reconstructed from BURNOUT_X360_ARTIST.XEX; every value below is dumped from the
// X360 image (scratchpad/waveB/hudmsg_rodata_dump.txt), NOT guessed.
//
//   * message-id string tables  -- rodata pointer tables @0x8206F59C..0x82F27824
//   * KF_REQUIRED_WRECK_DURATION -- flt_8206F8E0 == 0.6f
//   * KA_SKILLZ_MESSAGE_IDS      -- .data qword_82FB56D8 (4x14 CgsIDs), zero in the
//     image and filled by the TU's dynamic initialiser @0x82C56198 as
//     CgsIDCompress("Sklz_<row>_<skill>"); reproduced here as a dynamically
//     initialised static (the compiler emits the same compress-and-store init).
//
// The original TU kept the KAPC_* tables at file scope; the reconstructed TU is
// split across partfiles, so they are extern (declared in the class home header).

namespace BrnGui
{

// @0x8206F59C -- takedown-type message ids, indexed by BrnGameState::ETakedownType.
// Slots 6..8 are genuinely empty strings in the X360 image (the pointer parks on the
// shared "" rodata byte); slot 5 aliases slot 0's "TDGdGeneral".
const char* const KAPC_TAKEDOWN_TYPES[13] =
{
    "TDGdGeneral",   // [0]
    "TDGdGrind",     // [1]
    "TDGdTBone",     // [2]
    "TDGdVert",      // [3]
    "TDGdTraffCh",   // [4]
    "TDGdGeneral",   // [5]
    "",              // [6]
    "",              // [7]
    "",              // [8]
    "TDGdRevenge",   // [9]
    "TDGdCar",       // [10]
    "TDGdVan",       // [11]
    "TDGdBus",       // [12]
};

// @0x82F278BC -- chained-takedown message ids ("row" == chain length; index chain-2).
// ConstructTakedownMessage clamps chain overflow (chain-2 >= 9) to the last row.
const char* const KAPC_TAKEDOWN_CHAIN[9] =
{
    "TDGdRow2", "TDGdRow3", "TDGdRow4", "TDGdRow5", "TDGdRow6",
    "TDGdRow7", "TDGdRow8", "TDGdRow9", "TDGdRow10",
};

// @0x8206F618 / @0x8206F624 -- dirty-trick ("payback") takedown lines, indexed by
// BrnNetwork::EPaybackType (X360: 0 slash / 1 lock / 2 takeover; 3 == none). The X360
// tables hold THREE entries (the PS3-DWARF [4] included the six-axis slot).
const char* const KAPC_PAYBACK_TAKEDOWN_BD_MESSAGES[3] =
{
    "PbkBdSlashDn", "PbkBdLockDn", "PbkBdTOvrDn",
};
const char* const KAPC_PAYBACK_TAKEDOWN_GD_MESSAGES[3] =
{
    "PbkGdSlashDn", "PbkGdLockDn", "PbkGdTOvrDn",
};

// @0x82F277E0 -- finish-position string ids. HandleEliminatedEvent indexes from the
// SECOND slot (message = &KAPC_FINISH_POSITION_MESSAGES[1][position]); the four
// lowercase variants trail the eight uppercase slots in the X360 image.
const char* const KAPC_FINISH_POSITION_MESSAGES[12] =
{
    "POSITION_FIRST",   "POSITION_SECOND",  "POSITION_THIRD",   "POSITION_FOURTH",
    "POSITION_FIFTH",   "POSITION_SIXTH",   "POSITION_SEVENTH", "POSITION_EIGHTH",
    "POSITION_FIRST_LOWERCASE", "POSITION_SECOND_LOWERCASE",
    "POSITION_THIRD_LOWERCASE", "POSITION_FOURTH_LOWERCASE",
};

// @0x8206F684 -- "all collectibles of this family done" string ids, indexed by
// BrnGui::StuntType.
const char* const KAPC_COLLECTABLE_COMPLETION_STRINGID[3] =
{
    "HUDMESSAGE_JUMPS_COMPLETE",
    "HUDMESSAGE_SMASHES_COMPLETE",
    "HUDMESSAGE_STUNTS_COMPLETE",
};

// @0x82F27704 -- county string ids (BrnWorld::ECounty order), the second "StntAllDone"
// parameter. FLAG: consumer-named (see the header declaration).
const char* const KAPC_COUNTY_STRINGID[5] =
{
    "BRH_PBH",   // [0]
    "BRH_SL",    // [1]
    "BRH_HT",    // [2]
    "BRH_WM",    // [3]
    "BRH_DTP",   // [4]
};

// flt_8206F8E0 -- how long the wreck state must persist before the wrecked message
// fires (HandleWreckedEvent accumulates the frame delta against it).
const f32 HudMessageAnalyzer::KF_REQUIRED_WRECK_DURATION = 0.6f;

// .data qword_82FB56D8, filled by the TU's dynamic initialiser @0x82C56198: one
// compressed "Sklz_<messageType>_<skill>" id per (EBurnoutSkillzMessageTypes row,
// BurnoutSkillzData skill column). Columns 9, 12 and 13 are explicitly zeroed by the
// initialiser (no message for those skill slots); Update skips zero entries.
//   rows:    AbB == X_BEAT_YS, AbY == X_BEAT_YOUR, Ag == X_GOT, Yg == YOU_GOT
//   columns: InA Bar BCh Dri Spn InD NrM Onc PPk <0> RRT RRC <0> <0>
const CgsID HudMessageAnalyzer::KA_SKILLZ_MESSAGE_IDS[4][14] =
{
    {
        CgsIDCompress("Sklz_AbB_InA"), CgsIDCompress("Sklz_AbB_Bar"),
        CgsIDCompress("Sklz_AbB_BCh"), CgsIDCompress("Sklz_AbB_Dri"),
        CgsIDCompress("Sklz_AbB_Spn"), CgsIDCompress("Sklz_AbB_InD"),
        CgsIDCompress("Sklz_AbB_NrM"), CgsIDCompress("Sklz_AbB_Onc"),
        CgsIDCompress("Sklz_AbB_PPk"), 0,
        CgsIDCompress("Sklz_AbB_RRT"), CgsIDCompress("Sklz_AbB_RRC"),
        0, 0,
    },
    {
        CgsIDCompress("Sklz_AbY_InA"), CgsIDCompress("Sklz_AbY_Bar"),
        CgsIDCompress("Sklz_AbY_BCh"), CgsIDCompress("Sklz_AbY_Dri"),
        CgsIDCompress("Sklz_AbY_Spn"), CgsIDCompress("Sklz_AbY_InD"),
        CgsIDCompress("Sklz_AbY_NrM"), CgsIDCompress("Sklz_AbY_Onc"),
        CgsIDCompress("Sklz_AbY_PPk"), 0,
        CgsIDCompress("Sklz_AbY_RRT"), CgsIDCompress("Sklz_AbY_RRC"),
        0, 0,
    },
    {
        CgsIDCompress("Sklz_Ag_InA"),  CgsIDCompress("Sklz_Ag_Bar"),
        CgsIDCompress("Sklz_Ag_BCh"),  CgsIDCompress("Sklz_Ag_Dri"),
        CgsIDCompress("Sklz_Ag_Spn"),  CgsIDCompress("Sklz_Ag_InD"),
        CgsIDCompress("Sklz_Ag_NrM"),  CgsIDCompress("Sklz_Ag_Onc"),
        CgsIDCompress("Sklz_Ag_PPk"),  0,
        CgsIDCompress("Sklz_Ag_RRT"),  CgsIDCompress("Sklz_Ag_RRC"),
        0, 0,
    },
    {
        CgsIDCompress("Sklz_Yg_InA"),  CgsIDCompress("Sklz_Yg_Bar"),
        CgsIDCompress("Sklz_Yg_BCh"),  CgsIDCompress("Sklz_Yg_Dri"),
        CgsIDCompress("Sklz_Yg_Spn"),  CgsIDCompress("Sklz_Yg_InD"),
        CgsIDCompress("Sklz_Yg_NrM"),  CgsIDCompress("Sklz_Yg_Onc"),
        CgsIDCompress("Sklz_Yg_PPk"),  0,
        CgsIDCompress("Sklz_Yg_RRT"),  CgsIDCompress("Sklz_Yg_RRC"),
        0, 0,
    },
};

} // namespace BrnGui
