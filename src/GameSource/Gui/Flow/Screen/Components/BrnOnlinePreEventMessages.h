#ifndef BRN_ONLINE_PRE_EVENT_MESSAGES_H
#define BRN_ONLINE_PRE_EVENT_MESSAGES_H

#include "types.hpp"
#include "GameSource/Gui/Events/BrnGuiEventPreRaceMessages.h"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h" // CgsGui::GuiComponent
#include "GameSource/Gui/BrnGuiTextField.h"                         // BrnGui::TextField (by value)

// BrnGui::OnlinePreEventMessages - the screen component that shows the pre-event fly-by
// messages on the online pre-event screen: three localised text fields behind an apt
// transition, keyed by the active game mode. The base derivation (CgsGui::GuiComponent),
// the member names and order and the method set are the original declaration's; member
// placement is the console's (the strings at +0x8C, 0x128 apart; the showing flag +0x404).

namespace CgsGui { struct StateInterface; }

namespace BrnGui
{
    class OnlinePreEventMessages : public CgsGui::GuiComponent
    {
    public:
        // Build the component and its three "string<N>_mc" text fields under it.
        virtual void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                               const char* lpacParentName);

        // Fill the strings from one fly-by entry and transition in (no-op while showing).
        void Show(const PreEventInfo* lpInfo);
        // Transition out (only while showing).
        void Hide();
        bool IsShowing() const { return mbIsShowing; }
        // Re-push a just-loaded string's text; true when the name belongs to this component.
        bool HandleLoadNotification(const char* lpacComponentName);

        // 0x8241A010 -- pick the apt view-state key-frame for the active game mode and push
        // it through the component's apt output. The online fugitive / free-burn / mode-end
        // modes (EGameModeType 12 / 14 / 17) use "anim1_StuntRun"; everything else "anim1".
        // The X360 returns the r3 left by AddOutputAptViewState; the committed
        // AddOutputAptViewState is void, so this returns 0 (the value's only consumers are
        // Show/Hide, which discard it).
        int SelectScreenKeyFrameForGameMode();

    private:
        static const s32  KI_NUM_MESSAGE_STRINGS = 3;
        static const char KAC_STRING_TEXTFIELD_TEMPLATE[12];   // "string%d_mc"

        TextField maString[KI_NUM_MESSAGE_STRINGS];   // +0x8C
        bool      mbIsShowing;                        // +0x404
    };
}

#endif
