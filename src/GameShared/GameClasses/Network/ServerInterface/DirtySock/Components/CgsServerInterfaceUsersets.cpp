#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceUsersets.h"

#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceUsersetParams.h"
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterfaceErrors.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceEvents.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySockErrorHelpers.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "lobbyapi.h"        // LobbyApiStatus / LobbyApiRequestCB / LobbyApiMsgT
#include "lobbyname.h"       // LobbyNameCmp
#include "lobbytagfield.h"   // TagFieldSetString / TagFieldSetNumber

// The usersets component: create / delete / join / leave / update a lobby userset through
// the shared request path (StartAction -> LobbyApiRequestCB -> DefaultCallback), keep the
// lobby's userset-member display list allocated while the lobby exists, and kick members.
//
// The lobby's 'self' status record and the userset record are DirtySDK-owned blobs with no
// type in this tree; the three fields read out of them are addressed by their record
// offsets (named below).

// ---- DirtySDK entry points without a vendor header in this tree (vendor SDK, no body).
extern "C"
{
    s32               DispListCount(DispListRef* ref);
    void*             DispListIndex(DispListRef* ref, s32 index);
    s32               DispListChange(DispListRef* ref, s32 set);
    s32               DispListOrder(DispListRef* ref);
}

namespace CgsNetwork
{
    namespace
    {
        // The lobby list type holding the members of the local player's userset.
        const s32 KI_LOBBY_LIST_USERSET_USERS = 10;

        // LobbyApiStatus 'self' record.
        const s32 KI_SELECT_SELF             = 0x73656C66;   // 'self'
        const s32 KI_SELF_STATUS_BUFFER_SIZE = 564;
        const s32 KI_SELF_NAME_OFFSET        = 0x08;         // local persona name
        const s32 KI_SELF_USERSET_OFFSET     = 0x1C4;        // ident of the userset we are in

        // Userset record: the host persona name.
        const s32 KI_USERSET_HOST_NAME_OFFSET = 0x10;

        // A userset display-list entry: the member's persona id, then the persona name.
        const s32 KI_MEMBER_NAME_OFFSET = 4;

        const DSErrorToServerInterfaceError KA_CREATE_DS_SERVER_INTERFACE_ERROR_MAPPING[5] =
        {
            { 0x6D697373, E_SERVER_INTERFACE_USERSET_ERROR_MISSING_PARAMETER },   // 'miss'
            { 0x6475706C, E_SERVER_INTERFACE_USERSET_ERROR_DUPLICATE_NAME },      // 'dupl'
            { 0x696E7670, E_SERVER_INTERFACE_USERSET_ERROR_INVALID_PARAMETERS },  // 'invp'
            { 0x6D617574, E_SERVER_INTERFACE_USERSET_ERROR_NOT_AUTHORISED },      // 'maut'
            { 0x616A6F69, E_SERVER_INTERFACE_USERSET_ERROR_ALREADY_JOINED },      // 'ajoi'
        };

        const DSErrorToServerInterfaceError KA_JOIN_DS_SERVER_INTERFACE_ERROR_MAPPING[7] =
        {
            { 0x6E666E64, E_SERVER_INTERFACE_USERSET_ERROR_NOT_FOUND },           // 'nfnd'
            { 0x61757468, E_SERVER_INTERFACE_USERSET_ERROR_NO_PRIVILEGE },        // 'auth'
            { 0x70617373, E_SERVER_INTERFACE_USERSET_ERROR_INVALID_PASSWORD },    // 'pass'
            { 0x66756C6C, E_SERVER_INTERFACE_USERSET_ERROR_FULL },                // 'full'
            { 0x6D617574, E_SERVER_INTERFACE_USERSET_ERROR_NOT_AUTHORISED },      // 'maut'
            { 0x616A6F69, E_SERVER_INTERFACE_USERSET_ERROR_ALREADY_JOINED },      // 'ajoi'
            { 0x6C6F636B, E_SERVER_INTERFACE_USERSET_ERROR_LOCKED },              // 'lock'
        };

