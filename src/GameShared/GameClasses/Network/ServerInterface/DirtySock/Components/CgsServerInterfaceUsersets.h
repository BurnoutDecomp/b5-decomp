#ifndef CGS_SERVER_INTERFACE_USERSETS_H
#define CGS_SERVER_INTERFACE_USERSETS_H

#include "types.hpp"

#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceComponent.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h"  // CgsNetwork::EKickReason

// ===========================================================================
// CgsNetwork::ServerInterfaceUsersets
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfaceUsersets.{h,cpp}
//
// The usersets (lobby userset create/join/leave/delete/update) server-interface
// component. Derives from CgsNetwork::ServerInterfaceComponent
// (CgsServerInterfaceUsersets.h DWARF:
// `ServerInterfaceUsersets : public CgsNetwork::ServerInterfaceComponent`).
//
// LAYOUT: the ServerInterfaceComponent base (+0x00..+0x0F), then
//   +0x10  mpServerInterface
//   +0x14  meCurrentAction
//   +0x18  mpUsersInUsersetList  (the lobby's userset-member display list)
// ===========================================================================

// DirtySDK handles (vendor SDK, global namespace like LobbyApiRefT).
struct LobbyApiRefT;
struct LobbyApiMsgT;
struct LobbyApiUserSetT;
struct DispListRef;

namespace CgsNetwork
{
    struct ServerInterfaceUsersetParamsBase;
    struct DSErrorToServerInterfaceErrorTable;

    class ServerInterfaceUsersets : public ServerInterfaceComponent
    {
    public:
        // CgsServerInterfaceUsersets.h:72
        enum EAction
        {
            E_ACTION_CREATE = 0,
            E_ACTION_DELETE = 1,
            E_ACTION_JOIN   = 2,
            E_ACTION_LEAVE  = 3,
            E_ACTION_UPDATE = 4,
            E_ACTION_COUNT  = 5,
        };

        ServerInterfaceUsersets();

        virtual ~ServerInterfaceUsersets();

        // Userset membership queries and kicks. The by-name and by-id forms find the
        // member's display-list index and kick through KickPlayer(index).
        bool IsPlayerInOurUserset(const char* lpcPlayerName) const;
        bool IsPlayerInOurUserset(s32 lPlayerID) const;
        void KickPlayer(const char* lpcPlayerName, EKickReason leReason);
        void KickPlayer(s32 liMemberIndex, EKickReason leReason);
        void KickPlayerByID(s32 lPlayerID, EKickReason leReason);

        // --- component overrides and lifecycle ---
        virtual void Construct();
        virtual void OnEvent(EServerInterfaceEvent leEvent, void* lpData);
        void Destruct();
        bool Prepare(ServerInterfaceDirtySock* lpServerInterface);
        bool Release();
        void Update();
        void Suspend();
        void Resume();

        // --- userset actions and queries ---
        void CreateUserset(ServerInterfaceUsersetParamsBase* lpParams);
        void JoinUserset(ServerInterfaceUsersetParamsBase* lpParams);
        void LeaveUserset();
        void UpdateUserSetParams(ServerInterfaceUsersetParamsBase* lpParams);
        bool GetUserSetParams(ServerInterfaceUsersetParamsBase* lpParams) const;
        const LobbyApiUserSetT* GetUserSet() const;
        bool IsLocalPlayerInUserset() const;
        bool IsLocalPlayerHost() const;

    private:
        void AllocDisplayLists();
        void StartAction(EAction leAction);
        static void DefaultCallback(LobbyApiRefT* lpLobbyApi, LobbyApiMsgT* lpMsg, void* lpUserData);

        static const s32 KAI_ACTION_CODE_MAPPING[E_ACTION_COUNT];
        static const DSErrorToServerInterfaceErrorTable KA_DS_ERROR_TABLE_LOOKUP[E_ACTION_COUNT];
        static const char* KAPC_ACTION_NAMES[E_ACTION_COUNT];

        ServerInterfaceDirtySock* mpServerInterface;       // +0x10
        EAction                   meCurrentAction;         // +0x14
        DispListRef*              mpUsersInUsersetList;    // +0x18
    };
}

#endif // CGS_SERVER_INTERFACE_USERSETS_H
