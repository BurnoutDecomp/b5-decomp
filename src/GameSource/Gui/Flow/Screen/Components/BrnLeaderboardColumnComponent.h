#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"   // CgsGui::GuiComponent (base)

// BrnGui::LeaderboardColumnComponent - one column of the leaderboard table: it drives
// the column's apt view states (title, highlight/you rows, optional half-width flag,
// then up to eight per-cell texts in order). DWARF home
// BrnLeaderboardColumnComponent.h:43. This TU bodies Construct/Init/SetCell;
// FinaliseColumn (DWARF cpp:130) is its own ledger function (declaration-only here).
namespace BrnGui
{
    struct LeaderboardTableComponent;   // owns the columns by value and drives their cell cursor

    struct LeaderboardColumnComponent : public CgsGui::GuiComponent
    {
        // The owning table lays its columns out directly: DrawScoreboard reads each used
        // column's miCellCount (the "apt_CellsUsed" count) and zeroes it when blanking an
        // unused column. The X360 accesses the field inline from the table body, so the
        // table is a friend rather than going through an accessor.
        friend struct LeaderboardTableComponent;

        // DWARF BrnLeaderboardColumnComponent.h:72.
        static const s8 KI_MAX_CELLS = 8;

        // @0x824194C8 (this TU, DWARF cpp:54) -- base Construct + reset the cell cursor.
        virtual void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                               const char* lpacParentName);

        // @0x82419500 (this TU, DWARF cpp:75) -- push the column header view states.
        void Init(const char* lpcTitle, s8 liHighlightRow, s8 liYouRow, s32 liHalfWidth);

        // @0x824195E8 (this TU, DWARF cpp:111) -- push the next cell's text (bounded by
        // KI_MAX_CELLS) and advance the cursor.
        void SetCell(const char* lpcCellText);

        // DWARF cpp:130 -- declaration-only (its own ledger function).
        void FinaliseColumn();

    private:
        // DWARF cpp:21 -- the per-cell apt view-state names ("apt_Cell_<n>"). The .data
        // pointer table (@0x82F25300) holds load-time relocations, so only entry 0's
        // string is directly attested by the IDA export; the remaining entries are the
        // indexed continuation of that pattern. FLAG: entries 1-7 inferred by index.
        static const char* const KAPC_CELL_STRINGS[KI_MAX_CELLS];

        s8 miCellCount;   // DWARF h:76 (X360 +0x8C, right after the GuiComponent base)
    };
}