        const DSErrorToServerInterfaceError KA_LEAVE_DS_SERVER_INTERFACE_ERROR_MAPPING[5] =
        {
            { 0x6E666E64, E_SERVER_INTERFACE_USERSET_ERROR_NOT_FOUND },           // 'nfnd'
            { 0x61757468, E_SERVER_INTERFACE_USERSET_ERROR_NO_PRIVILEGE },        // 'auth'
            { 0x6D617574, E_SERVER_INTERFACE_USERSET_ERROR_NOT_AUTHORISED },      // 'maut'
            { 0x6A6F696E, E_SERVER_INTERFACE_USERSET_ERROR_NOT_JOINED },          // 'join'
            { 0x696E6F70, E_SERVER_INTERFACE_USERSET_ERROR_INVALID_OPERATION },   // 'inop'
        };

        const DSErrorToServerInterfaceError KA_DELETE_DS_SERVER_INTERFACE_ERROR_MAPPING[4] =
        {
            { 0x6E666E64, E_SERVER_INTERFACE_USERSET_ERROR_NOT_FOUND },           // 'nfnd'
            { 0x61757468, E_SERVER_INTERFACE_USERSET_ERROR_NO_PRIVILEGE },        // 'auth'
            { 0x6D617574, E_SERVER_INTERFACE_USERSET_ERROR_NOT_AUTHORISED },      // 'maut'
            { 0x6E6F776E, E_SERVER_INTERFACE_USERSET_ERROR_NOT_OWNER },           // 'nown'
        };

        const DSErrorToServerInterfaceError KA_UPDATE_DS_SERVER_INTERFACE_ERROR_MAPPING[5] =
        {
            { 0x6E666E64, E_SERVER_INTERFACE_USERSET_ERROR_NOT_FOUND },           // 'nfnd'
            { 0x61757468, E_SERVER_INTERFACE_USERSET_ERROR_NO_PRIVILEGE },        // 'auth'
            { 0x6D617574, E_SERVER_INTERFACE_USERSET_ERROR_NOT_AUTHORISED },      // 'maut'
            { 0x6E6F776E, E_SERVER_INTERFACE_USERSET_ERROR_NOT_OWNER },           // 'nown'
            { 0x75757372, E_SERVER_INTERFACE_USERSET_ERROR_UNKOWN_USER },         // 'uusr'
        };
    }

    // Lobby request kind per action.
    const s32 ServerInterfaceUsersets::KAI_ACTION_CODE_MAPPING[ServerInterfaceUsersets::E_ACTION_COUNT] =
    {
        0x75637265,   // 'ucre'
        0x7564656C,   // 'udel'
        0x756A6F69,   // 'ujoi'
        0x756C6561,   // 'ulea'
        0x7561646D,   // 'uadm'
    };

    const DSErrorToServerInterfaceErrorTable
    ServerInterfaceUsersets::KA_DS_ERROR_TABLE_LOOKUP[ServerInterfaceUsersets::E_ACTION_COUNT] =
    {
        { KA_CREATE_DS_SERVER_INTERFACE_ERROR_MAPPING, 5 },
        { KA_DELETE_DS_SERVER_INTERFACE_ERROR_MAPPING, 4 },
        { KA_JOIN_DS_SERVER_INTERFACE_ERROR_MAPPING,   7 },
        { KA_LEAVE_DS_SERVER_INTERFACE_ERROR_MAPPING,  5 },
        { KA_UPDATE_DS_SERVER_INTERFACE_ERROR_MAPPING, 5 },
    };

    const char* ServerInterfaceUsersets::KAPC_ACTION_NAMES[ServerInterfaceUsersets::E_ACTION_COUNT] =
    {
        "Creating Userset",
        "Deleting Userset",
        "Joining Userset",
        "Leaving Userset",
        "Updating Userset",
    };

    // The console object is laid out by Construct; the constructor only installs the vtable.
    ServerInterfaceUsersets::ServerInterfaceUsersets()
    {
    }

    ServerInterfaceUsersets::~ServerInterfaceUsersets()
    {
    }

    void ServerInterfaceUsersets::Construct()
    {
        meStatus             = 2;
        mpcCurrentAction     = "";
        miLastError          = 0;
        mpServerInterface    = 0;
        mpUsersInUsersetList = 0;
        meCurrentAction      = E_ACTION_COUNT;
    }

