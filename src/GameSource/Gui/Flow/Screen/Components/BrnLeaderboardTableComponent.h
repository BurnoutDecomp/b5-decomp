#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"        // CgsGui::GuiComponent (base)
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"            // CgsLanguage::LanguageManager::ParameterFormatType
#include "GameSource/Network/Managers/BrnNetworkScoreboard.h"              // BrnNetwork::Scoreboard / ScoreboardColumn::EDataType (by-value member)
#include "GameSource/Gui/Flow/Screen/Components/BrnLeaderboardColumnComponent.h"  // BrnGui::LeaderboardColumnComponent (by-value array)

namespace BrnResource { struct VehicleList; }   // mpVehicleList (pointer only)

// BrnGui::LeaderboardTableComponent - the online-results leaderboard table: it owns a
// fixed row of LeaderboardColumnComponent children plus the BrnNetwork::Scoreboard model
// they draw from. SetupScoreboard adopts a scoreboard snapshot (or resets an empty one)
// and DrawScoreboard lays the used columns out symmetrically about the table centre,
// filling each column's cells from the scoreboard rows via the per-type cell formatter.
//
// Layout recovered from the DecFIGS DWARF (BrnLeaderboardTableComponent.h) and pinned to
// the X360 ARTIST asm (Construct @0x824190B0): the LeaderboardColumnComponent base ends at
// +0x8C, so maColumns[10] (stride 0x90) spans +0x8C..+0x62C; miColumnsUsed +0x62C,
// miRowsUsed +0x630, mScoreboard (2924 bytes) +0x634..+0x11A0, miHighlight +0x11A0,
// miLocalPlayer +0x11A1, mpVehicleList +0x11A4.
namespace BrnGui
{
    struct LeaderboardTableComponent : public CgsGui::GuiComponent
    {
        // DWARF BrnLeaderboardTableComponent.h:91.
        static const s32 KI_MAX_COLUMNS = 10;

        // @0x824190B0 (this TU, DWARF cpp:50) -- base Construct, then Construct each of the
        // ten child columns ("Column_<n>"), zero the used counts, reset the highlight/local
        // player to -1, and cache the vehicle list off the gui cache's world-data controller.
        virtual void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                               const char* lpacParentName);

        // @0x8243BBD0 (this TU, DWARF cpp:85) -- adopt lpScoreboard (or reset an empty one
        // when null), latch its column/row counts, clear the local-player highlight, and redraw.
        void SetupScoreboard(const BrnNetwork::Scoreboard* lpScoreboard);

        // @0x82436188 (this TU, DWARF cpp:147) -- push the whole table's apt view states.
        void DrawScoreboard();

        // The following accessors are X360-attested (DWARF h:129/136/143/150/157) but are
        // their own ledger functions -- declared here for the type home, bodied elsewhere.
        void SetHighlight(s8 liHighlight);
        s8   GetHighlight() const;
        s8   GetRowsUsed() const;
        s8   GetRowsBefore() const;
        s8   GetRowsAfter() const;

        // @0x82419368 (this TU) -- return the value in the currently-highlighted row's first
        // score-bearing column (a number/time/currency data type), parsed as an integer.
        s32  GetHighlightedScore() const;
        // @0x82419208 (foreign ledger TU) -- copy the highlighted row's name-column cell
        // string into the caller's 16-byte buffer (walks the used columns for the name data
        // type, asserting bounds); returns the buffer. Declared here for the type home; the
        // body links from its own TU.
        char* GetHighlightedGamertag(char* lpacPlayerName);

        // @0x82436580 (this TU) -- highlight the row whose name-column cell string-matches
        // lpacPlayerName (clearing the highlight when the name is empty, there is no name column,
        // or no row matches), then redraw.
        void SetTargetGamertag(const char* lpacPlayerName);

    private:
        // @0x824346C8 (this TU, DWARF cpp:231) -- format one cell (row,column) from the
        // scoreboard row data according to the column's data type and push it into the column.
        void SetCell(s32 liRow, s32 liColumn, BrnNetwork::ScoreboardColumn::EDataType leDataType);

        // @0x82426938 (this TU, DWARF cpp:335) -- register a unique localisation key
        // ("$LEADERBOARD_<row>_<col>") mapping to the formatter's output, then set the column
        // cell to that "$"-sigil reference so the display layer resolves it at render time.
        void LocaliseTextInCell(s32 liRow, s32 liColumn, const char* lpcData,
                                CgsLanguage::LanguageManager::ParameterFormatType leFormat);

        // DWARF cpp:25 -- the column-name format template ("Column_%d").
        static const char KAC_COLUMN_NAME_TEMPLATE[10];
        // DWARF cpp:26 -- the per-column position apt view-state names ("apt_Pos_<n>"). The
        // .data pointer table (@0x82F252D8) holds load-time relocations, so only entry 0's
        // string is directly attested by the IDA export; entries 1-9 are the indexed
        // continuation of the pattern. FLAG: entries 1-9 inferred by index.
        static const char* const KAPC_COLUMN_STRINGS[KI_MAX_COLUMNS];

        LeaderboardColumnComponent  maColumns[KI_MAX_COLUMNS];  // +0x8C   DWARF h:93
        s32                         miColumnsUsed;              // +0x62C  DWARF h:96
        s32                         miRowsUsed;                 // +0x630  DWARF h:97
        BrnNetwork::Scoreboard      mScoreboard;                // +0x634  DWARF h:100
        s8                          miHighlight;                // +0x11A0 DWARF h:102
        s8                          miLocalPlayer;              // +0x11A1 DWARF h:103
        const BrnResource::VehicleList* mpVehicleList;          // +0x11A4 DWARF h:105
    };
}
