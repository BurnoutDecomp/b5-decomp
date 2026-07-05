#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/X360/CgsServerInterfaceGamesX360.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/X360/CgsServerInterfaceGameParamsX360.h" // ServerInterfaceGameParamsX360 (LIVE contexts + slot counts)
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h"           // ServerInterfaceDirtySock (GetMessageBuffer/GetConnAPIRef/GetLobbyAPIRef/OnEvent)
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGameParams.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerParams.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGameSearchParams.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceQuickJoinParams.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceEndGameData.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"      // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h" // CgsCore::SPrintf
#include "lobbytagfield.h"                              // DirtySDK TagFieldFind / TagFieldSetNumber

// Reconstructed from BURNOUT_X360_ARTIST.XEX (CgsServerInterfaceGamesX360.cpp).
//   Construct              @ 0x8287F738   (tail-call to base)
//   OnEvent                @ 0x828997B8   (tail-call to base)
//   Prepare                @ 0x828997B0   (tail-call to base)
//   Release                @ 0x8288C0C8   (tail-call to base)
//   Update                 @ 0x8287F748
//   Suspend                @ 0x8288C0D0
//   Resume                 @ 0x8288C128
//   CreateGame             @ 0x8288C178
//   JoinGame               @ 0x8288C268
//   QuickJoinGame          @ 0x8288C300
//   SearchForGames         @ 0x8288C460
//   EndGame                @ 0x8288C5B8
//   UpdateGameParameters   @ 0x8288C770
//   SetSessionFlags        @ 0x8287F7B0
//   ReceivedGameEvent      @ 0x8288C808
//   GetPlayerXUIDByID      @ 0x8288C880
//   scalar deleting dtor   @ 0x827DFC88
//
// The X360 leaf injects the Xbox LIVE matchmaking context plumbing on top of the
// shared ServerInterfaceGames; it reuses the committed ServerInterfaceGameParamsX360
// / ServerInterfaceGameSearchParamsX360 / ServerInterfaceQuickJoinParamsX360 /
// ServerInterfaceEndGameDataX360 leaves and the committed LobbyApiPlayT play record.

// ---- External DirtySDK / XDK / lobby C-API entry points used by this leaf. -------
// These live in the DirtySDK vendor SDK / the Xbox 360 XDK (not reconstructed in
// vendor/). A not-yet-homed callee is satisfied by its declaration under cl /c; no
// body is needed. Mirrors the sibling CgsServerInterfacePlayerInfo.cpp extern block.
struct ConnApiClientListT; // opaque DirtySDK client list (word[0]=count; entries @ +0x58 stride 0xBC)
struct LobbyApiRefT;       // opaque DirtySDK lobby API ref (matches CgsServerInterfaceDirtySock.h)

namespace CgsNetwork { namespace DirtySock { struct ConnApiRefT; } }

extern "C"
{
    // XDK Xbox LIVE matchmaking context setter.
    void XUserSetContext(u32 uUserIndex, u32 uContextId, u32 uContextValue);

    // DirtySDK ConnApi controls / client-list glue.
    s32   ConnApiControl(CgsNetwork::DirtySock::ConnApiRefT* pConn, s32 iControl, s32 a3, s32 a4, const void* a5);
    s32   ConnApiStatus(CgsNetwork::DirtySock::ConnApiRefT* pConn, s32 iSelect, void* pBuf, s32 iBufLen);
    s32   ConnApiStop(CgsNetwork::DirtySock::ConnApiRefT* pConn);
    ConnApiClientListT* ConnApiGetClientList(CgsNetwork::DirtySock::ConnApiRefT* pConn);

    // Documented client-list accessors (list word[0]=count; entry @ +0x58 stride 0xBC).
    // Opaque list reached only through these helpers, never as a fabricated sized struct.
    s32         ConnApiClientList_GetNumClients(const ConnApiClientListT* pList);
    const char* ConnApiClientList_GetEntry(const ConnApiClientListT* pList, s32 liIndex);

    // DirtySDK lobby name compare + host-address decode.
    s32  LobbyNameCmp(const char* pNameA, const char* pNameB);
    s32  DirtyAddrToHostAddr(void* pHostAddr, s32 iLen, const void* pField);

    // DirtySDK display-list ordering used by Update / Resume.
    s32  DispListChange(void* pList, s32 a2);
    s32  DispListOrder(void* pList);
    s32  DispListSort(void* pList, void* pUserData, s32 a3, void* pfnCompare);

    // DirtySDK lobby request cancel used by Suspend.
    s32  LobbyApiCancelCB(LobbyApiRefT* pLobbyApi, s32 iCallbackID);
}

