#pragma once

// ===================================================================================
// BrnGui::TextSelectionItem  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Shared/Components/BrnTextSelectionItem.h
//
// A selectable text row in a list/menu GUI (the per-row item TextSelection embeds at a
// 0x18 stride). It carries no data beyond its BrnGui::Selectable base -- the per-item
// text/state live in Selectable (mpcSelectionText / mxFlags / mId) -- and overrides
// BrnGui::Selectable::Select. Its own vtable (X360 off_8207182C) is installed at this+0
// by the ctor (the polymorphic Select() override makes the compiler emit that store).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   BrnGui::TextSelectionItem::TextSelectionItem  @ 0x824F2120
// SetText / GetText / Construct / Select are separate ledger functions (declare-only
// here; bodies land with their own TUs -- GROW this home then, do NOT fork the type).
// Mirrors the committed sibling BrnGui::ColourSelectionItem.
// ===================================================================================

#include "types.hpp"
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectable.h"     // BrnGui::Selectable (base)

namespace BrnGui
{
    struct TextSelectionItem : public Selectable
    {
        // @ 0x824F2120 -- installs this class's vtable (off_8207182C) at this+0 (implicit,
        // because the class is polymorphic via the Select() override). Stores nothing else.
        TextSelectionItem();

        // Overrides Selectable::Select (the sole observable virtual this item adds). Body owned
        // by its own recon pass; declare-only here so the class is polymorphic (vtable install).
        virtual void Select();
    };
}
