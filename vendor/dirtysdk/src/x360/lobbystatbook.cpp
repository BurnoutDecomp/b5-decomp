#include "lobbystatbook.h"
#include "dirtymem.h"
#include "dirtylib.h"
#include "lobbytagfield.h"
#include "lobbylocalize_internal.h"
#include <cstring>
#include <cstdlib>

// ARTIST 0x82B1AFE8; provider belongs to the LobbyApi lane.
extern "C" LobbyApiUserT* LobbyApiGetUserInfo(LobbyApiRefT*, const char*);

// DirtySDK 5.5.3 lobbystatbook.c: private declaration shape from DecFIGS;
// ARTIST accessors confirm eStatus, iError, pRowDescs and the two u16 counts.
struct LobbyStatbookRowT
{
    char strValue[32];
};
struct LobbyStatbookSlotT
{
    LobbyStatbookRowT* pRows;
    u32 bAvailable;
};
struct LobbyStatbookViewInfoT
{
    char strName[12];
    char strDesc[96];
    char strStyle[32];
};

enum LobbyStatbookStatusE
{
    LOBBYSTATBOOK_STATUS_IDLE = 0,
    LOBBYSTATBOOK_STATUS_HAVE_VIEW = 2,
    LOBBYSTATBOOK_STATUS_HAVE_VIEW_INFO = 4
};

enum LobbyStatbookDownloadStatusE
{
    LOBBYSTATBOOK_DOWNLOAD_STATUS_IDLE = 0,
    LOBBYSTATBOOK_DOWNLOAD_STATUS_VIEW = 1,
    LOBBYSTATBOOK_DOWNLOAD_STATUS_ROW = 2,
    LOBBYSTATBOOK_DOWNLOAD_STATUS_VIEW_INFO = 4
};

struct LobbyStatbookT
{
    LobbyApiRefT* pLobby;
    s32 iMemGroup;
    LobbyStatbookStatusE eStatus;
    LobbyStatbookDownloadStatusE eDownloadStatus;
    s32 iError;
    LobbyStatbookRowInfoT* pRowDescs;
    LobbyStatbookSlotT* pSlots;
    LobbyStatbookViewInfoT* pViews;
    LobbyLocalizeT* pLocalize;
    char strViewName[12];
    u16 uRowCount;
    u16 uViewCount;
    u16 uSlotCount;
    u16 uSlotDownloading;
};

// ARTIST 0x82B237D0. The slots are allocated immediately after the header.
LobbyStatbookT* LobbyStatbookCreate(LobbyApiRefT* pLobbyApi, u32 uSlots)
{
    const s32 iMemGroup = DirtyMemGroupQuery();
    if (!uSlots || uSlots > 1024)
    {
        NetPrintfCode("_LobbyStatbookCreate: uSlots %u is out of range\n", uSlots);
        uSlots = 10;
    }
    LobbyStatbookT* pStatbook = static_cast<LobbyStatbookT*>(DirtyMemAlloc(
        sizeof(LobbyStatbookT) + uSlots * sizeof(LobbyStatbookSlotT), 0x6c737461, iMemGroup));
    if (!pStatbook) return 0;
    pStatbook->iMemGroup = iMemGroup;
    pStatbook->pLobby = pLobbyApi;
    pStatbook->pLocalize = 0;
    pStatbook->eStatus = LOBBYSTATBOOK_STATUS_IDLE;
    pStatbook->eDownloadStatus = LOBBYSTATBOOK_DOWNLOAD_STATUS_IDLE;
    pStatbook->iError = 0;
    std::memset(pStatbook->strViewName, 0, sizeof(pStatbook->strViewName));
    pStatbook->pRowDescs = 0;
    pStatbook->pSlots = reinterpret_cast<LobbyStatbookSlotT*>(pStatbook + 1);
    std::memset(pStatbook->pSlots, 0, uSlots * sizeof(LobbyStatbookSlotT));
    pStatbook->uRowCount = 0;
    pStatbook->uSlotCount = static_cast<u16>(uSlots);
    pStatbook->uSlotDownloading = 0;
    pStatbook->uViewCount = 0;
    pStatbook->pViews = 0;
    return pStatbook;
}

// ARTIST 0x82B23898.
void LobbyStatbookDestroy(LobbyStatbookT* pStatbook)
{
    LobbyLocalizeDestroy(pStatbook->pLocalize);
    if (pStatbook->pRowDescs) DirtyMemFree(pStatbook->pRowDescs, 0x6c737461, pStatbook->iMemGroup);
    if (pStatbook->pViews) DirtyMemFree(pStatbook->pViews, 0x6c737461, pStatbook->iMemGroup);
    DirtyMemFree(pStatbook, 0x6c737461, pStatbook->iMemGroup);
}