namespace CgsNetwork
{
    // The shared debug-log stream (X360 off_82F335C8). Bodied in the base games TU
    // (CgsServerInterfaceGames.cpp); declared here so the leaf's EndGame call sites
    // link. Declared-not-defined in this TU is acceptable under the compile gate.
    void DirtySockDebugLog(const char* lpcText);

    namespace
    {
        // The shared message-buffer size (mirrors the base component's anon-namespace
        // constant; not visible across TUs, so redefined leaf-local).
        const s32 KI_MESSAGE_BUFFER_LEN = 2048;
    }
}

namespace CgsNetwork
{
    // ===================================================================
    // Forwarding lifecycle overrides (single `b` tail-call to the base).
    // ===================================================================

    // Construct @ 0x8287F738 -- single `b` tail-call to the base; pure forwarding
    // override that only occupies the X360 leaf's vtable slot.
    void ServerInterfaceGamesX360::Construct()
    {
        ServerInterfaceGames::Construct();
    }

    // OnEvent @ 0x828997B8 -- single `b` tail-call to the base; pure forwarding override.
    void ServerInterfaceGamesX360::OnEvent(EServerInterfaceEvent leEvent, void* lpData)
    {
        ServerInterfaceGames::OnEvent(leEvent, lpData);
    }

    // Prepare @ 0x828997B0 -- bare tail-call thunk to the base.
    bool ServerInterfaceGamesX360::Prepare(ServerInterfaceDirtySock* lpServerInterface)
    {
        return ServerInterfaceGames::Prepare(lpServerInterface);
    }

    // Release @ 0x8288C0C8 -- bare tail-call thunk to the base.
    bool ServerInterfaceGamesX360::Release()
    {
        return ServerInterfaceGames::Release();
    }

    // ===================================================================
    // Found-games display-list lifecycle.
    // ===================================================================

    // Update @ 0x8287F748 -- when the found-games list has a pending change, re-order
    // it and raise the "update game params" event up through the server interface.
    void ServerInterfaceGamesX360::Update()
    {
        if (mpFoundGames != 0)
        {
            if (DispListChange(mpFoundGames, 0))
            {
                DispListOrder(mpFoundGames);
                mpServerInterface->OnEvent(static_cast<EServerInterfaceEvent>(7), 0);
            }
        }
    }

    // Suspend @ 0x8288C0D0 -- free the found-games display list; if an action is in
    // flight, cancel the outstanding lobby request and end the action.
    void ServerInterfaceGamesX360::Suspend(s32 liUpdateFlags)
    {
        (void)liUpdateFlags;
        ServerInterfaceGames::FreeDisplayLists();

        if (meCurrentAction != E_ACTION_COUNT)
        {
            LobbyApiCancelCB(mpServerInterface->GetLobbyAPIRef(), miRequestCallbackID);
            EndAction(0);
        }
    }

    // Resume @ 0x8288C128 -- rebuild the found-games display list; if a search-result
    // sort callback is registered, re-sort the found-games list through FoundGamesSort.
    void* ServerInterfaceGamesX360::Resume()
    {
        // asm: r3 = AllocDisplayLists(this) is the no-callback return. The committed base
        // declares AllocDisplayLists() as void (homed in a sibling wave), so its result
        // cannot be surfaced here; this lifecycle-slot return is ignored by callers.
        AllocDisplayLists();
        if (mpSearchSortCallback)
        {
            return reinterpret_cast<void*>(
                DispListSort(mpFoundGames, this, 0, reinterpret_cast<void*>(&FoundGamesSort)));
        }
        return 0;
    }

    // ===================================================================
    // Action overrides (seed the Xbox LIVE search contexts, then chain to base).
    // ===================================================================

