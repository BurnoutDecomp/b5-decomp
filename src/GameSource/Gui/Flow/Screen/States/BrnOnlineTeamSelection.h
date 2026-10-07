#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsID.h"                                   // CgsID
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"                  // CgsGui::State (base)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"   // CgsGui::sResourceTuple
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuComponent.h"              // BrnGui::MenuComponent (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnHelpItem.h"                   // BrnGui::HelpItem (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnButtonIcon.h"                 // BrnGui::ButtonIconComponent (by value)

namespace CgsFsm    { struct ScriptedFsm; }
namespace CgsModule { struct Event; }

// BrnGui::OnlineTeamSelection -- the "ON_TEAMS" screen: the online lobby's team-selection
// page. It waits for the GuiCache event, then streams the "ON_TEAMS" movie, lists the lobby
// players by name and keeps the highlight on the lobby's selected player as the lobby
// roster (event 244) republishes. A disconnect (event 44) sends DISCONNECT.
// The class has no debug-info row (console-only); shape and layout from
// the console build (out-of-line ctor; vtable: OnEnter / OnLeave / Update / Construct
// overridden, GetResourcesToLoad is the base's). Member names follow the twin lobby screen
// BrnGui::OnlineGameRoomPlayerInfo.
namespace BrnGui
{
    class GuiCache;

    struct OnlineTeamSelection : public CgsGui::State
    {
        // The screen's load/run sub-state (+0x10F8).
        enum ESubState
        {
            E_SUBSTATE_LOADING_SCREEN     = 0,   // waiting for the "ON_TEAMS" movie resource
            E_SUBSTATE_LOADING_COMPONENTS = 1,   // waiting for the apt components to initialise
            E_SUBSTATE_MAIN               = 2,   // interactive
        };

        // Out-of-line: installs the vtable and default-constructs the components.
        OnlineTeamSelection();

        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();
        // Asserts the FSM, then the base Construct.
        virtual void Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm);

    private:
        void HandleGuiCacheEvent(const CgsModule::Event* lpEvent);
        void CheckForCompletedLoads();
        void SetExpectedLobbyComponents();
        void UnloadLobbyResources();
        void ShowPlayerList(s32 liSelectedPlayerID);
        void HandleControllerInputPressed(const CgsModule::Event* lpEvent);
        void HandleControllerInputMainSubState(const CgsModule::Event* lpEvent);

        // ---- statics (.rdata) ----
        static const s32                    maiEventToObserve[5];
        static const s32                    miNumEventsObserved;
        static const CgsGui::sResourceTuple maResourcesToLoad[1];

        // ---- members (console offsets documentary) ----
        MenuComponent       mPlayerNameComponent;   // +0x38   ("PlayerName", 8 rows)
        ESubState           meSubState;             // +0x10F8
        s32                 mSelectedPlayerID;      // +0x10FC (-1 == none; only ever reset here)
        GuiCache*           mpGuiCache;             // +0x1100 (latched from the GuiCache event)
        // FLAG: name inferred. Cleared when the components finish loading; while clear,
        // ShowPlayerList re-seats the highlight on the previous row.
        bool                mbHighlightOnSelectedPlayer;   // +0x1104
        // FLAG: constructed by the ctor (vtable installs) but driven by no method of this
        // build's screen; names are the component types'.
        HelpItem            mHelpItem;              // +0x1108
        ButtonIconComponent maButtonIcons[2];       // +0x1194 / +0x1224
    };
}
