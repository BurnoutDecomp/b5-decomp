#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::BootProfile - the boot profile/save-device GUI state (BF_PROFILE).
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct @0x824743F8  OnEnter @0x82478440  OnLeave @0x824784F8
//   Update @0x8247E500     HandleControllerInput @0x82478610
//   HandleProfileTaskResult @0x82474468  GetResourcesToLoad @0x825080F0
//
// The state waits for the GUI cache, plays the save/load prompt apt movie
// "SaveLoadComponent" at display level 3 (the asm string xref @0x8247E6FC pins the
// name), waits for its apt components to initialise, drives the profile manager's
// boot-up / message choices off controller input, and posts phase-complete
// (command 70) plus the loading screen once the profile task resolves.
//
// NOTE (flagged, not applied): the DecFIGS DWARF shows BootProfile deriving from the
// intermediate base BrnGui::ProfileTaskResultHandler (which itself terminates at
// CgsGui::State - it is where the HandleProfileTaskResult virtual lives). That handler
// base has no committed home yet. To keep this leaf header faithful to the committed
// sibling pattern we derive directly from CgsGui::State; when ProfileTaskResultHandler
// lands this base is re-pointed at it (additive).
namespace BrnGui
{
    class GuiCache;       // GameSource/Gui/BrnGuiCache.h (held by pointer only)
    class ProfileManager; // GameSource/Gui (unreconstructed; held by pointer only)

    struct BootProfile : public CgsGui::State
    {
        // The X360 Update's internal stage values (this+60; OnEnter seeds 0).
        enum EInternalState
        {
            E_PROFILE_WAIT_CACHE      = 0,   // wait for the cache event (64)
            E_PROFILE_WAIT_RESOURCES  = 1,   // wait resources + arm the component watch
            E_PROFILE_SHOW_SAVELOAD   = 2,   // post 138 + play "SaveLoadComponent" @ level 3
            E_PROFILE_WAIT_COMPONENTS = 3,   // wait apt components + profile-manager boot-up
            E_PROFILE_INTERACT        = 4,   // drive controller input into the profile flow
            E_PROFILE_SIGNAL_DONE     = 5,   // loading screen + phase-complete (70)
        };

        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x825080F0 - hands the boot-profile state's static resource list to the
        // loader (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

        // @ 0x82478610 - accept (sub-id 49) / back (50) presses drive the profile
        // manager's message choice; accept also fires the "Accept" audio trigger.
        void HandleControllerInput(const CgsModule::Event* lpEvent);

    private:
        static const s32 maiEventToObserve[];                     // @ 0x8205AB98 (.rdata)
        static const s32 miNumEventsObserved;                     // == 3
        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @ 0x82F25F78 (.rdata)
        static const u32                    muNumResourcesToLoad; // @ 0x82F25F80 (.rdata)

        GuiCache*       mpGuiCache;            // X360 this+1460 (the cache event fills it)
        ProfileManager* mpProfileManager;      // X360 this+1456 (threaded by the 3-arg Construct)
        EInternalState  meInternalState;       // X360 this+60
        s32             miMessageChoiceMode;   // X360 this+1440 (0 none / 1 ok / 2 ok-cancel)
        bool            mbCheckDiskSpaceMode;  // X360 this+1464 (re-entry disk-space mode)
    };
}