// ARTIST 0x82B23B70 (raw; slot availability and error word).
s32 LobbyStatbookStatus(LobbyStatbookT* pStatbook, u32 uSlot)
{
    if (uSlot >= pStatbook->uSlotCount) return LOBBYSTATBOOK_ERROR_BAD_SLOT;
    if (pStatbook->iError) return pStatbook->iError;
    return pStatbook->pSlots[uSlot].bAvailable;
}

// ARTIST 0x82B23BD0; ds_strnzcpy @0x82B2AD30 is inlined below.
const char* LobbyStatbookRowData(LobbyStatbookT* pStatbook, u32 uSlot, u32 uRow,
                                 char* pBuffer, s32 iBuflen)
{
    if (uSlot >= pStatbook->uSlotCount) return 0;
    LobbyStatbookSlotT* pSlot = &pStatbook->pSlots[uSlot];
    if (!pSlot->bAvailable || uRow >= pStatbook->uRowCount) return 0;
    const char* pValue = pSlot->pRows[uRow].strValue;
    if (pBuffer)
    {
        std::strncpy(pBuffer, pValue, iBuflen);
        pBuffer[iBuflen - 1] = 0;
    }
    return pValue;
}

// ARTIST 0x82B235E0, _LobbyStatbookRowCb.
static void _LobbyStatbookRowCb(LobbyApiRefT*, LobbyApiMsgT* pMsg, void* pUserData)
{
    LobbyStatbookT* pStatbook = static_cast<LobbyStatbookT*>(pUserData);
    if (!(pStatbook->eDownloadStatus & LOBBYSTATBOOK_DOWNLOAD_STATUS_ROW))
    {
        NetPrintfCode("_LobbyStatbookRowCb: incorrectly triggered in state %d\n", pStatbook->eDownloadStatus);
        return;
    }
    pStatbook->eDownloadStatus = static_cast<LobbyStatbookDownloadStatusE>(pStatbook->eDownloadStatus ^ 2);
    if (pMsg->code)
    {
        if (pMsg->code == 0x69706572) pStatbook->iError = LOBBYSTATBOOK_ERROR_PLAYER_NOT_FOUND;
        else if (pMsg->code == 0x74696d65) pStatbook->iError = LOBBYSTATBOOK_ERROR_TIMEOUT;
        else
        {
            if (pMsg->code != 0x64626572 && pMsg->code != 0x6f766572)
                NetPrintfCode("_LobbyStatbookRowCb: unexpected error %x\n", pMsg->code);
            pStatbook->iError = LOBBYSTATBOOK_ERROR_MISC;
        }
        return;
    }
    const int iSlot = TagFieldGetNumber(TagFieldFind(pMsg->pData, "SLOT"), -1);
    if (iSlot != pStatbook->uSlotDownloading)
    {
        NetPrintfCode("_LobbyStatbookRowCb: expected slot %u got %u\n", pStatbook->uSlotDownloading, iSlot);
        return;
    }
    const char* pStats = TagFieldFind(pMsg->pData, "STATS");
    LobbyStatbookSlotT* pSlot = &pStatbook->pSlots[iSlot];
    u32 uIndex = 0;
    for (; uIndex < pStatbook->uRowCount; ++uIndex)
    {
        char strValue[32];
        if (TagFieldGetDelim(pStats, strValue, sizeof(strValue), 0, uIndex, ',') < 0) break;
        NetPrintfCode("_LobbyStatbookRowCB: [%s]\n", strValue);
        LobbyLocalizeData(pStatbook->pLocalize, pStatbook->pRowDescs[uIndex].iType,
                          pSlot->pRows[uIndex].strValue, sizeof(pSlot->pRows[uIndex].strValue), strValue);
    }
    if (uIndex != pStatbook->uRowCount)
    {
        NetPrintfCode("_LobbyStatbookRowCb: expected %u stats found %u\n", pStatbook->uRowCount, uIndex);
        while (uIndex < pStatbook->uRowCount) pSlot->pRows[uIndex++].strValue[0] = 0;
    }
    pSlot->bAvailable = 1;
    pStatbook->iError = 0;
}

