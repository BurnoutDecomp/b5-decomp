#pragma once

// ===================================================================================
// BrnGui::TableDataSet  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Shared/Components/BrnTable.h
//
// A small fixed-capacity row table used by the GUI table component (BrnGui::Table).
// It holds up to 16 row-data words and a live row count, and hands a single row back
// by index with a bounds check.
//
// Layout/behaviour proven from BURNOUT_X360_ARTIST.XEX:
//   * GetRowData @0x824E47D8 - bounds-checks the index against the row count
//       (`liRow < 0 || liRow >= miRowCount`; the count is read with `lbz`+`extsb`, i.e.
//       a SIGNED BYTE at this+0x40), firing
//       "TableDataSet::GetRowData() invalid index specified" (BrnTable.h:477) on failure,
//       then returns the row word by VALUE: `slwi r11, idx, 2` / `lwzx r3, r11, this`
//       == maRowData[liRow]. The element stride is 4 bytes and the element buffer sits at
//       this+0x00, so the 16-word buffer (0x00..0x3F) is followed by the count byte at +0x40.
//   * Called by BrnGui::Table::SetRowData.
//
// All access is by name. sizeof is at least 0x44 (count byte at +0x40, word-aligned to +0x44).
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (GetRowData bounds check)
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectableGroup.h"  // SelectableGroup (base)
#include "GameSource/Gui/Flow/Shared/Components/BrnTableRow.h"         // TableRow (by value)

namespace BrnGui
{
    class TableDataSet
    {
    public:
        // Capacity of the inline row-data buffer. The 4-byte element stride and the count
        // byte landing at this+0x40 fix the buffer at 16 words (0x40 / 4).
        static const s32 KI_MAX_ROWS = 16;

        // @0x824E47D8 - return the row word at liRow by value. Asserts the index is in
        // [0, miRowCount) before returning maRowData[liRow]. The X360 read both the lower
        // (< 0) and upper (>= count) bounds, so the index is taken as signed here.
        // Defined out-of-line in BrnTable.cpp.
        u32 GetRowData(s32 liRow) const;

    private:
        u32 maRowData[KI_MAX_ROWS];   // +0x00 .. +0x3F (16 row words)
        s8  miRowCount;               // +0x40 (signed-byte live row count)
    };

    // ===================================================================================
    // BrnGui::Table  -- the GUI table component (a SelectableGroup of up to 16 TableRows).
    //
    // Layout DWARF-authoritative (references/DecFIGS/dwarfdump/.../BrnTable.h -> struct
    // Table : public SelectableGroup, members maRows[16]/mpData/miNumRows/miNumColumns/
    // miFirstRowDataSet/mbEnableShowingAnim), gated on the X360 ARTIST asm:
    //   Table : public SelectableGroup
    //     maRows[16]          this+0x238 (16 * sizeof(TableRow)==0x308 each, by value)
    //     mpData              TableDataSet*   (backing data-set table)
    //     miNumRows           s8
    //     miNumColumns        s8
    //     miFirstRowDataSet   s8
    //     mbEnableShowingAnim bool
    // Ctor @0x82500F40 default-constructs maRows[16] (each row default-constructs its 16
    // TableCells via a `vector constructor iterator'); the members are primed by
    // Construct/Clear. Update/Clear/HighlightNext/HighlightPrevious are virtual overrides
    // (DWARF BrnTable.cpp:135/166/191/231), so the compiler emits the vptr stores.
    //
    // GetSelectable returns TableRow* by reinterpret_cast-ing the base group's Selectable*
    // (TableRow derives from SelectableGroup, NOT Selectable, so a static_cast is ill-formed).
    // Its body is NOT homed in this wave (verify-FAIL sibling); it is declared here because
    // SetColourValue / SetLocalisedText call it. Body lands in a later wave.
    // ===================================================================================
    class Table : public SelectableGroup
    {
    public:
        // BrnTable.h:56 (DWARF) -- max rows in a table.
        static const s8 KI_MAX_ROWS_PER_TABLE = 16;

        // @0x82500F40 -- default-construct the 16 rows; members primed by Construct/Clear.
        Table();

        // @0x82483708 -- &maRows[liIndex] via the base group; asserts 0 <= liIndex < 16.
        // (Declared only in this wave; body not yet homed.)
        TableRow* GetSelectable(s32 liIndex);
        const TableRow* GetSelectable(s32 liIndex) const;   // DWARF BrnTable.h:422

        // @0x82489720 -- forward a colour value to row liRow's column liColumn.
        void SetColourValue(s32 liRow, s32 liColumn, s32 liColour);

        // @0x824897E0 -- forward a localised string to row liRow's column liColumn.
        void SetLocalisedText(s32 liRow, s32 liColumn, const char* lpacText,
                              CgsLanguage::LanguageManager::ParameterFormatType leFormat,
                              s32 liNumParams, const char* const* lppacParams,
                              CgsLanguage::LanguageManager::ParameterFormatType* lpeParamFormats);

        // (Other DWARF-declared members -- Construct/SetupTable/HasData/GetText/SetText/
        //  GetIconState/SetIconState/SelectRow/Update/Clear/HighlightNext/HighlightPrevious/
        //  AppendExpectedAptComponent/SetEnableShowingAnim/SetRowData -- land in later waves.)

    private:
        TableRow      maRows[KI_MAX_ROWS_PER_TABLE];  // this+0x238 (by value, 0x308 bytes each)
        TableDataSet* mpData;                         // backing data-set table
        s8            miNumRows;
        s8            miNumColumns;
        s8            miFirstRowDataSet;
        bool          mbEnableShowingAnim;
    };
}
