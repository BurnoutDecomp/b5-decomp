// DirtySDK lobby -- record extraction (core/source/lobby/lobbyapiutil.c).
//
// LobbyApiExtractPlayRecord decodes the tagfield text of a lobby game record into the
// LobbyApiPlayT layout the game reads (persona fields 16 bytes on this platform). Only the
// play-record decoder is reconstructed; the game build reaches the others through the lobby.

#include "lobbyapi.h"
#include "lobbytagfield.h"
#include "platform.h"      // ds_snzprintf

#include <cstring>

extern "C" void LobbyApiExtractPlayRecord(LobbyApiPlayT* pPlay, const char* pData)
{
    char strKey[32];
    s32 iPlayer;
    s32 iPart;

    memset(pPlay, 0, sizeof(*pPlay));

    pPlay->iIdent = TagFieldGetNumber(TagFieldFind(pData, "IDENT"), -1);
    TagFieldGetString(TagFieldFind(pData, "NAME"), pPlay->strName, sizeof(pPlay->strName), "");
    TagFieldGetString(TagFieldFind(pData, "HOST"), pPlay->strHost, sizeof(pPlay->strHost), "");
    TagFieldGetString(TagFieldFind(pData, "GPSHOST"), pPlay->strGPSHost, sizeof(pPlay->strGPSHost), "");
    TagFieldGetString(TagFieldFind(pData, "PARAMS"), pPlay->strParams, sizeof(pPlay->strParams), "");
    TagFieldGetString(TagFieldFind(pData, "PLATPARAMS"), pPlay->strPlatParams, sizeof(pPlay->strPlatParams), "");
    pPlay->iRoom          = TagFieldGetNumber(TagFieldFind(pData, "ROOM"), 0);
    pPlay->uCustflags     = (u32)TagFieldGetNumber(TagFieldFind(pData, "CUSTFLAGS"), 0);
    pPlay->uSysflags      = (u32)TagFieldGetNumber(TagFieldFind(pData, "SYSFLAGS"), 0);
    pPlay->iCount         = TagFieldGetNumber(TagFieldFind(pData, "COUNT"), 0);
    pPlay->iPrivSlots     = TagFieldGetNumber(TagFieldFind(pData, "PRIV"), 0);
    pPlay->iMinsize       = (s8)TagFieldGetNumber(TagFieldFind(pData, "MINSIZE"), 0);
    pPlay->iMaxsize       = (s8)TagFieldGetNumber(TagFieldFind(pData, "MAXSIZE"), 0);
    pPlay->iNumPartitions = TagFieldGetNumber(TagFieldFind(pData, "NUMPART"), 0);
    pPlay->uSeed          = (u32)TagFieldGetNumber(TagFieldFind(pData, "SEED"), 0);
    pPlay->uWhen          = TagFieldGetEpoch(TagFieldFind(pData, "WHEN"), 0);
    pPlay->uGamePort      = (u16)TagFieldGetNumber(TagFieldFind(pData, "GAMEPORT"), 0);
    pPlay->uVoipPort      = (u16)TagFieldGetNumber(TagFieldFind(pData, "VOIPPORT"), 0);
    pPlay->iGameMode      = (s8)TagFieldGetNumber(TagFieldFind(pData, "GAMEMODE"), -1);
    TagFieldGetString(TagFieldFind(pData, "AUTH"), pPlay->strAuth, sizeof(pPlay->strAuth), "");

    // the player table (the count is taken as sent)
    for (iPlayer = 0; iPlayer < pPlay->iCount; ++iPlayer)
    {
        LobbyApiPlayerT* pPlayer = &pPlay->aOpponents[iPlayer];

        ds_snzprintf(strKey, sizeof(strKey), "OPID%d", iPlayer);
        pPlayer->iIdent = TagFieldGetNumber(TagFieldFind(pData, strKey), 0);
        ds_snzprintf(strKey, sizeof(strKey), "OPPO%d", iPlayer);
        TagFieldGetString(TagFieldFind(pData, strKey), pPlayer->strPers, sizeof(pPlayer->strPers), "");
        ds_snzprintf(strKey, sizeof(strKey), "ADDR%d", iPlayer);
        pPlayer->uAddr = TagFieldGetAddress(TagFieldFind(pData, strKey), 0);
        ds_snzprintf(strKey, sizeof(strKey), "LADDR%d", iPlayer);
        pPlayer->uLocalAddr = TagFieldGetAddress(TagFieldFind(pData, strKey), 0);
        ds_snzprintf(strKey, sizeof(strKey), "MADDR%d", iPlayer);
        TagFieldGetString(TagFieldFind(pData, strKey), pPlayer->strMachineAddr, sizeof(pPlayer->strMachineAddr), "");
        ds_snzprintf(strKey, sizeof(strKey), "OPPART%d", iPlayer);
        pPlayer->iPartition = TagFieldGetNumber(TagFieldFind(pData, strKey), 0);
        ds_snzprintf(strKey, sizeof(strKey), "OPPARAM%d", iPlayer);
        TagFieldGetString(TagFieldFind(pData, strKey), pPlayer->strParams, sizeof(pPlayer->strParams), "");
        ds_snzprintf(strKey, sizeof(strKey), "OPFLAG%d", iPlayer);
        pPlayer->uFlags = (u32)TagFieldGetNumber(TagFieldFind(pData, strKey), 0);
        ds_snzprintf(strKey, sizeof(strKey), "PRES%d", iPlayer);
        pPlayer->iPresence = TagFieldGetNumber(TagFieldFind(pData, strKey), 0);
    }

    // the partitions (the count is taken as sent)
    for (iPart = 0; iPart < pPlay->iNumPartitions; ++iPart)
    {
        ds_snzprintf(strKey, sizeof(strKey), "PARTSIZE%d", iPart);
        pPlay->aPartitions[iPart].iSize = TagFieldGetNumber(TagFieldFind(pData, strKey), 0);
        ds_snzprintf(strKey, sizeof(strKey), "PARTPARAMS%d", iPart);
        TagFieldGetString(TagFieldFind(pData, strKey), pPlay->aPartitions[iPart].strParams,
                          sizeof(pPlay->aPartitions[iPart].strParams), NULL);
    }

    // the session string is only replaced when the record carries one
    const char* pSess = TagFieldFind(pData, "SESS");
    if (pSess != NULL)
    {
        TagFieldGetString(pSess, pPlay->strSess, sizeof(pPlay->strSess), "");
    }
}
