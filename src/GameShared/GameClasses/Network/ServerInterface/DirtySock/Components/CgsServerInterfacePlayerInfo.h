#ifndef CGS_SERVER_INTERFACE_PLAYER_INFO_H
#define CGS_SERVER_INTERFACE_PLAYER_INFO_H

#include "types.hpp"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceComponent.h"

// Vendor DirtySDK lobby SDK ref handles reached by pointer (opaque). Declared at the
// canonical vendor scope by the SDK headers.
struct LobbyApiRefT;
struct LobbyApiMsgT;
struct LobbyApiUserT;
struct LobbyFindUserT;
struct LobbyStatbookT;
typedef void (LobbyApiCallbackT)(LobbyApiRefT*, LobbyApiMsgT*, void*);

// ===========================================================================
// CgsNetwork::ServerInterfacePlayerInfo
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/
//         CgsServerInterfacePlayerInfo.{h,cpp}
//
// The player-info server-interface component (the E_COMPONENTS_PLAYER_INFO slot,
// embedded by value as BrnServerInterfaceBase::mPlayerInfo). Like every other
// DirtySock component it is a polymorphic leaf over the shared
// CgsNetwork::ServerInterfaceComponent base. It owns the DirtySock find-user and
// statbook modules, drives per-account settings through the lobby settings ref, and
// serves the per-player CgsNetwork::ServerInterfacePlayerInfoDataBase records the lobby
// reports (homed separately in CgsServerInterfacePlayerInfoData.{h,cpp}).
//
// LAYOUT (from the CgsServerInterfacePlayerInfo.h DWARF, with offsets confirmed against
// the X360 ARTIST asm @ 0x82878B78 Construct -- shared base at +0x00..+0x0C, leaf data
// from +0x10):
//   +0x00  vptr                        (shared ServerInterfaceComponent base)
//   +0x04  mpcCurrentAction            (base)
//   +0x08  meStatus                    (base; 2 == idle/ok)
//   +0x0C  miLastError                 (base)
//   +0x10  mpServerInterface           (ServerInterfaceDirtySock*; Construct stores 0)
//   +0x14  meCurrentAction             (EAction; Construct stores 9 == E_ACTION_COUNT)
//   +0x18  mpPlayerInfo                (ServerInterfacePlayerInfoDataBase*; 0)
//   +0x1C  mpFindUser                  (LobbyFindUserT*; 0)
//   +0x20  mpStatbook                  (LobbyStatbookT*; 0)
//   +0x24  miEventCallback             (s32; Construct stores -1)
//   +0x28  the four account-settings bool flags (Construct stores 0)
//
// NOTE: only the 21 functions recovered in this TU's export are bodied in the .cpp; the
// remaining declared members below (Suspend / Resume / GetNumberOfStatViews /
// SendFeedbackOnPlayer / the other lobby callbacks / ...) are declaration-only here,
// their bodies belonging to functions not present in this export. They are declared so
// the class shape (vtable order + member types) matches the DWARF.
// ===========================================================================

namespace CgsNetwork
{
    class ServerInterfaceDirtySock;
    class ServerInterfaceStructureInterface;
    struct ServerInterfacePlayerInfoDataBase;
    struct PlayerName;

    // The DirtySock-namespaced module handles the DWARF declares as the member types.
    // The find-user / statbook handles are the vendor SDK opaque structs; the lobby
    // settings ref is owned by the CgsNetwork::DirtySock layer.
    namespace DirtySock
    {
        struct LobbySettingRefT;
    }

    // Feedback category passed to SendFeedbackOnPlayer (declaration-only method below).
    // Forward-declared with a fixed underlying type so the signature is well-formed; the
    // full enumerator set is not needed by this TU's bodied functions.
    enum EFeedbackType : s32;

    class ServerInterfacePlayerInfo : public ServerInterfaceComponent
    {
    public:
        // CgsServerInterfacePlayerInfo.h:75 -- the action the component is currently
        // driving (stored at meCurrentAction; also the index into the action-name /
        // request-code / error-lookup tables).
        enum EAction
        {
            E_ACTION_FIND_USER             = 0,
            E_ACTION_UPDATE_AUXI           = 1,
            E_ACTION_SELECT_VIEW           = 2,
            E_ACTION_DOWNLOADING_STATS     = 3,
            E_ACTION_DOWNLOADING_VIEW_INFO = 4,
            E_ACTION_SENDING_FEEDBACK      = 5,
            E_ACTION_UPDATE_ACCOUNT        = 6,
            E_ACTION_UPDATE_SETTINGS       = 7,
            E_ACTION_LOAD_SETTINGS         = 8,
            E_ACTION_COUNT                 = 9
        };

