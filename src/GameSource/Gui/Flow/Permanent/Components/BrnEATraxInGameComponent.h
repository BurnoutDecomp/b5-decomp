#ifndef BRN_EATRAX_IN_GAME_COMPONENT_H
#define BRN_EATRAX_IN_GAME_COMPONENT_H

#include "types.hpp"

// ============================================================================
// GameSource/Gui/Flow/Permanent/Components/BrnEATraxInGameComponent.h
//
// BrnGui::EATraxInGameComponent -- the in-game "now playing" / EATrax track
// notification GUI component. This is a MINIMAL home: only the lifecycle methods
// the AlwaysAvailableComponentsManager (the X360 owner that embeds it BY VALUE)
// invokes in its reconstructed Construct / PrepareFlapt are declared, signatures
// taken from those ARTIST call sites. The component's runtime methods reached only
// by the (currently blocked) manager Update are intentionally left for that TU.
//
// The component is embedded by value inside the manager, so its layout MUST be
// preserved: the X360 manager places mAchievementPopupComponent exactly 0x40
// bytes after this one (0x10020 -> 0x10060), so the component footprint is 0x40.
// The full member set is uncommitted (no reconstructable home for the inner
// fields yet); the footprint is modelled as opaque storage so the embedding
// manager lays out correctly. FLAG: footprint-only placeholder type.
// ============================================================================

namespace CgsGui { class StateInterface; }
namespace BrnFlapt { struct FileRef; }

namespace BrnGui
{
    class EATraxInGameComponent
    {
    public:
        // @0x824F3628 Construct site: r4="EATrax_mc", r5=&StateInterface, r6=0.
        void Construct(const char* lpcMovieClipName,
                       CgsGui::StateInterface* lpStateInterface,
                       s32 liFlags);

        // PrepareFlapt site: r4="EATrax_mc", r5=FileRef.
        void Prepare(const char* lpcMovieClipName, const BrnFlapt::FileRef& lFile);

        // PrepareFlapt site: no extra args.
        void Initialize();

    private:
        // Footprint of the embedded component (0x10060 - 0x10020 in the owning
        // manager). The inner fields are uncommitted; this preserves by-value
        // layout. Accessed only through the methods above, never poked by offset.
        u8 maComponentStorage[0x40];
    };
}

#endif // BRN_EATRAX_IN_GAME_COMPONENT_H
