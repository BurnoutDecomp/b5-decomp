#ifndef BRN_SAVE_ICON_COMPONENT_H
#define BRN_SAVE_ICON_COMPONENT_H

#include "types.hpp"

// ============================================================================
// GameSource/Gui/Flow/Permanent/Components/BrnSaveIconComponent.h
//
// BrnGui::BrnSaveIconComponent -- the "saving..." spinner icon shown while the
// game writes to storage. MINIMAL home: only the methods the
// AlwaysAvailableComponentsManager (which embeds it BY VALUE) calls are
// declared.
//
// Layout note: in the X360 manager Construct the call into this component's
// Construct is dispatched THROUGH THE VTABLE (lwz r11,0(r3); lwz r11,0(r11);
// bctrl) -- so the component is polymorphic with a vptr at +0 and Construct is
// its first virtual. PrepareFlapt then calls Prepare directly (a non-virtual
// member). Embedded-by-value footprint = 0x18 (manager places mShowtimeMessage
// at 0x10124 -> 0x1013C). Inner fields uncommitted; modelled as opaque storage
// after the vptr. FLAG: footprint-only placeholder type.
// ============================================================================

namespace CgsGui { class StateInterface; }
namespace BrnFlapt { struct FileRef; }

namespace BrnGui
{
    class BrnSaveIconComponent
    {
    public:
        // @0x824F3628 Construct site: VIRTUAL (slot 0). r4="SaveIcon_mc",
        // r5=&StateInterface, r6=0.
        virtual void Construct(const char* lpcMovieClipName,
                               CgsGui::StateInterface* lpStateInterface,
                               s32 liFlags);

        // PrepareFlapt site (direct call): r4="SaveIcon_mc", r5=FileRef.
        void Prepare(const char* lpcMovieClipName, const BrnFlapt::FileRef& lFile);

    private:
        // Guest footprint 0x1013C - 0x10124 == 0x18 (24 bytes), of which +0x00 is
        // the vptr (the virtual above). The inner fields are uncommitted; this
        // opaque storage preserves the embedded layout. Byte size is not
        // load-bearing on the 64-bit host (pointers widen), so the storage is
        // sized to the guest data region; accessed only by name.
        u8 maComponentStorage[0x14];
    };
}

#endif // BRN_SAVE_ICON_COMPONENT_H
