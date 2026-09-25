// ============================================================================
// pclan_lobby.cpp -- the PC LAN lobby authority.
//
// [PC platform layer] Not console code. The console kept every game record on the online
// lobby server; with BP_LAN=1 the instance that creates a game keeps its record here and
// answers for it over the PC LAN transport (pclan.h):
//   'gcre'/'gpsc' host: build the record from the request, become its authority, post the
//                 lobby 'game' event (the game manager and the games component read it).
//   'gsea'        1 s DISCOVER sweep; every ADVERT goes into lobby list 3; then COUNT=n.
//   'gjoi'        JOIN_REQ to the host of a game the last sweep found; JOIN_RESP 0 or a
//                 lobby error ('ugam' 'full' 'lock' 'pass'); 'gqwk' sweeps first and joins
//                 the first open game, or answers 'nfnd'.
//   'gset' 'gsta' 'glea'/'gdel' 'rank' apply on the host (a member forwards them as LOBBYREQ);
//                 the host sends PLAY (the record) to its members on every change and once a
//                 second; 'gsta' sends the 'play' event.
//   KICK / LEAVE / BYE end a membership.
//   Host handover: when the host leaves ('glea', or the lobby disconnect) it does not end the game
//                 for everyone; it names the next host in a HANDOVER to every member. A member that
//                 loses the host without one (its BYE, a transport timeout) applies the same rule to
//                 the record it holds. The next host is the first remaining player of the record
//                 (record order is join order), so every member reaches the same answer without a
//                 vote. It takes the record over (member table from the peer book, HOST changed) and
//                 publishes it; the others retarget their host peer. The games component and the
//                 game manager then see an ordinary 'game' event whose HOST changed, as they did when
//                 the lobby server moved a game to a new host. A host's 'gdel' (the game's own
//                 leave of a hosted game) hands over the same way while others are in the record,
//                 and ends the game only when the host is alone.
// Records travel as the lobby's tagfield game-record text (the text LobbyApiExtractPlayRecord
// decodes) plus, per player, the PC-only PORT<n> field (the transport port, so members can
// open game links to each other). All state is this layer's own; single-threaded.
// ============================================================================

#include "pclan.h"
#include "pclan_lobby.h"
#include "pclan_link.h"   // PcNetConnByeAdd / PcNetConnByeDel (the PCLAN_BYE fan-out)

#include "lobbyapi.h"
#include "lobbytagfield.h"
#include "dirtyaddr.h"
#include "dirtylib.h"
#include "platform.h"   // ds_snzprintf

#include "GameShared/GameClasses/System/PC/CgsPcNetIdentity.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::WriteToLog

#include <windows.h>    // GetTickCount

#include <cstring>

namespace
{
    constexpr s32 FourCC(char a, char b, char c, char d)
    {
        return (s32)(((u32)(u8)a << 24) | ((u32)(u8)b << 16) | ((u32)(u8)c << 8) | (u32)(u8)d);
    }

    // requests the authority answers
    const s32 KI_REQ_CREATE     = FourCC('g', 'c', 'r', 'e');
    const s32 KI_REQ_CREATE_GPS = FourCC('g', 'p', 's', 'c');
    const s32 KI_REQ_JOIN       = FourCC('g', 'j', 'o', 'i');
    const s32 KI_REQ_QUICK      = FourCC('g', 'q', 'w', 'k');
    const s32 KI_REQ_SEARCH     = FourCC('g', 's', 'e', 'a');
    const s32 KI_REQ_LEAVE      = FourCC('g', 'l', 'e', 'a');
    const s32 KI_REQ_DELETE     = FourCC('g', 'd', 'e', 'l');
    const s32 KI_REQ_SET        = FourCC('g', 's', 'e', 't');
    const s32 KI_REQ_START      = FourCC('g', 's', 't', 'a');
    const s32 KI_REQ_RANK       = FourCC('r', 'a', 'n', 'k');

    // lobby error codes
    const s32 KI_ERR_UNKNOWN_GAME = FourCC('u', 'g', 'a', 'm');
    const s32 KI_ERR_FULL         = FourCC('f', 'u', 'l', 'l');
    const s32 KI_ERR_LOCKED       = FourCC('l', 'o', 'c', 'k');
    const s32 KI_ERR_PASSWORD     = FourCC('p', 'a', 's', 's');
    const s32 KI_ERR_IN_GAME      = FourCC('i', 'n', 'g', 'm');
    const s32 KI_ERR_NOT_IN_GAME  = FourCC('n', 'g', 'a', 'm');
    const s32 KI_ERR_NOT_FOUND    = FourCC('n', 'f', 'n', 'd');

    // lobby events
    const s32 KI_EVENT_GAME = FourCC('g', 'a', 'm', 'e');
    const s32 KI_EVENT_PLAY = FourCC('p', 'l', 'a', 'y');
    const s32 KI_EVENT_KICK = FourCC('k', 'i', 'c', 'k');
    const s32 KI_EVENT_USER = FourCC('u', 's', 'e', 'r');

    // the SYSFLAGS bit LockGame sets on the wire record
    const u32 KU_SYSFLAG_LOCKED = 0x1000u;

    // kick reasons (CgsNetwork::EKickReason order)
    const s32 KI_KICK_NOGAME          = 1;
    const s32 KI_KICK_LOST_CONNECTION = 4;

    const u32 KU_SEARCH_MS     = 1000;   // DISCOVER sweep length
    const u32 KU_RESWEEP_MS    = 500;    // second broadcast inside the sweep
    const u32 KU_JOIN_MS       = 6000;   // JOIN_RESP wait (past the transport's 5 s peer timeout)
    const u32 KU_PLAY_EVERY_MS = 1000;   // host record refresh

    const s32 KI_MAX_PLAYERS = 9;
    const s32 KI_MAX_FOUND   = 16;
    const s32 KI_TEXT_LEN    = 4096;
    const s32 KI_REQUEST_LEN = 2048;
    const s32 KI_LOG_BUDGET  = 96;

    struct MemberT
    {
        u32        uIdent;   // 0 == free
        u64        uXuid;
        PcLanPeerT Peer;
    };

    struct FoundT
    {
        LobbyApiPlayT Play;
        PcLanPeerT    Host;
        u32           uHostIdent;
    };

    struct StateT
    {
        LobbyApiRefT*  pLobbyApi;

        // the current game
        bool           bInGame;
        bool           bHost;
        u32            uGameIdent;
        u32            uHostIdent;
        PcLanPeerT     HostPeer;
        LobbyApiPlayT  Play;
        char           strPassword[36];
        MemberT        aMembers[KI_MAX_PLAYERS];
        u32            uLastPlaySend;
        u32            uGameSeqn;
        char           strLastText[KI_TEXT_LEN];   // member: last record text posted

        // pending requests
        s32            iSearchId;       // 'gsea' or the sweep of 'gqwk'
        bool           bQuickJoin;
        bool           bResweep;
        u32            uSearchStart;
        s32            iJoinId;         // 'gjoi' / 'gqwk' waiting for JOIN_RESP
        u32            uJoinStart;
        u32            uJoinGame;
        char           strJoinRequest[KI_REQUEST_LEN];

        FoundT         aFound[KI_MAX_FOUND];
        s32            iNumFound;
    };

    StateT g_Lobby;
    s32    g_iLogBudget = KI_LOG_BUDGET;   // log lines left, whole process

    // ---- small helpers ---------------------------------------------------------------------

    void _Log(const char* pText, s32 iValue)
    {
        if (g_iLogBudget > 0)
        {
            g_iLogBudget -= 1;
            // Into the game log (NetPrintf only reaches the debugger): the pair cases read it.
            char strLine[160];
            ds_snzprintf(strLine, sizeof(strLine), "[net] pclan_lobby: %s %d\n", pText, iValue);
            CgsDev::Log::WriteToLog(strLine);
        }
    }

    u32 _Now()
    {
        return GetTickCount();
    }

    void _Put32(u8* pDst, u32 uValue)
    {
        memcpy(pDst, &uValue, sizeof(uValue));
    }

    void _Put64(u8* pDst, u64 uValue)
    {
        memcpy(pDst, &uValue, sizeof(uValue));
    }

