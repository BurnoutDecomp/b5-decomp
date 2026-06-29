#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Network/Buddies/DirtySock/CgsBuddyApiDirtySock.h"

// =============================================================================
// CgsNetwork::BuddyManagerBase -- the platform-agnostic buddy-list manager that
// wraps the DirtySock HighLevelBuddy (HLB) API.
//
// Reconstructed from the X360 build (asm spine @ 0x8287D478 etc.) gated against
// the DecFIGS DWARF declaration shape. Member layout is pinned by the asm:
//
//   +0x00  vptr
//   +0x04  mpBuddies            (v4[1]; the HLBApiRefT* handle)
//   +0x08  meCurrentStatus      EBuddyActionStatus
//   +0x0C  mpNetworkManager     (v4[3])
//   +0x10  mpServerInterface    (v4[4])
//   ...    mpBuddiesChangedUserData / mBuddiesChangedCallback (private)
//
// The full method surface is declared from the DWARF (so the vtable order and
// the public API match the original); only the subset whose X360 bodies are
// attested for this TU is defined out-of-line in CgsBuddyManagerDirtySock.cpp.
// The remaining members are reconstructed in their own TUs.
// =============================================================================

namespace CgsNetwork
{
    // Forward declarations -- held by pointer only here.
    struct PlayerName;                  // Network/Players/CgsPlayerName.h
    struct NetworkManager;              // CgsNetworkManager.cpp (TU-local home)
    class  ServerInterface;             // ServerInterface/CgsServerInterface.h
    struct ServerInterfacePlayerParams; // ServerInterface/.../CgsServerInterfacePlayerParams.h

    // CgsBuddyManagerDirtySock.h:39
    const s32 KI_MAX_RECENT_PLAYERS = 20;

    // CgsBuddyManagerDirtySock.h:52
    enum EBuddyErrorCodes
    {
        E_BUDDY_ERROR_CODES_NONE = 0,
        E_BUDDY_ERROR_CODES_PENDING = 1,
        E_BUDDY_ERROR_CODES_NOTUSER = 2,
        E_BUDDY_ERROR_CODES_BADPARAMS = 3,
        E_BUDDY_ERROR_CODES_OFFLINE = 4,
        E_BUDDY_ERROR_CODES_NOTFOUND = 5,
        E_BUDDY_ERROR_CODES_FULL = 6,
        E_BUDDY_ERROR_CODES_PASSWORD = 7,
        E_BUDDY_ERROR_CODES_HACK = 8,
        E_BUDDY_ERROR_CODES_INTERNAL = 9,
        E_BUDDY_ERROR_CODES_BADRESOURCE = 10,
        E_BUDDY_ERROR_CODES_NETWORK = 11,
        E_BUDDY_ERROR_CODES_AUTH = 12,
        E_BUDDY_ERROR_CODES_TIMEOUT = 13,
        E_BUDDY_ERROR_CODES_CANCEL = 14,
        E_BUDDY_ERROR_CODES_NOOPERATION = 15,
        E_BUDDY_ERROR_CODES_DISCONNECTED = 16,
        E_BUDDY_ERROR_CODES_UNKNOWN = 17,
        E_BUDDY_ERROR_CODES_WRONGSTATE = 18,
        E_BUDDY_ERROR_CODES_COUNT = 19
    };

    // CgsBuddyManagerDirtySock.h:77
    enum EBuddyActionStatus
    {
        E_BUDDY_ACTION_IDLE = 0,
        E_BUDDY_ACTION_BUSY = 1,
        E_BUDDY_ACTION_DOWNLOADING_PROFILE = 2,
        E_BUDDY_ACTION_DOWNLOADING_PIC = 3,
        E_BUDDY_ACTION_FAILED = 4,
        E_BUDDY_ACTION_COUNT = 5
    };

    // CgsBuddyManagerDirtySock.h:104 -- DirtySock buddy error -> manager error map entry.
    struct DSBuddyErrorToBuddyManagerError
    {
        s32              meBuddyApiError;
        EBuddyErrorCodes meBuddyManagerError;
    };

    class BuddyManagerBase
    {
    public:
        // CgsBuddyManagerDirtySock.h:393
        static const s32 KI_INVALID_BUDDY_INDEX = -1;

        typedef CgsNetwork::DirtySock::HLBApiRefT   HLBApiRefT;
        typedef CgsNetwork::DirtySock::HLBBudT      HLBBudT;
        typedef CgsNetwork::DirtySock::HLBStatE     HLBStatE;

        // CgsBuddyManagerDirtySock.h:89/93 -- client callback typedefs.
        typedef void (*CgsBuddyStandardCallback)(EBuddyErrorCodes, void*);
        typedef s32  (*CgsBuddySortCallback)(void*, s32, void*, void*);

        // ---- lifecycle -------------------------------------------------------
        void Construct(NetworkManager* lpNetworkManager, ServerInterface* lpServerInterface);
        bool Prepare();
        virtual void Update(bool lbCanBlock);
        bool Release();
        void Destruct();
        void Suspend();
        void Resume();
        virtual EBuddyErrorCodes Disconnect();

        EBuddyActionStatus CurrentStatus();

