#ifndef BRN_SERVER_INTERFACE_CUSTOM_COMMANDS_H
#define BRN_SERVER_INTERFACE_CUSTOM_COMMANDS_H

#include "types.hpp"
#include "BrnCommonTypes.h"  // CgsID (u64)
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceCustomCommands.h"
#include "lobbyapi.h"        // DirtySDK LobbyApiRefT / LobbyApiMsgT (global namespace)

// ===========================================================================
// BrnNetwork::ServerInterfaceCustomCommands
//   Home: GameSource/Network/Components/BrnServerInterfaceCustomCommands.{h,cpp}
//
// The Burnout "custom commands" server-interface component: it serialises the
// game-specific lobby requests (road-rules score up/download, friends list
// sync, ELO get/set, offline-progression and live-revenge uploads) into the
// shared DirtySock message buffer and posts them through LobbyApiRequestCB,
// then parses the server's replies in HandleIncomingMessage.
//
// HIERARCHY (DecFIGS DWARF, BrnServerInterfaceCustomCommands.h:68):
//   BrnNetwork::ServerInterfaceCustomCommands
//       : public CgsNetwork::ServerInterfaceCustomCommands
//       : public CgsNetwork::ServerInterfaceComponent
// The interposed CgsNetwork::ServerInterfaceCustomCommands base (DWARF
// CgsServerInterfaceCustomCommands.h:48) adds NO data members -- it only
// re-declares Construct/OnEvent and the non-virtual Prepare/Release/Destruct.
// So the X360 object layout is identical to deriving straight from the 4-word
// CgsNetwork::ServerInterfaceComponent base, and we model it that way (deriving
// from the empty intermediate would not change the layout or this TU's offsets).
//
// LAYOUT confirmed against the X360 ARTIST asm (Construct @ 0x82583720 and every
// member access in this TU's functions), after the 4-word component base:
//   +0x00 vptr / mpcCurrentAction / meStatus / miLastError   (component base)
//   +0x10 mpServerInterface      (word 4  -- a1[4])
//   +0x14 mpiRaceElo             (word 5  -- a1[5])
//   +0x18 mpiRoadRageElo         (word 6  -- a1[6])
//   +0x1C mpiBurningHomeRunElo   (word 7  -- a1[7])
//   +0x20 meCurrentAction        (word 8  -- a1[8], 11 == E_ACTION_COUNT when idle)
//   +0x24 mpCallback             (word 9  -- a1[9])
//   +0x28 mpCallbackData         (word 10 -- a1[10])
// ===========================================================================

namespace CgsNetwork
{
    struct ServerInterfaceDirtySock;   // pointer member only
    struct PlayerName;                 // friends-list name slot (BrnCgsPlayerName.h)
}

namespace BrnNetwork
{
    class RoadRulesUploadData;         // SetRoadRulesForLocalPlayer input (Parameters/BrnNetworkRoadRulesData.h)
    class OnlineGameResults;           // UploadFreeBurnLobbyStats input (other TU)
    struct RivalDataT;                 // UploadLiveRevengeData rows (other TU)

    namespace NetworkInOfflineProgression
    {
        struct OfflineProgressionT;    // UploadOfflineProgress input (other TU)
    }

