// ===================================================================================
// BrnGui::Selectable -- implementation
//   class:BrnGui::Selectable
//
// The highlight/select item base every menu-ish component mixes in. Reconstructed from the
// X360 ARTIST build: the four state-flag setters return whether the flag changed and raise
// the dirty bit (0x10) on a change; Update toggles the dirty bit; Construct seeds the id +
// clears the flags (the GuiComponent identity is constructed by the derived class).
// ===================================================================================
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectable.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnGui
{
    // @0x824E30F0 -- mId = id (r7), mxFlags = 0. name/si/parentName feed the derived class's
    // GuiComponent::Construct, not this base.
    void Selectable::Construct(const char* /*lpacName*/, CgsGui::StateInterface* /*lpStateInterface*/,
                               const char* /*lpacParentName*/, u64 luId)
    {
        mId     = luId;
        mxFlags = 0;
    }

    // @0x824E56E0 -- set/clear the Active bit; dirty + return true on a change.
    bool Selectable::SetActive(bool lbActive)
    {
        const u8 lu = mxFlags;
        if (lbActive == ((lu & E_FLAG_ACTIVE) != 0))
            return false;
        mxFlags = static_cast<u8>(lbActive ? ((lu & ~static_cast<u8>(E_FLAG_ACTIVE)) | E_FLAG_ACTIVE)
                                           : (lu ^ E_FLAG_ACTIVE));
        mxFlags = static_cast<u8>(mxFlags | E_FLAG_DIRTY);
        return true;
    }

    // @0x824E5730 -- set/clear the Highlightable bit; dirty + return true on a change.
    bool Selectable::SetHighlightable(bool lbHighlightable)
    {
        const u8 lu = mxFlags;
        if (lbHighlightable == (((lu >> 1) & 1) != 0))
            return false;
        mxFlags = static_cast<u8>(lbHighlightable ? (lu | E_FLAG_HIGHLIGHTABLE)
                                                  : (lu ^ E_FLAG_HIGHLIGHTABLE));
        mxFlags = static_cast<u8>(mxFlags | E_FLAG_DIRTY);
        return true;
    }

    // @0x824E5780 -- set/clear the Selectable bit; dirty + return true on a change.
    bool Selectable::SetSelectable(bool lbSelectable)
    {
        const u8 lu = mxFlags;
        if (lbSelectable == (((lu >> 2) & 1) != 0))
            return false;
        mxFlags = static_cast<u8>(lbSelectable ? (lu | E_FLAG_SELECTABLE)
                                               : (lu ^ E_FLAG_SELECTABLE));
        mxFlags = static_cast<u8>(mxFlags | E_FLAG_DIRTY);
        return true;
    }

    // @0x824E57D0 -- set/clear the Highlighted bit (asserting a highlightable item); dirty.
    bool Selectable::SetHighlighted(bool lbHighlighted)
    {
        const u8 lu = mxFlags;
        if (lbHighlighted == (((lu >> 3) & 1) != 0))
            return false;
        if ((lu & E_FLAG_HIGHLIGHTABLE) == 0 && lbHighlighted)
        {
            CGS_ASSERT(false, "Setting unhighlightable item to highlighted!");
        }
        mxFlags = static_cast<u8>(lbHighlighted ? (mxFlags | E_FLAG_HIGHLIGHTED)
                                                : (mxFlags ^ E_FLAG_HIGHLIGHTED));
        mxFlags = static_cast<u8>(mxFlags | E_FLAG_DIRTY);
        return true;
    }

    // @0x8240FC88 -- the base per-frame update just toggles the dirty bit (MenuItem overrides
    // this to push its apt state).
    void Selectable::Update()
    {
        mxFlags = static_cast<u8>(mxFlags ^ E_FLAG_DIRTY);
    }

    // The base "select" action is a no-op (no out-of-line X360 body -- it is inlined/empty on
    // the base and overridden by the concrete selectables that act on selection, e.g.
    // ImageGallerySelectable::Select). MenuItem uses this base default (vtable slot 4).
    void Selectable::Select()
    {
    }
}
