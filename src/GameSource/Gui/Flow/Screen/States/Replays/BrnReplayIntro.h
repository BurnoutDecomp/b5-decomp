#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"                  // CgsGui::State (base)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"   // CgsGui::sResourceTuple

// BrnGui::ReplayIntro -- the "RE_INTRO" screen: the replay's opening card. It streams the
// "ReplaysIntro" loading-screen movie (and "ReplaysMain" when the replay HUD option is on),
// fades the clear screen out, and waits for two apt transition-complete triggers: the first
// starts the card's outro (GuiEvent 530), the second sends ADVANCE.
// Layout and virtual set from the console build (vtable: OnEnter / OnLeave / Update
// overridden; GetResourcesToLoad is the base's).
namespace BrnGui
{
    class GuiCache;

    struct ReplayIntro : public CgsGui::State
    {
        // The load/init/run machine (+0x3C).
        enum InternalState
        {
            E_INTERNALSTATE_LOADRESOURCES = 0,
            E_INTERNALSTATE_WFINIT        = 1,
            E_INTERNALSTATE_RUNNING       = 2,
            E_INTERNALSTATE_LEFT          = 3,
        };

        // The card's own two-step sequence (+0x40), advanced by apt transition triggers.
        enum IntroStage
        {
            E_INTROSTAGE_SHOWING = 0,   // waiting for the card's intro transition
            E_INTROSTAGE_OUTRO   = 1,   // outro started; waiting for its transition
            E_INTROSTAGE_DONE    = 2,   // ADVANCE sent
        };

        // Register for the apt trigger, latch the cache, start the outro event and reset the
        // replay's active-player table.
        virtual void OnEnter();
        // Unregister, clear the level-4 movie, unload the intro movie, fade the clear screen.
        virtual void OnLeave();
        // The internal-state ladder, then an unconditional in-queue Clear().
        virtual void Update();

    private:
        bool UpdateLoadResources();
        bool UpdateWFInit();
        void UpdateRunning();

        // ---- statics (.rdata) ----
        static const s32                    maiEventToObserve[1];
        static const s32                    miNumEventsObserved;
        static const CgsGui::sResourceTuple maIntroResourcesToLoad[1];
        static const CgsGui::sResourceTuple maMainResourcesToLoad[1];

        // ---- members (console offsets documentary) ----
        GuiCache*     mpGuiCache;        // +0x38
        InternalState meInternalState;   // +0x3C
        IntroStage    meIntroStage;      // +0x40
    };
}