    u32 _Get32(const u8* pSrc)
    {
        u32 uValue;
        memcpy(&uValue, pSrc, sizeof(uValue));
        return uValue;
    }

    u64 _Get64(const u8* pSrc)
    {
        u64 uValue;
        memcpy(&uValue, pSrc, sizeof(uValue));
        return uValue;
    }

    // Copy the NUL-terminated text at pBody[iOffset..iLen) into pText.
    void _BodyText(const u8* pBody, s32 iLen, s32 iOffset, char* pText, s32 iTextLen)
    {
        s32 iCount = iLen - iOffset;
        if (iCount < 0)
        {
            iCount = 0;
        }
        if (iCount > iTextLen - 1)
        {
            iCount = iTextLen - 1;
        }
        memcpy(pText, pBody + iOffset, (size_t)iCount);
        pText[iCount] = 0;
    }

    u64 _XuidOf(const char* pMachineAddr)
    {
        DirtyAddrT Addr;
        u64 uXuid = 0;
        memset(&Addr, 0, sizeof(Addr));
        strncpy(Addr.strMachineAddr, pMachineAddr, sizeof(Addr.strMachineAddr) - 1);
        if (Addr.strMachineAddr[0] == '$')
        {
            DirtyAddrToHostAddr(&uXuid, sizeof(uXuid), &Addr);
        }
        return uXuid;
    }

    // ---- player colour slots ----------------------------------------------------------------
    // The lobby server gave every player of a game a colour slot 0..7: byte 15 of the player's
    // USERPARAMS structure (pattern "13sbbblll": car id, team, flags, colour, marked player,
    // rank, car colour). No game code writes that byte: every writer Prepares it to -1 and
    // sends it back unchanged, and the HUD, the map icons and the stunt-run team assignment
    // read it as the player's lobby colour (out of range asserts every frame). The authority
    // stamps it: a player keeps its slot for as long as it is in the record, a new player
    // takes the lowest free one.
    const char* const KS_USERPARAMS_PATTERN = "13sbbblll";
    const s32         KI_USERPARAMS_SIZE    = 28;
    const s32         KI_USERPARAMS_COLOUR  = 15;
    const s32         KI_MAX_COLOURS        = 8;

    s32 _ParamsColour(const char* pParams)
    {
        u8 aData[KI_USERPARAMS_SIZE];
        memset(aData, 0xFF, sizeof(aData));
        if ((pParams == NULL) || (pParams[0] == 0))
        {
            return -1;
        }
        TagFieldGetStructure(pParams, aData, sizeof(aData), KS_USERPARAMS_PATTERN);
        return (s32)(s8)aData[KI_USERPARAMS_COLOUR];
    }

    void _ParamsSetColour(char* pParams, s32 iParamsLen, s32 iColour)
    {
        u8 aData[KI_USERPARAMS_SIZE];
        char strText[128];
        if (pParams[0] == 0)
        {
            return;
        }
        memset(aData, 0, sizeof(aData));
        TagFieldGetStructure(pParams, aData, sizeof(aData), KS_USERPARAMS_PATTERN);
        aData[KI_USERPARAMS_COLOUR] = (u8)(s8)iColour;
        strText[0] = 0;
        const s32 iLen = TagFieldSetStructure(strText, sizeof(strText), NULL, aData, sizeof(aData),
                                              KS_USERPARAMS_PATTERN);
        if ((iLen > 0) && (iLen < iParamsLen))
        {
            memcpy(pParams, strText, (size_t)iLen + 1);
        }
    }

    // Give player iPlayer of the record a colour slot: iKeep when it is a slot no other player
    // holds, else the lowest free one.
    void _HostStampColour(LobbyApiPlayT* pPlay, s32 iPlayer, s32 iKeep)
    {
        u32 uUsed = 0;
        for (s32 iOther = 0; iOther < pPlay->iCount; ++iOther)
        {
            const s32 iColour = (iOther == iPlayer) ? -1 : _ParamsColour(pPlay->aOpponents[iOther].strParams);
            if ((iColour >= 0) && (iColour < KI_MAX_COLOURS))
            {
                uUsed |= 1u << iColour;
            }
        }
        s32 iColour = iKeep;
        if ((iColour < 0) || (iColour >= KI_MAX_COLOURS) || ((uUsed & (1u << iColour)) != 0))
        {
            for (iColour = 0; (iColour < KI_MAX_COLOURS) && ((uUsed & (1u << iColour)) != 0); ++iColour)
            {
            }
        }
        if (iColour < KI_MAX_COLOURS)
        {
            _ParamsSetColour(pPlay->aOpponents[iPlayer].strParams,
                             (s32)sizeof(pPlay->aOpponents[iPlayer].strParams), iColour);
        }
    }

    // The record as lobby tagfield text, plus PORT<n> per player (0 when unknown).
    s32 _FormatPlay(const LobbyApiPlayT* pPlay, const u16* pPorts, char* pText, s32 iTextLen)
    {
        char strKey[32];
        pText[0] = 0;
        TagFieldSetNumber(pText, iTextLen, "IDENT", pPlay->iIdent);
        TagFieldSetString(pText, iTextLen, "NAME", pPlay->strName);
        TagFieldSetString(pText, iTextLen, "HOST", pPlay->strHost);
        if (pPlay->strParams[0] != 0)
        {
            TagFieldSetString(pText, iTextLen, "PARAMS", pPlay->strParams);
        }
        if (pPlay->strPlatParams[0] != 0)
        {
            TagFieldSetString(pText, iTextLen, "PLATPARAMS", pPlay->strPlatParams);
        }
        TagFieldSetNumber(pText, iTextLen, "ROOM", pPlay->iRoom);
        TagFieldSetNumber(pText, iTextLen, "CUSTFLAGS", (s32)pPlay->uCustflags);
        TagFieldSetNumber(pText, iTextLen, "SYSFLAGS", (s32)pPlay->uSysflags);
        TagFieldSetNumber(pText, iTextLen, "COUNT", pPlay->iCount);
        TagFieldSetNumber(pText, iTextLen, "PRIV", pPlay->iPrivSlots);
        TagFieldSetNumber(pText, iTextLen, "MINSIZE", pPlay->iMinsize);
        TagFieldSetNumber(pText, iTextLen, "MAXSIZE", pPlay->iMaxsize);
        TagFieldSetNumber(pText, iTextLen, "SEED", (s32)pPlay->uSeed);
        TagFieldSetEpoch(pText, iTextLen, "WHEN", pPlay->uWhen);
        if (pPlay->iGameMode != -1)
        {
            TagFieldSetNumber(pText, iTextLen, "GAMEMODE", pPlay->iGameMode);
        }
        for (s32 iPlayer = 0; iPlayer < pPlay->iCount; ++iPlayer)
        {
            const LobbyApiPlayerT* pPlayer = &pPlay->aOpponents[iPlayer];
            ds_snzprintf(strKey, sizeof(strKey), "OPID%d", iPlayer);
            TagFieldSetNumber(pText, iTextLen, strKey, pPlayer->iIdent);
            ds_snzprintf(strKey, sizeof(strKey), "OPPO%d", iPlayer);
            TagFieldSetString(pText, iTextLen, strKey, pPlayer->strPers);
            ds_snzprintf(strKey, sizeof(strKey), "ADDR%d", iPlayer);
            TagFieldSetAddress(pText, iTextLen, strKey, pPlayer->uAddr);
            ds_snzprintf(strKey, sizeof(strKey), "LADDR%d", iPlayer);
            TagFieldSetAddress(pText, iTextLen, strKey, pPlayer->uLocalAddr);
            ds_snzprintf(strKey, sizeof(strKey), "MADDR%d", iPlayer);
            TagFieldSetString(pText, iTextLen, strKey, pPlayer->strMachineAddr);
            if (pPlayer->strParams[0] != 0)
            {
                ds_snzprintf(strKey, sizeof(strKey), "OPPARAM%d", iPlayer);
                TagFieldSetString(pText, iTextLen, strKey, pPlayer->strParams);
            }
            ds_snzprintf(strKey, sizeof(strKey), "OPFLAG%d", iPlayer);
            TagFieldSetNumber(pText, iTextLen, strKey, (s32)pPlayer->uFlags);
            if (pPorts != NULL)
            {
                ds_snzprintf(strKey, sizeof(strKey), "PORT%d", iPlayer);
                TagFieldSetNumber(pText, iTextLen, strKey, pPorts[iPlayer]);
            }
        }
        TagFieldSetString(pText, iTextLen, "SESS", pPlay->strSess);
        return (s32)strlen(pText);
    }

