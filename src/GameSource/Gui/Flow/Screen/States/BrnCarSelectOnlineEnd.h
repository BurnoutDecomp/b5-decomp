#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "BrnCommonTypes.h"                                                     // CgsID
#include "GameSource/Gui/Flow/Screen/Components/BrnCarSelectOnlineCountdown.h"  // by value
#include "GameSource/Gui/Flow/Screen/Components/BrnCarSelectOnlinePlayerList.h" // by value
#include "GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h"       // by value

// BrnGui::CarSelectOnlineEnd - the online car-select "ready / countdown" screen flow state:
// the countdown, the lobby player table with each player's car, and (for a client of a
// host-chooses game) the host's pick, which it follows.
// The base derivation (CgsGui::State), the member names and order and the virtual set are
// the original declaration's; member placement is the console's.
namespace CgsModule { struct Event; }
namespace BrnNetwork { namespace BrnNetworkModuleIO { struct LobbyPlayerStatusData; } }

namespace BrnGui
{
    class GuiCache;   // pointer member only
    struct GuiEventNetworkLobbyPlayerList;

    struct CarSelectOnlineEnd : public CgsGui::State
    {
        // The screen's internal state machine (console +0x3C).
        enum InternalState
        {
            E_INTERNALSTATE_LOADRESOURCES = 0,
            E_INTERNALSTATE_WFINIT        = 1,
            E_INTERNALSTATE_RUNNING       = 2,
            E_INTERNALSTATE_LEFT          = 3,
            E_INTERNALSTATE_COUNT         = 4,
        };

        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x82508A80 - hands the online-car-select-end state's static resource list to
        // the loader (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        bool UpdateLoadResources();
        bool UpdateWFInit();
        void UpdateRunning();
        void UpdatePermanent();
        void HandleLobbyPlayerList(const GuiEventNetworkLobbyPlayerList* lpPlayerList);

        static const s32                    maiEventToObserve[6];
        static const s32                    miNumEventsObserved;  // == 6
        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @ 0x8205E714 (.rdata, 2 entries)
        static const u32                    muNumResourcesToLoad; // @ 0x8205E724 (.rdata, == 2)
        static const char                   KAC_ONLINE_COUNTDOWN_NAME[13];         // "Countdown_mc"
        static const char                   KAC_ONLINE_PLAYER_LIST[15];            // "PlayerTable_mc"
        static const char                   KAC_HOST_CHOOSING_ANIMATOR_NAME[16];   // "HostChoice_anim"

        // ---- data members (declaration order; console offsets in the comments) -------
        GuiCache*                 mpGuiCache;            // +0x38
        InternalState             meInternalState;       // +0x3C
        CarSelectOnlineCountdown  mOnlineCountdown;      // +0x40
        CarSelectOnlinePlayerList mOnlinePlayerList;     // +0x1F8
        const BrnNetwork::BrnNetworkModuleIO::LobbyPlayerStatusData* mpHostStatusData;   // +0x1A10
        CgsID                     mLastHostCarID;        // +0x1A18
        AnimationComponent        mHostChoosingAnimator; // +0x1A20
    };
}
