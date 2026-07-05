#pragma once

// ===================================================================================
// BrnGui::TableRow / BrnGui::TableRowDataSet  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Shared/Components/BrnTableRow.h
//   class:BrnGui::TableRow   class:BrnGui::TableRowDataSet
//
// A row of a GUI table: a SelectableGroup whose selectables are an array of up to 16
// TableCell children (each wrapping a text-field or icon component). TableRowDataSet is the
// row's backing data record (16 cells of either text or an integer).
//
// CLASS SHAPE (DecFIGS DWARF GameSource/Gui/Flow/Shared/Components/BrnTableRow.h, gated on
// the X360 ledger). TableRow : public SelectableGroup; layout proven from the X360 ARTIST
// asm (Construct @0x824E6F00, Clear @0x824E4D00, FindState @0x824E4EE8). Member byte offsets
// are guest references; the gate compiles 64-bit, so every member is accessed BY NAME:
//   maCells[16]        row+0x238 .. +0x2F7  (16 TableCell, 0xC bytes each -- by-value)
//   mpData             row+0x2F8            (backing TableRowDataSet*)
//   mePreviousState    row+0x2FC            (last computed TableRowStates; init INVISIBLE)
//   miNumColumns       row+0x300            (s8 live cell count; Construct stores a7)
//   mbEnableShowingAnim row+0x301           (FindState reads it for the SHOWING states)
// (The wrap flag the row toggles lives in the base as SelectableGroup::mbWrapped @+0xA6.)
//
// TableRowDataSet layout proven from Clear @0x824E4F68:
//   maCellData[16]     +0x000 .. +0x3FF  (16 x 64-byte CellData unions; first word set -1)
//   maiCellInt[16]     +0x400 .. +0x43F  (per-cell secondary int word, cleared to 0)
//   mabCellFlags[16]   +0x440 .. +0x44F  (per-cell flag byte, cleared to 0)
//
// macAptStateText / KAC_STATE_NAMES are FILE-STATIC scratch/data in BrnTableRow.cpp
// (DWARF "extern ... BrnTableRow.cpp:24/35"), NOT members -- they are not declared here.
// ===================================================================================

#include "types.hpp"
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectableGroup.h"   // base (by inheritance)
#include "GameSource/Gui/Flow/Shared/Components/BrnTableCell.h"         // BrnGui::TableCell (by value)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"     // CgsGui::StateInterface / GuiComponent
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                         // BrnGui::GuiFlow

namespace BrnGui
{
    class GuiCache;

    // The row's backing data: 16 cells, each either text or an integer.
    struct TableRowDataSet
    {
        static const s32 KI_MAX_CELLS_PER_ROW = 16;
        static const u8  KU_MAX_NAME_LENGTH   = 64;

        // One cell's value: text OR an integer (DWARF BrnTableRow.h:191).
        union CellData
        {
            char    macText[KU_MAX_NAME_LENGTH];   // 64 bytes
            s32     miInt;
        };

        // @0x824E4F68 -- reset every cell: set the value to "unset" (first word -1) and clear
        // the two per-cell side arrays. (DWARF declares void; BrnTableRow.h:198.)
        void Clear();

        const char* GetText(s32 liCell) const;
        s32  GetInteger(s32 liCell) const;
        s32  GetColourValue(s32 liCell) const;   // reads maiCellInt[liCell]   (@0x824E4678)
        bool GetUseColour(s32 liCell) const;     // reads mabCellFlags[liCell] (@0x824E4728)
        void SetText(s32 liCell, const char* lpacText);
        void SetInteger(s32 liCell, s32 liValue);
        void SetColourValue(s32 liCell, s32 liValue);   // writes maiCellInt[liCell]   (@0x824835A0)
        void SetUseColour(s32 liCell, u8 lbUseColour);  // writes mabCellFlags[liCell] (@0x82483658)

    private:
        CellData maCellData[KI_MAX_CELLS_PER_ROW];   // +0x000 (each 64 bytes)
        s32      maiCellInt[KI_MAX_CELLS_PER_ROW];   // +0x400 (per-cell secondary int)
        u8       mabCellFlags[KI_MAX_CELLS_PER_ROW]; // +0x440 (per-cell flag byte)
    };

