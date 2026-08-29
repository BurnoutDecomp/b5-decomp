#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // CgsID (u64)

// Owning header for the BrnGameState::GameStateModuleIO value-record types reconstructed by the
// GameMode/ModeManager leaf batch: PlayerInfo (X360 0x82355038) and GameStats (0x82354F38).
// Minimal slices -- only the members the reconstructed Construct() bodies touch are declared;
// the Get*/Set* accessors that need ETakedownType/StuntElementType/ECounty/ScoreType are left
// for those types' own TUs (so this header pulls no un-homed enum). Single owner; grow in place.

namespace BrnGameState
{
namespace GameStateModuleIO
{
// DWARF BrnGameActionData.h:219 -- one player's rank/car/violation state. Console-faithful
// layout: macName[32] @0x00, mCarId (u64) @0x20, then four int32s @0x28..0x34.
struct PlayerInfo
{
    void Construct(const char* lpcName,
                   CgsID       lCarId,
                   s32         liViolationPoints,
                   s32         liViolationPointsToNextRank,
                   s32         liCarUnlockedCount,
                   s32         liTotalViolationPoints,
                   s32         liRank);

private:
    static const s32 KI_NAME_LENGTH = 32;   // strncpy count (X360: li r5, 0x20)

    char  macName[KI_NAME_LENGTH];      // 0x00
    CgsID mCarId;                       // 0x20
    s32   miViolationPoints;            // 0x28
    s32   miViolationPointsToNextRank;  // 0x2C
    s32   miCarUnlockedCount;           // 0x30
    s32   miRank;                       // 0x34
};

// DWARF BrnGameActionData.h:53 -- flat stats block, no base. Construct() zeroes every member
// except miTotalRoads (the X360 asm never stores to it -- see the NOTE in Construct()).
//
// ⭐⭐ [pause-stats wave 2026-08-29] THE INT/FLOAT BOUNDARY WAS IN THE WRONG PLACE, AND IT
// LOOKED RIGHT BECAUSE THE TOTAL SIZE IS THE SAME. This header used to declare
// `maIntValues[32]` + `maFloatValues[4]`; the console has `maIntValues[33]` + `maFloatValues[3]`.
// Both spell 33 words between +0x18 and +0xA8, so sizeof, every later member's offset and
// Construct()'s zero-fill all agree -- only the TYPE of the word at +0x98 differs, and nothing
// in the tree read it until now. THREE independent X360 witnesses settle it:
//   1. Construct @0x82354F38 opens `addi r11, r3, 0x18 / li r10, 0x21 / mtctr r10` -- the merged
//      zero loop runs THIRTY-THREE (0x21) times from +0x18, i.e. to +0x9C, and the three
//      explicit stores that follow are at +0x9C/+0xA0/+0xA4. Not 32 + one float.
//   2. GetGameStats @0x8238A6A0 writes +0x9C and +0xA0 with `stfs` (mfAirMaximum,
//      mfCompletedAirSpinAngle) and +0xA4 with `stfs f1` (ComputeCompletionPercentage's f32
//      return) -- three FLOAT stores at 0x9C..0xA8 -- while +0x98 takes a plain `stw` of its
//      integer `liNumChallengesCompleted` argument.
//   3. The consumer agrees: TranslateGameActionsToGuiEvents case 180 @0x823EC8A0 loads
//      0x9C/0xA0/0xA4 with `lfs` and never touches 0x94 or 0x98 at all.
// The DWARF (BrnGameActionData.h:198, `float32_t[3] maFloatValues`, E_FLOAT_VALUE_TYPE_COUNT=3)
// says the same thing from the PS3 side; the X360's extra word is an extra INT enumerator, which
// is exactly the same X360-only delta that gives GetGameStats its third parameter (the PS3 DWARF
// declares `GetGameStats(GameStats*, StuntManager*)`, the X360 passes a challenge count too).
struct GameStats
{
    // DWARF BrnGameActionData.h:57. Values 0..31 are the DWARF's verbatim; 32 is X360-ONLY
    // (the PS3 enum ends at E_INT_VALUE_TYPE_COUNT = 32) and its NAME is reconstructed from
    // its producer -- GetGameStats' third argument is
    // ChallengeManager::CountCompletedChallenges()'s return. FLAGGED as such.
    enum IntValueType
    {
        E_INT_VALUE_TYPE_DISTANCE_DRIVEN_ONLINE                  = 0,
        E_INT_VALUE_TYPE_DISTANCE_DRIVEN_OFFLINE                 = 1,
        E_INT_VALUE_TYPE_TIME_PLAYED                             = 2,
        E_INT_VALUE_TYPE_CARS_COLLECTED                          = 3,
        E_INT_VALUE_TYPE_BEST_POWER_PARKING                      = 4,
        E_INT_VALUE_TYPE_BEST_POWER_PARKING_BETWEEN_OTHER_PLAYERS = 5,
        E_INT_VALUE_TYPE_MEDALS_GOLD                             = 6,
        E_INT_VALUE_TYPE_MEDALS_SILVER                           = 7,
        E_INT_VALUE_TYPE_MEDALS_BRONZE                           = 8,
        E_INT_VALUE_TYPE_NUM_EVENT_MEDALS                        = 9,
        E_INT_VALUE_TYPE_TOTAL_EVENT_MEDALS                      = 10,
        E_INT_VALUE_TYPE_NUM_ROAD_RULE_MEDALS                    = 11,
        E_INT_VALUE_TYPE_TOTAL_ROAD_RULE_MEDALS                  = 12,
        E_INT_VALUE_TYPE_JUMPS                                   = 13,
        E_INT_VALUE_TYPE_SMASHES                                 = 14,
        E_INT_VALUE_TYPE_STUNTS                                  = 15,
        E_INT_VALUE_TYPE_JUMPS_MAX                               = 16,
        E_INT_VALUE_TYPE_SMASHES_MAX                             = 17,
        E_INT_VALUE_TYPE_STUNTS_MAX                              = 18,
        E_INT_VALUE_TYPE_TAKEDOWNS                               = 19,
        E_INT_VALUE_TYPE_BEST_SHOWTIME                           = 20,
        E_INT_VALUE_TYPE_BEST_BOOST_CHAIN                        = 21,
        E_INT_VALUE_TYPE_BEST_DRIFT                              = 22,
        E_INT_VALUE_TYPE_BEST_ONCOMING                           = 23,
        E_INT_VALUE_TYPE_BEST_NO_BARREL_ROLLS                    = 24,
        E_INT_VALUE_TYPE_TOTAL_WINS_FOR_NEXT_RANK                = 25,
        E_INT_VALUE_TYPE_TOTAL_CARS_TO_SHUTDOWN                  = 26,
        E_INT_VALUE_TYPE_EVENTS_FOUND                            = 27,
        E_INT_VALUE_TYPE_EVENTS_TOTAL                            = 28,
        E_INT_VALUE_TYPE_HIGHEST_STUNT_SCORE                     = 29,
        E_INT_VALUE_TYPE_TOTALROADSRULED                         = 30,   // sic -- DWARF spelling
        E_INT_VALUE_TYPE_ACHIEVEMENTS                            = 31,
        // FLAG: X360-only enumerator, name reconstructed from its single producer.
        E_INT_VALUE_TYPE_FREEBURN_CHALLENGES_COMPLETE            = 32,
        E_INT_VALUE_TYPE_COUNT                                   = 33    // X360 (PS3 DWARF: 32)
    };

