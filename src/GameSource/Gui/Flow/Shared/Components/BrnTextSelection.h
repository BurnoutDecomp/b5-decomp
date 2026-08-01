#pragma once

// ===================================================================================
// BrnGui::TextSelection  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Shared/Components/BrnTextSelection.h
//
// A reusable text-selection widget: a BrnGui::SelectableGroup of up to 100 text rows
// (BrnGui::TextSelectionItem) plus one BrnGui::TextField that displays whichever row is
// currently highlighted. Embedded by value inside five screen states -- CarSelectVehicle,
// CarSelectLivery, CrashNavColourCalibrate, OnlineViewChallenges, OnlineGameRoomPlayerInfo
// (the five callers of the ctor in the X360 image).
//
// 2026-08-02 -- THIS CLASS WAS A HOLLOW SHELL AND IS NOW RE-HOMED.
// The previous revision declared `struct TextSelection` with NO base, ONE virtual
// destructor, no methods and a 0xB9C-byte reserved blob whose note claimed "the widget's
// sub-object types are not individually recoverable from this single ctor". All four
// claims were wrong, and the shell hid four real bodies plus its base class. The shape
// falls straight out of the ctor's own stores, and the DecFIGS DWARF for this exact header
// (references/DecFIGS/dwarfdump/.../BrnTextSelection.h) confirms every part of it.
//
//   ctor @0x824F9A48                       what it stores                    => member
//   stw off_82073020, 0x000(r3)            the widget's OWN vtable           => the group head
//   stw off_8207301C, 0x018(r3)            the GuiComponent sub-vtable       => SelectableGroup::mGuiComponentBase (+0x18)
//   100 x stw off_8207182C @ +0x238, +0x18 the TextSelectionItem row vtables => maSelectionItems[100] (+0x238)
//   stw off_82072F8C, 0xB98(r3)            the TextField vtable              => mTextField (+0xB98)
//
// and the offsets close EXACTLY, with nothing left over:
//   SelectableGroup::maSelectables[100] sits at +0xA8 and is 100 * 4 == 0x190 bytes, so the
//   group ends at +0x238 -- precisely where the row table starts. 100 rows * 0x18 (a bare
//   BrnGui::Selectable) == 0x960, so the rows end at +0xB98 -- precisely where the TextField
//   starts. sizeof(TextField) == 0x128 with its last member (mbAutosize) at +0x126, and
//   TextSelection::Construct's closing store is `stb 1, 0xCBE(this)` == mTextField + 0x126.
//
// The base is proved independently of the layout: TextSelection::Construct @0x824E8128
// calls `BrnGui::SelectableGroup::Construct(this, ...)` on THIS pointer unadjusted, and
// TextSelection::Update @0x824E83A0 tail-calls `BrnGui::SelectableGroup::Update(this)`.
//
// X360 VTABLE (off_82073020), read out of the image (`scratchpad/rd.py`, delta -1594):
//   slot  0 0x824E56E0 Selectable::SetActive          1 0x824E5730 Selectable::SetHighlightable
//        2 0x824E5780 Selectable::SetSelectable       3 0x824E57D0 Selectable::SetHighlighted
//        4 0x824E3EC8 SelectableGroup::Select         5 0x824E83A0 *TextSelection::Update*
//        6 0x824E3100 SelectableGroup::Clear          7 0x824E32F8 SelectableGroup::Add
//        8 0x824E3490 SelectableGroup::Enable         9 0x824E3620 SelectableGroup::Disable
//       10 0x824E3808 SelectableGroup::HighlightNext 11 0x824E3A78 SelectableGroup::HighlightPrevious
//       12 0x824E3CE0 SelectableGroup::HighlightIndex
// Update at slot 5 is the class's ONLY override; every other slot is the base's. This also
// independently re-validates the whole slot map documented in BrnSelectableGroup.h.
//
// ⚠️ Update() is declared NON-virtual here even though the DecFIGS DWARF attests it
// `virtual void Update()` (BrnTextSelection.cpp:143) and the vtable above shows it really
// does occupy the base's slot 5. That is a KNOWN, BOUNDED divergence, not an oversight:
// BrnSelectableGroup.h deliberately keeps the group NON-POLYMORPHIC and models the
// console's component vtable as the flat `mppVTable` data pointer, so no member of this
// family can be a true override until that family-wide model changes (which would insert a
// vptr into MenuComponent / MenuToggleGroup / Table / TableRow as well -- the live title
// menu -- and is therefore its own change). It is behaviourally inert here because BOTH
// X360 dispatch sites reach Update through the CONCRETE type:
//   * SetupTextSelection's own closing `(*(*this + 20))(this)`;
//   * CarSelectVehicle::UpdateComponents' `(*(*(this + 0x8F0) + 20))(this + 0x8F0)` on its
//     TextSelection-typed member mCarSelector.
// No call site anywhere holds a SelectableGroup* to a TextSelection. The committed siblings
// MenuComponent / MenuToggleGroup / TableRow already hide Clear()/GetSelectable()/Update()
// the same way.
// ===================================================================================