    // CreateGame @ 0x8288C178 -- seed the Xbox LIVE search contexts (XUserSetContext)
    // and the ConnApi 'slot' public/private slot counts, then chain to the base.
    void ServerInterfaceGamesX360::CreateGame(ServerInterfaceGameParamsBase* lpGameParams,
                                              ServerInterfacePlayerParamsBase* lpPlayerParams)
    {
        char* lpcBuffer = GetServerInterface()->GetMessageBuffer();
        CGS_ASSERT(lpcBuffer != 0, "mpacMessageBuffer");
        lpcBuffer[0] = 0;

        // On X360 the concrete game-params object is the committed leaf that carries
        // the LIVE matchmaking context array (maRankedContexts) + counters.
        ServerInterfaceGameParamsX360* lpX360 =
            static_cast<ServerInterfaceGameParamsX360*>(lpGameParams);

        CGS_ASSERT(lpX360->GetContextCount() < KI_MAX_SEARCH_PARAMS,
                   "lpGameParams->miNumSearchContexts < KI_MAX_SEARCH_PARAMS");

        for (s32 i = 0; i < lpX360->GetContextCount(); ++i)
        {
            const RankedContext& lrContext = lpX360->GetContext(i);
            XUserSetContext(lpX360->GetContextUserIndex(),
                            lrContext.muContextId,
                            lrContext.muValue);
        }

        // 'slot' == 0x736C6F74: set the public / private slot counts on the ConnApi.
        ConnApiControl(GetServerInterface()->GetConnAPIRef(), KI_CONN_CTRL_SLOT,
                       lpX360->GetNumPublicSlots(), lpX360->GetNumPrivateSlots(), 0);

        ServerInterfaceGames::CreateGame(lpGameParams, lpPlayerParams);
    }

    // JoinGame @ 0x8288C268 -- assert the search-context count, then chain to the base
    // (no XUserSetContext loop, unlike CreateGame).
    void ServerInterfaceGamesX360::JoinGame(ServerInterfaceGameParamsBase* lpGameParams,
                                            ServerInterfacePlayerParamsBase* lpPlayerParams)
    {
        char* lpcBuffer = GetServerInterface()->GetMessageBuffer();
        CGS_ASSERT(lpcBuffer != 0, "mpacMessageBuffer");
        lpcBuffer[0] = 0;

        ServerInterfaceGameParamsX360* lpX360 =
            static_cast<ServerInterfaceGameParamsX360*>(lpGameParams);
        CGS_ASSERT(lpX360->GetContextCount() < KI_MAX_SEARCH_PARAMS,
                   "lpGameParams->miNumSearchContexts < KI_MAX_SEARCH_PARAMS");

        ServerInterfaceGames::JoinGame(lpGameParams, lpPlayerParams);
    }

    // QuickJoinGame @ 0x8288C300 -- emit the Xbox LIVE search contexts into the outgoing
    // lobby record before chaining to the shared QuickJoinGame.
    void ServerInterfaceGamesX360::QuickJoinGame(ServerInterfaceQuickJoinParamsBase* lpQuickJoinParams,
                                                 ServerInterfacePlayerParamsBase* lpPlayerParams)
    {
        char* lpcBuffer = mpServerInterface->GetMessageBuffer();
        CGS_ASSERT(lpcBuffer != 0, "mpacMessageBuffer");

        ServerInterfaceQuickJoinParamsX360* lpParams =
            static_cast<ServerInterfaceQuickJoinParamsX360*>(lpQuickJoinParams);
        const s32 liNumContexts = static_cast<s32>(lpParams->muX360Field_9C); // +0x9C

        *lpcBuffer = 0;
        CGS_ASSERT(liNumContexts < KI_MAX_SEARCH_PARAMS,
                   "lpSearchParams->miNumSearchContexts < KI_MAX_SEARCH_PARAMS");

        TagFieldSetNumber(lpcBuffer, KI_MESSAGE_BUFFER_LEN, "XNCONTEXTS", liNumContexts);
        TagFieldSetNumber(lpcBuffer, KI_MESSAGE_BUFFER_LEN, "XNRANK", lpParams->IsRanked() ? 1 : 0); // +0x48

        if (liNumContexts > 0)
        {
            // Context {id,value} array begins at maX360Payload[0] (+0x4C); stride 8.
            const u32* lpauContext = reinterpret_cast<const u32*>(&lpParams->maX360Payload[0]);
            for (s32 i = 0; i < liNumContexts; ++i)
            {
                char lacKey[32];
                CgsCore::SPrintf(lacKey, 32, "XNCONTEXTID%d", i);
                TagFieldSetNumber(lpcBuffer, KI_MESSAGE_BUFFER_LEN, lacKey, lpauContext[0]);
                CgsCore::SPrintf(lacKey, 32, "XNCONTEXTVALUE%d", i);
                TagFieldSetNumber(lpcBuffer, KI_MESSAGE_BUFFER_LEN, lacKey, lpauContext[1]);
                lpauContext += 2;
            }
        }

        ServerInterfaceGames::QuickJoinGame(lpQuickJoinParams, lpPlayerParams);
    }