    // Host: the transport port of every player of the record (ours for our own entry).
    void _HostPorts(u16* pPorts)
    {
        const PcLanPeerT Self = PcLanSelf();
        for (s32 iPlayer = 0; iPlayer < KI_MAX_PLAYERS; ++iPlayer)
        {
            pPorts[iPlayer] = 0;
            if (iPlayer >= g_Lobby.Play.iCount)
            {
                continue;
            }
            const u32 uIdent = (u32)g_Lobby.Play.aOpponents[iPlayer].iIdent;
            if (uIdent == CgsPcNetIdentityLobbyIdent())
            {
                pPorts[iPlayer] = Self.uPort;
                continue;
            }
            for (s32 iMember = 0; iMember < KI_MAX_PLAYERS; ++iMember)
            {
                if (g_Lobby.aMembers[iMember].uIdent == uIdent)
                {
                    pPorts[iPlayer] = g_Lobby.aMembers[iMember].Peer.uPort;
                }
            }
        }
    }

    // Remember every other player of a record text in the peer book (ADDR<n> + PORT<n>).
    void _RememberPlayers(const LobbyApiPlayT* pPlay, const char* pText)
    {
        char strKey[32];
        for (s32 iPlayer = 0; (iPlayer < pPlay->iCount) && (iPlayer < KI_MAX_PLAYERS); ++iPlayer)
        {
            const LobbyApiPlayerT* pPlayer = &pPlay->aOpponents[iPlayer];
            if ((u32)pPlayer->iIdent == CgsPcNetIdentityLobbyIdent())
            {
                continue;
            }
            if ((u32)pPlayer->iIdent == g_Lobby.uHostIdent)
            {
                // the host is where its messages come from
                PcLanPeerRemember(_XuidOf(pPlayer->strMachineAddr), g_Lobby.uHostIdent, &g_Lobby.HostPeer);
                continue;
            }
            ds_snzprintf(strKey, sizeof(strKey), "PORT%d", iPlayer);
            const s32 iPort = TagFieldGetNumber(TagFieldFind(pText, strKey), 0);
            if ((iPort <= 0) || (pPlayer->uAddr == 0))
            {
                continue;
            }
            PcLanPeerT Peer;
            memset(&Peer, 0, sizeof(Peer));
            Peer.uAddr = pPlayer->uAddr;
            Peer.uPort = (u16)iPort;
            PcLanPeerRemember(_XuidOf(pPlayer->strMachineAddr), (u32)pPlayer->iIdent, &Peer);
        }
    }

    // Our own player entry from the request fields (USERPARAMS / USERFLAGS).
    void _FillSelfPlayer(LobbyApiPlayerT* pPlayer, const char* pRequest)
    {
        LobbyApiUserT* pSelf = LobbyApiPcSelf(g_Lobby.pLobbyApi);
        memset(pPlayer, 0, sizeof(*pPlayer));
        pPlayer->iIdent     = pSelf->ident;
        memcpy(pPlayer->strPers, pSelf->name, sizeof(pPlayer->strPers));
        pPlayer->uAddr      = PcLanSelf().uAddr;
        pPlayer->uLocalAddr = pPlayer->uAddr;
        memcpy(pPlayer->strMachineAddr, pSelf->MachineAddr.strMachineAddr, sizeof(pPlayer->strMachineAddr));
        TagFieldGetString(TagFieldFind(pRequest, "USERPARAMS"), pPlayer->strParams, sizeof(pPlayer->strParams), "");
        pPlayer->uFlags = (u32)TagFieldGetNumber(TagFieldFind(pRequest, "USERFLAGS"), 0);
    }

    void _ClearGame()
    {
        g_Lobby.bInGame    = false;
        g_Lobby.bHost      = false;
        g_Lobby.uGameIdent = 0;
        g_Lobby.uHostIdent = 0;
        memset(&g_Lobby.HostPeer, 0, sizeof(g_Lobby.HostPeer));
        memset(&g_Lobby.Play, 0, sizeof(g_Lobby.Play));
        memset(g_Lobby.aMembers, 0, sizeof(g_Lobby.aMembers));
        g_Lobby.strPassword[0] = 0;
        g_Lobby.strLastText[0] = 0;
        LobbyApiPcSelf(g_Lobby.pLobbyApi)->game = 0;
    }

    // Host: send the record (event kind 'game' or 'play') to every member, and post it locally.
    void _HostPublish(s32 iEventKind, bool bLocal)
    {
        static char strText[KI_TEXT_LEN];
        static u8   aBody[4 + KI_TEXT_LEN];
        u16 aPorts[KI_MAX_PLAYERS];

        _HostPorts(aPorts);
        const s32 iTextLen = _FormatPlay(&g_Lobby.Play, aPorts, strText, sizeof(strText));
        _Put32(aBody, (u32)iEventKind);
        memcpy(aBody + 4, strText, (size_t)iTextLen + 1);
        for (s32 iMember = 0; iMember < KI_MAX_PLAYERS; ++iMember)
        {
            if (g_Lobby.aMembers[iMember].uIdent != 0)
            {
                if (PcLanSendControl(&g_Lobby.aMembers[iMember].Peer, PCLAN_PLAY, g_Lobby.uGameIdent,
                                     aBody, 4 + iTextLen + 1) < 0)
                {
                    _Log("PLAY send failed, bytes", 4 + iTextLen + 1);
                }
            }
        }
        g_Lobby.uLastPlaySend = _Now();
        if (bLocal)
        {
            LobbyApiPcPostEvent(g_Lobby.pLobbyApi, LOBBYAPI_CBTYPE_EVNT, iEventKind, 0, strText);
        }
    }

    s32 _FindPlayerByIdent(u32 uIdent)
    {
        for (s32 iPlayer = 0; iPlayer < g_Lobby.Play.iCount; ++iPlayer)
        {
            if ((u32)g_Lobby.Play.aOpponents[iPlayer].iIdent == uIdent)
            {
                return iPlayer;
            }
        }
        return -1;
    }

    s32 _FindPlayerByName(const char* pName)
    {
        for (s32 iPlayer = 0; iPlayer < g_Lobby.Play.iCount; ++iPlayer)
        {
            if (strncmp(g_Lobby.Play.aOpponents[iPlayer].strPers, pName,
                        sizeof(g_Lobby.Play.aOpponents[iPlayer].strPers)) == 0)
            {
                return iPlayer;
            }
        }
        return -1;
    }

    // Host: drop a player from the record and the member table.
    void _HostRemovePlayer(u32 uIdent)
    {
        const s32 iPlayer = _FindPlayerByIdent(uIdent);
        if (iPlayer >= 0)
        {
            for (s32 iMove = iPlayer; iMove < g_Lobby.Play.iCount - 1; ++iMove)
            {
                g_Lobby.Play.aOpponents[iMove] = g_Lobby.Play.aOpponents[iMove + 1];
            }
            g_Lobby.Play.iCount -= 1;
            memset(&g_Lobby.Play.aOpponents[g_Lobby.Play.iCount], 0, sizeof(LobbyApiPlayerT));
        }
        for (s32 iMember = 0; iMember < KI_MAX_PLAYERS; ++iMember)
        {
            if (g_Lobby.aMembers[iMember].uIdent == uIdent)
            {
                memset(&g_Lobby.aMembers[iMember], 0, sizeof(g_Lobby.aMembers[iMember]));
            }
        }
    }

