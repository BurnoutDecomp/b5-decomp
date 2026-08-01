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
//
// VTABLE READ OUT OF THE IMAGE (2026-08-02, `scratchpad/rd.py`, calibration delta -1594).
// off_8207182C is, slot for slot:
//   +0x00 0x824E56E0 Selectable::SetActive       +0x04 0x824E5730 Selectable::SetHighlightable
//   +0x08 0x824E5780 Selectable::SetSelectable   +0x0C 0x824E57D0 Selectable::SetHighlighted
//   +0x10 0x8284CB38 <-- TextSelectionItem::Select
//   +0x14 0x8240FC88 Selectable::Update
// 0x8284CB38 is the image-wide ICF fold of the empty body (a single `blr`, 193 xrefs) --
// NOT _purecall. So TextSelectionItem::Select() is an EMPTY FUNCTION: the row overrides
// Select precisely to swallow the Selectable base default. The .cpp body is that empty
// function as RECOVERED console behaviour. (The same fold backs CarSelectMain's four empty
// event handlers, proven the same way from off_82075470.)
//
// SetText / GetText are the X360-inlined per-item accessors: TextSelection::SetItemText
// @0x824E8278 inlines SetText -- its assert bakes ".../BrnTextSelectionItem.cpp", line 65
// -- and TextSelection::Update @0x824E83A0 inlines GetText (`lwz r4, 8(r30)`).
// Construct stays a separate ledger function (declare-only; GROW this home when its body
// lands, do NOT fork the type). Mirrors the committed sibling BrnGui::ColourSelectionItem.
//
// The whole surface below is DWARF-attested (references/DecFIGS/dwarfdump/GameSource/Gui/
// Flow/Shared/Components/BrnTextSelectionItem.h): `struct TextSelectionItem : public
// BrnGui::Selectable` with Construct (h:52), SetText (h:57), GetText (h:83) and the single
// virtual Select (h:96). It declares NO data members of its own -- the row's text/state
// live in the Selectable base -- which is why 100 rows stride by exactly 0x18 inside
// BrnGui::TextSelection.
// ===================================================================================

#include "types.hpp"
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectable.h"     // BrnGui::Selectable (base)

namespace CgsGui { struct StateInterface; }

namespace BrnGui
{
    struct TextSelectionItem : public Selectable
    {
        // @ 0x824F2120 -- installs this class's vtable (off_8207182C) at this+0 (implicit,
        // because the class is polymorphic via the Select() override). Stores nothing else.
        TextSelectionItem();

        // DWARF h:96. Overrides Selectable::Select. PROVEN EMPTY -- see the vtable dump above.
        virtual void Select();

        // DWARF h:57 / cpp:63 -- adopt the row's text and dirty the row. The X360 inlines
        // this into BrnGui::TextSelection::SetItemText @0x824E8278, which reproduces its
        // `lpcText` assert (BrnTextSelectionItem.cpp:65), stores the pointer at item+0x08
        // (Selectable::mpcSelectionText) and ORs 0x10 (E_FLAG_DIRTY) into item+0x0C.
        void SetText(const char* lpcText);

        // DWARF h:83 -- the row's current text (Selectable::mpcSelectionText @+0x08).
        // Header-inline: every X360 call site reads the member directly (e.g.
        // TextSelection::Update's `lwz r4, 8(r30)`).
        const char* GetText() const { return mpcSelectionText; }

        // DWARF h:52 / cpp:42 -- name + wire the row. DECLARED ONLY: the X360 emits no
        // symbol for it (every call site inlines it) and no reconstructed caller reaches it
        // yet, so it grows a body when a TU that needs it lands. Do NOT fork the type.
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpacParentName, u64 luId);
    };
}
