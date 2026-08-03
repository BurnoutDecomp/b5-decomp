// ===================================================================================
// BrnGui::TableRow / BrnGui::TableRowDataSet  -- implementation
//   class:BrnGui::TableRow   class:BrnGui::TableRowDataSet
//
//   TableRow::Construct                @0x824E6F00
//   TableRow::SetupRow                 @0x824E4BF8
//   TableRow::Clear                    @0x824E4D00
//   TableRow::HighlightNext            @0x824E4DE0
//   TableRow::FindState                @0x824E4EE8
//   TableRow::AppendExpectedAptComponent @0x824E4DF0
//   TableRowDataSet::Clear             @0x824E4F68
//
// Reconstructed store-for-store from the X360 ARTIST pseudocode/asm. Member access is BY
// NAME. The component flag byte the row dirties is the base SelectableGroup +0xC byte
// (muFlags); 0x10 is its "needs re-output" bit.
//
// ⚠️⚠️ 2026-08-03 -- THE FLAT-VTABLE DISPATCH IN THIS FILE WAS UNSOUND AND IS RETIRED,
// as it already was in BrnMenuToggleGroup.cpp and its sibling group TUs (2026-08-02). Every
// component virtual used to be reached through `mppVTable[slot]`. `SelectableGroup::
// mppVTable` is a MODELLED data member that NOTHING in this tree ever writes -- its own
// header says so, and BrnMenuComponent.cpp:28 explicitly stores nullptr into it -- so the
// first call through it would have jumped through a null pointer. This TU was unmounted,
// which is the only reason it never fired; it is mounted from today (BrnOnlineCustomMatch.h
// embeds a Table by value, so BrnScreenFlow needs the whole Table/TableRow/TableCell
// family), so the dispatches had to go before the mount, not after.
//
// Every target is known BY NAME. The slot map is the one resolved from .rdata for the
// sibling groups (BrnMenuToggleGroup.cpp:18-23): slots 0/1/2/3 == SetActive /
// SetHighlightable / SetSelectable / SetHighlighted, slot 6 == Clear. The two slot-6 sites
// dispatch on the ROW'S OWN vptr, so they land on TableRow::Clear, not the base:
//   * Construct @0x824E70EC  `bl SelectableGroup::Construct` then `lwz r11,0(r20)` /
//     `lwz r11,0x18(r11)` / `bctrl` with r3 = this and no second argument.
//   * SetupRow  @0x824E4C9C  the same pair, then slot 0 with r4 = 1.
//   * TableRow::Clear @0x824E4D00 is itself reached ONLY through a vtable -- the export's
//     xrefs_to is EMPTY -- and it opens with `bl BrnGui__SelectableGroup__Clear`, the
//     derived-override-calls-base shape. Its own four dispatches read 0x0/0x4/0xC/0x8 with
//     r4 = 0/1/0/1, i.e. SetActive(false) / SetHighlightable(true) / SetHighlighted(false)
//     / SetSelectable(true) -- exactly the four calls below, now by name.
//
// ⚠️ ONE LIVE FLAT DISPATCH SURVIVES IN THE TREE after this file:
// BrnOnlineTimeoutTimerComponent.cpp:23 still calls through its own `mppVTable[3]`. That TU
// is UNMOUNTED, which is the only thing keeping it inert -- de-flatten it BEFORE mounting it,
// exactly as was done here.
// ===================================================================================
#include "GameSource/Gui/Flow/Shared/Components/BrnTableRow.h"

#include "GameSource/Gui/BrnGuiCache.h"                       // BrnGui::GuiCache
#include "GameShared/GameClasses/Core/CgsStringUtils.h"       // CgsCore::SPrintf
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT

#include <cstring>   // std::strlen (cell-name length assert)

namespace BrnGui
{
    // ---------------------------------------------------------------------------------
    // The component "needs re-output" bit OR'd into the base flag byte (X360 `ori 0x10`).
    static const u8 KU_FLAG_DIRTY = 0x10;

