#ifndef BRN_SAVE_ICON_COMPONENT_H
#define BRN_SAVE_ICON_COMPONENT_H

#include "types.hpp"
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptIconComponent.h"  // BrnGui::FlaptIconComponent (base)

// ============================================================================
// GameSource/Gui/Flow/Permanent/Components/BrnSaveIconComponent.h
//
// BrnGui::BrnSaveIconComponent -- the always-available "saving..." spinner icon: a
// FlaptIconComponent that slides itself in/out on the AnimIn/AnimOut timeline labels
// and tracks its own visible state. DWARF home BrnSaveIconComponent.h:59.
//
// This home was previously a footprint-only placeholder (opaque 0x18 storage) grown
// by the AlwaysAvailableComponentsManager TU; the BrnSaveIconComponent TU upgraded it
// to the real DWARF shape, which reproduces that footprint exactly: the polymorphic
// FlaptIconComponent base (vptr @+0x00, sizeof 0x14 -- the manager's vtable-dispatched
// Construct is the base's slot-0 virtual) plus meComponentState @+0x14 == 0x18.
//
// This TU bodies Prepare/ShowSaveIcon/HideSaveIcon (BrnSaveIconComponent.cpp);
// Update (DWARF cpp:65) is its own ledger function (declaration-only here).
// ============================================================================

namespace BrnGui
{
    class BrnSaveIconComponent : public FlaptIconComponent
    {
    public:
        // DWARF BrnSaveIconComponent.h:80.
        enum ComponentState
        {
            E_CS_INVISIBLE = 0,
            E_CS_VISIBLE   = 1,
        };

        // @0x82424E28 (this TU, DWARF h:66) -- bind the icon clip (the inlined
        // FlaptIconComponent::Prepare chain), blank it and hide it. Non-virtual (the
        // PrepareFlapt call site dispatches it directly).
        void Prepare(const char* lacName, const BrnFlapt::FileRef& lFile);

        // DWARF h:70 / cpp:65 -- declaration-only (its own ledger function).
        void Update();

        // @0x824163A8 / @0x82416400 (this TU, DWARF h:73/h:76).
        void ShowSaveIcon();
        void HideSaveIcon();

    private:
        ComponentState meComponentState;   // DWARF h:86 (X360 this+0x14)
    };
}

#endif // BRN_SAVE_ICON_COMPONENT_H
