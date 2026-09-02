#include "GameSource/Gui/Flow/Shared/Components/BrnTable.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf (Construct's "%s_%d" row names)
#include <cstring>                                         // std::strlen (Construct's name-length tripwire)

// BrnGui::TableDataSet - the row-data getter for the GUI table component.
// See BrnTable.h for the recovered layout and the X360 address.

namespace BrnGui
{
    // @0x824E47D8 - bounds-checked row-data fetch. The X360 `lwzx` reads a 4-byte element
    // at mapRowData[liRow]; that element is a TableRowDataSet* (DWARF BrnTable.h:219/211),
    // and 4 is the CONSOLE pointer size -- it widens on the host, so the fetch is written
    // by name. The dual bound (liRow < 0 || liRow >= count) is faithful: the X360 reads the
    // count with lbz+extsb (a signed byte) and tests both ends, firing
    // "TableDataSet::GetRowData() invalid index specified" (assert line 477).
    TableRowDataSet* TableDataSet::GetRowData(s32 liRow)
    {
        CGS_ASSERT(liRow >= 0 && liRow < miNumRowDataSets,
                   "TableDataSet::GetRowData() invalid index specified");
        return mapRowData[liRow];
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

    // @0x824E4890 -- Table::Update (43 insns), the component-slot-5 override. Landed
    // 2026-09-02 with OnlineCustomMatch::Update, its first mounted caller.
    //   0x824E48A8  lbz 0xC ; rlwinm 0,27,27 ; xori 0x10 ; stb   -> clear the group's queried bit
    //   0x824E48C0  lwz 0x32B8 (mpData) == 0            -> return
    //   0x824E48CC  lbz 0x32BC (miNumRows), extsb <= 0  -> return
    //   0x824E48E0  per row (stride 0x308 from +0x238): lbz row+0xC & 0x10 -> row vtable
    //               slot 5 (row+0x0 -> +0x14) == the row's SelectableGroup::Update.
    // The loop counter is an s8 (`extsb r31`) compared against the s8 row count.
    void Table::Update()
    {
        if (IsQueried())
        {
            ClearQueriedFlag();
        }

        if (mpData != 0 && miNumRows > 0)
        {
            for (s8 liRow = 0; liRow < miNumRows; ++liRow)
            {
                TableRow& lrRow = maRows[liRow];
                if (lrRow.IsQueried())
                {
                    lrRow.Update();   // row component vtable slot 5 == SelectableGroup::Update
                }
            }
        }
    }

    // =====================================================================================
    // The rest of the Table / TableDataSet family, landed 2026-09-02 with the mount of the
    // online custom-match screen (the first mounted consumer of every one of them). Each
    // body is read off the X360 ARTIST assembly; the streamed asserts are lowered to
    // CGS_ASSERT with their static text, per the standing rule. Every one of the console's
    // asserts is NON-GATING (BeginAssert/FireAssert/EndAssert then fall through).
    // =====================================================================================

    // @0x824E6ED8 (10 insns) -- zero the count and the 16 slots.
    void TableDataSet::Construct()
    {
        miNumRowDataSets = 0;
        for (s32 li = 0; li < KI_MAX_DATASETS_PER_TABLE; ++li)
        {
            mapRowData[li] = 0;
        }
    }

    // @0x824E4AD0 (74 insns) -- append; both tripwires fall through, and the store is
    // UNCONDITIONAL (the console writes slot 16 on overflow; kept, not "fixed").
    bool TableDataSet::AddRowData(TableRowDataSet* lpRowData)
    {
        CGS_ASSERT(lpRowData != 0, "TableDataSet::AddRowData() Invalid dataset");          // cpp:346
        CGS_ASSERT(miNumRowDataSets < KI_MAX_DATASETS_PER_TABLE,
                   "TableDataSet::AddRowData() No room for any more row datasets!");        // cpp:347

        mapRowData[miNumRowDataSets] = lpRowData;
        miNumRowDataSets = static_cast<s8>(miNumRowDataSets + 1);
        return true;   // `li r3, 1`
    }

    // Header-inline on the console (the `lbz 0x40` every caller emits).
    s32 TableDataSet::GetNumRowDataSets() const
    {
        return miNumRowDataSets;
    }

    // @0x824E96E8 (183 insns) -- build the table: three parameter tripwires, the base group,
    // slot-6 Clear, the three counters, then one TableRow::Construct per row named
    // "<name>_<row>" with the row's id preset to its index and its queried bit raised.
    //   r4 lpacName  r5 lpStateInterface  r6 lpacCellName  r7 lappCellComponents (+0x40/row)
    //   r8 lpeComponentTypes  r9 liNumRows  r10 liNumColumns  stack lpacParentName / luAptId
    //   TableRow::Construct gets (name, iface, cellName, components[row], types,
    //   numColumns, parentName, KU64_NO_ID) -- the apt id it passes is the `li -1 ; clrldi 32`
    //   at 0x824E98BC, NOT the table's own.
    void Table::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                          const char* lpacCellName,
                          CgsGui::GuiComponent* (*lappCellComponents)[16],
                          const TableCell::TableCellComponentTypes* lpeComponentTypes,
                          s32 liNumRows, s32 liNumColumns,
                          const char* lpacParentName, u64 luAptId)
    {
        CGS_ASSERT(lpStateInterface != 0, "Invalid stat interface passed");                // cpp:51
        CGS_ASSERT(lpacName != 0, "Invalid name passed in");                               // cpp:52
        CGS_ASSERT(liNumRows <= KI_MAX_ROWS_PER_TABLE,
                   "Too many rows in table for current setting in SelectableGroup");       // cpp:54

        SelectableGroup::Construct(lpacName, lpStateInterface, lpacParentName, luAptId);
        Clear();                                                       // slot 6 -> Table::Clear

        miNumColumns        = static_cast<s8>(liNumColumns);           // stb 0x32BD
        mbEnableShowingAnim = false;                                   // stb 0, 0x32BF
        miNumRows           = static_cast<s8>(liNumRows);              // stb 0x32BC (extsb)

        for (s8 liRow = 0; liRow < miNumRows; ++liRow)
        {
            // 0x824E98C4..0x824E98F0: strlen of the GROUP's stored name (+0x1C) + 4 must
            // fit the 64-byte row-name buffer.
            CGS_ASSERT(std::strlen(GetName()) + 4 < TableRowDataSet::KU_MAX_NAME_LENGTH,
                       "Name too long.");                                                  // cpp:69

            char lacRowName[TableRowDataSet::KU_MAX_NAME_LENGTH];
            CgsCore::SPrintf(lacRowName, TableRowDataSet::KU_MAX_NAME_LENGTH, "%s_%d",
                             lpacName, static_cast<s32>(liRow));

            TableRow& lrRow = maRows[liRow];
            lrRow.muFlags          = static_cast<u8>(lrRow.muFlags | KU_FLAG_QUERIED);   // 0x824E9970
            lrRow.mu64SelectedId   = static_cast<u64>(static_cast<s64>(liRow));            // std extsw(row), +0x10

            lrRow.Construct(lacRowName, lpStateInterface, lpacCellName,
                            lappCellComponents[liRow], lpeComponentTypes,
                            liNumColumns, lpacParentName, KU64_NO_ID);
        }
    }