    void ServerInterfaceUsersets::Destruct()
    {
        mpServerInterface    = 0;
        mpUsersInUsersetList = 0;
        meCurrentAction      = E_ACTION_COUNT;
    }

    bool ServerInterfaceUsersets::Prepare(ServerInterfaceDirtySock* lpServerInterface)
    {
        mpServerInterface = lpServerInterface;
        meCurrentAction   = E_ACTION_COUNT;
        AllocDisplayLists();
        return true;
    }

    bool ServerInterfaceUsersets::Release()
    {
        if (mpUsersInUsersetList != 0)
        {
            LobbyApiListFree(mpServerInterface->GetLobbyAPIRef(), KI_LOBBY_LIST_USERSET_USERS,
                             mpUsersInUsersetList);
            mpUsersInUsersetList = 0;
        }
        mpServerInterface = 0;
        meCurrentAction   = E_ACTION_COUNT;
        return true;
    }

    void ServerInterfaceUsersets::Update()
    {
        if (mpUsersInUsersetList != 0 && DispListChange(mpUsersInUsersetList, 0) != 0)
        {
            DispListOrder(mpUsersInUsersetList);
            mpServerInterface->OnEvent(E_SERVER_INTERFACE_USERSETS_EVENT_USERLIST_CHANGED, 0);
        }

        // Members listed but no userset any more: drop the stale list.
        if (mpUsersInUsersetList != 0 && DispListCount(mpUsersInUsersetList) > 0 && GetUserSet() == 0)
        {
            LobbyApiListFlush(mpServerInterface->GetLobbyAPIRef(), KI_LOBBY_LIST_USERSET_USERS);
        }
    }

    void ServerInterfaceUsersets::Suspend()
    {
        if (mpUsersInUsersetList != 0)
        {
            LobbyApiListFree(mpServerInterface->GetLobbyAPIRef(), KI_LOBBY_LIST_USERSET_USERS,
                             mpUsersInUsersetList);
            mpUsersInUsersetList = 0;
        }
    }

    void ServerInterfaceUsersets::Resume()
    {
        AllocDisplayLists();
    }

    void ServerInterfaceUsersets::OnEvent(EServerInterfaceEvent leEvent, void* /*lpData*/)
    {
        switch (leEvent)
        {
            case E_SERVER_INTERFACE_GENERAL_EVENT_LOBBY_API_CREATED:
                AllocDisplayLists();
                break;

            case E_SERVER_INTERFACE_GENERAL_EVENT_LOBBY_API_DESTROYING:
                if (mpUsersInUsersetList != 0)
                {
                    LobbyApiListFree(mpServerInterface->GetLobbyAPIRef(), KI_LOBBY_LIST_USERSET_USERS,
                                     mpUsersInUsersetList);
                    mpUsersInUsersetList = 0;
                }
                break;

            case E_SERVER_INTERFACE_CONNECTION_EVENT_DISCONNECTED:
                meCurrentAction = E_ACTION_COUNT;
                break;

            default:
                break;
        }
    }

    void ServerInterfaceUsersets::AllocDisplayLists()
    {
        if (mpUsersInUsersetList == 0)
        {
            mpUsersInUsersetList = LobbyApiListAlloc(mpServerInterface->GetLobbyAPIRef(),
                                                     KI_LOBBY_LIST_USERSET_USERS, 0, 0);
            CGS_ASSERT(mpUsersInUsersetList != 0, "Failed to create users in userset list");
        }
    }

    const LobbyApiUserSetT* ServerInterfaceUsersets::GetUserSet() const
    {
        char lacSelf[KI_SELF_STATUS_BUFFER_SIZE];
        LobbyApiStatus(mpServerInterface->GetLobbyAPIRef(), KI_SELECT_SELF, lacSelf, KI_SELF_STATUS_BUFFER_SIZE);

        const s32 liUsersetIdent = *reinterpret_cast<const s32*>(lacSelf + KI_SELF_USERSET_OFFSET);
        if (liUsersetIdent == 0)
        {
            return 0;
        }
        return LobbyApiGetUserSetInfoByIdent(mpServerInterface->GetLobbyAPIRef(), liUsersetIdent);
    }

