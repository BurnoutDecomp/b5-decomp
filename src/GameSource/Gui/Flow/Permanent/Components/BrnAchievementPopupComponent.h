#ifndef BRN_ACHIEVEMENT_POPUP_COMPONENT_H
#define BRN_ACHIEVEMENT_POPUP_COMPONENT_H

#include "types.hpp"
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponent.h"      // BrnFlaptComponent (base)
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptIconComponent.h"  // FlaptIconComponent (embedded)
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"                            // TextFieldRef (embedded)
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                        // BitArray<60> (embedded)

// ============================================================================
// GameSource/Gui/Flow/Permanent/Components/BrnAchievementPopupComponent.h
//
// BrnGui::AchievementPopupComponent -- the front-end pop-up that announces a
// newly-unlocked achievement. It derives from BrnFlaptComponent (it drives one
// apt movie clip via the inherited mAptRef), embeds the achievement icon and the
// "awarded" / "name" text fields, and runs a tiny two-shot animation state
// machine (play "AnimOut", wait an interval, become invisible again).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Member names / logical types come
// from the DecFIGS DWARF (BrnAchievementPopupComponent.h), gated on the X360
// ledger; offsets are proven by the ARTIST asm of Construct @ 0x82430110,
// Initialize @ 0x8242C310 and Update @ 0x82424A98.
//
// LAYOUT (X360-attested; sizeof == 0x58, matching the owner
// AlwaysAvailableComponentsManager which embeds it BY VALUE at +0x10060):
//   +0x00  BrnFlaptComponent base (mpStateInterface @ +0x00, mAptRef @ +0x04)  -- 0x0C
//   +0x0C  (4 bytes implicit alignment padding before the u64-backed BitArray)
//   +0x10  mAchievementsToShow       BitArray<60>  (one u64 field)
//   +0x18  miCurrentAchievementShowing   s32  (init 60 == "none showing")
//   +0x1C  meComponentState          ComponentState
//   +0x20  mfCurrentGameTime_Seconds f32
//   +0x24  mfAnimOutTime_Seconds     f32
//   +0x28  mfIntervalTime_Seconds    f32
//   +0x2C  mAchievementIcon          FlaptIconComponent  (sizeof 0x14)
//   +0x40  mAchievementAwardedRef    TextFieldRef        (sizeof 0x0C)
//   +0x4C  mAchievementNameRef       TextFieldRef        (sizeof 0x0C)
// All member access is BY NAME; there are no raw-offset hacks.
//
// NOTE on the X360 build: the ARTIST Update body only services the VISIBLE and
// INTERVAL states (the INVISIBLE -> pick-next-achievement-and-show path the
// DecFIGS .cpp hints describe is compiled out of this build). BecomeInvisible /
// SetAll-style helpers the DWARF lists have no standalone X360 function, so they
// are not declared here (DWARF is gated on the X360 ledger).
// ============================================================================

namespace CgsGui { class StateInterface; }
namespace BrnFlapt { struct FileRef; }

namespace BrnGui
{
    class AchievementPopupComponent : public BrnFlaptComponent
    {
    public:
        // The pop-up's animation state. Values are the raw ints the Update state
        // machine compares against (1 == VISIBLE, 2 == INTERVAL).
        enum ComponentState
        {
            E_CS_INVISIBLE = 0,
            E_CS_VISIBLE   = 1,
            E_CS_INTERVAL  = 2,
        };

        // The achievements-pending bit set: one bit per achievement id (capacity 60).
        typedef CgsContainers::BitArray<60> AchievementsBitArray;

        // Construct @ 0x82430110 -- bind the state interface, invalidate the apt
        // clip + text handles, zero the pending set, and construct the embedded
        // icon. lpcMovieClipName is the manager's "AchievementPopup_mc" DEBUG name
        // (unused by the body); liFlags is the manager's 0 (unused).
        void Construct(const char* lpcMovieClipName,
                       CgsGui::StateInterface* lpStateInterface,
                       s32 liFlags);

        // Prepare @ 0x82415C20 -- bind this component's apt clip out of lFile, then
        // bind the embedded icon and the two child text fields.
        void Prepare(const char* lacName, const BrnFlapt::FileRef& lFile);

        // Initialize @ 0x8242C310 -- reset the runtime state (time, state, pending
        // set, current-index) to its idle defaults.
        void Initialize();

        // Update @ 0x82424A98 -- advance the VISIBLE -> INTERVAL -> INVISIBLE
        // animation state machine off mfCurrentGameTime_Seconds.
        void Update();

        // SetTime @ 0x82415D60 -- set the component's current game time (seconds).
        void SetTime(f32 lfGameTime_Seconds);

        // DisplayNewAchievementNotification @ 0x8242C338 -- OR the supplied pending
        // set into mAchievementsToShow (queue those achievements for display).
        void DisplayNewAchievementNotification(const AchievementsBitArray* lpAchievementsToShow);

    private:
        AchievementsBitArray   mAchievementsToShow;          // +0x10
        s32                    miCurrentAchievementShowing;  // +0x18
        ComponentState         meComponentState;             // +0x1C
        f32                    mfCurrentGameTime_Seconds;    // +0x20
        f32                    mfAnimOutTime_Seconds;        // +0x24
        f32                    mfIntervalTime_Seconds;       // +0x28
        FlaptIconComponent     mAchievementIcon;             // +0x2C
        BrnFlapt::TextFieldRef mAchievementAwardedRef;       // +0x40
        BrnFlapt::TextFieldRef mAchievementNameRef;          // +0x4C
    };
}

#endif // BRN_ACHIEVEMENT_POPUP_COMPONENT_H
