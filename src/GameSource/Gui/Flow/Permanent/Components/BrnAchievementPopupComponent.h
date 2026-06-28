#ifndef BRN_ACHIEVEMENT_POPUP_COMPONENT_H
#define BRN_ACHIEVEMENT_POPUP_COMPONENT_H

#include "types.hpp"

// ============================================================================
// GameSource/Gui/Flow/Permanent/Components/BrnAchievementPopupComponent.h
//
// BrnGui::AchievementPopupComponent -- the front-end pop-up that announces a
// newly-unlocked achievement. MINIMAL home: only the methods the
// AlwaysAvailableComponentsManager (which embeds it BY VALUE) calls are
// declared, signatures from the ARTIST call sites
// (BrnGuiAlwaysAvailableComponentsManager.cpp).
//
// Embedded-by-value footprint = 0x58 (manager places mOnlineNotification at
// 0x10060 -> 0x100B8). The inner member set is uncommitted; the footprint is
// modelled as opaque storage to preserve the owner's layout. FLAG:
// footprint-only placeholder type.
// ============================================================================

namespace CgsGui { class StateInterface; }
namespace BrnFlapt { struct FileRef; }

namespace BrnGui
{
    class AchievementPopupComponent
    {
    public:
        // @0x824F3628 Construct site: r4="AchievementPopup_mc", r5=&StateInterface, r6=0.
        void Construct(const char* lpcMovieClipName,
                       CgsGui::StateInterface* lpStateInterface,
                       s32 liFlags);

        // PrepareFlapt site: r4="AchievementPopup_mc", r5=FileRef.
        void Prepare(const char* lpcMovieClipName, const BrnFlapt::FileRef& lFile);

        // PrepareFlapt site: no extra args.
        void Initialize();

    private:
        // Footprint (0x100B8 - 0x10060 in the owning manager). Inner fields
        // uncommitted; preserves by-value layout. Accessed only by name.
        u8 maComponentStorage[0x58];
    };
}

#endif // BRN_ACHIEVEMENT_POPUP_COMPONENT_H