// ARTIST 0x82B23A20.
s32 LobbyStatbookFetch(LobbyStatbookT* pStatbook, u32 uSlot, const u8* pPersona)
{
    if (!(pStatbook->eStatus & LOBBYSTATBOOK_STATUS_HAVE_VIEW)) return LOBBYSTATBOOK_ERROR_VIEW_NOT_SELECTED;
    if (pStatbook->eDownloadStatus & LOBBYSTATBOOK_DOWNLOAD_STATUS_ROW) return LOBBYSTATBOOK_ERROR_BUSY;
    if (uSlot >= pStatbook->uSlotCount) return LOBBYSTATBOOK_ERROR_BAD_SLOT;
    char strPersona[16];
    char strRequest[64];
    strRequest[0] = 0;
    const char* pName = reinterpret_cast<const char*>(pPersona);
    if (!*pName)
    {
        LobbyApiStatus(pStatbook->pLobby, 0x70657273, strPersona, sizeof(strPersona));
        pName = strPersona;
    }
    TagFieldSetString(strRequest, sizeof(strRequest), "PERS", pName);
    TagFieldSetNumber(strRequest, sizeof(strRequest), "SLOT", uSlot);
    TagFieldSetString(strRequest, sizeof(strRequest), "VIEW", pStatbook->strViewName);
    pStatbook->iError = 0;
    pStatbook->uSlotDownloading = static_cast<u16>(uSlot);
    pStatbook->eDownloadStatus = LOBBYSTATBOOK_DOWNLOAD_STATUS_ROW;
    pStatbook->pSlots[uSlot].bAvailable = 0;
    if (!pStatbook->pLocalize)
    {
        u32 uLocality = 0x656e5553;
        LobbyApiUserT* pUser = LobbyApiGetUserInfo(pStatbook->pLobby, 0);
        if (pUser) uLocality = pUser->uLocality;
        pStatbook->pLocalize = LobbyLocalizeCreate(uLocality, 0);
    }
    LobbyApiRequestCB(pStatbook->pLobby, 0x73647461, strRequest, _LobbyStatbookRowCb, pStatbook);
    return 0;
}

// ARTIST 0x82B230D8, _LobbyStatbookViewCb.
static void _LobbyStatbookViewCb(LobbyApiRefT*, LobbyApiMsgT* pMsg, void* pUserData)
{
    LobbyStatbookT* pStatbook = static_cast<LobbyStatbookT*>(pUserData);
    if (!(pStatbook->eDownloadStatus & LOBBYSTATBOOK_DOWNLOAD_STATUS_VIEW))
    {
        NetPrintfCode("_LobbyStatbookViewCb: incorrectly triggered in state %d\n", pStatbook->eDownloadStatus);
        return;
    }
    pStatbook->eDownloadStatus = static_cast<LobbyStatbookDownloadStatusE>(pStatbook->eDownloadStatus ^ 1);
    if (pMsg->code)
    {
        if (pMsg->code == 0x74696d65) pStatbook->iError = LOBBYSTATBOOK_ERROR_TIMEOUT;
        else if (pMsg->code == 0x75766977) pStatbook->iError = LOBBYSTATBOOK_ERROR_INVALID_VIEW;
        else
        {
            if (pMsg->code != 0x64626572 && pMsg->code != 0x6f766572)
                NetPrintfCode("_LobbyStatbookViewCb: unexpected error %x\n", pMsg->code);
            pStatbook->iError = LOBBYSTATBOOK_ERROR_MISC;
        }
        return;
    }
    const u32 uCount = TagFieldGetNumber(TagFieldFind(pMsg->pData, "N"), 0);
    const u32 uSize = (pStatbook->uSlotCount * sizeof(LobbyStatbookRowT) + sizeof(LobbyStatbookRowInfoT)) * uCount;
    if (pStatbook->pRowDescs) DirtyMemFree(pStatbook->pRowDescs, 0x6c737461, pStatbook->iMemGroup);
    LobbyStatbookRowInfoT* pRows = static_cast<LobbyStatbookRowInfoT*>(
        DirtyMemAlloc(uSize, 0x6c737461, pStatbook->iMemGroup));
    if (!pRows)
    {
        NetPrintfCode("_LobbyStatbookViewCb: DirtyMemAlloc failed\n");
        pStatbook->iError = LOBBYSTATBOOK_ERROR_MISC;
        return;
    }
    pStatbook->pRowDescs = pRows;
    LobbyStatbookRowT* pValues = reinterpret_cast<LobbyStatbookRowT*>(pRows + uCount);
    for (u32 i = 0; i < pStatbook->uSlotCount; ++i)
    {
        pStatbook->pSlots[i].pRows = pValues;
        pValues += uCount;
    }
    const char* pNames = TagFieldFind(pMsg->pData, "NAMES");
    const char* pDescs = TagFieldFind(pMsg->pData, "DESCS");
    const char* pParams = TagFieldFind(pMsg->pData, "PARAMS");
    const char* pTypes = TagFieldFind(pMsg->pData, "TYPES");
    const char* pSymbols = TagFieldFind(pMsg->pData, "SYMS");
    u32 uIndex = 0;
    for (; uIndex < uCount; ++uIndex)
    {
        char strIndex[20];
        char strType[20];
        LobbyStatbookRowInfoT* pRow = &pRows[uIndex];
        if (TagFieldGetDelim(pNames, strIndex, sizeof(strIndex), "", uIndex, ',') <= 0) break;
        if (TagFieldGetDelim(pSymbols, pRow->strName, sizeof(pRow->strName), "", std::atoi(strIndex), ',') <= 0 || !pRow->strName[0]) break;
        if (TagFieldGetDelim(pDescs, strIndex, sizeof(strIndex), "", uIndex, ',') <= 0) break;
        if (TagFieldGetDelim(pSymbols, pRow->strDesc, sizeof(pRow->strDesc), "", std::atoi(strIndex), ',') <= 0 || !pRow->strDesc[0]) break;
        if (TagFieldGetDelim(pParams, strIndex, sizeof(strIndex), "", uIndex, ',') <= 0) break;
        if (TagFieldGetDelim(pSymbols, pRow->strParams, sizeof(pRow->strParams), "", std::atoi(strIndex), ',') <= 0) break;
        if (TagFieldGetDelim(pTypes, strType, sizeof(strType), "", uIndex, ',') <= 0) break;
        pRow->iType = TagFieldGetToken(strType, 0);
    }
    pStatbook->uRowCount = static_cast<u16>(uIndex);
    if (uCount != uIndex)
        NetPrintfCode("_LobbyStatbookViewCb: expected %d found %d\n", uCount, uIndex);
    pStatbook->iError = 0;
    pStatbook->eStatus = static_cast<LobbyStatbookStatusE>(pStatbook->eStatus | 2);
}