    // Host: apply a game-record request ('gset', 'gsta', 'glea' ...) from player uFromIdent.
    // Returns the lobby result code.
    s32 _HostApply(s32 iKind, const char* pRequest, u32 uFromIdent)
    {
        if (iKind == KI_REQ_START)
        {
            _HostPublish(KI_EVENT_PLAY, true);
            return 0;
        }
        if ((iKind == KI_REQ_LEAVE) || (iKind == KI_REQ_DELETE))
        {
            _HostRemovePlayer(uFromIdent);
            _HostPublish(KI_EVENT_GAME, true);
            return 0;
        }
        if (iKind != KI_REQ_SET)
        {
            return 0;
        }

        const char* pField;
        char strName[20];

        // kick a player by persona
        if ((pField = TagFieldFind(pRequest, "KICK")) != NULL)
        {
            TagFieldGetString(pField, strName, sizeof(strName), "");
            const s32 iPlayer = _FindPlayerByName(strName);
            if (iPlayer >= 0)
            {
                const u32 uIdent = (u32)g_Lobby.Play.aOpponents[iPlayer].iIdent;
                u8 aBody[4];
                _Put32(aBody, (u32)TagFieldGetNumber(TagFieldFind(pRequest, "KICK_REASON"), 0));
                for (s32 iMember = 0; iMember < KI_MAX_PLAYERS; ++iMember)
                {
                    if (g_Lobby.aMembers[iMember].uIdent == uIdent)
                    {
                        PcLanSendControl(&g_Lobby.aMembers[iMember].Peer, PCLAN_KICK, g_Lobby.uGameIdent, aBody, 4);
                    }
                }
                _HostRemovePlayer(uIdent);
            }
        }

        // player parameters: the named player, or the requester
        s32 iTarget = _FindPlayerByIdent(uFromIdent);
        if ((pField = TagFieldFind(pRequest, "PERS")) != NULL)
        {
            TagFieldGetString(pField, strName, sizeof(strName), "");
            iTarget = _FindPlayerByName(strName);
        }
        if (iTarget >= 0)
        {
            LobbyApiPlayerT* pPlayer = &g_Lobby.Play.aOpponents[iTarget];
            if ((pField = TagFieldFind(pRequest, "USERPARAMS")) != NULL)
            {
                const s32 iColour = _ParamsColour(pPlayer->strParams);
                TagFieldGetString(pField, pPlayer->strParams, sizeof(pPlayer->strParams), "");
                _HostStampColour(&g_Lobby.Play, iTarget, iColour);
            }
            if ((pField = TagFieldFind(pRequest, "USERFLAGS")) != NULL)
            {
                pPlayer->uFlags = (u32)TagFieldGetNumber(pField, 0);
            }
        }

        // game parameters
        if ((pField = TagFieldFind(pRequest, "NAME")) != NULL)
        {
            TagFieldGetString(pField, g_Lobby.Play.strName, sizeof(g_Lobby.Play.strName), "");
        }
        if ((pField = TagFieldFind(pRequest, "PARAMS")) != NULL)
        {
            TagFieldGetString(pField, g_Lobby.Play.strParams, sizeof(g_Lobby.Play.strParams), "");
        }
        if ((pField = TagFieldFind(pRequest, "SYSFLAGS")) != NULL)
        {
            g_Lobby.Play.uSysflags = (u32)TagFieldGetNumber(pField, 0);
        }
        if ((pField = TagFieldFind(pRequest, "CUSTFLAGS")) != NULL)
        {
            g_Lobby.Play.uCustflags = (u32)TagFieldGetNumber(pField, 0);
        }
        if ((pField = TagFieldFind(pRequest, "MINSIZE")) != NULL)
        {
            g_Lobby.Play.iMinsize = (s8)TagFieldGetNumber(pField, 0);
        }
        if ((pField = TagFieldFind(pRequest, "MAXSIZE")) != NULL)
        {
            g_Lobby.Play.iMaxsize = (s8)TagFieldGetNumber(pField, 0);
        }
        if ((pField = TagFieldFind(pRequest, "PRIV")) != NULL)
        {
            g_Lobby.Play.iPrivSlots = TagFieldGetNumber(pField, 0);
        }
        // the session strings are the host's
        if (((pField = TagFieldFind(pRequest, "SESS")) != NULL) && (uFromIdent == CgsPcNetIdentityLobbyIdent()))
        {
            TagFieldGetString(pField, g_Lobby.Play.strSess, sizeof(g_Lobby.Play.strSess), "");
        }
        if (((pField = TagFieldFind(pRequest, "PLATPARAMS")) != NULL) && (uFromIdent == CgsPcNetIdentityLobbyIdent()))
        {
            TagFieldGetString(pField, g_Lobby.Play.strPlatParams, sizeof(g_Lobby.Play.strPlatParams), "");
        }
        _HostPublish(KI_EVENT_GAME, true);
        return 0;
    }

    // Member: the game is over for us (kicked, host gone, record without us).
    void _MemberDropped(s32 iReason)
    {
        char strReason[32];
        strReason[0] = 0;
        TagFieldSetNumber(strReason, sizeof(strReason), "REASON", iReason);
        _ClearGame();
        LobbyApiPcPostEvent(g_Lobby.pLobbyApi, LOBBYAPI_CBTYPE_EVNT, KI_EVENT_KICK, 0, strReason);
        LobbyApiPcPostEvent(g_Lobby.pLobbyApi, LOBBYAPI_CBTYPE_EVNT, KI_EVENT_USER, 0, "");
    }

    // The next host: the named one when it is still in the record, else the first remaining player.
    u32 _PickSuccessor(u32 uNamed)
    {
        if ((uNamed != 0) && (_FindPlayerByIdent(uNamed) >= 0))
        {
            return uNamed;
        }
        return (g_Lobby.Play.iCount > 0) ? (u32)g_Lobby.Play.aOpponents[0].iIdent : 0;
    }

    // Member: the host uOldHost is gone (HANDOVER, BYE or timeout). uNamed is the successor the
    // leaving host named, 0 when it could not name one.
    void _MemberHostGone(u32 uOldHost, u32 uNamed, s32 iReasonIfOver)
    {
        const s32 iOld = _FindPlayerByIdent(uOldHost);
        if (iOld >= 0)
        {
            for (s32 iMove = iOld; iMove < g_Lobby.Play.iCount - 1; ++iMove)
            {
                g_Lobby.Play.aOpponents[iMove] = g_Lobby.Play.aOpponents[iMove + 1];
            }
            g_Lobby.Play.iCount -= 1;
            memset(&g_Lobby.Play.aOpponents[g_Lobby.Play.iCount], 0, sizeof(LobbyApiPlayerT));
        }
        const u32 uSelf = CgsPcNetIdentityLobbyIdent();
        const u32 uNext = _PickSuccessor(uNamed);
        if ((uNext == 0) || (_FindPlayerByIdent(uSelf) < 0))
        {
            _Log("host gone, no successor; game over", (s32)uOldHost);
            _MemberDropped(iReasonIfOver);
            return;
        }
        const s32 iNext = _FindPlayerByIdent(uNext);
        memcpy(g_Lobby.Play.strHost, g_Lobby.Play.aOpponents[iNext].strPers, sizeof(g_Lobby.Play.strHost));
        g_Lobby.strLastText[0] = 0;

        if (uNext == uSelf)
        {
            // take the record over: every other player is a member we now serve
            g_Lobby.bHost      = true;
            g_Lobby.uHostIdent = uSelf;
            memset(&g_Lobby.HostPeer, 0, sizeof(g_Lobby.HostPeer));
            memset(g_Lobby.aMembers, 0, sizeof(g_Lobby.aMembers));
            s32 iMember = 0;
            for (s32 iPlayer = 0; iPlayer < g_Lobby.Play.iCount; ++iPlayer)
            {
                const LobbyApiPlayerT* pPlayer = &g_Lobby.Play.aOpponents[iPlayer];
                PcLanPeerT Peer;
                if (((u32)pPlayer->iIdent == uSelf) || !PcLanPeerByIdent((u32)pPlayer->iIdent, &Peer))
                {
                    continue;
                }
                g_Lobby.aMembers[iMember].uIdent = (u32)pPlayer->iIdent;
                g_Lobby.aMembers[iMember].uXuid  = _XuidOf(pPlayer->strMachineAddr);
                g_Lobby.aMembers[iMember].Peer   = Peer;
                iMember += 1;
            }
            _Log("host handover: took over game", (s32)g_Lobby.uGameIdent);
            _HostPublish(KI_EVENT_GAME, true);
            return;
        }

        PcLanPeerT NextPeer;
        if (!PcLanPeerByIdent(uNext, &NextPeer))
        {
            _Log("host handover: next host has no address; game over", (s32)uNext);
            _MemberDropped(iReasonIfOver);
            return;
        }
        g_Lobby.uHostIdent = uNext;
        g_Lobby.HostPeer   = NextPeer;
        _Log("host handover: new host", (s32)uNext);

        static char strText[KI_TEXT_LEN];
        _FormatPlay(&g_Lobby.Play, NULL, strText, sizeof(strText));
        LobbyApiPcPostEvent(g_Lobby.pLobbyApi, LOBBYAPI_CBTYPE_EVNT, KI_EVENT_GAME, 0, strText);
    }

