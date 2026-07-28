// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-H partfile 17: the three remaining
// expect-list builders (previously blocked on a missing MenuComponent declaration).
//   SetExpectedLobbyComponents     @0x82484E78  (DWARF cpp:3879, assert line 3908)
//   SetExpectedPauseComponents     @0x82484FE0  (DWARF cpp:3917, assert line 3944)
//   SetExpectedViewEventComponents @0x824851A8  (DWARF cpp:4436, assert line 4482)
//
// Same shape as the two builders already landed in partfile 03
// (SetExpectedChallengesComponents / SetExpectedSettingsComponents): assert the cache
// pointer, drop the SCREEN flow's previous expected-component list, then re-append
// exactly the components that page owns, in the asm's call order. Reconstructed
// store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode + asm; see
// scratchpad/waveB/GameSource_Gui_Flow_Screen_States_BrnOnlineGameRoomPlayerInfo.cpp.asm.txt).
//
// Two console-literal displacements are deliberately NOT carried across:
//  * where the X360 reaches the GuiCache name-taking face (sub_824F87C0 == the
//    GuiCache::AppendExpectedAptComponent(GuiFlow, const char*) entry @0x824F87C0) with
//    r5 = <component>+4, the +4 is CgsGui::GuiComponent's macName buffer -- that is
//    GetName(), and the host buffer does not sit at +4 once the vptr widens, so the
//    reconstruction calls GetName() (partfile 03's convention);
//  * the lobby icon loop's r30 cursor walks a1+25080 with the console strides
//    148 (IconComponent) / 1184 (one Icons row) and biases every operand by the same
//    macName +4. Those are X360 sizeofs baked into the binary, so the loop is restored
//    as an index over maIcons[] with the eight icon members named, not as pointer
//    arithmetic over host-invalid strides.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/Gui/BrnGuiCache.h"              // BrnGui::GuiCache (Clear/AppendExpectedAptComponent)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"      // BrnGui::GuiFlow (E_GUIFLOW_SCREEN)

namespace BrnGui
{
    // @0x82484E78 -- arm the SCREEN flow for the main lobby page: the options animation,
    // the tab-title text field and the button-prompt animation, the player-name and
    // player-option menus, the stats panel, and then every status icon of all eight
    // player rows. The asm passes flow 0 (E_GUIFLOW_SCREEN) to every call.
    void OnlineGameRoomPlayerInfo::SetExpectedLobbyComponents()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:3908

        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);

        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                               mOptionsAnimation.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                               mMessageText.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                               mPlayersButtonPromptsAnimation.GetName());

        mPlayerNameComponent.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);   // 0x82484F14
        mOptionComponent.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);       // 0x82484F28
        mPlayerStatsDisplay.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);

        // 0x82484F48..0x82484FD4: eight rows (r29 counts down from 8), each row's eight
        // icons appended in member order.
        for (s32 liPlayer = 0; liPlayer < KI_MAX_PLAYERS; ++liPlayer)
        {
            Icons& lIcons = maIcons[liPlayer];

            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                                   lIcons.mMarkedManIcon.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                                   lIcons.mRevengeStatusIcon.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                                   lIcons.mReadyStatusIcon.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                                   lIcons.mVideoStatusIcon.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                                   lIcons.mVoipStatusIcon.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                                   lIcons.mTeamStatusIcon.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                                   lIcons.mGameConnectionTypeIcon.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                                   lIcons.mVoipConnectionTypeIcon.GetName());
        }
    }

    // @0x82484FE0 -- arm the SCREEN flow for the pause page: the pause/security option
    // menu and its button-prompt animation are the whole page.
    void OnlineGameRoomPlayerInfo::SetExpectedPauseComponents()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:3944

        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);

        mPauseComponent.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);   // 0x82485048

        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                               mPauseButtonPromptsAnimation.GetName());
    }

    // @0x824851A8 -- arm the SCREEN flow for the view-event page: the options menu and
    // the route-info panel (which walks its own owned text fields).
    void OnlineGameRoomPlayerInfo::SetExpectedViewEventComponents()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:4482

        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);

        mViewEventOptions.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);   // 0x82485210
        mRouteInfoDisplay.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);   // 0x82485224
    }
}