// ARTIST 0x82B23918.
s32 LobbyStatbookFetchView(LobbyStatbookT* pStatbook, const u8* pView)
{
    if (pStatbook->eStatus & LOBBYSTATBOOK_STATUS_HAVE_VIEW)
    {
        if (std::strcmp(reinterpret_cast<const char*>(pView), pStatbook->strViewName) == 0)
            return 0;
        pStatbook->eStatus = static_cast<LobbyStatbookStatusE>(pStatbook->eStatus & ~2);
    }
    if (pStatbook->eDownloadStatus == LOBBYSTATBOOK_DOWNLOAD_STATUS_VIEW && !pStatbook->iError) return 0;
    if (pStatbook->eDownloadStatus) return LOBBYSTATBOOK_ERROR_BUSY;
    char strRequest[32];
    strRequest[0] = 0;
    TagFieldSetString(strRequest, sizeof(strRequest), "VIEW", reinterpret_cast<const char*>(pView));
    pStatbook->iError = 0;
    pStatbook->uRowCount = 0;
    pStatbook->eDownloadStatus = LOBBYSTATBOOK_DOWNLOAD_STATUS_VIEW;
    std::strncpy(pStatbook->strViewName, reinterpret_cast<const char*>(pView), sizeof(pStatbook->strViewName));
    pStatbook->strViewName[sizeof(pStatbook->strViewName) - 1] = 0;
    std::memset(pStatbook->pSlots, 0, pStatbook->uSlotCount * sizeof(LobbyStatbookSlotT));
    LobbyApiRequestCB(pStatbook->pLobby, 0x73766977, strRequest, _LobbyStatbookViewCb, pStatbook);
    return 0;
}