    // Host: leave without ending the game -- name the next host to every member.
    void _HostHandOver()
    {
        const u32 uSelf = CgsPcNetIdentityLobbyIdent();
        u32 uNext = 0;
        for (s32 iPlayer = 0; (iPlayer < g_Lobby.Play.iCount) && (uNext == 0); ++iPlayer)
        {
            if ((u32)g_Lobby.Play.aOpponents[iPlayer].iIdent != uSelf)
            {
                uNext = (u32)g_Lobby.Play.aOpponents[iPlayer].iIdent;
            }
        }
        u8 aBody[4];
        _Put32(aBody, uNext);
        for (s32 iMember = 0; iMember < KI_MAX_PLAYERS; ++iMember)
        {
            if (g_Lobby.aMembers[iMember].uIdent != 0)
            {
                PcLanSendControl(&g_Lobby.aMembers[iMember].Peer, PCLAN_HANDOVER, g_Lobby.uGameIdent, aBody, 4);
            }
        }
        _Log("host handover: leaving, next host", (s32)uNext);
    }

    void _SendJoin(const FoundT* pFound)
    {
        static u8 aBody[12 + KI_REQUEST_LEN];
        const s32 iTextLen = (s32)strlen(g_Lobby.strJoinRequest);
        _Put64(aBody, CgsPcNetIdentityXuid());
        _Put32(aBody + 8, CgsPcNetIdentityLobbyIdent());
        memcpy(aBody + 12, g_Lobby.strJoinRequest, (size_t)iTextLen + 1);

        g_Lobby.uJoinGame  = (u32)pFound->Play.iIdent;
        g_Lobby.uJoinStart = _Now();
        g_Lobby.HostPeer   = pFound->Host;
        g_Lobby.uHostIdent = pFound->uHostIdent;
        if (PcLanSendControl(&pFound->Host, PCLAN_JOIN_REQ, g_Lobby.uJoinGame, aBody, 12 + iTextLen + 1) < 0)
        {
            LobbyApiPcComplete(g_Lobby.pLobbyApi, g_Lobby.iJoinId, KI_ERR_UNKNOWN_GAME, "");
            g_Lobby.iJoinId = 0;
        }
    }

    const FoundT* _FindFound(u32 uGameIdent)
    {
        for (s32 iFound = 0; iFound < g_Lobby.iNumFound; ++iFound)
        {
            if ((u32)g_Lobby.aFound[iFound].Play.iIdent == uGameIdent)
            {
                return &g_Lobby.aFound[iFound];
            }
        }
        return NULL;
    }

    // ---- transport handlers ------------------------------------------------------------------

    void _OnDiscover(void* /*pRef*/, const PcLanPeerT* pFrom, const PcLanHeaderT* /*pHeader*/, const uint8_t* /*pBody*/)
    {
        if (!g_Lobby.bInGame || !g_Lobby.bHost)
        {
            return;
        }
        static char strText[KI_TEXT_LEN];
        static u8   aBody[12 + KI_TEXT_LEN];
        const s32 iTextLen = _FormatPlay(&g_Lobby.Play, NULL, strText, sizeof(strText));
        _Put64(aBody, CgsPcNetIdentityXuid());
        _Put32(aBody + 8, CgsPcNetIdentityLobbyIdent());
        memcpy(aBody + 12, strText, (size_t)iTextLen + 1);
        PcLanSendControl(pFrom, PCLAN_ADVERT, g_Lobby.uGameIdent, aBody, 12 + iTextLen + 1);
    }

    void _OnAdvert(void* pRef, const PcLanPeerT* pFrom, const PcLanHeaderT* pHeader, const uint8_t* pBody)
    {
        LobbyApiRefT* pLobbyApi = (LobbyApiRefT*)pRef;
        if ((g_Lobby.iSearchId == 0) || (pHeader->uLen < 12))
        {
            return;
        }
        static char strText[KI_TEXT_LEN];
        _BodyText(pBody, pHeader->uLen, 12, strText, sizeof(strText));
        const u64 uHostXuid  = _Get64(pBody);
        const u32 uHostIdent = _Get32(pBody + 8);
        PcLanPeerRemember(uHostXuid, uHostIdent, pFrom);

        if ((_FindFound(pHeader->uGameIdent) != NULL) || (g_Lobby.iNumFound >= KI_MAX_FOUND))
        {
            return;
        }
        FoundT* pFound = &g_Lobby.aFound[g_Lobby.iNumFound++];
        LobbyApiExtractPlayRecord(&pFound->Play, strText);
        pFound->Host       = *pFrom;
        pFound->uHostIdent = uHostIdent;
        _Log("advert for game", pFound->Play.iIdent);
        if (!g_Lobby.bQuickJoin)
        {
            LobbyApiPcListAdd(pLobbyApi, LOBBYAPI_LIST_GAMES, &pFound->Play);
        }
    }

    void _OnJoinReq(void* /*pRef*/, const PcLanPeerT* pFrom, const PcLanHeaderT* pHeader, const uint8_t* pBody)
    {
        if (pHeader->uLen < 12)
        {
            return;
        }
        static char strRequest[KI_REQUEST_LEN];
        static char strText[KI_TEXT_LEN];
        static u8   aBody[4 + KI_TEXT_LEN];
        _BodyText(pBody, pHeader->uLen, 12, strRequest, sizeof(strRequest));
        const u64 uXuid  = _Get64(pBody);
        const u32 uIdent = _Get32(pBody + 8);

        s32 iCode = 0;
        if (!g_Lobby.bInGame || !g_Lobby.bHost || (pHeader->uGameIdent != g_Lobby.uGameIdent))
        {
            iCode = KI_ERR_UNKNOWN_GAME;
        }
        else if (_FindPlayerByIdent(uIdent) < 0)
        {
            char strPassword[36];
            TagFieldGetString(TagFieldFind(strRequest, "PASS"), strPassword, sizeof(strPassword), "");
            const s32 iMax = (g_Lobby.Play.iMaxsize > 0) ? g_Lobby.Play.iMaxsize : KI_MAX_PLAYERS;
            if ((g_Lobby.Play.iCount >= iMax) || (g_Lobby.Play.iCount >= KI_MAX_PLAYERS))
            {
                iCode = KI_ERR_FULL;
            }
            else if ((g_Lobby.Play.uSysflags & KU_SYSFLAG_LOCKED) != 0)
            {
                iCode = KI_ERR_LOCKED;
            }
            else if ((g_Lobby.strPassword[0] != 0) && (strcmp(strPassword, g_Lobby.strPassword) != 0))
            {
                iCode = KI_ERR_PASSWORD;
            }
            else
            {
                LobbyApiPlayerT* pPlayer = &g_Lobby.Play.aOpponents[g_Lobby.Play.iCount];
                memset(pPlayer, 0, sizeof(*pPlayer));
                pPlayer->iIdent = (s32)uIdent;
                TagFieldGetString(TagFieldFind(strRequest, "PERS"), pPlayer->strPers, sizeof(pPlayer->strPers), "");
                pPlayer->uAddr      = pFrom->uAddr;
                pPlayer->uLocalAddr = pFrom->uAddr;
                TagFieldGetString(TagFieldFind(strRequest, "MADDR"), pPlayer->strMachineAddr,
                                  sizeof(pPlayer->strMachineAddr), "");
                TagFieldGetString(TagFieldFind(strRequest, "USERPARAMS"), pPlayer->strParams,
                                  sizeof(pPlayer->strParams), "");
                pPlayer->uFlags = (u32)TagFieldGetNumber(TagFieldFind(strRequest, "USERFLAGS"), 0);
                g_Lobby.Play.iCount += 1;
                _HostStampColour(&g_Lobby.Play, g_Lobby.Play.iCount - 1, -1);

                for (s32 iMember = 0; iMember < KI_MAX_PLAYERS; ++iMember)
                {
                    if (g_Lobby.aMembers[iMember].uIdent == 0)
                    {
                        g_Lobby.aMembers[iMember].uIdent = uIdent;
                        g_Lobby.aMembers[iMember].uXuid  = uXuid;
                        g_Lobby.aMembers[iMember].Peer   = *pFrom;
                        break;
                    }
                }
                PcLanPeerRemember(uXuid, uIdent, pFrom);
                _Log("host accepted player", (s32)uIdent);
            }
        }

        // answer the joiner, then tell everyone (the joiner hears the record in JOIN_RESP)
        u16 aPorts[KI_MAX_PLAYERS];
        _HostPorts(aPorts);
        const s32 iTextLen = (iCode == 0) ? _FormatPlay(&g_Lobby.Play, aPorts, strText, sizeof(strText)) : 0;
        if (iCode != 0)
        {
            strText[0] = 0;
        }
        _Put32(aBody, (u32)iCode);
        memcpy(aBody + 4, strText, (size_t)iTextLen + 1);
        PcLanSendControl(pFrom, PCLAN_JOIN_RESP, pHeader->uGameIdent, aBody, 4 + iTextLen + 1);
        if (iCode == 0)
        {
            _HostPublish(KI_EVENT_GAME, true);
        }
    }

