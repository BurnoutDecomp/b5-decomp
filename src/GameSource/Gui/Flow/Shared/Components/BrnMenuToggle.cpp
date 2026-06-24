// ===================================================================================
// BrnGui::MenuToggle -- implementation (cursor functions)
//   class:BrnGui::MenuToggle
//
// Reconstructed store-for-store from the X360 ARTIST build. Member-by-name access; the
// SelectableGroup virtual cursor methods (vtable byte offsets +0x28 / +0x2C) are dispatched
// through the group's vtable, matching the X360 `(*(*(this+0xA8) + 0x28))(this+0xA8, 0)`.
// ===================================================================================
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuToggle.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnGui
{
    namespace
    {
        // SelectableGroup virtual cursor method: `bool (*)(SelectableGroup*, bool)`.
        // X360 dispatch: load the group's vtable from *(group), call the entry at the byte
        // offset (slot index), passing the group and the `false` argument.
        typedef bool (*HighlightStepFn)(SelectableGroup*, bool);

        inline bool HighlightStep(SelectableGroup* lpGroup, s32 liVtableSlot, bool lbArg)
        {
            void** lppVTable = *reinterpret_cast<void***>(lpGroup);   // vtable @ group +0x00
            HighlightStepFn lpfStep =
                reinterpret_cast<HighlightStepFn>(lppVTable[liVtableSlot]);
            return lpfStep(lpGroup, lbArg);
        }

        const s32 KI_VSLOT_HIGHLIGHT_NEXT     = 10;  // +0x28 == slot index 10
        const s32 KI_VSLOT_HIGHLIGHT_PREVIOUS = 11;  // +0x2C == slot index 11
    }

    // @ 0x82489080 -- return the highlighted toggle item's id.
    // X360: call mSelectableGroup.GetHighlighted(); if the returned item is null, fire the
    // "GetHighlighted()" assert (BrnSelectableGroup.h:218); then re-fetch the highlighted item
    // and return its id (the u64 at item+0x10). The committed GetHighlighted() returns the
    // item as a guest 32-bit pointer (the X360 Selectable*); reinterpret it to read mId.
    u64 MenuToggle::GetHighlightedId()
    {
        u32 luHighlighted = mSelectableGroup.GetHighlighted();
        CGS_ASSERT(luHighlighted != 0, "GetHighlighted()");

        const SelectableIdView* lpItem =
            reinterpret_cast<const SelectableIdView*>(
                static_cast<uintptr_t>(mSelectableGroup.GetHighlighted()));
        return lpItem->mId;
    }

    // @ 0x82482860 -- advance the highlight (virtual slot +0x28 with false); on success set the
    // +0xC dirty flag (|= 0x10) and return true, otherwise return false.
    bool MenuToggle::HighlightNext()
    {
        if (!HighlightStep(&mSelectableGroup, KI_VSLOT_HIGHLIGHT_NEXT, false))
        {
            return false;
        }
        muFlags |= KU_FLAG_DIRTY;
        return true;
    }

    // @ 0x824828D8 -- retreat the highlight (virtual slot +0x2C with false); on success set the
    // +0xC dirty flag (|= 0x10) and return true, otherwise return false.
    bool MenuToggle::HighlightPrevious()
    {
        if (!HighlightStep(&mSelectableGroup, KI_VSLOT_HIGHLIGHT_PREVIOUS, false))
        {
            return false;
        }
        muFlags |= KU_FLAG_DIRTY;
        return true;
    }
}