    class TableRow : public SelectableGroup
    {
    public:
        static const u8 KU_MAX_NAME_LENGTH    = 64;
        static const s8 KI_MAX_CELLS_PER_ROW  = 16;

        // The row's display state, computed by FindState from its flags (DWARF :149).
        enum TableRowStates
        {
            E_TABLEROWSTATES_UNUSED                = 0,
            E_TABLEROWSTATES_INVISIBLE             = 1,
            E_TABLEROWSTATES_DISABLED              = 2,
            E_TABLEROWSTATES_UNHIGHLIGHTED         = 3,
            E_TABLEROWSTATES_HIGHLIGHTED           = 4,
            E_TABLEROWSTATES_SHOWING_HIGHLIGHTED   = 5,
            E_TABLEROWSTATES_SHOWING_UNHIGHLIGHTED = 6,
            E_TABLEROWSTATES_COUNT                 = 7,
        };

        // @0x824E6F00 -- build the row: validate its arguments, run the base Construct, clear
        // the cell array, then build miNumColumns cells (names formatted "<cellname>_<idx>").
        // PARAM ORDER/ROLES are taken from the X360 ASM (the DWARF labels positions 3 and 7
        // "parentName"/"cellName" but the asm asserts position 3 as the cell name -- "Invalid
        // cell name passed in" -- and forwards position 7 to the base Construct as parentName,
        // so the two are named here by their true asm role).
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpacCellName, CgsGui::GuiComponent** lppComponents,
                       const TableCell::TableCellComponentTypes* lpeComponentTypes,
                       s32 liNumColumns, const char* lpacParentName, u64 luAptId);

        // @0x824E4BF8 -- (re)show the row at a column count, optionally enabling its showing
        // animation. Asserts 0 <= liColumn <= miNumColumns.
        void SetupRow(s32 liColumn, bool lbEnableShowingAnim);

        // @0x824E4D00 -- reset the row to its empty/invisible state.
        void Clear();

        // @0x824E4DE0 -- highlight the next selectable (no wrap): forwards to the base.
        bool HighlightNext();
        bool HighlightPrevious();

        void Select();
        void Update();

        // @0x824E4DF0 -- register the row's apt component, and each cell's component, as
        // expected on the given flow layer.
        void AppendExpectedAptComponent(GuiFlow leFlow, GuiCache* lpGuiCache);

        void SetData(TableRowDataSet* lpData);
        const char* GetText(s32 liCell) const;
        void SetText(s32 liCell, const char* lpacText);
        u32  GetIconState(s32 liCell) const;
        void SetIconState(s32 liCell, u32 luState);

        // @0x82483288 -- &maCells[liIndex]; bounds-checked 0 <= liIndex < 16. Used by SetIconState.
        TableCell* GetSelectable(s32 liIndex);
        // @0x82489210 -- set a cell's colour value + use-colour flag, then dirty.
        void SetColourValue(s32 liColumn, s32 liColour);
        // @0x82489300 -- set a TextField cell's text from a localised string (asserts IsText()).
        void SetLocalisedText(s32 liColumn, const char* lpacText,
                              CgsLanguage::LanguageManager::ParameterFormatType leFormat,
                              s32 liNumParams, const char* const* lppacParams,
                              CgsLanguage::LanguageManager::ParameterFormatType* lpeParamFormats);

        void SetEnableShowingAnim(bool lbEnable);

    private:
        // @0x824E4EE8 -- derive the row's display state from its component flags + previous
        // state + showing-anim flag.
        TableRowStates FindState() const;

        TableCell        maCells[16];          // row+0x238 (by value, 0xC bytes each)
        TableRowDataSet* mpData;               // row+0x2F8
        TableRowStates   mePreviousState;      // row+0x2FC
        s8               miNumColumns;         // row+0x300
        bool             mbEnableShowingAnim;  // row+0x301
    };
}