        ServerInterfacePlayerInfo();
        virtual ~ServerInterfacePlayerInfo();

        // ---- Lifecycle (driven by the aggregate ServerInterfaceDirtySock) -----------
        virtual void Construct();
        void Destruct();
        bool Prepare(ServerInterfaceDirtySock* lpServerInterface);
        bool Release();
        void Update();
        void Suspend();
        void Resume();

        // ---- Player-info queries ----------------------------------------------------
        void GetPlayerInfoByName(const PlayerName* lpcPlayerName,
                                 ServerInterfacePlayerInfoDataBase* lpPlayerInfo);
        void GetLocalPlayerInfo(ServerInterfacePlayerInfoDataBase* lpPlayerInfo);
        void UpdateUserAuxiliaryInfo(ServerInterfaceStructureInterface* lpInfo);

        // ---- Stats / feedback -------------------------------------------------------
        void SendFeedbackOnPlayer(const PlayerName* lpcPlayerName, EFeedbackType leType);
        bool HasStatViewInfoBeenDownloaded();
        void DownloadStatViews();
        s32  GetNumberOfStatViews();
        void SelectStatView(const char* lpcView);
        void DownloadPlayersStats(const char* lpacStatList);
        bool GetPlayerStats(s32 liRow, char* lpacStatDataBuffer, s32 liBufferLength);
        bool GetRowDescription(s32 liRow, char* lpacBuffer, s32 liBufferLength);
        bool GetRowType(s32 liRow, s32* lpnType);

        // ---- Event slot -------------------------------------------------------------
        virtual void OnEvent(EServerInterfaceEvent leEvent, void* lpData);

        // ---- Account settings -------------------------------------------------------
        f32 GetMinimumTimeBetweenPlayerInfoRequests();
        void EditAccount(bool lbAgreeToShareInfoEA, bool lbAgreeToShareInfoPartners,
                         bool lbTelemetryEnable);
        void GetAccountSettings(bool* lpbAgreeToShareInfoEA,
                                bool* lpbAgreeToShareInfoPartners,
                                bool* lpbTelemetryEnable);
        void LoadSettings();

    private:
        // ---- Internals --------------------------------------------------------------
        EServerInterfaceError ConvertError(int liError, EAction leAction);
        void StartAction(EAction leAction, LobbyApiCallbackT* lpfCallback);
        void EndAction(int liError);
        void GetLocalPlayerUserData(LobbyApiUserT* lpUser) const;

        // Static DirtySock callbacks (function-pointer targets). Bodies that are not in
        // this TU's export are declared only.
        static void DefaultCallback(LobbyApiRefT* lpRef, LobbyApiMsgT* lpMsg, void* lpData);
        static void FindUserCallback(void* lpUserData, const char* lpcName,
                                     LobbyApiUserT* lpUser);
        static void EventStatusCallback(LobbyApiRefT* lpRef, LobbyApiMsgT* lpMsg,
                                        void* lpData);
        static void UploadFeedbackCallback(LobbyApiRefT* lpRef, LobbyApiMsgT* lpMsg,
                                           void* lpData);
        static void UpdateAccountCallback(LobbyApiRefT* lpRef, LobbyApiMsgT* lpMsg,
                                          void* lpData);
        static void UpdateSettingsCallback(DirtySock::LobbySettingRefT* lpRef,
                                           s32 liResult, void* lpData);
        static void LoadSettingsCallback(DirtySock::LobbySettingRefT* lpRef,
                                         s32 liResult, void* lpData);
        void _GetPlayerXUIDCallback(LobbyApiRefT* lpRef, LobbyApiMsgT* lpMsg, void* lpData);

        // ---- Data members (DWARF order; base occupies +0x00..+0x0C) -----------------
        ServerInterfaceDirtySock*          mpServerInterface;        // +0x10
        EAction                            meCurrentAction;          // +0x14
        ServerInterfacePlayerInfoDataBase* mpPlayerInfo;             // +0x18
        LobbyFindUserT*                    mpFindUser;               // +0x1C
        LobbyStatbookT*                    mpStatbook;               // +0x20
        s32                                miEventCallback;          // +0x24
        bool                               mbUseCachedAccountSettings; // +0x28
        bool                               mbAgreeToShareInfoEA;
        bool                               mbAgreeToShareInfoPartners;
        bool                               mbTelemetryEnable;
    };
}

#endif // CGS_SERVER_INTERFACE_PLAYER_INFO_H
