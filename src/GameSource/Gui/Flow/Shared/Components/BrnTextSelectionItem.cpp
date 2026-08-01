// ===================================================================================
// BrnGui::TextSelectionItem  -- implementation
//   class:BrnGui::TextSelectionItem
//
//   TextSelectionItem() @ 0x824F2120 -- installs the class vtable at this+0 and returns.
//   Select()                         -- the EMPTY override at vtable slot 4 (see below).
//   SetText(const char*)             -- X360-inlined into TextSelection::SetItemText.
// Reconstructed store-for-store from the X360 asm.
// ===================================================================================
#include "GameSource/Gui/Flow/Shared/Components/BrnTextSelectionItem.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnGui
{
    // @ 0x824F2120
    // The X360 ctor materialises this class's vtable (off_8207182C) and stores it at this+0
    // (lis/addi off_8207182C; stw r11,0(r3); blr); it stores nothing else. Because
    // TextSelectionItem is polymorphic (it overrides Selectable::Select -- DWARF
    // BrnTextSelectionItem.h:96), the compiler emits the vtable store implicitly, so the C++
    // body is empty. off_8207182C is the per-row selection-item vtable; TextSelection embeds
    // these rows (0x18 stride at +0x238, ctor @0x824F9A48).
    TextSelectionItem::TextSelectionItem()
    {
    }

    // Vtable slot 4 of off_8207182C -- this class's own vtable, read out of the image with
    // `scratchpad/rd.py` (calibration delta -1594) -- holds 0x8284CB38, which disassembles
    // to a single `blr` and carries 193 xrefs: the image-wide ICF fold of the empty body,
    // and demonstrably NOT _purecall. The row therefore overrides Selectable::Select with a
    // DELIBERATELY EMPTY body -- selecting a text row must not run the Selectable base
    // default. The surrounding slots of the same vtable are the real
    // Selectable::SetActive/SetHighlightable/SetSelectable/SetHighlighted/Update addresses,
    // so the read is calibrated and this body is the recovered console behaviour.
    void TextSelectionItem::Select()
    {
    }

    // BrnTextSelectionItem.cpp:65 -- adopt the row's text and dirty the row.
    // Recovered from the X360's inlined copy inside BrnGui::TextSelection::SetItemText
    // @0x824E8278: the `lpcText` assert (this file, line 65), then
    //   *(item + 0x08) = lpcText     -- Selectable::mpcSelectionText
    //   *(item + 0x0C) |= 0x10       -- Selectable::E_FLAG_DIRTY
    void TextSelectionItem::SetText(const char* lpcText)
    {
        CGS_ASSERT(lpcText != 0, "lpcText");   // BrnTextSelectionItem.cpp:65

        mpcSelectionText = lpcText;
        SetDirty();
    }
}
