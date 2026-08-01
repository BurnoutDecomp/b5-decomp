// ===================================================================================
// BrnGui::TextSelection  -- implementation
//   class:BrnGui::TextSelection
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   TextSelection (ctor)  @ 0x824F9A48
//   Construct             @ 0x824E8128   (DWARF BrnTextSelection.cpp:42)
//   SetItemText           @ 0x824E8278   (DWARF BrnTextSelection.cpp:125)
//   Update                @ 0x824E83A0   (DWARF BrnTextSelection.cpp:143 -- vtable slot 5)
//   SetupTextSelection    @ 0x824E9C40   (DWARF BrnTextSelection.cpp:64)
//
// The widget is a BrnGui::SelectableGroup of 100 BrnGui::TextSelectionItem rows plus one
// BrnGui::TextField that shows whichever row is highlighted; see BrnTextSelection.h for the
// offset proof and the vtable dump. Every member is reached BY NAME (the x64 gate widens the
// group's Selectable* array and the row vptrs, so the console offsets are documentary).
//
// The X360 reaches the base + row methods through the component vtable by SLOT; the slot map
// (BrnSelectableGroup.h, re-validated against off_82073020 for this class) is quoted at each
// dispatch so the by-name calls below are checkable against the asm:
//   group   5 Update   7 Add   12 HighlightIndex
//   row     0 SetActive  1 SetHighlightable  2 SetSelectable  3 SetHighlighted
// ===================================================================================
#include "GameSource/Gui/Flow/Shared/Components/BrnTextSelection.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnGui
{
    namespace
    {
        // The pooled empty string @0x820046A7 -- SetupTextSelection's fallback row text.
        const char KAC_EMPTY_TEXT[] = "";
    }

    // ---- TextSelection (ctor) @ 0x824F9A48 -----------------------------------------
    // The X360 ctor stores FOUR kinds of vtable and nothing else: the widget's own
    // (off_82073020 @+0x000), the GuiComponent sub-object's (off_8207301C @+0x018), the 100
    // row vtables (off_8207182C from +0x238 at a 0x18 stride) and the display field's
    // (off_82072F8C @+0xB98). In C++ every one of those is emitted implicitly -- by this
    // object's own construction, by the SelectableGroup base's embedded GuiComponent, by the
    // 100 TextSelectionItem default ctors and by the TextField member -- so the body carries
    // only the one store the model cannot emit implicitly. In particular the console does
    // NOT clear the row table or the group state here; SelectableGroup::Construct's Clear()
    // does that, and a "helpful" zero-fill (which the retired shell did) would diverge.
    TextSelection::TextSelection()
    {
        // BrnSelectableGroup.h keeps the group non-polymorphic and models the console's
        // component vtable as the `mppVTable` data pointer, so the ctor's `stw off_82073020,
        // 0(r3)` has no implicit counterpart. Null it, exactly as the committed sibling
        // BrnGui::MenuComponent does (BrnMenuComponent.cpp:28) -- nothing dispatches a
        // TextSelection through mppVTable (see the header note), and leaving it
        // indeterminate is how a stale pointer gets called.
        mppVTable = 0;
    }

    // ---- Construct @ 0x824E8128 ----------------------------------------------------
    void TextSelection::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                                  const char* lpacParentName, u64 luAptId)
    {
        // Both X360 asserts are the streamed-message form and are non-gating.
        CGS_ASSERT(lpacName != 0, "Invalid name");                     // cpp:44
        CGS_ASSERT(lpStateInterface != 0, "Invalid state interface");   // cpp:45

        SelectableGroup::Construct(lpacName, lpStateInterface, lpacParentName, luAptId);

        // X360: `lwz r11, 0xB98(this) / lwz r11, 0(r11) / bctrl` -- the field's vtable slot 0,
        // i.e. TextField::Construct(name, stateInterface, parentName). The display field
        // takes the SAME component name as the group.
        mTextField.Construct(lpacName, lpStateInterface, lpacParentName);

        // X360: `li r11, 1 / stb r11, 0xCBE(this)` == mTextField + 0x126 == mbAutosize.
        mTextField.SetAutoSize(true);
    }

    // ---- SetupTextSelection @ 0x824E9C40 -------------------------------------------
    // Note the loop runs over ALL 100 rows, not just the requested count: rows past
    // liNumberOfItems are explicitly deactivated and blanked, and EVERY row -- active or not
    // -- is registered with the group. lppacItemTexts may be null, and any individual entry
    // may be null; both fall back to the pooled empty string.
    void TextSelection::SetupTextSelection(s32 liNumberOfItems, bool lbWrapped,
                                           const char** lppacItemTexts, u64* lpaIds)
    {
        CGS_ASSERT(liNumberOfItems <= KI_MAX_ITEMS_PER_SELECTION,
                   "Too many selection items");   // cpp:66

        // Only touch the wrap flag (and dirty the widget) when it actually changes.
        if (lbWrapped != mbWrapped)
        {
            mbWrapped = lbWrapped;
            muFlags   = static_cast<u8>(muFlags | KU_FLAG_QUERIED);
        }

        for (s32 liRow = 0; liRow < KI_MAX_ITEMS_PER_SELECTION; ++liRow)
        {
            TextSelectionItem& lrItem = maSelectionItems[liRow];

            lrItem.SetHighlighted(false);            // row slot 3 -- unconditional

            const bool lbInUse = (liRow < liNumberOfItems);
            lrItem.SetActive(lbInUse);               // row slot 0
            lrItem.SetHighlightable(lbInUse);        // row slot 1
            lrItem.SetSelectable(lbInUse);           // row slot 2

            const char* lpacText = KAC_EMPTY_TEXT;
            if (lbInUse && lppacItemTexts != 0 && lppacItemTexts[liRow] != 0)
            {
                lpacText = lppacItemTexts[liRow];
            }
            SetItemText(liRow, lpacText);

            Add(&lrItem);                            // group slot 7
        }

        SelectableGroup::SetIds(lpaIds);

        if (liNumberOfItems > 0)
        {
            HighlightIndex(0);                       // group slot 12
        }

        muFlags = static_cast<u8>(muFlags | KU_FLAG_QUERIED);
        Update();                                    // group slot 5 -- this class's override
    }

    // ---- SetItemText @ 0x824E8278 --------------------------------------------------
    // Both asserts are non-gating on the console (the stores below run regardless). The
    // third assert the X360 emits here belongs to the INLINED BrnGui::TextSelectionItem::
    // SetText -- its message file/line is BrnTextSelectionItem.cpp:65 -- so the call is
    // spelled out as that method rather than as two raw member stores.
    void TextSelection::SetItemText(s32 liIndex, const char* lpacItemText)
    {
        CGS_ASSERT(lpacItemText != 0, "lpItemText");   // cpp:127
        // X360 `cmpwi r27, 0 / blt` + `cmpwi r27, 0x64` -- a SIGNED two-sided bound.
        CGS_ASSERT(liIndex >= 0 && liIndex < KI_MAX_ITEMS_PER_SELECTION,
                   "Invalid index. Setup the TextSelection first.");   // cpp:128

        maSelectionItems[liIndex].SetText(lpacItemText);

        muFlags = static_cast<u8>(muFlags | KU_FLAG_QUERIED);
    }

    // ---- Update @ 0x824E83A0 -- component vtable slot 5 ----------------------------
    // Dirty-gated: push the highlighted row's text into the display field, then run the base
    // group update. When nothing is highlighted the field is blanked with the pooled empty
    // string. miHighlightedIndex is compared as a SIGNED byte (X360 `extsb / cmpwi -1`).
    void TextSelection::Update()
    {
        if ((muFlags & KU_FLAG_QUERIED) == 0)
            return;

        const char* lpacText = KAC_EMPTY_TEXT;

        if (miHighlightedIndex > -1)
        {
            Selectable* lpCurrentlySelected = GetHighlighted();
            CGS_ASSERT(lpCurrentlySelected != 0, "lpCurrentlySelected");   // cpp:153

            // X360 `lwz r4, 8(r30)` -- Selectable::mpcSelectionText, read through the row
            // type that owns the accessor (DWARF BrnTextSelectionItem.h:83 GetText()).
            lpacText = static_cast<TextSelectionItem*>(lpCurrentlySelected)->GetText();
        }

        mTextField.SetText(lpacText);

        SelectableGroup::Update();
    }
}
