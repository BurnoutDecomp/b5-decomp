#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"     // CgsGui::GuiComponent (second base)
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectable.h"        // BrnGui::Selectable (first base)

// BrnGui::ImageGalleryCarouselSelectable - one carousel slot of the image-gallery
// screen: a selectable whose apt state reflects highlight x used. DWARF home
// BrnImageGalleryCarouselSelectable.h:42. The X360 layout pins the bases:
// Selectable @+0x00 (flags @+0x0C, id @+0x10) then the GuiComponent subobject
// @+0x18 (Construct chains both @0x82419A40; Update posts through this+0x18),
// with mbUsed after the component (+0xA4).
namespace BrnGui
{
    struct ImageGalleryCarouselSelectable : public Selectable, public CgsGui::GuiComponent
    {
        // @0x82419A40 (this TU, DWARF cpp:43) -- both base Constructs + clear the
        // used flag.
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpacParentName, u64 luId);

        // @0x82419A90 (this TU, DWARF cpp:76) -- consume the dirty flag and push the
        // matching apt state.
        virtual void Update();

        // DWARF cpp:61 / h:86 -- their own ledger functions (declaration-only /
        // trivial inline).
        virtual void Select();
        void SetUsed(bool lbUsed) { mbUsed = lbUsed; }

    private:
        bool mbUsed;   // DWARF h:69 (X360 +0xA4)
    };
}
