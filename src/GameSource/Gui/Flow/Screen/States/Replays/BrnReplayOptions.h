#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"                  // CgsGui::State (base)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"   // CgsGui::sResourceTuple
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuToggleGroup.h"            // BrnGui::MenuToggleGroupVarSize<5> (by value)

namespace CgsModule { struct Event; }

// BrnGui::ReplayOptions -- the "RE_OPTIONS" screen: the five replay toggles (camera, replay
// HUD, export quality, player names, credits) for the reel picked on the clip list. Accept
// plays the reel, the export button exports it; either way the toggles are written back to
// the GuiCache's replay option bytes, the replay is started and the flow ADVANCEs.
// Layout and virtual set from the console build (vtable: OnEnter / OnLeave / Update /
// GetResourcesToLoad overridden; the inline ctor installs the vtable and the toggle group).
namespace BrnGui
{
    class GuiCache;

    struct ReplayOptions : public CgsGui::State
    {
        // The load/init/run machine (+0x3C).
        enum InternalState
        {
            E_INTERNALSTATE_LOADRESOURCES = 0,
            E_INTERNALSTATE_WFINIT        = 1,
            E_INTERNALSTATE_RUNNING       = 2,
            E_INTERNALSTATE_LEFT          = 3,
        };

        // The toggle rows, in group order.
        enum EOptionToggle
        {
            E_OPTION_TOGGLE_CAMERA            = 0,   // $CAMERA: chase / bumper
            E_OPTION_TOGGLE_DISPLAY_HUD       = 1,   // $DISPLAY_REPLAY_HUD: on / off
            E_OPTION_TOGGLE_EXPORT_QUALITY    = 2,   // $EXPORT_QUALITY: maximum / PSP
            E_OPTION_TOGGLE_SHOW_PLAYER_NAMES = 3,   // $SHOW_PLAYER_NAMES: on / off
            E_OPTION_TOGGLE_SHOW_CREDITS      = 4,   // $SHOW_CREDITS: on / off
            E_OPTION_TOGGLE_COUNT             = 5,
        };

        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // Hands the loader the options movie resource.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const;

    private:
        bool UpdateLoadResources();
        bool UpdateWFInit();
        void UpdateRunning();
        void HandleControllerInputPressed(const CgsModule::Event* lpEvent);
        // Store the toggles back into the cache and start the reel in liSlotIndex.
        void PlayReel(s32 liSlotIndex);

        // ---- statics (.rdata) ----
        static const s32                    maiEventToObserve[1];
        static const s32                    miNumEventsObserved;
        static const CgsGui::sResourceTuple maResourcesToLoad[1];
        static const u32                    muNumResourcesToLoad;
        static const CgsGui::sResourceTuple maInfoResourcesToLoad[1];

        // ---- members (console offsets documentary) ----
        GuiCache*                 mpGuiCache;        // +0x38
        InternalState             meInternalState;   // +0x3C
        MenuToggleGroupVarSize<5> mOptionToggles;    // +0x40 ("MenuToggle")
    };
}
