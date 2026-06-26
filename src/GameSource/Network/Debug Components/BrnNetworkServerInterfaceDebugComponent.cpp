// Bodies for the network server-interface debug component, reconstructed from
// BURNOUT_X360_ARTIST.XEX:
//   Construct   @ 0x82585700
//   Disconnect  @ 0x825857A8  (static menu-action callback)
//   GetName     @ 0x82585798
//   OnActivate  @ 0x8258AD10
//   Update      @ 0x8258AC70
//
// The component mirrors / drives the BrnServerInterfaceBase it is constructed with: a "Disconnect"
// menu action and two enum variables (connection type, server type) plus a display-status toggle.
// The connection and server-type pokes are deferred to the next Update (mbDisconnectNextUpdate /
// mbApplyServerTypeNextUpdate) so they happen on the network thread's tick, not from the menu.

#include "GameSource/Network/Debug Components/BrnNetworkServerInterfaceDebugComponent.h"

#include "GameSource/Network/BrnNetworkManager.h"        // BrnNetworkManager::GetNetworkServers
#include "GameSource/Network/BrnNetworkServers.h"        // NetworkServers::GetServerType / SetServerType
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h" // DisconnectFromServer
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"      // EGameServerConnectionType
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h" // CgsDev::DebugUI::StringList
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT

namespace BrnNetwork
{
    namespace
    {
        // Debug-menu option labels for the two enum variables. The X360 feeds .rdata StringList
        // arrays (&unk_820873A8 / &unk_820873D8) to SetOptions; their string bytes are not in the
        // available exports, so the display labels below are derived from the DecFIGS enum constant
        // names (CgsServerInterfaceGames.h EGameServerConnectionType / CgsNetworkConstants.h
        // EServerType). The value column IS attested (the SetRange bounds + the enum). DWARF lists
        // the connection table as a file-scope StringList[6] (5 entries + null terminator).
        const CgsDev::DebugUI::StringList KA_CONNECTION_TYPE_OPTIONS[] =
        {
            { CgsNetwork::ServerInterfaceGames::E_GAME_SERVER_CONNECTION_TYPE_NONE,             "None"               },
            { CgsNetwork::ServerInterfaceGames::E_GAME_SERVER_CONNECTION_TYPE_FALLBACK_BOTH,    "Fallback Both"      },
            { CgsNetwork::ServerInterfaceGames::E_GAME_SERVER_CONNECTION_TYPE_FALLBACK_VOIP_ONLY,"Fallback VOIP Only" },
            { CgsNetwork::ServerInterfaceGames::E_GAME_SERVER_CONNECTION_TYPE_FALLBACK_GAME_ONLY,"Fallback Game Only" },
            { CgsNetwork::ServerInterfaceGames::E_GAME_SERVER_CONNECTION_TYPE_NO_FALLBACK,      "No Fallback"        },
            { 0, nullptr },
        };

        const CgsDev::DebugUI::StringList KA_SERVER_TYPE_OPTIONS[] =
        {
            { CgsNetwork::E_SERVER_TYPE_LOCAL,  "Local"  },
            { CgsNetwork::E_SERVER_TYPE_DEV,    "Dev"    },
            { CgsNetwork::E_SERVER_TYPE_TEST,   "Test"   },
            { CgsNetwork::E_SERVER_TYPE_JUICE,  "Juice"  },
            { CgsNetwork::E_SERVER_TYPE_ARTIST, "Artist" },
            { CgsNetwork::E_SERVER_TYPE_DEMO_1, "Demo 1" },
            { CgsNetwork::E_SERVER_TYPE_DEMO_2, "Demo 2" },
            { 0, nullptr },
        };
    }

