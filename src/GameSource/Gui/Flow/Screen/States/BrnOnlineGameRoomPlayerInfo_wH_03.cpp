// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-H partfile 03: expect-list builders.
//   SetExpectedChallengesComponents @0x82485078  (DWARF cpp:3935, assert line 3962)
//   SetExpectedSettingsComponents   @0x8248C628  (DWARF cpp:4312, assert line 4354)
//
// Each is the "arm the loading-screen watcher for this page" step the sub-state ladder
// runs before it waits on CheckForCompletedLoads: assert the cache pointer, drop the
// SCREEN flow's previous expected-component list, then re-append exactly the components
// that page owns. Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode
// + asm; see scratchpad/waveB/...BrnOnlineGameRoomPlayerInfo.cpp.asm.txt).
//
// The X360 reaches the GuiCache name-taking face as sub_824F87C0 with r5 = <component>+4,
// i.e. the component's macName buffer -- that is CgsGui::GuiComponent::GetName(), so the
// reconstruction calls GetName() rather than reproducing the console +4 displacement (the
// host GuiComponent's name buffer is not at +4 once the vptr widens).
//
// SetExpectedViewEventComponents @0x824851A8 belongs with these two but is NOT in this
// file -- see the block comment at the end for the blocking reason.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/Gui/BrnGuiCache.h"              // BrnGui::GuiCache (Clear/AppendExpectedAptComponent)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"      // BrnGui::GuiFlow (E_GUIFLOW_SCREEN)

namespace BrnGui
{
    // @0x82485078 -- arm the SCREEN flow for the freeburn-challenge page: the challenge
    // mode toggle (which forwards its own row), the challenge list itself and the two
    // scroll-arrow animations. The asm passes flow 0 (E_GUIFLOW_SCREEN) to every call and
    // 1 (true) as MenuToggle::AppendExpectedAptComponent's trailing argument.
    void OnlineGameRoomPlayerInfo::SetExpectedChallengesComponents()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:3962

        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);

        mChallengeToggle.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache, true);

        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                               mChallengeListComponent.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                               mChallengeListUpArrowAnimation.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                               mChallengeListDownArrowAnimation.GetName());
    }

    // @0x8248C628 -- arm the SCREEN flow for the settings page: the 4-slot toggle group is
    // the whole page, and its AppendExpectedAptComponent walks its own live rows.
    void OnlineGameRoomPlayerInfo::SetExpectedSettingsComponents()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:4354

        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);

        mSettingToggle.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache, true);
    }

    // ---------------------------------------------------------------------------------
    // SetExpectedViewEventComponents @0x824851A8 is bodied in BrnOnlineGameRoomPlayerInfo_wH_17.cpp.
}