    void _OnJoinResp(void* pRef, const PcLanPeerT* pFrom, const PcLanHeaderT* pHeader, const uint8_t* pBody)
    {
        LobbyApiRefT* pLobbyApi = (LobbyApiRefT*)pRef;
        if ((g_Lobby.iJoinId == 0) || (pHeader->uLen < 4) || (pHeader->uGameIdent != g_Lobby.uJoinGame))
        {
            return;
        }
        const s32 iCode = (s32)_Get32(pBody);
        const s32 iJoinId = g_Lobby.iJoinId;
        g_Lobby.iJoinId = 0;
        if (iCode != 0)
        {
            _Log("join refused, code", iCode);
            LobbyApiPcComplete(pLobbyApi, iJoinId, iCode, "");
            return;
        }

        static char strText[KI_TEXT_LEN];
        _BodyText(pBody, pHeader->uLen, 4, strText, sizeof(strText));
        LobbyApiExtractPlayRecord(&g_Lobby.Play, strText);
        g_Lobby.bInGame    = true;
        g_Lobby.bHost      = false;
        g_Lobby.uGameIdent = pHeader->uGameIdent;
        g_Lobby.uHostIdent = pHeader->uSenderIdent;
        g_Lobby.HostPeer   = *pFrom;
        strncpy(g_Lobby.strLastText, strText, sizeof(g_Lobby.strLastText) - 1);
        LobbyApiPcSelf(pLobbyApi)->game = (s32)g_Lobby.uGameIdent;
        _RememberPlayers(&g_Lobby.Play, strText);
        _Log("joined game", (s32)g_Lobby.uGameIdent);

        LobbyApiPcComplete(pLobbyApi, iJoinId, 0, "");
        LobbyApiPcPostEvent(pLobbyApi, LOBBYAPI_CBTYPE_EVNT, KI_EVENT_GAME, 0, strText);
    }

    void _OnPlay(void* pRef, const PcLanPeerT* /*pFrom*/, const PcLanHeaderT* pHeader, const uint8_t* pBody)
    {
        LobbyApiRefT* pLobbyApi = (LobbyApiRefT*)pRef;
        if (!g_Lobby.bInGame || g_Lobby.bHost || (pHeader->uGameIdent != g_Lobby.uGameIdent) ||
            (pHeader->uLen < 4))
        {
            return;
        }
        const s32 iEventKind = (s32)_Get32(pBody);
        static char strText[KI_TEXT_LEN];
        _BodyText(pBody, pHeader->uLen, 4, strText, sizeof(strText));

        if (iEventKind == KI_EVENT_PLAY)
        {
            LobbyApiPcPostEvent(pLobbyApi, LOBBYAPI_CBTYPE_EVNT, KI_EVENT_PLAY, 0, strText);
            return;
        }
        if (strcmp(strText, g_Lobby.strLastText) == 0)
        {
            return;   // the 1 Hz refresh of a record we already have
        }
        LobbyApiExtractPlayRecord(&g_Lobby.Play, strText);
        strncpy(g_Lobby.strLastText, strText, sizeof(g_Lobby.strLastText) - 1);
        if (_FindPlayerByIdent(CgsPcNetIdentityLobbyIdent()) < 0)
        {
            _MemberDropped(KI_KICK_NOGAME);
            return;
        }
        _RememberPlayers(&g_Lobby.Play, strText);
        LobbyApiPcPostEvent(pLobbyApi, LOBBYAPI_CBTYPE_EVNT, KI_EVENT_GAME, 0, strText);
    }

    void _OnLobbyReq(void* /*pRef*/, const PcLanPeerT* /*pFrom*/, const PcLanHeaderT* pHeader, const uint8_t* pBody)
    {
        if (!g_Lobby.bInGame || !g_Lobby.bHost || (pHeader->uGameIdent != g_Lobby.uGameIdent) ||
            (pHeader->uLen < 4))
        {
            return;
        }
        static char strRequest[KI_REQUEST_LEN];
        _BodyText(pBody, pHeader->uLen, 4, strRequest, sizeof(strRequest));
        _HostApply((s32)_Get32(pBody), strRequest, pHeader->uSenderIdent);
    }

    void _OnKick(void* /*pRef*/, const PcLanPeerT* /*pFrom*/, const PcLanHeaderT* pHeader, const uint8_t* pBody)
    {
        if (!g_Lobby.bInGame || g_Lobby.bHost || (pHeader->uGameIdent != g_Lobby.uGameIdent))
        {
            return;
        }
        const s32 iReason = (pHeader->uLen >= 4) ? (s32)_Get32(pBody) : KI_KICK_NOGAME;
        _Log("kicked, reason", iReason);
        _MemberDropped(iReason);
    }

    void _OnHandover(void* /*pRef*/, const PcLanPeerT* /*pFrom*/, const PcLanHeaderT* pHeader, const uint8_t* pBody)
    {
        if (!g_Lobby.bInGame || g_Lobby.bHost || (pHeader->uGameIdent != g_Lobby.uGameIdent) ||
            (pHeader->uSenderIdent != g_Lobby.uHostIdent))
        {
            return;
        }
        const u32 uNamed = (pHeader->uLen >= 4) ? _Get32(pBody) : 0;
        _Log("host left, named next host", (s32)uNamed);
        _MemberHostGone(pHeader->uSenderIdent, uNamed, KI_KICK_NOGAME);
    }

    void _OnLeave(void* /*pRef*/, const PcLanPeerT* /*pFrom*/, const PcLanHeaderT* pHeader, const uint8_t* /*pBody*/)
    {
        if (!g_Lobby.bInGame || !g_Lobby.bHost || (pHeader->uGameIdent != g_Lobby.uGameIdent))
        {
            return;
        }
        _Log("member left", (s32)pHeader->uSenderIdent);
        _HostApply(KI_REQ_LEAVE, "", pHeader->uSenderIdent);
    }

    void _OnBye(void* /*pRef*/, const PcLanPeerT* /*pFrom*/, const PcLanHeaderT* pHeader, const uint8_t* /*pBody*/)
    {
        if (!g_Lobby.bInGame || (pHeader->uSenderIdent == 0))
        {
            return;
        }
        if (g_Lobby.bHost)
        {
            if (_FindPlayerByIdent(pHeader->uSenderIdent) >= 0)
            {
                _Log("member gone", (s32)pHeader->uSenderIdent);
                _HostApply(KI_REQ_LEAVE, "", pHeader->uSenderIdent);
            }
        }
        else if (pHeader->uSenderIdent == g_Lobby.uHostIdent)
        {
            _Log("host gone", (s32)pHeader->uSenderIdent);
            _MemberHostGone(pHeader->uSenderIdent, 0, KI_KICK_LOST_CONNECTION);
        }
    }