    class ServerInterfaceCustomCommands : public CgsNetwork::ServerInterfaceCustomCommands
    {
    public:
        // BrnServerInterfaceCustomCommands.h:71 -- one custom-command action slot. The value
        // indexes KPC_ACTION_NAMES / KI_ACTIONS_TO_MESSAGE_CODES; the idle marker stored in
        // meCurrentAction is E_ACTION_COUNT itself (the X360 Construct stores 11 at +0x20 and
        // SendCustomCommand asserts the new action != E_ACTION_COUNT, comparing against 11).
        //
        // VALUES ARE X360-AUTHORITATIVE (read off the `li r4, <n>` action codes the request
        // entry points pass to SendCustomCommand): GET_ELO == 9 and SET_ELO == 10, with
        // E_ACTION_COUNT == 11. The PS3 DecFIGS DWARF lists a 10-action enum (GET_ELO == 8,
        // COUNT == 10); the X360 build inserted one extra action (index 8, the un-homed
        // "event score data" upload reached by the cpp's UploadEventScoreData declaration),
        // shifting GET_ELO/SET_ELO up by one. The asm wins, so the X360 numbering is used.
        enum EAction
        {
            E_ACTION_START                       = 0,
            E_ACTION_SET_ROAD_RULES_SCORES       = 0,  // li r4,0  (SetRoadRulesForLocalPlayer)
            E_ACTION_GET_ROAD_RULES_SCORES       = 1,  // stw 1    (GetRoadRulesHighScores)
            E_ACTION_GET_LOCAL_ROAD_RULES_SCORES = 2,  // li r4,2  (GetLocalRoadRulesHighScores)
            E_ACTION_GET_FRIENDS                 = 3,  // li r4,3  (GetFriendsListFromServer)
            E_ACTION_UPLOAD_FRIENDS              = 4,  // stw 4    (Update/OverWriteServerFriendsRecord)
            E_ACTION_UPLOAD_FREEBURN_LOBBY_STATS = 5,  // (UploadFreeBurnLobbyStats, other TU)
            E_ACTION_UPLOAD_OFFLINE_PROGRESSION  = 6,  // li r4,6  (UploadOfflineProgress)
            E_ACTION_UPLOAD_RIVAL_DATA           = 7,  // stw 7    (UploadLiveRevengeData)
            E_ACTION_UPLOAD_EVENT_SCORE_DATA     = 8,  // (UploadEventScoreData, other TU)
            E_ACTION_GET_ELO                     = 9,  // li r4,9  (GetEloOfLocalPlayer)
            E_ACTION_SET_ELO                     = 10, // li r4,10 (SetEloOfLocalPlayer)
            E_ACTION_COUNT                       = 11  // idle sentinel stored at +0x20
        };

        // BrnServerInterfaceCustomCommands.h:56 -- completion callback delivered to the
        // requester: (user data, parsed result blob or NULL, success flag).
        typedef void (*CustomCommandCallback)(void* lpData, void* lpResult, bool lbSuccess);

        // --- vtable / lifecycle ---------------------------------------------------
        // Construct @ 0x82583720 -- base Construct then reset the Brn members.
        virtual void Construct();

        // Non-virtual lifecycle (DWARF: Prepare/Release/Destruct are non-virtual).
        bool Prepare(CgsNetwork::ServerInterfaceDirtySock* lpServerInterface); // @ 0x82583770
        bool Release();                                                        // @ 0x82583810
        void Destruct();                                                       // declared-only (other TU)
        void Update();                                                         // declared-only (other TU)