    bool ServerInterfaceUsersets::GetUserSetParams(ServerInterfaceUsersetParamsBase* lpParams) const
    {
        CGS_ASSERT(lpParams != 0, "lpUsersetParams");

        lpParams->Prepare();

        const LobbyApiUserSetT* lpUserSet = GetUserSet();
        if (lpUserSet == 0)
        {
            return false;
        }
        lpParams->SerialiseFromUserset(lpUserSet);
        return true;
    }

    bool ServerInterfaceUsersets::IsLocalPlayerInUserset() const
    {
        return GetUserSet() != 0;
    }

    bool ServerInterfaceUsersets::IsPlayerInOurUserset(s32 lPlayerID) const
    {
        if (mpUsersInUsersetList != 0)
        {
            for (s32 liMember = 0; liMember < DispListCount(mpUsersInUsersetList); ++liMember)
            {
                const s32* lpMember = static_cast<const s32*>(DispListIndex(mpUsersInUsersetList, liMember));
                if (*lpMember == lPlayerID)
                {
                    return true;
                }
            }
        }
        return false;
    }

    bool ServerInterfaceUsersets::IsPlayerInOurUserset(const char* lpcPlayerName) const
    {
        if (mpUsersInUsersetList != 0)
        {
            for (s32 liMember = 0; liMember < DispListCount(mpUsersInUsersetList); ++liMember)
            {
                const char* lpMember = static_cast<const char*>(DispListIndex(mpUsersInUsersetList, liMember));
                if (LobbyNameCmp(lpcPlayerName, lpMember + KI_MEMBER_NAME_OFFSET) == 0)
                {
                    return true;
                }
            }
        }
        return false;
    }

    bool ServerInterfaceUsersets::IsLocalPlayerHost() const
    {
        const LobbyApiUserSetT* lpUserSet = GetUserSet();
        if (lpUserSet == 0)
        {
            return false;
        }

        char lacSelf[KI_SELF_STATUS_BUFFER_SIZE];
        LobbyApiStatus(mpServerInterface->GetLobbyAPIRef(), KI_SELECT_SELF, lacSelf, KI_SELF_STATUS_BUFFER_SIZE);

        return LobbyNameCmp(reinterpret_cast<const char*>(lpUserSet) + KI_USERSET_HOST_NAME_OFFSET,
                            lacSelf + KI_SELF_NAME_OFFSET) == 0;
    }

    void ServerInterfaceUsersets::StartAction(EAction leAction)
    {
        meCurrentAction = leAction;
        ServerInterfaceComponent::StartActionCore(KAPC_ACTION_NAMES[leAction]);

        LobbyApiRequestCB(mpServerInterface->GetLobbyAPIRef(), KAI_ACTION_CODE_MAPPING[meCurrentAction],
                          mpServerInterface->GetMessageBuffer(), DefaultCallback, this);
    }

    void ServerInterfaceUsersets::DefaultCallback(LobbyApiRefT* /*lpLobbyApi*/, LobbyApiMsgT* lpMsg,
                                                  void* lpUserData)
    {
        ServerInterfaceUsersets* lpThis = static_cast<ServerInterfaceUsersets*>(lpUserData);

        const DSErrorToServerInterfaceErrorTable& lrTable = KA_DS_ERROR_TABLE_LOOKUP[lpThis->meCurrentAction];
        lpThis->ServerInterfaceComponent::EndActionCore(
            lpThis->ConvertError(lpMsg->code, lrTable.mpMappingTable, lrTable.miNumMappings));
        lpThis->meCurrentAction = E_ACTION_COUNT;
    }

    void ServerInterfaceUsersets::CreateUserset(ServerInterfaceUsersetParamsBase* lpParams)
    {
        char* lpcRecord = mpServerInterface->GetMessageBuffer();
        lpcRecord[0] = 0;
        lpParams->SerialiseToString(lpcRecord, KI_MESSAGE_BUFFER_SIZE);

        StartAction(E_ACTION_CREATE);
        LobbyApiListFlush(mpServerInterface->GetLobbyAPIRef(), KI_LOBBY_LIST_USERSET_USERS);
    }

