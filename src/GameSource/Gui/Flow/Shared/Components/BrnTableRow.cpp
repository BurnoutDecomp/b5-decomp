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
// NAME. Where the X360 dispatches one of the component's still-unnamed virtuals, the call is
// routed through the modelled base vtable (SelectableGroup::mppVTable) with the X360 byte
// offset / slot index documented at the site -- the project's modelled-vtable convention
// (see BrnOnlineTimeoutTimerComponent). The component flag byte the row dirties is the base
// SelectableGroup +0xC byte (muFlags); 0x10 is its "needs re-output" bit.
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

    // Slot indices into the component vtable for the still-unnamed component virtuals the
    // row drives (X360 byte offsets: slot N == *(*this + 4*N)).
    namespace
    {
        typedef void (*ComponentToggleFn)(void* lpThis, s32 liArg);  // (this, bool/int)

        inline void DispatchToggle(SelectableGroup* lpThis, s32 liSlot, s32 liArg)
        {
            ComponentToggleFn lpfFn =
                reinterpret_cast<ComponentToggleFn>(lpThis->mppVTable[liSlot]);
            lpfFn(lpThis, liArg);
        }
    }

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

        // X360: (*(*this + 24))(this) -- vtable slot 6, the component's parameterless
        // re-init/clear virtual, dispatched immediately after the base construct.
        reinterpret_cast<void (*)(SelectableGroup*)>(mppVTable[6])(this);

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

        // X360: (*(*this + 24))(this) -- vtable slot 6 (parameterless re-init), then
        // (**this)(this, 1) -- vtable slot 0 with arg 1.
        reinterpret_cast<void (*)(SelectableGroup*)>(mppVTable[6])(this);
        DispatchToggle(this, 0, 1);

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

        // X360: dispatch four component virtuals through the vtable -- slot 0(this,0),
        // slot 1(this,1), slot 3(this,0), slot 2(this,1) -- the re-output / show / hide /
        // enable toggles that re-publish the cleared row to its apt clip.
        DispatchToggle(this, 0, 0);
        DispatchToggle(this, 1, 1);
        DispatchToggle(this, 3, 0);
        DispatchToggle(this, 2, 1);

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
}
