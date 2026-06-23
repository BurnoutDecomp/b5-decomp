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
}
