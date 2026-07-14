// Bodies for the network server-interface debug component, reconstructed from
// BURNOUT_X360_ARTIST.XEX:
//   Construct   @ 0x82585700
//   Disconnect  @ 0x825857A8  (static menu-action callback)
//   GetName     @ 0x82585798
//   OnActivate  @ 0x8258AD10
//   Update      @ 0x8258AC70
//   RenderConnectionStatus @ 0x82594AC0   RenderHUD @ 0x82597690
//   ServerTypeSelectCallback @ 0x82585800
//
// The component mirrors / drives the BrnServerInterfaceBase it is constructed with: a "Disconnect"
// menu action and two enum variables (connection type, server type) plus a display-status toggle.
// The connection and server-type pokes are deferred to the next Update (mbDisconnectNextUpdate /
// mbApplyServerTypeNextUpdate) so they happen on the network thread's tick, not from the menu.

#include "GameSource/Network/Debug Components/BrnNetworkServerInterfaceDebugComponent.h"

#include "GameSource/Network/BrnNetworkManager.h"        // BrnNetworkManager::GetNetworkServers
#include "GameSource/Network/BrnNetworkServers.h"        // NetworkServers::GetServerType / SetServerType
#include "GameSource/Network/BrnServerInterface.h"        // BrnServerInterface::GetGameComponent / GetPlayerInfoComponent / GetConnAPIRef
#include "GameSource/Network/Parameters/BrnNetworkPlayerInfoData.h" // BrnNetwork::PlayerInfoData (concrete leaf; vtable off_820821EC)
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h" // DisconnectFromServer
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"      // EGameServerConnectionType / IsLocalPlayerInGame
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfo.h"  // GetLocalPlayerInfo
#include "GameShared/GameClasses/Development/DebugSystem/Core/Internal/CgsDebugInternal.h" // CgsDev::Internal::DebugInternal::GetUI
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h" // CgsDev::DebugUI::DebugUI::GetMetrics
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h" // CgsDev::DebugUI::StringList / Metrics / RGBA
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h" // CgsDev::Debug2DImmediateRender (MaybeDrawText target)
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT

// ---- Shared debug-HUD text helper + DirtySDK ConnApi client-list glue this TU calls. -------
// MaybeDrawText lives in its own TU (X360 bl's it directly); the ConnApi client-list + LobbyNameCmp
// entry points are DirtySDK vendor C-API (same extern block idiom as the sibling
// CgsServerInterfaceGamesX360.cpp / CgsNetworkPlayerManagerDebugComponent.cpp). A not-yet-homed
// callee is satisfied by its declaration under cl /c; no body is needed here.
struct ConnApiClientListT;   // opaque DirtySDK client list (word[0] == client count; entries @ +0x58 stride 0xBC)
namespace CgsNetwork { namespace DirtySock { struct ConnApiRefT; } }

int MaybeDrawText(CgsDev::Debug2DImmediateRender* lpDisplay, const char* lpcText,
                  f32 lfX, f32 lfY, f32 lfScale, CgsDev::RGBA lColour, bool lbCentred);

