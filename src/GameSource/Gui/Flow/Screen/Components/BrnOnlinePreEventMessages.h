#ifndef BRN_ONLINE_PRE_EVENT_MESSAGES_H
#define BRN_ONLINE_PRE_EVENT_MESSAGES_H

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h" // CgsGui::GuiComponent

// BrnGui::OnlinePreEventMessages - the screen component that shows the pre-event
// messages on the online "ready up" screen. It drives an apt movie's view-state, and
// picks the key-frame to play based on the active game mode.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   BrnGui::OnlinePreEventMessages::SelectScreenKeyFrameForGameMode @ 0x8241A010
//
// MINIMAL-SLICE class: only SelectScreenKeyFrameForGameMode is in scope. It derives
// from CgsGui::GuiComponent (the call site invokes the base AddOutputAptViewState on
// `this`) and adds the one member it reads -- the state-interface back-pointer it walks
// to reach the gui cache and the active game-mode type. The rest of the component is
// uncommitted and OMITTED. FLAG: minimal-slice class; the guest +0x88 state-interface
// offset is not load-bearing on the 64-bit host (member declared by name).

namespace CgsGui { struct StateInterface; }

namespace BrnGui
{
    class OnlinePreEventMessages : public CgsGui::GuiComponent
    {
    public:
        // 0x8241A010 -- pick the apt view-state key-frame for the active game mode and push
        // it through the component's apt output. The online fugitive / free-burn / mode-end
        // modes (EGameModeType 12 / 14 / 17) use "anim1_StuntRun"; everything else "anim1".
        // The X360 returns the r3 left by AddOutputAptViewState; the committed
        // AddOutputAptViewState is void, so this returns 0 (the value's only consumers are
        // Show/Hide, which discard it).
        int SelectScreenKeyFrameForGameMode();

    private:
        // Guest +0x88 (the asm names it via mpStateInterface): the GUI state-interface the
        // component outputs through and walks to reach the access pointers / gui cache.
        CgsGui::StateInterface* mpStateInterface;
    };
}

#endif