    void ServerInterfaceUsersets::JoinUserset(ServerInterfaceUsersetParamsBase* lpParams)
    {
        char* lpcRecord = mpServerInterface->GetMessageBuffer();
        lpcRecord[0] = 0;
        lpParams->SerialiseToString(lpcRecord, KI_MESSAGE_BUFFER_SIZE);

        StartAction(E_ACTION_JOIN);
        LobbyApiListFlush(mpServerInterface->GetLobbyAPIRef(), KI_LOBBY_LIST_USERSET_USERS);
    }

    void ServerInterfaceUsersets::LeaveUserset()
    {
        mpServerInterface->GetMessageBuffer()[0] = 0;

        // The host leaving deletes the userset.
        StartAction(IsLocalPlayerHost() ? E_ACTION_DELETE : E_ACTION_LEAVE);
        LobbyApiListFlush(mpServerInterface->GetLobbyAPIRef(), KI_LOBBY_LIST_USERSET_USERS);
    }

    void ServerInterfaceUsersets::UpdateUserSetParams(ServerInterfaceUsersetParamsBase* lpParams)
    {
        CGS_ASSERT(IsLocalPlayerHost(), "IsLocalPlayerHost()");

        char* lpcRecord = mpServerInterface->GetMessageBuffer();
        lpcRecord[0] = 0;
        lpParams->SerialiseToString(lpcRecord, KI_MESSAGE_BUFFER_SIZE);

        StartAction(E_ACTION_UPDATE);
    }

    void ServerInterfaceUsersets::KickPlayer(const char* lpcPlayerName, EKickReason leReason)
    {
        CGS_ASSERT(mpUsersInUsersetList != 0, "Trying to kick a player without a user list");

        for (s32 liMember = 0; liMember < DispListCount(mpUsersInUsersetList); ++liMember)
        {
            const char* lpMember = static_cast<const char*>(DispListIndex(mpUsersInUsersetList, liMember));
            if (LobbyNameCmp(lpcPlayerName, lpMember + KI_MEMBER_NAME_OFFSET) == 0)
            {
                KickPlayer(liMember, leReason);
                return;
            }
        }

        const bool lbFoundPlayer = false;
        CGS_ASSERT(lbFoundPlayer, "Failed to find player to kick");
    }

    void ServerInterfaceUsersets::KickPlayer(s32 liMemberIndex, EKickReason leReason)
    {
        CGS_ASSERT(mpUsersInUsersetList != 0, "Trying to kick a player without a user list");
        CGS_ASSERT(liMemberIndex >= 0 && liMemberIndex < DispListCount(mpUsersInUsersetList),
                   "Invalid userset member index");

        const char* lpMember = static_cast<const char*>(DispListIndex(mpUsersInUsersetList, liMemberIndex));
        if (lpMember == 0)
        {
            CGS_ASSERT(lpMember != 0, "Failed to find player to kick");
            return;
        }

        char* lpcRecord = mpServerInterface->GetMessageBuffer();
        lpcRecord[0] = 0;
        TagFieldSetString(lpcRecord, KI_MESSAGE_BUFFER_SIZE, "KICK", lpMember + KI_MEMBER_NAME_OFFSET);
        TagFieldSetNumber(lpcRecord, KI_MESSAGE_BUFFER_SIZE, "KICK_REASON", leReason);

        StartAction(E_ACTION_UPDATE);
    }

    void ServerInterfaceUsersets::KickPlayerByID(s32 lPlayerID, EKickReason leReason)
    {
        CGS_ASSERT(mpUsersInUsersetList != 0, "Trying to kick a player without a user list");

        for (s32 liMember = 0; liMember < DispListCount(mpUsersInUsersetList); ++liMember)
        {
            const s32* lpMember = static_cast<const s32*>(DispListIndex(mpUsersInUsersetList, liMember));
            if (*lpMember == lPlayerID)
            {
                KickPlayer(liMember, leReason);
                return;
            }
        }

        const bool lbFoundPlayer = false;
        CGS_ASSERT(lbFoundPlayer, "Failed to find player to kick");
    }
}