    // @ 0x82585700. Initialise the component for the server interface it debugs, then register it
    // with the debug manager. The X360 emits a `bl` to an empty (blr-only) function @ 0x8284CB38
    // here (decompiled as BaseCollisionGenerator::Destruct via COMDAT folding of identical empty
    // bodies); it is a no-op and carries no observable effect, so it is dropped.
    void ServerInterfaceDebugComponent::Construct(BrnServerInterfaceBase* lpServerInterfaceBase)
    {
        CGS_ASSERT(lpServerInterfaceBase != nullptr, "lpServerInterfaceBase");

        mpServerInterfaceBase       = lpServerInterfaceBase;
        miConnectionType            = CgsNetwork::ServerInterfaceGames::E_GAME_SERVER_CONNECTION_TYPE_FALLBACK_GAME_ONLY; // 3
        mbDisconnectNextUpdate      = false;
        mbApplyServerTypeNextUpdate = false;
        mbDisplayConnectionStatus   = false;
        mpNetworkManager            = nullptr;
        miServerType                = CgsNetwork::E_SERVER_TYPE_COUNT; // 7 (sentinel: no server type yet)

        Register();
    }

    // @ 0x825857A8. "Disconnect" menu action: flag a disconnect for the next Update tick. The void*
    // user-data is the component (registered via RegisterFunction(&Disconnect, this, "Disconnect")).
    void ServerInterfaceDebugComponent::Disconnect(void* lpData)
    {
        CGS_ASSERT(lpData != nullptr, "lpServerInterfaceDebugComponent");
        static_cast<ServerInterfaceDebugComponent*>(lpData)->mbDisconnectNextUpdate = true;
    }

    // @ 0x82585798.
    const char* ServerInterfaceDebugComponent::GetName() const
    {
        return "ServerInterface";
    }

    // @ 0x8258AD10. Register the menu surface when the component is activated: the Disconnect
    // action, the connection-type enum, the display-status toggle, and -- only when a network
    // manager is attached -- the server-type enum (seeded from the manager's current server type,
    // with a select callback that applies the change).
    void ServerInterfaceDebugComponent::OnActivate()
    {
        RegisterFunction(&ServerInterfaceDebugComponent::Disconnect, this, "Disconnect");

        s32* lpiConnectionType = reinterpret_cast<s32*>(&miConnectionType);
        RegisterVariable(lpiConnectionType, "Connection Type");
        SetRange(lpiConnectionType, 0, CgsNetwork::ServerInterfaceGames::E_GAME_SERVER_CONNECTION_TYPE_NO_FALLBACK); // [0,4]
        SetOptions(lpiConnectionType, KA_CONNECTION_TYPE_OPTIONS);

        RegisterVariable(&mbDisplayConnectionStatus, "Display Connection Status");

        if (mpNetworkManager != nullptr)
        {
            miServerType = mpNetworkManager->GetNetworkServers()->GetServerType();

            s32* lpiServerType = reinterpret_cast<s32*>(&miServerType);
            RegisterVariable(lpiServerType, "Server Type");
            SetRange(lpiServerType, 0, CgsNetwork::E_SERVER_TYPE_DEMO_2); // [0,6]
            SetOptions(lpiServerType, KA_SERVER_TYPE_OPTIONS);
            SetSelectCallback(lpiServerType, &ServerInterfaceDebugComponent::ServerTypeSelectCallback, this);
        }
    }

    // @ 0x8258AC70. Per-tick: apply any pending disconnect / server-type change requested from the
    // debug menu (each deferred here so it runs on the network update rather than the UI thread).
    void ServerInterfaceDebugComponent::Update()
    {
        if (mbDisconnectNextUpdate)
        {
            mbDisconnectNextUpdate = false;
            GetConnectionComponent(mpServerInterfaceBase)->DisconnectFromServer();
        }

        if (mbApplyServerTypeNextUpdate)
        {
            mbApplyServerTypeNextUpdate = false;
            CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");
            mpNetworkManager->GetNetworkServers()->SetServerType(static_cast<CgsNetwork::EServerType>(miServerType));
        }
    }
}