extern "C"
{
    ConnApiClientListT* ConnApiGetClientList(CgsNetwork::DirtySock::ConnApiRefT* pConn);
    s32         ConnApiClientList_GetNumClients(const ConnApiClientListT* pList);
    const char* ConnApiClientList_GetEntry(const ConnApiClientListT* pList, s32 liIndex);
    s32         LobbyNameCmp(const char* pNameA, const char* pNameB);
}

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

        // Every cell in the connection-status table is drawn white (X360 r29 = -1 == 0xFFFFFFFF,
        // the constant colour argument to every MaybeDrawText call). Matches the sibling
        // CgsNetworkPlayerManagerDebugComponent KU_TEXT_COLOUR.
        const CgsDev::RGBA KU_TEXT_COLOUR = 0xFFFFFFFFu;   // white (X360 r29 = -1)

        // Per-client byte fields inside a DirtySDK ConnApiClientT entry (the opaque client-list
        // entry is reached only through ConnApiClientList_GetEntry; these are the load/compare
        // displacements the X360 reads off the returned entry base). The game/voip status bytes are
        // an EConnApiConnStatus (0..6); bit 1 of the conn-flags byte selects game-server vs peer.
        const s32 KI_ENTRY_GAME_DEMANGLING = 0x44;   // v56[0x44] (0 -> "No")
        const s32 KI_ENTRY_GAME_CONN_FLAGS = 0x45;   // v56[0x45] (bit1 -> "GameServer")
        const s32 KI_ENTRY_GAME_STATUS     = 0x46;   // v56[0x46] (status switch)
        const s32 KI_ENTRY_VOIP_DEMANGLING = 0x4C;   // v56[0x4C] (0 -> "No")
        const s32 KI_ENTRY_VOIP_CONN_FLAGS = 0x4D;   // v56[0x4D] (bit1 -> "GameServer")
        const s32 KI_ENTRY_VOIP_STATUS     = 0x4E;   // v56[0x4E] (status switch)

        const u8 KU_CONN_FLAG_GAMESERVER = 2;        // conn-flags bit 1

        // ConnApi connection-status -> display label (X360 7-case switch, drawn identically for the
        // game and voip columns). Returns nullptr on an out-of-range status: the X360 default case
        // fires the assert and draws NOTHING, so the caller must skip the cell.
        const char* GetConnApiStatusLabel(u8 luStatus)
        {
            switch (luStatus)
            {
            case 0: return "Init";
            case 1: return "Connecting";
            case 2: return "Demangling";
            case 3: return "Active";
            case 4: return "Closing";
            case 5:
            case 6: return "Disconnected";
            default: break;
            }
            CGS_ASSERT(false, "Unknown connapi status");
            return nullptr;
        }
    }

    // The "not in game" banner and the connection-status header/data columns are drawn at per-column
    // X offsets the X360 loads from a consecutive .rdata float table based at flt_82F2A018 (idx 0 ==
    // the banner X; idx 1..8 == the eight table columns, stride 4). FLAG: those float bytes sit above
    // the exported X360 range (max export 0x82CCF730) and the DecFIGS DWARF carries no hint for this
    // debug TU, so their VALUES are not recoverable and are NOT fabricated -- the offsets are declared
    // by name and resolve to their real .rdata definition when that float pool is dumped/homed, the
    // same deferred-value idiom as WheelStateMachine.h's `extern const f32` un-recovered floats. Each
    // cell's screen X is mfScreenBorderLeft + the column offset; the render layout (branches, labels,
    // row stride) is fully grounded in the asm.
    extern const f32 KF_COL_MESSAGE;          // flt_82F2A018 ("Local Player not in game" banner X)
    extern const f32 KF_COL_PLAYER;           // flt_82F2A01C (player-name column)
    extern const f32 KF_COL_LOCAL;            // flt_82F2A020 (is-local column)
    extern const f32 KF_COL_GAME_STATUS;      // flt_82F2A024
    extern const f32 KF_COL_GAME_DEMANGLE;    // flt_82F2A028
    extern const f32 KF_COL_VOIP_STATUS;      // flt_82F2A02C
    extern const f32 KF_COL_VOIP_DEMANGLE;    // flt_82F2A030
    extern const f32 KF_COL_GAME_CONN_TYPE;   // flt_82F2A034
    extern const f32 KF_COL_VOIP_CONN_TYPE;   // flt_82F2A038

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

    // ================================================================================================
    // RenderConnectionStatus  @0x82594AC0  (called by RenderHUD when "Display Connection Status" is on)
    // ------------------------------------------------------------------------------------------------
    // The DirtySDK ConnApi connection-status HUD table. Reads the debug-UI screen metrics (text size +
    // top/left border origin), then: if the local player is NOT in a game, draws a single banner;
    // otherwise walks the ConnApi client list and draws one row per connected client -- the client's
    // lobby name, whether it is the local player (name-compared against the local player-info record),
    // and, for remote peers, the game/voip connection status, demangling flags and connection type.
    //
    // The local-player row tag reads the local player's name from a stack BrnNetwork::PlayerInfoData
    // (the CONCRETE leaf whose vtable off_820821EC the X360 stores into the record; it overrides the
    // five ServerInterfaceStructureInterface pure virtuals, so it is instantiable). It is upcast to
    // the ServerInterfacePlayerInfoDataBase* GetLocalPlayerInfo expects -- the same stack-record idiom
    // as LoginManagerBase::UpdateWaitingForPlayerID. The record is constructed unconditionally at the
    // top (matching the X360's unconditional vptr store).
    void ServerInterfaceDebugComponent::RenderConnectionStatus(CgsDev::Debug2DImmediateRender* lpDisplay)
    {
        const CgsDev::DebugUI::Metrics& lrMetrics = CgsDev::Internal::DebugInternal::GetUI().GetMetrics();
        const f32 lfBaseX    = lrMetrics.mfScreenBorderLeft;   // f30 (UI+0x70)
        const f32 lfBaseY    = lrMetrics.mfScreenBorderTop;    // f29 (UI+0x74)
        const f32 lfTextSize = lrMetrics.mfTextSize;           // f31 (UI+0x5C)

        // The debug component is embedded by value in the BrnServerInterface it debugs; the X360 reads
        // the games/player-info components and the ConnApi handle straight off the shared DirtySock
        // sub-object (base +0x0C / +0x14 / +0x80), which the named leaf accessors return.
        BrnServerInterface* lpServerInterface = static_cast<BrnServerInterface*>(mpServerInterfaceBase);

        BrnNetwork::PlayerInfoData lLocalPlayerInfo;   // vtable off_820821EC stored by the ctor

        if (!lpServerInterface->GetGameComponent()->IsLocalPlayerInGame())
        {
            MaybeDrawText(lpDisplay, "Local Player not in game",
                          lfBaseX + KF_COL_MESSAGE, lfBaseY - (lfTextSize * 1.5f), lfTextSize * 1.5f,
                          KU_TEXT_COLOUR, false);
            return;
        }

        CgsNetwork::DirtySock::ConnApiRefT* lpConnApi = lpServerInterface->GetConnAPIRef();
        if (lpConnApi == nullptr)
        {
            return;
        }

        ConnApiClientListT* lpClientList = ConnApiGetClientList(lpConnApi);
        if (lpClientList == nullptr)
        {
            return;
        }

        lpServerInterface->GetPlayerInfoComponent()->GetLocalPlayerInfo(&lLocalPlayerInfo);

        // Column headers, one text line above the first data row.
        const f32 lfHeaderY = lfBaseY - lfTextSize;
        MaybeDrawText(lpDisplay, "Player",         lfBaseX + KF_COL_PLAYER,         lfHeaderY, lfTextSize, KU_TEXT_COLOUR, false);
        MaybeDrawText(lpDisplay, "Local",          lfBaseX + KF_COL_LOCAL,          lfHeaderY, lfTextSize, KU_TEXT_COLOUR, false);
        MaybeDrawText(lpDisplay, "Game Status",    lfBaseX + KF_COL_GAME_STATUS,    lfHeaderY, lfTextSize, KU_TEXT_COLOUR, false);
        MaybeDrawText(lpDisplay, "Game Demang",    lfBaseX + KF_COL_GAME_DEMANGLE,  lfHeaderY, lfTextSize, KU_TEXT_COLOUR, false);
        MaybeDrawText(lpDisplay, "Voip Status",    lfBaseX + KF_COL_VOIP_STATUS,    lfHeaderY, lfTextSize, KU_TEXT_COLOUR, false);
        MaybeDrawText(lpDisplay, "Voip Demang",    lfBaseX + KF_COL_VOIP_DEMANGLE,  lfHeaderY, lfTextSize, KU_TEXT_COLOUR, false);
        MaybeDrawText(lpDisplay, "Game Conn Type", lfBaseX + KF_COL_GAME_CONN_TYPE, lfHeaderY, lfTextSize, KU_TEXT_COLOUR, false);
        MaybeDrawText(lpDisplay, "Voip Conn Type", lfBaseX + KF_COL_VOIP_CONN_TYPE, lfHeaderY, lfTextSize, KU_TEXT_COLOUR, false);

        f32 lfRowY = lfBaseY;
        for (s32 liIndex = 0; liIndex < ConnApiClientList_GetNumClients(lpClientList); ++liIndex)
        {
            const char* lpcEntry = ConnApiClientList_GetEntry(lpClientList, liIndex);
            const u8*   lpEntry   = reinterpret_cast<const u8*>(lpcEntry);

            // The client's lobby name (entry +0).
            MaybeDrawText(lpDisplay, lpcEntry, lfBaseX + KF_COL_PLAYER, lfRowY, lfTextSize, KU_TEXT_COLOUR, false);

            if (LobbyNameCmp(lLocalPlayerInfo.GetName(), lpcEntry) != 0)
            {
                // A remote peer: show the full connection status.
                MaybeDrawText(lpDisplay, "No", lfBaseX + KF_COL_LOCAL, lfRowY, lfTextSize, KU_TEXT_COLOUR, false);

                const char* lpcGameStatus = GetConnApiStatusLabel(lpEntry[KI_ENTRY_GAME_STATUS]);
                if (lpcGameStatus != nullptr)
                {
                    MaybeDrawText(lpDisplay, lpcGameStatus, lfBaseX + KF_COL_GAME_STATUS, lfRowY, lfTextSize, KU_TEXT_COLOUR, false);
                }

                MaybeDrawText(lpDisplay, (lpEntry[KI_ENTRY_GAME_DEMANGLING] != 0) ? "Yes" : "No",
                              lfBaseX + KF_COL_GAME_DEMANGLE, lfRowY, lfTextSize, KU_TEXT_COLOUR, false);

                const char* lpcVoipStatus = GetConnApiStatusLabel(lpEntry[KI_ENTRY_VOIP_STATUS]);
                if (lpcVoipStatus != nullptr)
                {
                    MaybeDrawText(lpDisplay, lpcVoipStatus, lfBaseX + KF_COL_VOIP_STATUS, lfRowY, lfTextSize, KU_TEXT_COLOUR, false);
                }

                MaybeDrawText(lpDisplay, (lpEntry[KI_ENTRY_VOIP_DEMANGLING] != 0) ? "Yes" : "No",
                              lfBaseX + KF_COL_VOIP_DEMANGLE, lfRowY, lfTextSize, KU_TEXT_COLOUR, false);

                MaybeDrawText(lpDisplay,
                              ((lpEntry[KI_ENTRY_GAME_CONN_FLAGS] & KU_CONN_FLAG_GAMESERVER) == KU_CONN_FLAG_GAMESERVER) ? "GameServer" : "Peer-peer",
                              lfBaseX + KF_COL_GAME_CONN_TYPE, lfRowY, lfTextSize, KU_TEXT_COLOUR, false);

                MaybeDrawText(lpDisplay,
                              ((lpEntry[KI_ENTRY_VOIP_CONN_FLAGS] & KU_CONN_FLAG_GAMESERVER) == KU_CONN_FLAG_GAMESERVER) ? "GameServer" : "Peer-peer",
                              lfBaseX + KF_COL_VOIP_CONN_TYPE, lfRowY, lfTextSize, KU_TEXT_COLOUR, false);
            }
            else
            {
                // The local player: mark the row local and dash out the peer-only columns.
                MaybeDrawText(lpDisplay, "Yes", lfBaseX + KF_COL_LOCAL,          lfRowY, lfTextSize, KU_TEXT_COLOUR, false);
                MaybeDrawText(lpDisplay, "-",   lfBaseX + KF_COL_GAME_STATUS,    lfRowY, lfTextSize, KU_TEXT_COLOUR, false);
                MaybeDrawText(lpDisplay, "-",   lfBaseX + KF_COL_GAME_DEMANGLE,  lfRowY, lfTextSize, KU_TEXT_COLOUR, false);
                MaybeDrawText(lpDisplay, "-",   lfBaseX + KF_COL_VOIP_STATUS,    lfRowY, lfTextSize, KU_TEXT_COLOUR, false);
                MaybeDrawText(lpDisplay, "-",   lfBaseX + KF_COL_VOIP_DEMANGLE,  lfRowY, lfTextSize, KU_TEXT_COLOUR, false);
                MaybeDrawText(lpDisplay, "-",   lfBaseX + KF_COL_GAME_CONN_TYPE, lfRowY, lfTextSize, KU_TEXT_COLOUR, false);
                MaybeDrawText(lpDisplay, "-",   lfBaseX + KF_COL_VOIP_CONN_TYPE, lfRowY, lfTextSize, KU_TEXT_COLOUR, false);
            }

            lfRowY += lfTextSize;
        }
    }

    // ================================================================================================
    // RenderHUD  @0x82597690  (CgsDev::DebugComponent::RenderHUD override)
    // ------------------------------------------------------------------------------------------------
    // Gate the connection-status table on the "Display Connection Status" menu toggle (+0x1E). The X360
    // tail-calls RenderConnectionStatus, passing the display through unchanged.
    void ServerInterfaceDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpDisplay)
    {
        if (mbDisplayConnectionStatus)
        {
            RenderConnectionStatus(lpDisplay);
        }
    }

    // ================================================================================================
    // ServerTypeSelectCallback  @0x82585800
    // ------------------------------------------------------------------------------------------------
    // Debug-menu select callback for the "Server Type" enum: flag the change to be applied on the next
    // Update tick (deferred so it runs on the network thread). The menu passes the changed value in the
    // first argument (unused here) and the registered component in the second. The X360 asserts the
    // component is non-null, then unconditionally sets the deferred-apply flag.
    void ServerInterfaceDebugComponent::ServerTypeSelectCallback(void* /*lpData*/, void* lpUserData)
    {
        CGS_ASSERT(lpUserData != nullptr, "lpServerInterfaceDebugComponent");
        static_cast<ServerInterfaceDebugComponent*>(lpUserData)->mbApplyServerTypeNextUpdate = true;
    }
}
