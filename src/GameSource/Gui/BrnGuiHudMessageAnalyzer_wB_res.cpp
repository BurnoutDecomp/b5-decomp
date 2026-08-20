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

// -----------------------------------------------------------------------------------------
// [gateui r3] The THREE drive-through message tables HandleDriveThrough @0x8251D570 selects
// between, all indexed by GuiDriveThroughEvent::DriveThroughType (0..5). Names verbatim from
// the DWARF (BrnGuiHudMessageAnalyzer.cpp:95 / :105 / :115) -- including the original's
// KPAC_ transposition in the third one, which is reproduced, not "corrected".
// Values dumped from the X360 image (scratchpad/waveB/hudmsg_rodata_dump.txt, the 18-entry
// run at 0x8206F5D0 that IDA labels as one block: 0x8206F5D0 / 0x8206F5E8 / 0x8206F600).
//
// The NULL slots are REAL and load-bearing, not gaps in the dump: the console reaches a table
// slot only on the paths its own branch structure allows, so the empty ones are unreachable
// -- MAGIC is only ever indexed for types 0/2 (the only two with a "2" variant), and
// INEFFECTIVE is only ever indexed when mbEffective is false. Reproduced as NULL, and
// HandleDriveThrough deliberately does NOT guard the Construct against a null id (that guard
// would be a behaviour the binary does not have).
// -----------------------------------------------------------------------------------------

// @0x8206F5D0 -- the normal ("basic") line for each drive-through type.
const char* const KAPC_DRIVE_THROUGH_MESSAGES[6] =
{
    "DriThrCarWsh",   // [0] E_DRIVE_THROUGH_TYPE_CAR_WASH
    "DriThrBdyShp",   // [1] E_DRIVE_THROUGH_TYPE_BODY_SHOP
    "DriThrPntShp",   // [2] E_DRIVE_THROUGH_TYPE_PAINT_SHOP
    "DriThrGasStn",   // [3] E_DRIVE_THROUGH_TYPE_GAS_STATION
    "DriThrAPts",     // [4] E_DRIVE_THROUGH_TYPE_AUTO_PARTS
    "DriThrClosed",   // [5] E_DRIVE_THROUGH_TYPE_FAILED
};

// @0x8206F5E8 -- the rare "magic" variant, drawn 1-in-
// KU_FREQUENCY_OF_DRIVETHROUGH_MAGIC_MESSAGES for the two effective types that have one.
const char* const KAPC_DRIVE_THROUGH_MAGIC_MESSAGES[6] =
{
    "DriThrCarWs2",   // [0] E_DRIVE_THROUGH_TYPE_CAR_WASH
    NULL,             // [1] (never indexed: only types 0 and 2 take the random branch)
    "DriThrPntSh2",   // [2] E_DRIVE_THROUGH_TYPE_PAINT_SHOP
    NULL,             // [3]
    NULL,             // [4]
    NULL,             // [5]
};

// @0x8206F600 -- the "you drove through but it did nothing" line (mbEffective == false).
// DWARF spelling KPAC_ (sic) preserved.
const char* const KPAC_DRIVE_THROUGH_INEFFECTIVE_MESSAGES[6] =
{
    NULL,             // [0] (car wash has no ineffective line)
    "DriThrBdyShX",   // [1] E_DRIVE_THROUGH_TYPE_BODY_SHOP
    "DriThrPntShX",   // [2] E_DRIVE_THROUGH_TYPE_PAINT_SHOP
    NULL,             // [3]
    NULL,             // [4]
    "DriThrClosed",   // [5] E_DRIVE_THROUGH_TYPE_FAILED (aliases the basic table's slot 5)
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

// [gateui] .data qword_82FB5898 (the three qwords immediately after the 4x14
// KA_SKILLZ_MESSAGE_IDS block, which ends at 0x82FB5890) -- the collectible-tally message
// names HandleStuntInfo @0x8251F650 indexes by BrnGui::StuntType (`slwi r8,r8,3; ldx`).
// Zero in the image and filled by the TU's OWN second dynamic initialiser @0x82C56040
// (a distinct thunk from the skillz one @0x82C56198), which the pseudocode gives
// verbatim:
//     CgsIDCompress("StuntPartJmp") -> qword_82FB5898[0]
//     CgsIDCompress("StuntPartSma") -> qword_82FB58A0
//     CgsIDCompress("StuntFull")    -> qword_82FB58A8
// Reproduced here as a dynamically initialised static, exactly as KA_SKILLZ_MESSAGE_IDS
// is (the compiler emits the same compress-and-store init).
//
// (This closes the scout map's "PARK: qword_82FB5898 needs headless IDA" item -- the
// values were never in rodata to dump; they are constructed by a dyn-init thunk that IS
// in the JSON export set. Same class of recovery as the k*Def* CRT-init dumps: walk the
// initialiser, do not read the zeroed .data.)
const CgsID HudMessageAnalyzer::KA_STUNT_INFO_MESSAGES[3] =
{
    CgsIDCompress("StuntPartJmp"),   // [0] E_STUNTTYPE_JUMP
    CgsIDCompress("StuntPartSma"),   // [1] E_STUNTTYPE_SMASH
    CgsIDCompress("StuntFull"),      // [2] E_STUNTTYPE_STUNT
};

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