    // @0x824E4940 (34 insns) -- see the header note.
    void Table::Clear()
    {
        SelectableGroup::Clear();

        const bool lbWasWrapped = mbWrapped;    // read BEFORE the resets (lbz 0xA6 first)
        miFirstRowDataSet = -1;                 // stb -1, 0x32BE
        mpData            = 0;                  // stw 0,  0x32B8
        if (lbWasWrapped)
        {
            mbWrapped = false;
            muFlags   = static_cast<u8>(muFlags | KU_FLAG_QUERIED);
        }

        for (s8 liRow = 0; liRow < miNumRows; ++liRow)
        {
            maRows[liRow].Clear();              // row slot 6 == TableRow::Clear
        }
    }

    // @0x824E6A90 (113 insns) -- see the header note.
    void Table::SetupTable(TableDataSet* lpData, bool lbWrapped, bool lbEnableShowingAnim)
    {
        CGS_ASSERT(lpData != 0, "Table::SetupTable() Invalid DataSet passed in");           // cpp:92

        Clear();                                                       // slot 6 -> Table::Clear
        miFirstRowDataSet = 0;
        mpData            = lpData;

        // `lbz 0x40(data) ; cmpw ; ble` -- clamp the row count down to the data set's.
        if (lpData->GetNumRowDataSets() <= miNumRows)
        {
            miNumRows = static_cast<s8>(lpData->GetNumRowDataSets());
        }

        for (s8 liRow = 0; liRow < miNumRows; ++liRow)
        {
            TableRow& lrRow = maRows[liRow];
            if (liRow >= miNumRows)
            {
                // The console's own dead arm (the loop bound is the same byte): row slot 0
                // with r4 = 0 == SetActive(false). Kept as shipped.
                lrRow.SetActive(false);
            }
            else
            {
                lrRow.SetupRow(miNumColumns, lbEnableShowingAnim);
                lrRow.SetEnableShowingAnim(mbEnableShowingAnim);       // stb 0x32BF -> row+0x301
                // slot 7 on the table == SelectableGroup::Add. The row IS the group's item on
                // the console (TableRow derives from SelectableGroup, whose head is a
                // Selectable) -- the same reinterpret the committed Table::GetSelectable uses
                // on the way back out.
                Add(reinterpret_cast<Selectable*>(&lrRow));
            }
        }

        if (miNumRows > 0)
        {
            HighlightIndex(0);                                         // slot 12, r4 = 0
        }

        if (lbWrapped != mbWrapped)
        {
            mbWrapped = lbWrapped;
            muFlags   = static_cast<u8>(muFlags | KU_FLAG_QUERIED);
        }

        SetRowData();
        muFlags = static_cast<u8>(muFlags | KU_FLAG_QUERIED);
    }

