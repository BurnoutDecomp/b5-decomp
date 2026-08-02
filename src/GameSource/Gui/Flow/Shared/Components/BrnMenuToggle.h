#pragma once

// ===================================================================================
// BrnGui::MenuToggle -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Shared/Components/BrnMenuToggle.h
//   class:BrnGui::MenuToggle
//
// One row of a BrnGui::MenuToggleGroupVarSize<N>: a caption TextField plus a
// BrnGui::TextSelection of the row's options, on a BrnGui::Selectable base.
//
// 2026-08-02 -- RE-HOMED. The previous revision modelled the class as a flat struct
// (`u8 maHeadReserved[0x0C]; u8 muFlags; u8 maMidReserved[..]; SelectableGroup mSelectableGroup`)
// with NO base and only three methods, and its group driver dispatched every other method
// through a raw `*(void***)this` flat vtable. That could never have worked on the host build:
// nothing in the tree ever writes a vtable word into a modelled component head, so the first
// such call would have jumped through an uninitialised pointer. MenuToggle::Construct
// @0x824E8938 settles the real shape --
//     std  r19, 0x10(this)                     -> Selectable::mId
//     stb  r0,  0x0C(this)                     -> Selectable::mxFlags = 0
//     bl   CgsGui::GuiComponent::Construct(this + 0x18, ...)
//     bl   BrnGui::TextSelection::Construct(this + 0xA8, "ItemText", si, this + 0x1C, -1)
//     bctrl (slot 0) on this + 0xD68 with ("TitleText", si, this + 0x1C)
// -- i.e. exactly the Selectable + GuiComponent head SelectableGroup already models, then a
// TextSelection at +0xA8 (NOT a bare SelectableGroup: the row's options are texts, and
// SetupMenuToggle forwards straight to TextSelection::SetupTextSelection) and a TextField
// at +0xD68. Every method below is now a plain by-name call.
//
// Recovered functions (X360 ARTIST):
//   Construct                  @ 0x824E8938  (BrnMenuToggle.cpp:59/:60 asserts)
//   Update                     @ 0x824E8AB0
//   SetupMenuToggle            @ 0x824EA0A0
//   Unloaded                   @ 0x824E55D0
//   Clear                      @ 0x824E55E0
//   Select                     @ 0x824E55F8
//   AppendExpectedAptComponent @ 0x824E5610
//   GetHighlightedId           @ 0x82489080
//   HighlightNext              @ 0x82482860
//   HighlightPrevious          @ 0x824828D8
//
// Layout (X360 offsets are documentary -- the x64 gate widens every embedded pointer, so
// every access below is BY NAME):
//   +0x000 Selectable base (vptr / mpcSelectionText / mxFlags / mId)
//   +0x018 CgsGui::GuiComponent mGuiComponentBase
//   +0x0A8 TextSelection mItemText      ("ItemText",  sizeof == 0xCC0)
//   +0xD68 TextField     mTitleText     ("TitleText")
//   +0xE8E bool mbLoaded                (Construct stores 1)
//   +0xE90 s32  miLoadedItems           (Unloaded stores 0)   -- sizeof(MenuToggle) == 0xE98
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"    // CgsGui::GuiComponent (identity base, by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectable.h"       // BrnGui::Selectable (base)
#include "GameSource/Gui/Flow/Shared/Components/BrnTextSelection.h"    // BrnGui::TextSelection (mItemText, by value)
#include "GameSource/Gui/BrnGuiTextField.h"                            // BrnGui::TextField (mTitleText, by value)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                        // BrnGui::GuiFlow (AppendExpectedAptComponent selector)

namespace CgsGui { struct StateInterface; }

namespace BrnGui
{
    // The apt-component cache (BrnGuiCache.h). Pointer-only here; the .cpp includes the
    // full header.
    class GuiCache;

    struct MenuToggle : public Selectable
    {
        // The row's apt view states, indexed by the same ladder MenuItem / ColourMenuToggle
        // use (off_82F27460).
        enum MenuToggleStates
        {
            E_MENUTOGGLESTATES_UNUSED        = 0,
            E_MENUTOGGLESTATES_INVISIBLE     = 1,
            E_MENUTOGGLESTATES_DISABLED      = 2,
            E_MENUTOGGLESTATES_UNHIGHLIGHTED = 3,
            E_MENUTOGGLESTATES_HIGHLIGHTED   = 4,
            E_MENUTOGGLESTATES_COUNT         = 5,
        };

        // Component dirty flag (== Selectable::E_FLAG_DIRTY). Kept as a named constant
        // because the group driver and the X360 both spell it `*(this+0xC) |= 0x10`.
        static const u8 KU_FLAG_DIRTY = 0x10;

        // @ 0x824E8938 -- name the row, wire its state interface + apt id, then Construct the
        // embedded option selection ("ItemText") and caption field ("TitleText"), both
        // parented on this row's own component name.
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpacParentName, u64 luAptId);

        // @ 0x824E5610 -- register the row's own name, optionally the caption field's, and the
        // option selection's, as expected apt components.
        void AppendExpectedAptComponent(GuiFlow leFlow, GuiCache* lpGuiCache, bool lbWithTitle);

        // @ 0x824EA0A0 -- (re)configure the row.
        // ⭐ PARAMETER ROLES CORRECTED 2026-08-02 (see the note on
        // MenuToggleGroupVarSize<N>::SetupToggle). The committed declaration read
        // `SetupMenuToggle(s32 lbActive, const char*, const char**, u64*, s32 liUnused)`,
        // which typed the LAST parameter `s32` while the value the console passes in r8 is a
        // `u64*` -- a pointer TRUNCATED TO 32 BITS on x64. The asm forwards r4..r8 straight
        // into TextSelection::SetupTextSelection(count, wrapped, texts, ids), so the roles are
        // the ones below. (`lbWrapped` really is the group's "active" argument arriving in the
        // selection's wrap slot -- that IS what the console does; see SetupToggle.)
        void SetupMenuToggle(s32 liNumOptions, bool lbWrapped, const char* lpacText,
                             const char** lppacOptions, u64* lpu64Ids);

        // @ 0x824E55E0 -- component slot 6: clear the option selection.
        void Clear();
        // @ 0x824E55D0 -- release the row's loaded items.
        void Unloaded();
        // @ 0x824E55F8 -- component slot 4 (Selectable::Select): forward to the selection.
        virtual void Select();
        // @ 0x824E8AB0 -- component slot 5 (Selectable::Update): when dirty, resolve the apt
        // view state off the flag byte, push it, then update the option selection.
        virtual void Update();

        // @ 0x82489080 -- return the highlighted option's id.
        u64  GetHighlightedId();
        // @ 0x82482860 / 0x824828D8 -- step the option highlight; on success mark dirty.
        bool HighlightNext();
        bool HighlightPrevious();

        // ---- layout (X360 offsets documentary) --------------------------------------
        CgsGui::GuiComponent mGuiComponentBase;   // +0x018
        TextSelection        mItemText;           // +0x0A8 ("ItemText")
        TextField            mTitleText;          // +0xD68 ("TitleText")
        bool                 mbLoaded;            // +0xE8E (Construct stores 1)
        s32                  miLoadedItems;       // +0xE90 (Unloaded stores 0)

        // off_82F27460 -- the per-state apt view-state names, indexed by MenuToggleStates.
        // Shared vocabulary with BrnGui::MenuItem / BrnGui::ColourMenuToggle; entry [0]
        // (UNUSED) aliases INVISIBLE exactly as those two do.
        static const char* const KAC_STATE_NAMES[E_MENUTOGGLESTATES_COUNT];
    };
}
