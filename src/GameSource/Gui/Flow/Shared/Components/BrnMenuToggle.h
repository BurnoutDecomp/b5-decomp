#pragma once

// ===================================================================================
// BrnGui::MenuToggle -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Shared/Components/BrnMenuToggle.h
//   class:BrnGui::MenuToggle
//
// A menu component that wraps a BrnGui::SelectableGroup of toggle items (the group lives at
// the +0xA8 sub-object) plus the component dirty-flag byte at +0xC. This header bodies the
// three recovered cursor functions; the rest of MenuToggle (Construct/Update/Select/...) is
// owned by other TUs. Member-by-name / recovered-offset access (the gate compiles 64-bit, so
// the reserved-head offsets are illustrative, not byte-exact).
//
// Recovered functions (X360 ARTIST):
//   GetHighlightedId  @ 0x82489080 -> mSelectableGroup.GetHighlighted(); assert non-null
//                                     (BrnSelectableGroup.h:218); return highlighted->mId (+0x10).
//   HighlightNext     @ 0x82482860 -> virtual SelectableGroup slot +0x28 (idx 10)(group, false);
//                                     on success set the +0xC dirty flag (|=0x10), return true.
//   HighlightPrevious @ 0x824828D8 -> virtual SelectableGroup slot +0x2C (idx 11)(group, false);
//                                     on success set the +0xC dirty flag (|=0x10), return true.
// ===================================================================================

#include "types.hpp"
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectableGroup.h"  // BrnGui::SelectableGroup (base; also fwd-declares CgsGui::StateInterface)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                        // BrnGui::GuiFlow (AppendExpectedAptComponent selector; complete enum needed to pass by value)

namespace BrnGui
{
    // The apt-component cache (BrnGuiCache.h). Pointer-only in the decl below, so a forward
    // declaration suffices; the bodies that dereference it (in other TUs) include the full header.
    class GuiCache;

    // Minimal view of a BrnGui::Selectable as MenuToggle::GetHighlightedId reads it: the X360
    // SelectableGroup::GetHighlighted() returns the highlighted item (a guest Selectable*),
    // and GetHighlightedId loads the item's id (u64 mId @ +0x10). The full Selectable class is
    // owned elsewhere; only the +0x10 id is touched here (DWARF BrnSelectable.h:161 mId @ +0x10:
    // vptr@+0x00, mpcSelectionText@+0x04, mxFlags@+0x08, mId@+0x10).
    struct SelectableIdView
    {
        u8  maHead[0x10];   // +0x00..+0x0F (vptr / mpcSelectionText / mxFlags)
        u64 mId;            // +0x10
    };

    struct MenuToggle
    {
        // Component dirty/state flags. HighlightNext/Previous OR in 0x10 on success.
        static const u8 KU_FLAG_DIRTY = 0x10;   // *(this+0xC) |= 0x10

        // @ 0x82489080 -- return the highlighted toggle item's id.
        u64  GetHighlightedId();
        // @ 0x82482860 -- advance the highlight; on success mark dirty. Returns success.
        bool HighlightNext();
        // @ 0x824828D8 -- retreat the highlight; on success mark dirty. Returns success.
        bool HighlightPrevious();

        // ----- members bodied by other MenuToggle TUs (declared-only here so the group can
        //       forward to them BY NAME; the MenuToggleGroupVarSize<N> template drives the calls) -----

        // Build the toggle row (name it, wire its state interface + parent, take its apt id).
        // Called per-row by MenuToggleGroupVarSize<N>::Construct.
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpacParentName, u64 luAptId);

        // Register the row's expected apt component on the GUI cache for the given flow.
        // Called per-live-row by MenuToggleGroupVarSize<N>::AppendExpectedAptComponent.
        void AppendExpectedAptComponent(GuiFlow leFlow, GuiCache* lpGuiCache, bool lbArg);

        // (Re)configure the row: text + option strings + option ids (+ a trailing selector). Called
        // by MenuToggleGroupVarSize<N>::SetupToggle when a row is (de)activated.
        void SetupMenuToggle(s32 lbActive, const char* lpacText, const char** lppacOptions,
                             u64* lpu64Ids, s32 liUnused);

        // Tear the row down (release its loaded resources). Called per-row by
        // MenuToggleGroupVarSize<N>::Unloaded.
        void Unloaded();

        // ----- recovered layout (member-by-name; reserved head carries the unrecovered base) -----
        u8               maHeadReserved[0x0C];   // +0x00..+0x0B (GuiComponent base head)
        u8               muFlags;                // +0x0C (dirty flags; HighlightNext/Prev |= 0x10)
        u8               maMidReserved[0xA8 - 0x0D]; // +0x0D..+0xA7
        SelectableGroup  mSelectableGroup;       // +0xA8 (the toggle items' group)
    };
}