    // @0x824E49C8 (66 insns) -- see the header note.
    void Table::SetRowData()
    {
        CGS_ASSERT(mpData != 0, "Table::SetRowData() Table data not set");                  // cpp:270

        for (s8 liRow = 0; liRow < miNumRows; ++liRow)
        {
            // The bound is re-read from mpData every iteration (`lwz 0x32B8` in the loop).
            if (miFirstRowDataSet + liRow < mpData->GetNumRowDataSets())
            {
                maRows[liRow].SetData(mpData->GetRowData(miFirstRowDataSet + liRow));
            }
        }

        muFlags = static_cast<u8>(muFlags | KU_FLAG_QUERIED);
    }

    // @0x824E6C58 (70 insns) -- move the highlight down; when the cursor sits on the last
    // visible row, scroll the data-set window instead (or wrap it to the top when wrapping
    // is on and the window is at the end).
    bool Table::HighlightNext()
    {
        CGS_ASSERT(mpData != 0, "Table::HighlightNext() Row data not set");                 // cpp:193

        if (miHighlightedIndex == miNumRows - 1)
        {
            if (miFirstRowDataSet + miNumRows >= mpData->GetNumRowDataSets())
            {
                if (mbWrapped)
                {
                    miFirstRowDataSet = 0;
                    SetRowData();
                }
                return SelectableGroup::HighlightNext(false);
            }

            miFirstRowDataSet = static_cast<s8>(miFirstRowDataSet + 1);
            SetRowData();
            return true;
        }

        return SelectableGroup::HighlightNext(false);
    }

    // @0x824E6D70 (66 insns) -- the mirror: on the first visible row scroll the window up,
    // or wrap it to the end.
    bool Table::HighlightPrevious()
    {
        CGS_ASSERT(mpData != 0, "Table::HighlightPrevious() Row data not set");             // cpp:233

        if (miHighlightedIndex == 0)
        {
            if (miFirstRowDataSet <= 0)
            {
                if (mbWrapped)
                {
                    miFirstRowDataSet = static_cast<s8>(mpData->GetNumRowDataSets() - miNumRows);
                    SetRowData();
                }
                return SelectableGroup::HighlightPrevious(false);
            }

            miFirstRowDataSet = static_cast<s8>(miFirstRowDataSet - 1);
            SetRowData();
            return true;
        }

        return SelectableGroup::HighlightPrevious(false);
    }

    // @0x824895F8 (73 insns) -- assert the row exists and the text is non-null, then forward.
    void Table::SetText(s32 liRow, s32 liColumn, const char* lpacText)
    {
        CGS_ASSERT(GetSelectable(liRow) != 0, "Invalid selectable specified");             // h:268
        CGS_ASSERT(lpacText != 0, "Invalid text specified");                                // h:269

        GetSelectable(liRow)->SetText(liColumn, lpacText);
    }

    // @0x82489928 (48 insns) -- assert the row exists, then forward.
    void Table::SetIconState(s32 liRow, s32 liColumn, u32 luState)
    {
        CGS_ASSERT(GetSelectable(liRow) != 0, "Invalid selectable specified");             // h:341

        GetSelectable(liRow)->SetIconState(liColumn, luState);
    }
}
