#pragma once

// ===================================================================================
// BrnGui::TableDataSet  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Shared/Components/BrnTable.h
//
// A small fixed-capacity row table used by the GUI table component (BrnGui::Table).
// It holds up to 16 TableRowDataSet POINTERS and a live count, and hands a single row
// back by index with a bounds check.
//
// Shape DWARF-authoritative (references/DecFIGS/dwarfdump/.../BrnTable.h, "struct
// BrnGui::TableDataSet", BrnTable.h:192-220):
//     private: TableRowDataSet* mapRowData[16];  s8 miNumRowDataSets;
//     public:  Construct / Clear / AddRowData / GetRowData / GetNumRowDataSets
//
// Layout/behaviour proven from BURNOUT_X360_ARTIST.XEX:
//   * Construct @0x824E6ED8 - stores 0 into the count byte at this+0x40 and then zeroes
//       16 consecutive 4-byte slots from this+0x00 (`li r10, 0x10` / `stw r9, 0(r11)` /
//       `addi r11, r11, 4`), which is what fixes the capacity at 16 and the element at
//       one CONSOLE POINTER wide.
//   * AddRowData @0x824E4AD0 - asserts the incoming pointer is non-null
//       ("TableDataSet::AddRowData() Invalid data...") and that the count is < 0x10
//       ("TableDataSet::AddRowData() No room for ..."), then
//       `slwi r11, count, 2` / `stwx r22, r11, this` == mapRowData[count] = lpRowData,
//       bumps the count byte and returns 1 unconditionally (`li r3, 1`).
//   * GetRowData @0x824E47D8 - bounds-checks the index against the count
//       (`liRow < 0 || liRow >= miNumRowDataSets`; the count is read with `lbz`+`extsb`,
//       i.e. a SIGNED BYTE at this+0x40), firing
//       "TableDataSet::GetRowData() invalid index specified" (assert line 0x1DD == 477)
//       on failure, then `slwi r11, idx, 2` / `lwzx r3, r11, this` == mapRowData[liRow].
//   * Called by BrnGui::Table::SetRowData; the count is read directly (inlined
//       GetNumRowDataSets) by Table::SetupTable/HighlightNext/HighlightPrevious.
//
// The 4-byte element stride is the CONSOLE pointer size and is documentation only: on the
// LLP64 host every element widens to 8 bytes, so the count no longer lands at +0x40.
// All access is by name; never transplant these console offsets into host arithmetic.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (GetRowData bounds check)
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectableGroup.h"  // SelectableGroup (base)
#include "GameSource/Gui/Flow/Shared/Components/BrnTableRow.h"         // TableRow (by value) / TableRowDataSet
#include "GameSource/Gui/Flow/Shared/Components/BrnTableCell.h"        // TableCell::TableCellComponentTypes (Construct)

namespace BrnGui
{
    class TableDataSet
    {
    public:
        // DWARF BrnTable.h:195 -- capacity of the row-data slot array. Confirmed by
        // Construct's 16-iteration clear loop and by AddRowData's `cmpwi count, 0x10`.
        static const s32 KI_MAX_DATASETS_PER_TABLE = 16;

        // @0x824E6ED8 -- clear the count and null every slot. Defined in BrnTable.cpp's TU.
        void Construct();

        // DWARF BrnTable.h:203. No out-of-line X360 symbol exists (it is inlined at every
        // call site), so this is a declaration only; body lands with the rest of the TU.
        void Clear();

        // @0x824E4AD0 -- append lpRowData to the next free slot and bump the count.
        // Asserts the pointer is non-null and that there is room. Returns true (the X360
        // returns a constant 1; the assert arms fall through rather than returning false).
        bool AddRowData(TableRowDataSet* lpRowData);

        // @0x824E47D8 -- return the row-data pointer at liRow. Asserts the index is in
        // [0, miNumRowDataSets) before returning mapRowData[liRow]. The X360 reads both the
        // lower (< 0) and upper (>= count) bounds, so the index is taken as signed here.
        // NOT const: DWARF BrnTable.h:211 declares it without the cv-qualifier (contrast
        // GetNumRowDataSets/GetSelectable, which the same dump does mark const).
        // Defined out-of-line in BrnTable.cpp.
        TableRowDataSet* GetRowData(s32 liRow);

        // DWARF BrnTable.h:215 (rendered `const int32_t GetNumRowDataSets() const` -- the
        // top-level const on the returned value is dropped here, it is not part of the
        // signature). Inlined at every X360 call site (e.g. `lbz r9, 0x40(r9)` inside
        // Table::HighlightNext @0x824E6D14), so there is no out-of-line symbol to cite.
        s32 GetNumRowDataSets() const;

    private:
        // CONSOLE OFFSETS BELOW ARE DOCUMENTATION ONLY -- the elements are pointers and
        // widen on the host, so nothing here may be used as a host byte offset.
        TableRowDataSet* mapRowData[KI_MAX_DATASETS_PER_TABLE];  // console +0x00..+0x3F (DWARF :219)
        s8               miNumRowDataSets;                       // console +0x40 (signed byte, DWARF :220)
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
    // ON THE CONSOLE (DWARF BrnTable.cpp:135/166/191/231), so the original emitted vptr
    // stores. They are declared PLAIN here, exactly as the base SelectableGroup declares
    // its own dispatchable methods (the project's flat-vtable convention): adding `virtual`
    // to this class would introduce a host vptr the base does not have. Measured console
    // slots are named per method below.
    //
    // Console member offsets confirmed by Construct @0x824E96E8 / SetupTable @0x824E6A90:
    //   maRows base +0x238 (stride 0x308), mpData +0x32B8, miNumRows +0x32BC,
    //   miNumColumns +0x32BD, miFirstRowDataSet +0x32BE, mbEnableShowingAnim +0x32BF.
    //   DOCUMENTATION ONLY -- TableRow itself contains pointers, so its host size is not
    //   0x308 and none of these console offsets may enter host arithmetic.
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