    // DWARF BrnGameActionData.h:100, verbatim (the X360 count matches).
    enum FloatValueType
    {
        E_FLOAT_VALUE_TYPE_BEST_AIRTIME  = 0,
        E_FLOAT_VALUE_TYPE_BEST_SPIN     = 1,
        E_FLOAT_VALUE_PERCENTAGE_COMPLETE = 2,   // sic -- DWARF spelling (no _TYPE_)
        E_FLOAT_VALUE_TYPE_COUNT         = 3
    };

    // DWARF BrnGameActionData.h:109, verbatim.
    enum IdValueType
    {
        E_ID_VALUE_TYPE_FAVOURITE_CAR = 0,
        E_ID_VALUE_TYPE_FORGOTTEN_CAR = 1,
        E_ID_VALUE_TYPE_NEMESIS       = 2,
        E_ID_VALUE_TYPE_COUNT         = 3
    };

    // The X360 loop bounds of the three non-enumerated arrays, which are also their declared
    // extents (BrnTakedownType.h E_TAKEDOWN_COUNT == 13, BrnChallengeData.h E_SCORE_TYPE_COUNT
    // == 2, BrnWorldRegion.h E_COUNTY_VALID_COUNT == 5, BrnGameStateTypes.h
    // E_STUNT_ELEMENT_TYPE_COUNT == 3).
    enum { KI_TAKEDOWN_TYPE_COUNT      = 13 };
    enum { KI_SCORE_TYPE_COUNT         = 2  };
    enum { KI_COUNTY_VALID_COUNT       = 5  };
    enum { KI_STUNT_ELEMENT_TYPE_COUNT = 3  };