    void _Register(LobbyApiRefT* pLobbyApi, bool bOn)
    {
        PcLanRegister(PCLAN_DISCOVER, bOn ? &_OnDiscover : NULL, pLobbyApi);
        PcLanRegister(PCLAN_ADVERT,   bOn ? &_OnAdvert   : NULL, pLobbyApi);
        PcLanRegister(PCLAN_JOIN_REQ, bOn ? &_OnJoinReq  : NULL, pLobbyApi);
        PcLanRegister(PCLAN_JOIN_RESP,bOn ? &_OnJoinResp : NULL, pLobbyApi);
        PcLanRegister(PCLAN_PLAY,     bOn ? &_OnPlay     : NULL, pLobbyApi);
        PcLanRegister(PCLAN_LOBBYREQ, bOn ? &_OnLobbyReq : NULL, pLobbyApi);
        PcLanRegister(PCLAN_KICK,     bOn ? &_OnKick     : NULL, pLobbyApi);
        PcLanRegister(PCLAN_LEAVE,    bOn ? &_OnLeave    : NULL, pLobbyApi);
        PcLanRegister(PCLAN_HANDOVER, bOn ? &_OnHandover : NULL, pLobbyApi);
        // departures come through NetConn's fan-out (pclan has one handler per type)
        if (bOn)
        {
            PcNetConnByeAdd(&_OnBye, pLobbyApi);
        }
        else
        {
            PcNetConnByeDel(&_OnBye, pLobbyApi);
        }
    }

    void _StartSweep(s32 iId, bool bQuickJoin)
    {
        g_Lobby.iSearchId    = iId;
        g_Lobby.bQuickJoin   = bQuickJoin;
        g_Lobby.bResweep     = false;
        g_Lobby.uSearchStart = _Now();
        g_Lobby.iNumFound    = 0;
        PcLanBroadcast(PCLAN_DISCOVER, 0, NULL, 0);
    }

    // ---- request handlers ----------------------------------------------------------------------

    void _RequestCreate(s32 iId, const char* pRequest)
    {
        LobbyApiRefT* pLobbyApi = g_Lobby.pLobbyApi;
        if (g_Lobby.bInGame)
        {
            LobbyApiPcComplete(pLobbyApi, iId, KI_ERR_IN_GAME, "");
            return;
        }
        _ClearGame();

        LobbyApiUserT* pSelf = LobbyApiPcSelf(pLobbyApi);
        g_Lobby.uGameSeqn += 1;
        u32 uGameIdent = ((CgsPcNetIdentityLobbyIdent() << 8) ^ g_Lobby.uGameSeqn) & 0x7FFFFFFFu;
        if (uGameIdent == 0)
        {
            uGameIdent = 1;
        }

        LobbyApiPlayT* pPlay = &g_Lobby.Play;
        pPlay->iIdent = (s32)uGameIdent;
        TagFieldGetString(TagFieldFind(pRequest, "NAME"), pPlay->strName, sizeof(pPlay->strName), "");
        memcpy(pPlay->strHost, pSelf->name, sizeof(pPlay->strHost));
        pPlay->iRoom = TagFieldGetNumber(TagFieldFind(pRequest, "ROOM"), 0);
        ds_snzprintf(pPlay->strSess, sizeof(pPlay->strSess), "PCLAN%08X", uGameIdent);
        TagFieldGetString(TagFieldFind(pRequest, "PARAMS"), pPlay->strParams, sizeof(pPlay->strParams), "");
        pPlay->uCustflags = (u32)TagFieldGetNumber(TagFieldFind(pRequest, "CUSTFLAGS"), 0);
        pPlay->uSysflags  = (u32)TagFieldGetNumber(TagFieldFind(pRequest, "SYSFLAGS"), 0);
        pPlay->iMinsize   = (s8)TagFieldGetNumber(TagFieldFind(pRequest, "MINSIZE"), 0);
        pPlay->iMaxsize   = (s8)TagFieldGetNumber(TagFieldFind(pRequest, "MAXSIZE"), 0);
        pPlay->iGameMode  = -1;
        pPlay->uSeed      = (u32)TagFieldGetNumber(TagFieldFind(pRequest, "SEED"), 0);
        pPlay->uWhen      = ds_timeinsecs();
        pPlay->iPrivSlots = TagFieldGetNumber(TagFieldFind(pRequest, "PRIV"), 0);
        pPlay->iCount     = 1;
        _FillSelfPlayer(&pPlay->aOpponents[0], pRequest);
        _HostStampColour(pPlay, 0, -1);
        TagFieldGetString(TagFieldFind(pRequest, "PASS"), g_Lobby.strPassword, sizeof(g_Lobby.strPassword), "");

        g_Lobby.bInGame    = true;
        g_Lobby.bHost      = true;
        g_Lobby.uGameIdent = uGameIdent;
        g_Lobby.uHostIdent = CgsPcNetIdentityLobbyIdent();
        pSelf->game = (s32)uGameIdent;
        _Log("hosting game", (s32)uGameIdent);

        LobbyApiPcComplete(pLobbyApi, iId, 0, "");
        _HostPublish(KI_EVENT_GAME, true);
    }

    void _RequestJoin(s32 iId, s32 iKind, const char* pRequest)
    {
        LobbyApiRefT* pLobbyApi = g_Lobby.pLobbyApi;
        if (g_Lobby.bInGame || (g_Lobby.iJoinId != 0))
        {
            LobbyApiPcComplete(pLobbyApi, iId, KI_ERR_IN_GAME, "");
            return;
        }

        // what the host needs to seat us: persona, machine address, player parameters
        LobbyApiUserT* pSelf = LobbyApiPcSelf(pLobbyApi);
        char* pJoin = g_Lobby.strJoinRequest;
        pJoin[0] = 0;
        TagFieldSetString(pJoin, KI_REQUEST_LEN, "PERS", pSelf->name);
        TagFieldSetString(pJoin, KI_REQUEST_LEN, "MADDR", pSelf->MachineAddr.strMachineAddr);
        char strValue[260];
        TagFieldGetString(TagFieldFind(pRequest, "USERPARAMS"), strValue, sizeof(strValue), "");
        TagFieldSetString(pJoin, KI_REQUEST_LEN, "USERPARAMS", strValue);
        TagFieldSetNumber(pJoin, KI_REQUEST_LEN, "USERFLAGS", TagFieldGetNumber(TagFieldFind(pRequest, "USERFLAGS"), 0));
        TagFieldGetString(TagFieldFind(pRequest, "PASS"), strValue, sizeof(strValue), "");
        TagFieldSetString(pJoin, KI_REQUEST_LEN, "PASS", strValue);

        g_Lobby.iJoinId = iId;
        if (iKind == KI_REQ_QUICK)
        {
            _StartSweep(iId, true);
            return;
        }

        const FoundT* pFound = _FindFound((u32)TagFieldGetNumber(TagFieldFind(pRequest, "IDENT"), 0));
        if (pFound == NULL)
        {
            g_Lobby.iJoinId = 0;
            LobbyApiPcComplete(pLobbyApi, iId, KI_ERR_UNKNOWN_GAME, "");
            return;
        }
        _SendJoin(pFound);
    }

    // A member forwards a game-record request to the host and succeeds locally.
    void _MemberForward(s32 iId, s32 iKind, const char* pRequest)
    {
        static u8 aBody[4 + KI_REQUEST_LEN];
        s32 iTextLen = (s32)strlen(pRequest);
        if (iTextLen > KI_REQUEST_LEN - 1)
        {
            iTextLen = KI_REQUEST_LEN - 1;
        }
        _Put32(aBody, (u32)iKind);
        memcpy(aBody + 4, pRequest, (size_t)iTextLen);
        aBody[4 + iTextLen] = 0;
        PcLanSendControl(&g_Lobby.HostPeer, PCLAN_LOBBYREQ, g_Lobby.uGameIdent, aBody, 4 + iTextLen + 1);
        LobbyApiPcComplete(g_Lobby.pLobbyApi, iId, 0, "");
    }

