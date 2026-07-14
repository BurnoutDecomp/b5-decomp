#include "GameSource/Gui/Flow/Shared/Components/BrnTable.h"

// BrnGui::TableDataSet - the row-data getter for the GUI table component.
// See BrnTable.h for the recovered layout and the X360 address.

namespace BrnGui
{
    // @0x824E47D8 - bounds-checked row-data fetch, returned by value (X360 lwzx of the
    // 4-byte element at maRowData[liRow]). The dual bound (liRow < 0 || liRow >= count)
    // is faithful: the X360 reads the count with lbz+extsb (a signed byte) and tests both
    // ends, firing "TableDataSet::GetRowData() invalid index specified" (BrnTable.h:477).
    u32 TableDataSet::GetRowData(s32 liRow) const
    {
        CGS_ASSERT(liRow >= 0 && liRow < miRowCount,
                   "TableDataSet::GetRowData() invalid index specified");
        return maRowData[liRow];
    }

    // @0x82500F40 -- default constructor. The X360 body writes the base SelectableGroup
    // vtable then the derived Table vtables, and default-constructs each of the 16 rows
    // (the per-row loop is the inlined TableRow default constructor: it writes the row's
    // two vtables and runs a 16-element `vector constructor iterator' over its TableCells,
    // 0xC bytes each). All of that is the C++ compiler-generated construction of the
    // by-value maRows[16] member array plus the implicit vptr stores, so the hand-written
    // body is empty; the members are primed by Construct/Clear. (DWARF attests an empty body.)
    Table::Table()
    {
    }

    // @0x82483708 -- fetch row liIndex as a TableRow*. The X360 bounds-checks the index
    // with a single UNSIGNED compare (`a2 >= 0x10`, i.e. liIndex outside [0, 16)), firing
    // "Table::GetSelectable() invalid index specified" (BrnTable.h:356), then forwards to
    // the base SelectableGroup::GetSelectable and returns its slot. TableRow derives from
    // SelectableGroup (not Selectable), so the base Selectable* is reinterpret_cast — a
    // static_cast would be ill-formed across the unrelated bases.
    TableRow* Table::GetSelectable(s32 liIndex)
    {
        CGS_ASSERT(static_cast<u32>(liIndex) < static_cast<u32>(KI_MAX_ROWS_PER_TABLE),
                   "Table::GetSelectable() invalid index specified");

        return reinterpret_cast<TableRow*>(SelectableGroup::GetSelectable(liIndex));
    }

    // @0x82489720 -- forward a colour value to a row's cell. Resolves the row via
    // GetSelectable(liRow), asserting it exists ("Invalid selectable specified",
    // BrnTable.h:285), then re-resolves it and forwards to TableRow::SetColourValue.
    void Table::SetColourValue(s32 liRow, s32 liColumn, s32 liColour)
    {
        CGS_ASSERT(GetSelectable(liRow) != 0, "Invalid selectable specified");

        GetSelectable(liRow)->SetColourValue(liColumn, liColour);
    }

    // @0x824897E0 -- forward a localised string to a row's cell. Asserts the row exists
    // ("Invalid selectable specified", BrnTable.h:310) and that a text string was supplied
    // ("Invalid text specified", BrnTable.h:311), re-resolves the row, then forwards to
    // TableRow::SetLocalisedText(column, text, format, numParams, params, paramFormats).
    void Table::SetLocalisedText(s32 liRow, s32 liColumn, const char* lpacText,
                                 CgsLanguage::LanguageManager::ParameterFormatType leFormat,
                                 s32 liNumParams, const char* const* lppacParams,
                                 CgsLanguage::LanguageManager::ParameterFormatType* lpeParamFormats)
    {
        CGS_ASSERT(GetSelectable(liRow) != 0, "Invalid selectable specified");
        CGS_ASSERT(lpacText != 0, "Invalid text specified");

        GetSelectable(liRow)->SetLocalisedText(liColumn, lpacText, leFormat,
                                               liNumParams, lppacParams, lpeParamFormats);
    }
}