    void Construct();   // X360 0x82354F38

    // ---- the DWARF's accessor set (BrnGameActionData.h:123..:193) -----------------------
    // The X360 emits no standalone symbol for any of them: every one is inlined at its call
    // site (GetGameStats @0x8238A6A0 is one long run of them, each preceded by the range
    // assert the DWARF declaration's own body carries). Defined inline here for that reason
    // -- the same precedent as Profile::GetNumWinsForGameMode.
    // ⚠️ DELIBERATE SHAPE DEVIATION, NAMED: the DWARF spells the index parameters of the last
    // five as BrnGameState::ETakedownType / BrnStreetData::ScoreType /
    // BrnGameState::StuntElementType / BrnWorld::ECounty. They are kept as the underlying s32
    // here so this header keeps pulling in no un-homed enum (the standing decision recorded in
    // this file's banner); every caller passes the enum, which converts implicitly.
    s32   GetValue(IntValueType leType) const   { return maIntValues[leType]; }
    CgsID GetValue(IdValueType leType) const    { return maIdValues[leType]; }
    f32   GetValue(FloatValueType leType) const { return maFloatValues[leType]; }

    void  SetValue(IntValueType leType, s32 liValue)   { maIntValues[leType]   = liValue; }
    void  SetValue(IdValueType leType, CgsID lValue)   { maIdValues[leType]    = lValue; }
    void  SetValue(FloatValueType leType, f32 lfValue) { maFloatValues[leType] = lfValue; }

    s32  GetTakedownTypeCount(s32 leTakedownType) const { return maTakedownTypeCounts[leTakedownType]; }
    void SetTakedownTypeCount(s32 leTakedownType, s32 liCount) { maTakedownTypeCounts[leTakedownType] = liCount; }

    s32  GetRoadsRuledCount(s32 leScoreType) const { return maRoadsRuledCounts[leScoreType]; }
    void SetRoadsRuledCount(s32 leScoreType, s32 liCount) { maRoadsRuledCounts[leScoreType] = liCount; }

    s32  GetTotalRoads() const        { return miTotalRoads; }
    void SetTotalRoads(s32 liRoads)   { miTotalRoads = liRoads; }

    s32  GetMaxStuntElementPerCounty(s32 leStuntElementType, s32 leCounty) const
    { return maaiMaxStuntElementsPerCounty[leStuntElementType][leCounty]; }
    void SetMaxStuntElementPerCounty(s32 leStuntElementType, s32 leCounty, s32 liCount)
    { maaiMaxStuntElementsPerCounty[leStuntElementType][leCounty] = liCount; }

    s32  GetCurrentStuntElementPerCounty(s32 leStuntElementType, s32 leCounty) const
    { return maaiCurrentStuntElementsPerCounty[leStuntElementType][leCounty]; }
    void SetCurrentStuntElementPerCounty(s32 leStuntElementType, s32 leCounty, s32 liCount)
    { maaiCurrentStuntElementsPerCounty[leStuntElementType][leCounty] = liCount; }

private:
    // DWARF BrnGameActionData.h:196-206 (CgsID == u64), with the X360's 33-int / 3-float split
    // (see the banner above).
    CgsID maIdValues[E_ID_VALUE_TYPE_COUNT];                          // +0x000
    s32   maIntValues[E_INT_VALUE_TYPE_COUNT];                        // +0x018
    f32   maFloatValues[E_FLOAT_VALUE_TYPE_COUNT];                    // +0x09C
    s32   maTakedownTypeCounts[KI_TAKEDOWN_TYPE_COUNT];               // +0x0A8
    s32   maRoadsRuledCounts[KI_SCORE_TYPE_COUNT];                    // +0x0DC
    s32   maaiMaxStuntElementsPerCounty[KI_STUNT_ELEMENT_TYPE_COUNT][KI_COUNTY_VALID_COUNT];     // +0x0E4
    s32   maaiCurrentStuntElementsPerCounty[KI_STUNT_ELEMENT_TYPE_COUNT][KI_COUNTY_VALID_COUNT]; // +0x120
    s32   miTotalRoads;                                               // +0x15C
};

// The case-79 post site (ProcessGameEvents @0x823A2D3C) queues this record with
// `li r5, 0xB4 / li r6, 0x160` -- action 180, exactly 352 bytes.
static_assert(sizeof(GameStats) == 352,
              "X360 ProcessGameEvents case 79 posts GameStats as 0x160 bytes");
}
}