    // SearchForGames @ 0x8288C460 -- emit the Xbox LIVE search contexts into the outgoing
    // lobby record before chaining to the shared SearchForGames.
    void ServerInterfaceGamesX360::SearchForGames(ServerInterfaceGameSearchParamsBase* lpSearchParams)
    {
        char* lpcBuffer = mpServerInterface->GetMessageBuffer();
        CGS_ASSERT(lpcBuffer != 0, "mpacMessageBuffer");
        *lpcBuffer = 0;

        ServerInterfaceGameSearchParamsX360* lpParams =
            static_cast<ServerInterfaceGameSearchParamsX360*>(lpSearchParams);
        const s32 liNumContexts    = static_cast<s32>(lpParams->muX360Field_68); // +0x68
        const u32 luGameFlagsValue = lpParams->GetGameFlagsValue();              // +0x10

        CGS_ASSERT(liNumContexts < KI_MAX_SEARCH_PARAMS,
                   "lpSearchParams->miNumSearchContexts < KI_MAX_SEARCH_PARAMS");

        TagFieldSetNumber(lpcBuffer, KI_MESSAGE_BUFFER_LEN, "XNCONTEXTS", liNumContexts);
        TagFieldSetNumber(lpcBuffer, KI_MESSAGE_BUFFER_LEN, "XNRANK",
                          ((luGameFlagsValue >> 10) & 1) != 0 ? 1 : 0);

        if (liNumContexts > 0)
        {
            // Context {id,value} array begins at maX360Payload[0] (+0x18); stride 8.
            const u32* lpauContext = reinterpret_cast<const u32*>(&lpParams->maX360Payload[0]);
            for (s32 i = 0; i < liNumContexts; ++i)
            {
                char lacKey[32];
                CgsCore::SPrintf(lacKey, 32, "XNCONTEXTID%d", i);
                TagFieldSetNumber(lpcBuffer, KI_MESSAGE_BUFFER_LEN, lacKey, lpauContext[0]);
                CgsCore::SPrintf(lacKey, 32, "XNCONTEXTVALUE%d", i);
                TagFieldSetNumber(lpcBuffer, KI_MESSAGE_BUFFER_LEN, lacKey, lpauContext[1]);
                lpauContext += 2;
            }
        }

        ServerInterfaceGames::SearchForGames(lpSearchParams);
    }

    // UpdateGameParameters @ 0x8288C770 -- host-only; clear the message buffer, chain
    // to the base updater.
    void ServerInterfaceGamesX360::UpdateGameParameters(ServerInterfaceGameParamsBase* lpGameParams)
    {
        CGS_ASSERT(IsLocalPlayerHost(), "IsLocalPlayerHost()");
        CGS_ASSERT(mpServerInterface->GetMessageBuffer() != 0, "mpacMessageBuffer");

        *mpServerInterface->GetMessageBuffer() = 0;

        ServerInterfaceGames::UpdateGameParameters(lpGameParams);
    }

    // EndGame @ 0x8288C5B8 -- post each in-game client's end-of-game stat value
    // ('skil') through the ConnApi client list, then stop the ConnApi session.
    void ServerInterfaceGamesX360::EndGame(ServerInterfaceEndGameDataBase* lpEndGameData)
    {
        CGS_ASSERT(IsLocalPlayerInGame(), "IsLocalPlayerInGame()");

        ConnApiClientListT* lpClientList =
            ConnApiGetClientList(GetServerInterface()->GetConnAPIRef());

        if (lpClientList == 0)
        {
            CGS_ASSERT(false, "We have no clients when trying to update stats");
            ConnApiStop(GetServerInterface()->GetConnAPIRef());
            return;
        }

        // The X360 concrete end-game object is the committed leaf; its 16-word result
        // payload holds 8 { playerId, value } pairs.
        ServerInterfaceEndGameDataX360* lpData =
            static_cast<ServerInterfaceEndGameDataX360*>(lpEndGameData);

        for (s32 liPair = 0; liPair < KI_END_GAME_RESULT_PAIRS; ++liPair)
        {
            s32 liPlayerId = static_cast<s32>(lpData->maResultWords[liPair * 2]);
            s32 liValue    = static_cast<s32>(lpData->maResultWords[liPair * 2 + 1]);
            if (liPlayerId == 0)
                continue;

            s32 liIndex = 0;
            if (ConnApiClientList_GetNumClients(lpClientList) > 0)
            {
                while (LobbyNameCmp(reinterpret_cast<const char*>(liPlayerId),
                                    ConnApiClientList_GetEntry(lpClientList, liIndex)) != 0)
                {
                    ++liIndex;
                    if (liIndex >= ConnApiClientList_GetNumClients(lpClientList))
                        break;
                }

                if (liIndex < ConnApiClientList_GetNumClients(lpClientList))
                {
                    // 'skil' == 0x736B696C: post this client's end-of-game stat value.
                    ConnApiControl(GetServerInterface()->GetConnAPIRef(), KI_CONN_CTRL_SKILL,
                                   liIndex, liValue, 0);
                }
            }

            if (liIndex == ConnApiClientList_GetNumClients(lpClientList))
            {
                DirtySockDebugLog("CgsNetwork::ServerInterfaceGamesX360::EndGame");
                DirtySockDebugLog(": Failed to find ");
                DirtySockDebugLog(reinterpret_cast<const char*>(liPlayerId));
                DirtySockDebugLog(" in the client list\n");
            }
        }

        ConnApiStop(GetServerInterface()->GetConnAPIRef());
    }