#include "types.hpp"
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectableGroup.h"     // BrnGui::SelectableGroup (base)
#include "GameSource/Gui/Flow/Shared/Components/BrnTextSelectionItem.h"   // BrnGui::TextSelectionItem (rows)
#include "GameSource/Gui/BrnGuiTextField.h"                               // BrnGui::TextField (by value)

namespace CgsGui { struct StateInterface; }

namespace BrnGui
{
    struct TextSelection : public SelectableGroup
    {
        // DWARF BrnTextSelection.h:80. Also the X360 ctor's loop count (r11 = 0x63 .. -1)
        // and SetupTextSelection's capacity assert bound.
        static const s32 KI_MAX_ITEMS_PER_SELECTION = 100;

        // @ 0x824F9A48 -- vtable-install-only ctor: the widget's own vtable at +0x000, the
        // GuiComponent sub-object vtable at +0x018, the 100 row vtables at +0x238 (stride
        // 0x18) and the TextField vtable at +0xB98. It stores NOTHING else -- in particular
        // it does not clear the row table (SelectableGroup::Construct's Clear() does that).
        TextSelection();

        // @ 0x824E8128 (DWARF BrnTextSelection.cpp:42) -- name the widget through the group
        // base, give the display field the SAME component name, and mark that field
        // autosizing.
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpacParentName, u64 luAptId);

        // @ 0x824E9C40 (DWARF BrnTextSelection.cpp:64) -- (re)populate the widget: latch the
        // wrap flag, walk ALL 100 rows enabling the first liNumberOfItems and disabling the
        // rest, push each row's text, register every row with the group, apply the id array,
        // highlight row 0 and refresh.
        void SetupTextSelection(s32 liNumberOfItems, bool lbWrapped,
                                const char** lppacItemTexts, u64* lpaIds);

        // @ 0x824E8278 (DWARF BrnTextSelection.cpp:125) -- set one row's text (and dirty the
        // row + the widget). The index is SIGNED: the X360 bounds it with two `cmpwi`
        // (>= 0 and < 100), so Hex-Rays' `unsigned int` rendering is wrong.
        void SetItemText(s32 liIndex, const char* lpacItemText);

        // @ 0x824E83A0 (DWARF BrnTextSelection.cpp:143) -- component vtable slot 5. When the
        // widget is dirty, push the highlighted row's text into the display field and run
        // the group update. See the virtual/hiding note above.
        void Update();

        // ---- members over the SelectableGroup base (DWARF h:82/h:83; the X360 offsets are
        //      documentary -- the x64 gate widens the row/base pointers, so every access is
        //      BY NAME) ---------------------------------------------------------------
        TextSelectionItem maSelectionItems[KI_MAX_ITEMS_PER_SELECTION];  // +0x238 (0x18 stride)
        TextField         mTextField;                                    // +0xB98
    };
}
