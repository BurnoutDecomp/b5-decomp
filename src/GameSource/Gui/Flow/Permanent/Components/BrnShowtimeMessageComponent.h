#ifndef BRN_SHOWTIME_MESSAGE_COMPONENT_H
#define BRN_SHOWTIME_MESSAGE_COMPONENT_H

#include "types.hpp"

// ============================================================================
// GameSource/Gui/Flow/Permanent/Components/BrnShowtimeMessageComponent.h
//
// BrnGui::BrnShowtimeMessageComponent -- the "Showtime!" banner shown when a
// Showtime crash run starts. MINIMAL home: only the methods the
// AlwaysAvailableComponentsManager (which embeds it BY VALUE) calls are
// declared.
//
// Layout note: like the save-icon component, the X360 manager Construct calls
// this component's Construct THROUGH THE VTABLE (slot 0), so it is polymorphic
// with a vptr at +0; PrepareFlapt calls Prepare directly. Embedded-by-value
// footprint = 0x18 (manager places macComposerTextId at 0x1013C -> 0x10154).
// Inner fields uncommitted; modelled as opaque storage. FLAG: footprint-only
// placeholder type.
// ============================================================================

namespace CgsGui { class StateInterface; }
namespace BrnFlapt { struct FileRef; }

namespace BrnGui
{
    class BrnShowtimeMessageComponent
    {
    public:
        // @0x824F3628 Construct site: VIRTUAL (slot 0). r4="ShowtimeMsg_cpt",
        // r5=&StateInterface, r6=0.
        virtual void Construct(const char* lpcMovieClipName,
                               CgsGui::StateInterface* lpStateInterface,
                               s32 liFlags);

        // PrepareFlapt site (direct call): r4="ShowtimeMsg_cpt", r5=FileRef.
        void Prepare(const char* lpcMovieClipName, const BrnFlapt::FileRef& lFile);

    private:
        // Guest footprint 0x10154 - 0x1013C == 0x18 (24 bytes); +0x00 is the
        // vptr. Inner fields uncommitted; opaque storage preserves the embedded
        // layout. Accessed only by name.
        u8 maComponentStorage[0x14];
    };
}

#endif // BRN_SHOWTIME_MESSAGE_COMPONENT_H