        // --- request entry points (post into the message buffer + LobbyApiRequestCB) ---
        // GetEloOfLocalPlayer @ 0x8258F0F0 -- NOTE the parameter order. The X360 asm proves
        // the source order is (race, BURNING-HOME-RUN, ROAD-RAGE): a2 asserts "lpRaceElo"
        // (cpp:601) and stores to mpiRaceElo (a1[5]); a3 asserts "lpBurningHomeRunElo"
        // (cpp:603) and stores to mpiBurningHomeRunElo (a1[7]); a4 asserts "lpRoadRageElo"
        // (cpp:602) and stores to mpiRoadRageElo (a1[6]). This is INTENTIONALLY the reverse
        // of SetEloOfLocalPlayer's (race, road-rage, burning-home-run) order below -- the two
        // entry points disagree in the binary, so each is declared from its own asm.
        void GetEloOfLocalPlayer(s32* lpiRaceElo, s32* lpiBurningHomeRunElo,   // @ 0x8258F0F0
                                 s32* lpiRoadRageElo,
                                 CustomCommandCallback lCallback, void* lpData);
        void SetEloOfLocalPlayer(s32 liRaceElo, s32 liRoadRageElo,             // @ 0x8258F200
                                 s32 liBurningHomeRunElo,
                                 CustomCommandCallback lCallback, void* lpData);
        void SetRoadRulesForLocalPlayer(RoadRulesUploadData* lpRoadRulesData,  // @ 0x8258F3E0
                                        const char* lpcRoadRulesServerKey,
                                        CustomCommandCallback lCallback, void* lpData);
        void GetRoadRulesHighScores(u32 luTimeStamp, s32 liNumScores,          // @ 0x8258F900
                                    s32 liStartIndex,
                                    CustomCommandCallback lCallback, void* lpData);
        void GetLocalRoadRulesHighScores(s32 liNumScoresToDownload,            // @ 0x8258FB10
                                         s32 liNextRoadIndexToDownload,
                                         const char* lpcRoadRulesServerKey,
                                         CustomCommandCallback lCallback, void* lpData);
        void GetFriendsListFromServer(CustomCommandCallback lCallback, void* lpData); // @ 0x8258FD18
        void UpdateServerFriendsRecord(const CgsNetwork::PlayerName* lpaPlayerNamesToAdd,    // @ 0x8258FE78
                                       const CgsNetwork::PlayerName* lpaPlayerNamesToRemove,
                                       s32 liNumPlayersToAdd, s32 liNumPlayersToRemove,
                                       CustomCommandCallback lCallback, void* lpData);
        void OverWriteServerFriendsRecord(const CgsNetwork::PlayerName* lpaFriendsNames,     // @ 0x825902B0
                                          s32 liNumBuddies);
        void UploadOfflineProgress(                                           // @ 0x82590908
            const NetworkInOfflineProgression::OfflineProgressionT* lpOfflineProgression,
            s32 liFreeburnChallengeSuccessCount);
        void UploadLiveRevengeData(const RivalDataT* lpRivalData, const s32* lpiRivalIDs,    // @ 0x82590A88
                                   s32 liNumberToUpload);

        // Declared-only (bodied in other TUs; not part of this TU's function set):
        void UploadFreeBurnLobbyStats(const OnlineGameResults* lpResults,
                                      s32 liArg2, s32 liArg3);

    private:
        // SendCustomCommand @ 0x8258F028 -- common tail: stamp the action, open the action
        // log scope (StartActionCore) and post lpcMessageData via LobbyApiRequestCB.
        void SendCustomCommand(EAction leNewAction, const char* lpcMessageData);

        // HandleIncomingMessage @ 0x82589C70 -- parse a completed lobby reply, dispatch on
        // its message kind, then run the stored CustomCommandCallback.
        void HandleIncomingMessage(::LobbyApiMsgT* lpMsg);

        // EndAction @ 0x... (declared-only; bodied in another TU).
        void EndAction(s32 liError, s32 liArg2);

        // ConvertError @ 0x82583888 -- map a {kind, code} fourcc pair to a server-interface
        // error value using the static KA_CUSTOM_COMMAND_ERROR_MAPPING table.
        int ConvertError(int liMessageKind, int liMessageCode);

        // _CustomCommandSentCallback @ 0x8258F020 -- the LobbyApiRequestCB completion
        // trampoline: forwards to HandleIncomingMessage(lpUserData, lpMsg). Signature must
        // match DirtySDK LobbyApiCallbackT so it can be handed to LobbyApiRequestCB.
        static void _CustomCommandSentCallback(::LobbyApiRefT* lpLobbyApi,
                                               ::LobbyApiMsgT* lpMsg,
                                               void* lpUserData);

        // ---- data members (after the 4-word component base) ----
        CgsNetwork::ServerInterfaceDirtySock* mpServerInterface;   // +0x10
        s32*                                  mpiRaceElo;          // +0x14
        s32*                                  mpiRoadRageElo;      // +0x18
        s32*                                  mpiBurningHomeRunElo; // +0x1C
        EAction                               meCurrentAction;     // +0x20
        CustomCommandCallback                 mpCallback;          // +0x24
        void*                                 mpCallbackData;      // +0x28
    };
}

#endif // BRN_SERVER_INTERFACE_CUSTOM_COMMANDS_H
