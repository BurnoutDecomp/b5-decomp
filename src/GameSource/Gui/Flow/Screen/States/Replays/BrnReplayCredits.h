#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"                  // CgsGui::State (base)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"   // CgsGui::sResourceTuple

// BrnGui::ReplayCredits -- the "RE_CREDITS" screen: the closing credits roll of a replay.
// OnEnter publishes the player's name and three tiers of the replay's most active cars
// (by the cache's sorted replay-player-active table) as REPLAY_CREDITS_DETAIL_0..3 strings
// and sizes the roll's display time from them; Update counts that time down and sends
// ADVANCE once it has run out and the replay status no longer holds the screen.
// Layout and virtual set from the console build (vtable: OnEnter / OnLeave / Update /
// GetResourcesToLoad overridden).
namespace BrnGui
{
    class GuiCache;

    struct ReplayCredits : public CgsGui::State
    {
        // The load/run machine (+0x3C). WFINIT has no work of its own and falls straight
        // through to RUNNING.
        enum InternalState
        {
            E_INTERNALSTATE_LOADRESOURCES = 0,
            E_INTERNALSTATE_WFINIT        = 1,
            E_INTERNALSTATE_RUNNING       = 2,
            E_INTERNALSTATE_LEFT          = 3,
        };

        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // Hands the loader the credits movie resource.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const;

    private:
        bool UpdateLoadResources();
        void UpdateRunning();

        // ---- statics (.rdata) ----
        static const CgsGui::sResourceTuple maResourcesToLoad[1];
        static const u32                    muNumResourcesToLoad;

        // ---- members (console offsets documentary) ----
        GuiCache*     mpGuiCache;         // +0x38
        InternalState meInternalState;    // +0x3C
        f32           mfTimeRemaining;    // +0x40 (seconds of roll left)
    };
}
