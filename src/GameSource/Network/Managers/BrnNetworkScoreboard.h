// ---- GameSource/Network/Managers/BrnNetworkScoreboard.h ----
// The online-results scoreboard model: a fixed grid of typed columns and string-cell rows
// that the front-end leaderboard tables read. Layout (member names / types / order / enum
// values / sizes) recovered from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Network/Managers/BrnNetworkScoreboard.h) and gated
// against the X360 binary.
//
// Layout proof for the only binary-recovered member here, Scoreboard::GetRow @ 0x8240E9A8
// (called by BrnGui::LeaderboardTableComponent::SetCell):
//   * miNumberOfRows is read as a SIGN-EXTENDED BYTE at +0xB68 (lbz + extsb), confirming it is
//     an int8 at offset 2920.
//   * the returned row pointer is `this + 0x190 + liRowNumber * 0x137`, i.e. &maRows[liRowNumber]
//     with maRows starting at +0x190 (400) and a row stride of 0x137 (311).
// Both offsets fall out of the DWARF field order:
//   maColumns[10] : 10 * sizeof(ScoreboardColumn 40) == 400 (0x190)  -> maRows begins at +0x190
//   each ScoreboardRow : char[10][31] (310) + int8 (1) == 311 (0x137) -> the row stride
//   maRows[8] spans +0x190..+0xB48; macTitle[31] +0xB48; miNumberOfColumns +0xB67;
//   miNumberOfRows +0xB68 (matching the lbz/extsb load).
#pragma once

#include "types.hpp"

namespace BrnNetwork
{
    // DWARF BrnNetworkScoreboard.h:30/31
    const s32 KI_MAX_SCOREBOARD_COLUMNS = 10;
    const s32 KI_MAX_SCOREBOARD_ROWS    = 8;

    // DWARF BrnNetworkScoreboard.h:46 -- one typed column heading (sizeof 40: the EDataType enum
    // forces 4-byte alignment, so the int16/char[31]/int8 tail pads from 38 up to 40).
    struct ScoreboardColumn
    {
        // DWARF BrnNetworkScoreboard.h:50
        enum EDataType
        {
            E_DATATYPE_START      = 0,
            E_DATATYPE_NUMBER     = 0,
            E_DATATYPE_PERCENT    = 1,
            E_DATATYPE_NAME       = 2,
            E_DATATYPE_STRING     = 3,
            E_DATATYPE_CAR_CGS_ID = 4,
            E_DATATYPE_TIME       = 5,
            E_DATATYPE_DISTANCE   = 6,
            E_DATATYPE_CURRENCY   = 7,
            E_DATATYPE_COUNT      = 8,
        };

        void      Construct();                                            // own TU
        bool      Prepare(const char*, s32, s32, EDataType);              // own TU
        bool      Release();                                              // own TU
        void      Destruct();                                             // own TU
        void      Clear();                                                // own TU
        const char* GetTitle() const;                                     // own TU
        s32       GetWidth() const;                                       // own TU
        s32       GetStyle() const;                                       // own TU
        EDataType GetType() const;                                        // own TU

    private:
        EDataType meDataType;     // +0   DWARF :107
        s16       miWidth;        // +4   DWARF :108
        char      macTitle[31];   // +6   DWARF :111
        s8        miStyle;        // +37  DWARF :112
    };

    // DWARF BrnNetworkScoreboard.h:125 -- one row of up to 10 string cells (sizeof 311 == 0x137;
    // all-char members so alignment 1, no tail pad). This is the X360 row stride for GetRow.
    struct ScoreboardRow
    {
        void      Construct();                                            // own TU
        bool      Release();                                              // own TU
        void      Destruct();                                             // own TU
        void      Clear();                                                // own TU
        void      AddCell(const char*);                                   // own TU
        const char* GetData(s32) const;                                   // own TU

    private:
        char maacData[10][31];    // +0   DWARF :157  (10 cells x 31 chars == 310)
        s8   miNumberOfColumns;   // +310 DWARF :160
    };

    // DWARF BrnNetworkScoreboard.h:173 -- the scoreboard grid.
    struct Scoreboard
    {
        void      Construct();                                            // own TU
        bool      Prepare(const char*);                                   // own TU
        bool      Release();                                              // own TU
        void      Destruct();                                             // own TU
        s32       GetNumberOfRows() const;                                // own TU
        s32       GetNumberOfColumns() const;                             // own TU
        const ScoreboardColumn* GetColumn(s32) const;                     // own TU

        // === Reconstructed in this TU ===
        const ScoreboardRow*    GetRow(s32 liRowNumber) const;            // @ 0x8240E9A8

        const char* GetTitle() const;                                     // own TU
        void      AddRow(ScoreboardRow*);                                 // own TU
        void      AddColumn(ScoreboardColumn*);                           // own TU
        void      AddNumberBeforeAndAfter(s8, s8);                        // own TU
        s8        GetNumberBefore() const;                                // own TU
        s8        GetNumberAfter() const;                                 // own TU

    private:
        void      ClearAllScoreboardData();                               // own TU
        void      ClearAllRowData();                                      // own TU
        void      ClearAllColumnData();                                   // own TU

    private:
        ScoreboardColumn maColumns[KI_MAX_SCOREBOARD_COLUMNS]; // +0     DWARF :242 (10 x 40 == 0x190)
        ScoreboardRow    maRows[KI_MAX_SCOREBOARD_ROWS];       // +0x190 DWARF :245 (8 x 311)
        char             macTitle[31];                          // +0xB48 DWARF :247
        s8               miNumberOfColumns;                     // +0xB67 DWARF :250
        s8               miNumberOfRows;                         // +0xB68 DWARF :253 (lbz/extsb @0x8240E9E8)
        s8               miNumberOfRowsBefore;                   // +0xB69 DWARF :256
        s8               miNumberOfRowsAfter;                    // +0xB6A DWARF :259
    };
}