        // ---- buddy queries ---------------------------------------------------
        bool IsABuddy(const PlayerName* lpPlayerName);
        bool IsFullBuddy(const PlayerName* lpPlayerName);
        bool GetBuddyName(s32 liIndex, PlayerName* lpOutName);
        s32  GetBuddyIndex(const PlayerName* lpPlayerName);
        s32  GetNumBuddies();
        s32  GetNumFullBuddies();
        s32  GetNumOnlineBuddies();
        bool CanBuddyVoiceChat(const PlayerName* lpPlayerName);

        virtual void BuddyListHasChanged();
        virtual void AddPlayerToHistory(ServerInterfacePlayerParams* lpParams);

        s32  GetAllBuddyNames(const char** lpaNames);
        void SetSortFunction(CgsBuddySortCallback lpfnSort, void* lpUserData);
        EBuddyErrorCodes SortBuddies();
        bool IsBuddyJoinable(const PlayerName* lpPlayerName);

        virtual void JoinBuddy(const PlayerName* lpPlayerName);
        virtual void SendInvite(const PlayerName* lpPlayerName);
        virtual void CancelInvites();
        virtual void AcceptInvite(const PlayerName* lpPlayerName);
        virtual void RevokeInvite(const PlayerName* lpPlayerName);
        virtual void RevokeAllInvites();
        virtual void DeclineInvite(const PlayerName* lpPlayerName);
        virtual void InviteAccepted(const PlayerName* lpBuddyName);
        virtual void InviteRevoked(const PlayerName* lpBuddyName);
        virtual void InviteDeclined(const PlayerName* lpBuddyName);
        virtual bool AreAnyInvitesOpen();

        virtual s32  GetTotalNumberOfMessages(const PlayerName* lpPlayerName);
        virtual s32  GetNumberOfUnreadMessages(const PlayerName* lpPlayerName);
        virtual bool GetNextUnreadMessage(const PlayerName* lpPlayerName, char* lpcOut, s32 liMaxLength);
        virtual bool GetMessage(const PlayerName* lpPlayerName, s32 liIndex, char* lpcOut, s32 liMaxLength);
        virtual void SendMessage(const PlayerName* lpBuddyName, const char* lpcMessage);
        virtual const char* GetTitle(const PlayerName* lpPlayerName);

        void SetPresence(const char* lpcPresence);
        void GetBuddyPresence(const PlayerName* lpBuddyName, char* lpcOut);
        void SetJoinable(bool lbJoinable);
        bool IsBuddyJoinable(s32 liIndex);

        virtual bool HasBuddyInvitedMe(const PlayerName* lpPlayerName);
        virtual bool HaveIInvitedMyBuddy(const PlayerName* lpPlayerName);

        bool IsBuddyOnline(const PlayerName* lpBuddyName);
        bool IsBuddyBlocked(const PlayerName* lpPlayerName);

        virtual void RefreshBuddyList();

    protected:
        virtual bool IsConnectedToNetworkService() const;
        virtual s32  GetNumberOfNewOnlineBuddies();
        virtual s32  GetIndexOfLastBuddyToComeOnline();
        virtual void MessageArrived(const char* lpcMessage);
        virtual void MessageSent(bool lbSuccess, s32 liError);
        virtual void InviteArrived(const PlayerName* lpPlayerName, s32 liInvitePayload);
        virtual void InviteSent(bool lbSuccess);

        s32 _SortBuddyFunction(const void* lpA, const void* lpB);

        // Reset hook for the buddies-changed client callback slot (+0x18). The
        // X360 BuddyManagerX360::Prepare zeroes this private base word (asm:
        // stw r10, 0x18(this) @0x8287F8E0) alongside the members it clears
        // directly; expose a protected setter so the derived Prepare can do so
        // without reaching across the class' private state.
        void _ClearBuddiesChangedCallback() { mBuddiesChangedCallback = 0; }

    private:
        bool IsFullBuddy(s32 liIndex);
        EBuddyErrorCodes _ConvertError(s32 liApiError);
        void _BuddyManagerDebugPrint(void* lpContext, const char* lpcText);

        // DirtySock callbacks (static -- registered with the HLB API; the trailing
        // void* is the owning BuddyManagerBase* user-data).
        static void _BuddiesChangedCallback(HLBApiRefT* lpApi, s32 liArg2, s32 liArg3, void* lpBuddyManager);
        static void _PresenceChangedCallback(HLBApiRefT* lpApi, HLBBudT* lpBuddy, HLBStatE leState, void* lpBuddyManager);
        static void _MessageSendingCallback(HLBApiRefT* lpApi, s32 lnOperation, s32 liError, void* lpBuddyManager);
        static void _MessageArrivedCallback(HLBApiRefT* lpApi, CgsNetwork::DirtySock::BuddyApiMsgT* lpMsg, void* lpBuddyManager);
        static void _MessageSendInviteCallback(HLBApiRefT* lpApi, s32 liArg2, s32 liArg3, void* lpBuddyManager);
        static s32  _SortBuddyFunction(void* lpUserData, s32 liArg2, void* lpA, void* lpB);

    protected:
        HLBApiRefT*        mpBuddies;
        EBuddyActionStatus meCurrentStatus;
        NetworkManager*    mpNetworkManager;
        ServerInterface*   mpServerInterface;

    private:
        void*                    mpBuddiesChangedUserData;
        CgsBuddyStandardCallback mBuddiesChangedCallback;
    };
}