    // @0x824E6F00 -----------------------------------------------------------------------
    void TableRow::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                             const char* lpacCellName, CgsGui::GuiComponent** lppComponents,
                             const TableCell::TableCellComponentTypes* lpeComponentTypes,
                             s32 liNumColumns, const char* lpacParentName, u64 luAptId)
    {
        // X360 assert order/messages (lines 63..66), each guarding the named argument:
        CGS_ASSERT(lpStateInterface != 0, "Invalid state interface passed"); // X360 line 63
        CGS_ASSERT(lpacName != 0, "Invalid name passed in");                 // X360 line 64
        CGS_ASSERT(lpacCellName != 0, "Invalid cell name passed in");        // X360 line 65
        CGS_ASSERT(liNumColumns <= 16,
                   "Too many colums in table row for current setting in SelectableGroup"); // line 66

        // Base group construct: (name, state interface, parent name, apt id).
        SelectableGroup::Construct(lpacName, lpStateInterface, lpacParentName, luAptId);

        // X360 @0x824E70EC: slot 6 on the row's own vptr, r3 = this, no second argument --
        // TableRow::Clear, dispatched immediately after the base construct.
        Clear();

        // Pre-clear the cell array: the X360 zeroes each cell's two data words
        // (meComponentType + mpComponent) inline before constructing the live cells; that is
        // exactly TableCell::Clear, de-inlined here.
        for (s32 liCell = 0; liCell < KI_MAX_CELLS_PER_ROW; ++liCell)
        {
            maCells[liCell].Clear();
        }

        miNumColumns = static_cast<s8>(liNumColumns);

        // Build the live cells. Each cell is named "<cellname>_<index>" and gets its
        // component pointer + component-type from the caller's parallel arrays; its parent
        // is the row's own name. The cell apt id seed is the X360's 0xFFFFFFFF.
        for (s32 liCell = 0; liCell < miNumColumns; ++liCell)
        {
            // The X360 asserts the formatted name fits the 64-byte cell-name buffer
            // before formatting: strlen(cellName)+4 (the "_<index>" suffix) must stay
            // under KU_MAX_NAME_LENGTH (BrnTableRow.cpp:82, fired every iteration).
            CGS_ASSERT(std::strlen(lpacCellName) + 4 < TableRowDataSet::KU_MAX_NAME_LENGTH,
                       "Name too long.");

            char lacCellName[TableRowDataSet::KU_MAX_NAME_LENGTH];
            CgsCore::SPrintf(lacCellName, TableRowDataSet::KU_MAX_NAME_LENGTH,
                             "%s_%d", lpacCellName, liCell);

            maCells[liCell].Construct(lacCellName, lpStateInterface,
                                      lppComponents[liCell], lpeComponentTypes[liCell],
                                      lpacName, static_cast<u64>(0xFFFFFFFFu));
        }

        mbEnableShowingAnim = false;                        // X360 stb 0, 0x301
        mePreviousState     = E_TABLEROWSTATES_INVISIBLE;   // X360 stw 1, 0x2FC
    }

    // @0x824E4BF8 -----------------------------------------------------------------------
    void TableRow::SetupRow(s32 liColumn, bool lbEnableShowingAnim)
    {
        CGS_ASSERT(liColumn >= 0 && liColumn <= miNumColumns,
                   "TableRow::SetupRow() too many columns!");   // X360 line 103

        // X360 @0x824E4C9C: slot 6 on the row's own vptr (TableRow::Clear), then slot 0 with
        // r4 = 1 (SelectableGroup::SetActive).
        Clear();
        SetActive(true);

        // Toggle the base wrap flag if it changed, dirtying the component when it does.
        if (lbEnableShowingAnim != mbWrapped)
        {
            mbWrapped = lbEnableShowingAnim;
            muFlags |= KU_FLAG_DIRTY;
        }

        // The X360 unconditionally re-raises the dirty bit on the way out.
        muFlags |= KU_FLAG_DIRTY;
    }

    // @0x824E4D00 -----------------------------------------------------------------------
    void TableRow::Clear()
    {
        SelectableGroup::Clear();

        // If the wrap flag was set, clear it and dirty the component.
        if (mbWrapped)
        {
            mbWrapped = false;
            muFlags |= KU_FLAG_DIRTY;
        }

        // Reset the row's runtime state (X360 0x824E4D4C..0x824E4D68):
        mu64SelectedId    = KU64_NO_ID;                 // std 0xFFFFFFFF, 0x10 (base id -> none)
        mpData            = 0;                           // stw 0, 0x2F8
        mePreviousState   = E_TABLEROWSTATES_INVISIBLE;  // stw 1, 0x2FC
        muFlags |= KU_FLAG_DIRTY;

        // X360 @0x824E4D40..0x824E4DB8: four component virtuals in this order -- slot 0
        // (r4=0), slot 1 (r4=1), slot 3 (r4=0), slot 2 (r4=1). By the .rdata slot map that is
        // SetActive(false) / SetHighlightable(true) / SetHighlighted(false) /
        // SetSelectable(true): the flag toggles that re-publish the cleared row to its clip.
        SetActive(false);
        SetHighlightable(true);
        SetHighlighted(false);
        SetSelectable(true);

        muFlags |= KU_FLAG_DIRTY;
    }

    // @0x824E4DE0 -----------------------------------------------------------------------
    bool TableRow::HighlightNext()
    {
        // Tail-call to the base with wrap = false (X360 `li r4,0 ; b SelectableGroup::HighlightNext`).
        return SelectableGroup::HighlightNext(false);
    }

    // @0x824E4EE8 -----------------------------------------------------------------------
    TableRow::TableRowStates TableRow::FindState() const
    {
        // Not enabled  -> INVISIBLE; not visible -> DISABLED.
        if ((muFlags & 0x01) == 0)
            return E_TABLEROWSTATES_INVISIBLE;
        if ((muFlags & 0x04) == 0)
            return E_TABLEROWSTATES_DISABLED;

        const bool lbHighlighted = (muFlags & 0x08) != 0;
        const bool lbPrevInvisible = (mePreviousState == E_TABLEROWSTATES_INVISIBLE);

        if (lbHighlighted)
        {
            // Highlighted: SHOWING_HIGHLIGHTED while transitioning in, else HIGHLIGHTED.
            if (lbPrevInvisible && mbEnableShowingAnim)
                return E_TABLEROWSTATES_SHOWING_HIGHLIGHTED;
            return E_TABLEROWSTATES_HIGHLIGHTED;
        }

        // Unhighlighted: SHOWING_UNHIGHLIGHTED while transitioning in, else UNHIGHLIGHTED.
        if (lbPrevInvisible && mbEnableShowingAnim)
            return E_TABLEROWSTATES_SHOWING_UNHIGHLIGHTED;
        return E_TABLEROWSTATES_UNHIGHLIGHTED;
    }

    // @0x824E4DF0 -----------------------------------------------------------------------
    void TableRow::AppendExpectedAptComponent(GuiFlow leFlow, GuiCache* lpGuiCache)
    {
        CGS_ASSERT(lpGuiCache != 0, "lpGuiCache");   // X360 line 234

        // Register the row's own apt component by name (X360 reads the row's name field and
        // forwards it to the cache's name-taking AppendExpectedAptComponent entry).
        lpGuiCache->AppendExpectedAptComponent(leFlow, GetName());

        // Then register each live cell's wrapped component by name.
        for (s32 liCell = 0; liCell < miNumColumns; ++liCell)
        {
            CGS_ASSERT(lpGuiCache != 0, "lpGuiCache");   // BrnTableCell.cpp:102

            CgsGui::GuiComponent* lpComponent = maCells[liCell].GetComponent();
            CGS_ASSERT(lpComponent != 0, "mpComponent");  // BrnTableCell.cpp:103

            lpGuiCache->AppendExpectedAptComponent(leFlow, lpComponent->GetName());
        }
    }

    // @0x82483288 -- return a pointer to the row's cell at liIndex (&maCells[liIndex]).
    // stride 0xC, base row+0x238. Bounds-checked 0 <= liIndex < 16.
    TableCell* TableRow::GetSelectable(s32 liIndex)
    {
        CGS_ASSERT(liIndex >= 0 && liIndex < KI_MAX_CELLS_PER_ROW,
                   "TableRow::GetSelectable() invalid index specified"); // X360 line 395

        return &maCells[liIndex];
    }

    // @0x824E4468 -- attach the row's backing data set and dirty the component.
    void TableRow::SetData(TableRowDataSet* lpData)
    {
        CGS_ASSERT(lpData != 0, "TableRow::SetData() Invalid DataSet"); // X360 line 271

        mpData = lpData;
        muFlags |= KU_FLAG_DIRTY;
    }

    // @0x824890E8 -- set a cell's text directly in the backing data set and dirty the component.
    void TableRow::SetText(s32 liCell, const char* lpacText)
    {
        CGS_ASSERT(mpData != 0, "TableRow::SetText() Row data not set"); // X360 line 302
        CGS_ASSERT(lpacText != 0, "invalid text specified");             // X360 line 303

        mpData->SetText(liCell, lpacText);

        muFlags |= KU_FLAG_DIRTY;
    }

    // @0x82489210 -- set a cell's colour: drive the cell's text-field colour and record the
    // value + "use colour" flag in the backing data set, then dirty the component.
    void TableRow::SetColourValue(s32 liColumn, s32 liColour)
    {
        CGS_ASSERT(mpData != 0, "TableRow::SetText() Row data not set"); // X360 line 320 (shared string)

        maCells[liColumn].SetColourValue(static_cast<u32>(liColour));
        mpData->SetColourValue(liColumn, liColour);
        mpData->SetUseColour(liColumn, 1);

        muFlags |= KU_FLAG_DIRTY;
    }

    // @0x82489300 -- set a cell's text from a localised string. Only valid on TextField cells
    // (asserts maCells[liColumn].meComponentType == TEXTFIELD, via the public IsText() accessor).
    // Formats the localised text into the cell's text field, then mirrors the resolved text
    // into the backing data set. (No dirty flag raised -- matches the X360.)
    void TableRow::SetLocalisedText(s32 liColumn, const char* lpacText,
                                    CgsLanguage::LanguageManager::ParameterFormatType leFormat,
                                    s32 liNumParams, const char* const* lppacParams,
                                    CgsLanguage::LanguageManager::ParameterFormatType* lpeParamFormats)
    {
        CGS_ASSERT(mpData != 0, "TableRow::SetLocalisedText() Row data not set"); // X360 line 345
        CGS_ASSERT(lpacText != 0, "invalid text specified");                       // X360 line 346
        CGS_ASSERT(maCells[liColumn].IsText(),
                   "Trying to set localised text to something that is not a textfield in TableRow::SetLocalisedText()"); // X360 line 347

        TableCell& lrCell = maCells[liColumn];
        lrCell.SetLocalisedText(lpacText, leFormat, liNumParams, lppacParams, lpeParamFormats);
        mpData->SetText(liColumn, lrCell.GetText());
    }

    // @0x824894C8 -- set a cell's icon state: record the integer state in the backing data
    // set (validated against the row's cells) and dirty the component.
    void TableRow::SetIconState(s32 liCell, u32 luState)
    {
        CGS_ASSERT(mpData != 0, "TableRow::SetText() Row data not set"); // X360 line 378 (shared string)
        CGS_ASSERT(GetSelectable(liCell) != 0, "Invalid selectable specified"); // X360 line 379

        mpData->SetInteger(liCell, static_cast<s32>(luState));

        muFlags |= KU_FLAG_DIRTY;
    }

    // @0x824E4F68 -----------------------------------------------------------------------
    // DWARF declares this void (BrnTableRow.h:198); the X360 r3=this is incidental.
    void TableRowDataSet::Clear()
    {
        for (s32 liCell = 0; liCell < KI_MAX_CELLS_PER_ROW; ++liCell)
        {
            // First word of each cell's value union -> -1 ("unset"); the per-cell side
            // arrays -> 0.
            maCellData[liCell].miInt = -1;
            maiCellInt[liCell]       = 0;
            mabCellFlags[liCell]     = 0;
        }
    }

    // @0x824E4518 -- return a pointer to the cell's text buffer (X360 liCell*64 ==
    // &maCellData[liCell]).
    const char* TableRowDataSet::GetText(s32 liCell) const
    {
        CGS_ASSERT(liCell >= 0 && liCell < KI_MAX_CELLS_PER_ROW,
                   "TableRowDataSet::GetText() invalid index specified"); // X360 line 439

        return maCellData[liCell].macText;
    }

    // @0x824E45C8 -- read the cell value as an int (through the CellData union's leading word).
    s32 TableRowDataSet::GetInteger(s32 liCell) const
    {
        CGS_ASSERT(liCell >= 0 && liCell < KI_MAX_CELLS_PER_ROW,
                   "TableRowDataSet::GetInteger() invalid index specified"); // X360 line 453

        return maCellData[liCell].miInt;
    }

    // @0x824E4678 -- read the per-cell secondary int word maiCellInt[liCell] (byte 0x400 +
    // liCell*4). NB: the X360 asserts with the GetInteger() message text (shared string).
    s32 TableRowDataSet::GetColourValue(s32 liCell) const
    {
        CGS_ASSERT(liCell >= 0 && liCell < KI_MAX_CELLS_PER_ROW,
                   "TableRowDataSet::GetInteger() invalid index specified"); // X360 line 467

        return maiCellInt[liCell];
    }

    // @0x824E4728 -- read the per-cell flag byte mabCellFlags[liCell] as the "use colour"
    // flag. NB: the X360 asserts with the GetInteger() message text (shared string).
    bool TableRowDataSet::GetUseColour(s32 liCell) const
    {
        CGS_ASSERT(liCell >= 0 && liCell < KI_MAX_CELLS_PER_ROW,
                   "TableRowDataSet::GetInteger() invalid index specified"); // X360 line 481

        return mabCellFlags[liCell] != 0;
    }

    // @0x82483340 -- format text into the cell's value buffer (&maCellData[liCell]).
    void TableRowDataSet::SetText(s32 liCell, const char* lpacText)
    {
        CGS_ASSERT(static_cast<u32>(liCell) < 16,
                   "TableRowDataSet::SetText() invalid index specified");   // X360 line 496
        CGS_ASSERT(lpacText != 0,
                   "TableRowDataSet::SetValue() invalid text specified");   // X360 line 497
        CGS_ASSERT(std::strlen(lpacText) < KU_MAX_NAME_LENGTH,
                   "TableRowDataSet::SetText() Text too long");             // X360 line 498

        CgsCore::SPrintf(maCellData[liCell].macText, KU_MAX_NAME_LENGTH, "%s", lpacText);
    }

    // @0x824834F0 -- store the cell value as an int (through the CellData union's leading word).
    void TableRowDataSet::SetInteger(s32 liCell, s32 liValue)
    {
        CGS_ASSERT(liCell >= 0 && liCell < KI_MAX_CELLS_PER_ROW,
                   "TableRowDataSet::SetInteger() invalid index specified"); // X360 line 513

        maCellData[liCell].miInt = liValue;
    }

    // @0x824835A0 -- store a colour value into the per-cell secondary int word maiCellInt[liCell]
    // (byte 0x400 + liCell*4).
    void TableRowDataSet::SetColourValue(s32 liCell, s32 liValue)
    {
        CGS_ASSERT(liCell >= 0 && liCell < KI_MAX_CELLS_PER_ROW,
                   "TableRowDataSet::SetColourValue() invalid index specified"); // X360 line 528

        maiCellInt[liCell] = liValue;
    }

    // @0x82483658 -- store the "use colour" flag byte mabCellFlags[liCell] (0x440 + liCell).
    void TableRowDataSet::SetUseColour(s32 liCell, u8 lbUseColour)
    {
        CGS_ASSERT(static_cast<u32>(liCell) < 16,
                   "TableRowDataSet::SetUseColour() invalid index specified");  // X360 line 542

        mabCellFlags[liCell] = lbUseColour;   // X360 stb value, 0x440(this + index)
    }
}