// ARTIST 0x82B23430, _LobbyStatbookViewInfoCb.
static void _LobbyStatbookViewInfoCb(LobbyApiRefT*, LobbyApiMsgT* pMsg, void* pUserData)
{
    LobbyStatbookT* pStatbook = static_cast<LobbyStatbookT*>(pUserData);
    if (!(pStatbook->eDownloadStatus & LOBBYSTATBOOK_DOWNLOAD_STATUS_VIEW_INFO))
    {
        NetPrintfCode("_LobbyStatbookViewInfoCb: incorrectly triggered in state %d\n", pStatbook->eDownloadStatus);
        return;
    }
    pStatbook->eDownloadStatus = static_cast<LobbyStatbookDownloadStatusE>(pStatbook->eDownloadStatus ^ 4);
    if (pMsg->code)
    {
        NetPrintfCode("_LobbyStatbookViewInfoCb: unexpected error %x\n", pMsg->code);
        pStatbook->iError = -1;
        return;
    }
    const u32 uCount = TagFieldGetNumber(TagFieldFind(pMsg->pData, "COUNT"), 0);
    if (pStatbook->pViews) DirtyMemFree(pStatbook->pViews, 0x6c737461, pStatbook->iMemGroup);
    LobbyStatbookViewInfoT* pViews = static_cast<LobbyStatbookViewInfoT*>(
        DirtyMemAlloc(sizeof(LobbyStatbookViewInfoT) * uCount, 0x6c737461, pStatbook->iMemGroup));
    if (!pViews)
    {
        NetPrintfCode("_LobbyStatbookViewInfoCb: DirtyMemAlloc failed\n");
        pStatbook->iError = -1;
        return;
    }
    pStatbook->pViews = pViews;
    u32 uIndex = 0;
    for (; uIndex < uCount; ++uIndex)
    {
        const char* pView = TagFieldFindIdx(pMsg->pData, "VIEW", uIndex);
        if (!pView) break;
        TagFieldGetDelim(pView, pViews[uIndex].strName, sizeof(pViews[uIndex].strName), "", 0, ',');
        TagFieldGetDelim(pView, pViews[uIndex].strDesc, sizeof(pViews[uIndex].strDesc), "", 1, ',');
        TagFieldGetDelim(pView, pViews[uIndex].strStyle, sizeof(pViews[uIndex].strStyle), "", 2, ',');
    }
    if (uCount != uIndex)
        NetPrintfCode("_LobbyStatbookViewInfoCb: expected %d found %d\n", uCount, uIndex);
    pStatbook->uViewCount = static_cast<u16>(uIndex);
    pStatbook->iError = 0;
    pStatbook->eStatus = static_cast<LobbyStatbookStatusE>(pStatbook->eStatus | 4);
}

// ARTIST 0x82B23C98: request the remote view list once.
s32 LobbyStatbookFetchViewInfo(LobbyStatbookT* pStatbook)
{
    if (!(pStatbook->eStatus & LOBBYSTATBOOK_STATUS_HAVE_VIEW_INFO))
    {
        if (pStatbook->eDownloadStatus != LOBBYSTATBOOK_DOWNLOAD_STATUS_VIEW_INFO || pStatbook->iError)
        {
            if (pStatbook->eDownloadStatus) return LOBBYSTATBOOK_ERROR_BUSY;
            char strRequest[32];
            LobbyApiUserT lUser;
            strRequest[0] = 0;
            LobbyApiStatus(pStatbook->pLobby, 0x73656c66, &lUser, sizeof(lUser));
            TagFieldSetToken(strRequest, sizeof(strRequest), "LOC", lUser.uLocality);
            pStatbook->iError = 0;
            pStatbook->uViewCount = 0;
            pStatbook->eDownloadStatus = LOBBYSTATBOOK_DOWNLOAD_STATUS_VIEW_INFO;
            LobbyApiRequestCB(pStatbook->pLobby, 0x736c7374, strRequest, _LobbyStatbookViewInfoCb, pStatbook);
        }
    }
    return 0;
}

// ARTIST 0x82B23C90.
s32 LobbyStatbookViewCount(LobbyStatbookT* pStatbook)
{
    return pStatbook->uViewCount;
}

// ARTIST 0x82B23C60.
s32 LobbyStatbookViewStatus(LobbyStatbookT* pStatbook)
{
    if (pStatbook->eStatus & LOBBYSTATBOOK_STATUS_HAVE_VIEW)
        return pStatbook->uRowCount;
    if (pStatbook->eStatus & 1)
        return 0;
    return pStatbook->iError;
}

// ARTIST 0x82B23D68.
s32 LobbyStatbookViewInfoStatus(LobbyStatbookT* pStatbook)
{
    if (pStatbook->eStatus & LOBBYSTATBOOK_STATUS_HAVE_VIEW_INFO)
        return pStatbook->uViewCount;
    return pStatbook->iError;
}

// ARTIST 0x82B23BA8.
LobbyStatbookRowInfoT* LobbyStatbookRowInfo(LobbyStatbookT* pStatbook, u32 uRow)
{
    if (uRow >= pStatbook->uRowCount)
        return 0;
    return &pStatbook->pRowDescs[uRow];
}