    // ===================================================================
    // X360 queries / session control.
    // ===================================================================

    // SetSessionFlags @ 0x8287F7B0 -- OR session-flag bits into the ConnApi session
    // flags ('sflg'), preserving the untouched bits.
    void ServerInterfaceGamesX360::SetSessionFlags(s32 liFlags)
    {
        CGS_ASSERT(mpServerInterface->GetConnAPIRef() != 0,
                   "mpServerInterface->GetConnAPIRef()");

        s32 liStatus = ConnApiStatus(mpServerInterface->GetConnAPIRef(),
                                     KI_CONN_SESSION_FLAGS, 0, 0);
        ConnApiControl(mpServerInterface->GetConnAPIRef(),
                       KI_CONN_SESSION_FLAGS,
                       (liStatus & 0xFFFFF0FF) | liFlags, 0, 0);
    }

    // ReceivedGameEvent @ 0x8288C808 -- for create/join/quick-join (meCurrentAction <= 2)
    // end the action once the lobby message carries a "SESS" tagfield; otherwise chain
    // to the base.
    s32 ServerInterfaceGamesX360::ReceivedGameEvent(s32* lpauMsg)
    {
        if (static_cast<u32>(meCurrentAction) > 2u)
            return ServerInterfaceGames::ReceivedGameEvent(this, lpauMsg);

        if (TagFieldFind(reinterpret_cast<const char*>(lpauMsg[4]), "SESS") != 0) // msg->pData (+0x10)
        {
            EndAction(lpauMsg[3]);   // msg error field (+0xC); base EndAction is void
            return lpauMsg[3];
        }
        return 0;
    }

    // GetPlayerXUIDByID @ 0x8288C880 -- find the player with liPlayerID in the current
    // play record and decode their 8-byte secure XUID into lpXUIDOut.
    bool ServerInterfaceGamesX360::GetPlayerXUIDByID(s32 liPlayerID, void* lpXUIDOut)
    {
        CGS_ASSERT(IsLocalPlayerInGame(), "Calling GetPlayerXUIDByIndex when not in a game");
        CGS_ASSERT(lpXUIDOut != 0, "No XUID pointer supplied to GetXUIDOfGamerInGame");

        DirtySock::LobbyApiPlayT* lpRecord = GetLastGameRecord();
        for (s32 i = 0; i < KI_MAX_GAME_PLAYERS; ++i)
        {
            if (lpRecord->aPlayers[i].iIdent == liPlayerID)
            {
                // The 8-byte secure XUID lives at +0x1C inside the 164-byte player
                // record (within the reserved mPad24 region).
                const u8* lpXUIDField =
                    reinterpret_cast<const u8*>(&lpRecord->aPlayers[i]) + 0x1C;
                DirtyAddrToHostAddr(lpXUIDOut, 8, lpXUIDField);
                return true;
            }
        }
        return false;
    }

    // ===================================================================
    // scalar deleting destructor @ 0x827DFC88 -- empty; the leaf owns no heap
    // members. MSVC synthesises the vptr-restore + conditional operator delete.
    // ===================================================================
    ServerInterfaceGamesX360::~ServerInterfaceGamesX360()
    {
    }
}