    void _RequestLeave(s32 iId, s32 iKind)
    {
        LobbyApiRefT* pLobbyApi = g_Lobby.pLobbyApi;
        if (!g_Lobby.bInGame)
        {
            LobbyApiPcComplete(pLobbyApi, iId, KI_ERR_NOT_IN_GAME, "");
            return;
        }
        if (g_Lobby.bHost && ((iKind == KI_REQ_LEAVE) || (g_Lobby.Play.iCount > 1)))
        {
            // The host leaving hands the game to the next player. The game's own leave of a
            // hosted game is always 'gdel' (it never enables the game manager's 'mgrt'
            // host-migration mode that turns it into 'glea'); while other players are in the
            // record this authority hands the game over for it as well, and ends it only
            // when the host is alone.
            _HostHandOver();
        }
        else if (g_Lobby.bHost)
        {
            // the host deleting the game ends it for everyone
            u8 aBody[4];
            _Put32(aBody, (u32)KI_KICK_NOGAME);
            for (s32 iMember = 0; iMember < KI_MAX_PLAYERS; ++iMember)
            {
                if (g_Lobby.aMembers[iMember].uIdent != 0)
                {
                    PcLanSendControl(&g_Lobby.aMembers[iMember].Peer, PCLAN_KICK, g_Lobby.uGameIdent, aBody, 4);
                }
            }
        }
        else
        {
            PcLanSendControl(&g_Lobby.HostPeer, PCLAN_LEAVE, g_Lobby.uGameIdent, NULL, 0);
        }
        _Log("left game", (s32)g_Lobby.uGameIdent);
        _ClearGame();
        LobbyApiPcComplete(pLobbyApi, iId, 0, "");
        LobbyApiPcPostEvent(pLobbyApi, LOBBYAPI_CBTYPE_EVNT, KI_EVENT_USER, 0, "");
    }
}

// ---- seam entry points -----------------------------------------------------------------------

void PcLanLobbyAttach(LobbyApiRefT* pLobbyApi)
{
    memset(&g_Lobby, 0, sizeof(g_Lobby));
    g_Lobby.pLobbyApi = pLobbyApi;
    _Register(pLobbyApi, true);
}

void PcLanLobbyDetach(LobbyApiRefT* pLobbyApi)
{
    if (g_Lobby.pLobbyApi != pLobbyApi)
    {
        return;
    }
    _Register(pLobbyApi, false);
    g_Lobby.pLobbyApi = NULL;
}

bool PcLanLobbyRequest(LobbyApiRefT* pLobbyApi, s32 iId, s32 iKind, const char* pRequest)
{
    if (g_Lobby.pLobbyApi != pLobbyApi)
    {
        return false;
    }

    if ((iKind == KI_REQ_CREATE) || (iKind == KI_REQ_CREATE_GPS))
    {
        _RequestCreate(iId, pRequest);
        return true;
    }
    if ((iKind == KI_REQ_JOIN) || (iKind == KI_REQ_QUICK))
    {
        _RequestJoin(iId, iKind, pRequest);
        return true;
    }
    if (iKind == KI_REQ_SEARCH)
    {
        if (TagFieldGetNumber(TagFieldFind(pRequest, "CANCEL"), 0) != 0)
        {
            // cancel: the running sweep answers with what it has now
            if ((g_Lobby.iSearchId != 0) && !g_Lobby.bQuickJoin)
            {
                char strCount[32];
                strCount[0] = 0;
                TagFieldSetNumber(strCount, sizeof(strCount), "COUNT", g_Lobby.iNumFound);
                LobbyApiPcComplete(pLobbyApi, g_Lobby.iSearchId, 0, strCount);
                g_Lobby.iSearchId = 0;
            }
            LobbyApiPcComplete(pLobbyApi, iId, 0, "");
            return true;
        }
        if (g_Lobby.iSearchId != 0)
        {
            LobbyApiPcComplete(pLobbyApi, iId, FourCC('x', 'i', 's', 't'), "");
            return true;
        }
        _StartSweep(iId, false);
        return true;
    }
    if ((iKind == KI_REQ_LEAVE) || (iKind == KI_REQ_DELETE))
    {
        _RequestLeave(iId, iKind);
        return true;
    }
    if ((iKind == KI_REQ_SET) || (iKind == KI_REQ_START) || (iKind == KI_REQ_RANK))
    {
        if (!g_Lobby.bInGame)
        {
            LobbyApiPcComplete(pLobbyApi, iId, KI_ERR_NOT_IN_GAME, "");
            return true;
        }
        if (g_Lobby.bHost)
        {
            LobbyApiPcComplete(pLobbyApi, iId, _HostApply(iKind, pRequest, CgsPcNetIdentityLobbyIdent()), "");
        }
        else if (iKind == KI_REQ_RANK)
        {
            LobbyApiPcComplete(pLobbyApi, iId, 0, "");
        }
        else
        {
            _MemberForward(iId, iKind, pRequest);
        }
        return true;
    }
    return false;
}

void PcLanLobbyCancel(LobbyApiRefT* pLobbyApi, s32 iId)
{
    if (g_Lobby.pLobbyApi != pLobbyApi)
    {
        return;
    }
    if (g_Lobby.iSearchId == iId)
    {
        g_Lobby.iSearchId = 0;
    }
    if (g_Lobby.iJoinId == iId)
    {
        g_Lobby.iJoinId = 0;
    }
}

void PcLanLobbyUpdate(LobbyApiRefT* pLobbyApi)
{
    if (g_Lobby.pLobbyApi != pLobbyApi)
    {
        return;
    }
    const u32 uNow = _Now();

    // the DISCOVER sweep
    if (g_Lobby.iSearchId != 0)
    {
        if (!g_Lobby.bResweep && ((uNow - g_Lobby.uSearchStart) >= KU_RESWEEP_MS))
        {
            g_Lobby.bResweep = true;
            PcLanBroadcast(PCLAN_DISCOVER, 0, NULL, 0);
        }
        if ((uNow - g_Lobby.uSearchStart) >= KU_SEARCH_MS)
        {
            const s32 iSearchId = g_Lobby.iSearchId;
            g_Lobby.iSearchId = 0;
            _Log("sweep done, games", g_Lobby.iNumFound);
            if (!g_Lobby.bQuickJoin)
            {
                char strCount[32];
                strCount[0] = 0;
                TagFieldSetNumber(strCount, sizeof(strCount), "COUNT", g_Lobby.iNumFound);
                LobbyApiPcComplete(pLobbyApi, iSearchId, 0, strCount);
            }
            else
            {
                // quick join: the first game with room that is not locked
                const FoundT* pPick = NULL;
                for (s32 iFound = 0; (iFound < g_Lobby.iNumFound) && (pPick == NULL); ++iFound)
                {
                    const LobbyApiPlayT* pPlay = &g_Lobby.aFound[iFound].Play;
                    const s32 iMax = (pPlay->iMaxsize > 0) ? pPlay->iMaxsize : KI_MAX_PLAYERS;
                    if ((pPlay->iCount < iMax) && ((pPlay->uSysflags & KU_SYSFLAG_LOCKED) == 0))
                    {
                        pPick = &g_Lobby.aFound[iFound];
                    }
                }
                if (pPick == NULL)
                {
                    LobbyApiPcComplete(pLobbyApi, g_Lobby.iJoinId, KI_ERR_NOT_FOUND, "");
                    g_Lobby.iJoinId = 0;
                }
                else
                {
                    _SendJoin(pPick);
                }
            }
        }
    }

    // a join nobody answered
    if ((g_Lobby.iJoinId != 0) && (g_Lobby.iSearchId == 0) && ((uNow - g_Lobby.uJoinStart) >= KU_JOIN_MS))
    {
        _Log("join timed out, game", (s32)g_Lobby.uJoinGame);
        LobbyApiPcComplete(pLobbyApi, g_Lobby.iJoinId, KI_ERR_UNKNOWN_GAME, "");
        g_Lobby.iJoinId = 0;
    }

    // the host's record refresh
    if (g_Lobby.bInGame && g_Lobby.bHost && ((uNow - g_Lobby.uLastPlaySend) >= KU_PLAY_EVERY_MS))
    {
        _HostPublish(KI_EVENT_GAME, false);
    }
}

void PcLanLobbyLeave(LobbyApiRefT* pLobbyApi)
{
    if ((g_Lobby.pLobbyApi != pLobbyApi) || !g_Lobby.bInGame)
    {
        return;
    }
    if (g_Lobby.bHost)
    {
        _HostHandOver();
    }
    else
    {
        PcLanSendControl(&g_Lobby.HostPeer, PCLAN_LEAVE, g_Lobby.uGameIdent, NULL, 0);
    }
    _ClearGame();
}