        // @0x824E96E8 (DWARF BrnTable.cpp:49) -- build the table. Measured argument roles,
        // register by register, from the X360 body:
        //   r4 lpacName          asserted non-null ("Invalid name passed in"), forwarded to
        //                        SelectableGroup::Construct and used as the "<name>_<row>"
        //                        row-name stem (`"%s_%d"` @0x824E9954).
        //   r5 lpStateInterface  asserted non-null ("Invalid stat interface passed" -- sic).
        //   r6 lpacCellName      stashed and forwarded to TableRow::Construct as ITS cell
        //                        name (position 3), matching BrnTableRow.h's asm-derived
        //                        parameter roles.
        //   r7 lappCellComponents  advanced by 0x40 == 16 CONSOLE pointers per row
        //                        (`addi r26, r26, 0x40` @0x824E99AC) and handed to
        //                        TableRow::Construct as its flat GuiComponent** row. That
        //                        0x40 stride is the SECOND dimension of the caller's
        //                        [16][16] matrix, which is why this is a pointer-to-array
        //                        of 16 and not a GuiComponent***. DWARF renders the
        //                        parameter `CgsGui::GuiComponent * *[16]`; the pointer-to-
        //                        array spelling is the form that actually binds the call.
        //                        The 16 here is a HOST array dimension, not a byte stride.
        //   r8 lpeComponentTypes forwarded to TableRow::Construct unchanged.
        //   r9 liNumRows         asserted <= 16 ("Too many rows in table for current
        //                        settings"), then `extsb` into miNumRows (+0x32BC).
        //   r10 liNumColumns     stored into miNumColumns (+0x32BD).
        //   stack lpacParentName / luAptId  passed straight through to
        //                        SelectableGroup::Construct (the apt id is loaded with a
        //                        64-bit `ld` @0x824E9868, hence u64).
        // mbEnableShowingAnim (+0x32BF) is cleared here.
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpacCellName,
                       CgsGui::GuiComponent* (*lappCellComponents)[16],
                       const TableCell::TableCellComponentTypes* lpeComponentTypes,
                       s32 liNumRows, s32 liNumColumns,
                       const char* lpacParentName, u64 luAptId);

        // @0x824E6A90 (DWARF BrnTable.cpp:87) -- bind the row-data set and re-show the rows.
        // Asserts lpData ("Table::SetupTable() Invalid DataSet pas..."), clears
        // miFirstRowDataSet, stores lpData into mpData, clamps miNumRows down to
        // lpData->GetNumRowDataSets() when that is smaller, then SetupRow()s each row.
        //   lbWrapped            written to the base group's mbWrapped byte (+0xA6) with the
        //                        queried/dirty flag, i.e. SelectableGroup's wrap-around flag.
        //   lbEnableShowingAnim  forwarded verbatim as TableRow::SetupRow's second argument
        //                        (`mr r5, r25` @0x824E6B94); named after that parameter, it
        //                        is not separately attested here.
        void SetupTable(TableDataSet* lpData, bool lbWrapped, bool lbEnableShowingAnim);

        // DWARF BrnTable.h:233. Header-inline on the console (no out-of-line symbol); it is
        // `mpData != 0` -- e.g. inlined at 0x824A3568 as a load of mTable+0x32B8 tested
        // against zero. Declared non-const, as DWARF has it.
        bool HasData();

        // @0x824895F8 (DWARF BrnTable.h:262) -- assert the row exists ("Invalid selectable
        // specified") and lpacText is non-null ("Invalid text specified"), then
        // GetSelectable(liRow)->SetText(liColumn, lpacText).
        void SetText(s32 liRow, s32 liColumn, const char* lpacText);

        // @0x82489928 (DWARF BrnTable.h:318) -- assert the row exists, then
        // GetSelectable(liRow)->SetIconState(liColumn, luState). u32 matches both the DWARF
        // parameter and TableRow::SetIconState (BrnTableRow.h:126).
        void SetIconState(s32 liRow, s32 liColumn, u32 luState);

        // @0x824E6C58 / @0x824E6D70 (DWARF BrnTable.cpp:191/231; console vtable slots +0x34
        // and +0x38 of off_820747D4). Both take NO argument -- the X360 entry points read
        // only r3 -- and both scroll the data window (miFirstRowDataSet) when the highlight
        // is already at the end, then tail-call the base SelectableGroup::HighlightNext/
        // HighlightPrevious with lbQuiet == false. Both assert mpData is bound
        // ("Table::HighlightNext() Row data not set" / "...HighlightPrevious()...").
        // NOTE: these HIDE the base SelectableGroup::HighlightNext(bool)/HighlightPrevious(bool)
        // for Table objects, which is the console behaviour (different vtable slots).
        bool HighlightNext();
        bool HighlightPrevious();

        // (Other DWARF-declared members -- GetText/GetIconState/SelectRow/Update/Clear/
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
