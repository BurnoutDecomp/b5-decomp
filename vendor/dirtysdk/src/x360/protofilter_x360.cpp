// DirtySDK ProtoFilter provider used by the X360 Lobby API.
// ARTIST: ProtoFilterCreate @ 0x82B404F8, Request @ 0x82B40580,
// RequestCancel @ 0x82B40678, Result @ 0x82B40730, ApplyFilter @ 0x82B407B0,
// Control @ 0x82B40820, Destroy @ 0x82B40870.

#include "types.hpp"
#include "dirtylib.h"
#include "dirtymem.h"
#include "protofilter.h"

#include <xtl.h>
#include <xonline.h>
#include <stddef.h>
#include <string.h>
#include <wchar.h>

extern "C" char* ds_strnzcpy(char* pDst, const char* pSrc, s32 iSize);

struct ProtoFilterSlotT
{
    STRING_DATA              StringData;       // +0x00; packed size 6
    u8                       aPad6[2];         // +0x06
    XOVERLAPPED              Overlapped;       // +0x08
    DWORD                    uResultsSize;     // +0x24; 12 for one result
    STRING_VERIFY_RESPONSE   Results;          // +0x28; packed size 6
    u8                       aPad46[2];        // +0x2e
    HRESULT                  iStringResult;    // +0x30; result[0]
    u8                       bPending;         // +0x34
    u8                       aPad53[3];        // +0x35
    WCHAR                    aString[1024];    // +0x38
};

struct ProtoFilterRefT
{
    s32               iMemGroup;              // +0x00
    char              strProfane[32];         // +0x04
    ProtoFilterSlotT  aRequests[2];           // +0x24; stride 0x838
};

CGS_STATIC_ASSERT(sizeof(XOVERLAPPED) == 28, "XDK XOVERLAPPED size");
CGS_STATIC_ASSERT(sizeof(STRING_DATA) == 6, "XDK STRING_DATA size");
CGS_STATIC_ASSERT(offsetof(STRING_DATA, pszString) == 2,
                  "XDK STRING_DATA string pointer");
CGS_STATIC_ASSERT(sizeof(STRING_VERIFY_RESPONSE) == 6,
                  "XDK STRING_VERIFY_RESPONSE size");
CGS_STATIC_ASSERT(offsetof(STRING_VERIFY_RESPONSE, pStringResult) == 2,
                  "XDK STRING_VERIFY_RESPONSE result pointer");
CGS_STATIC_ASSERT(sizeof(ProtoFilterSlotT) == 2104, "ProtoFilter slot stride");
CGS_STATIC_ASSERT(offsetof(ProtoFilterSlotT, Overlapped) == 8,
                  "ProtoFilter XOVERLAPPED offset");
CGS_STATIC_ASSERT(offsetof(ProtoFilterSlotT, Results) == 40,
                  "ProtoFilter result header offset");
CGS_STATIC_ASSERT(offsetof(ProtoFilterSlotT, iStringResult) == 48,
                  "ProtoFilter result word offset");
CGS_STATIC_ASSERT(offsetof(ProtoFilterSlotT, bPending) == 52,
                  "ProtoFilter pending byte offset");
CGS_STATIC_ASSERT(offsetof(ProtoFilterSlotT, aString) == 56,
                  "ProtoFilter UTF-16 input offset");
CGS_STATIC_ASSERT(sizeof(ProtoFilterRefT) == 4244, "ProtoFilter state size");
CGS_STATIC_ASSERT(offsetof(ProtoFilterRefT, strProfane) == 4,
                  "ProtoFilter replacement string offset");
CGS_STATIC_ASSERT(offsetof(ProtoFilterRefT, aRequests) == 36,
                  "ProtoFilter first request offset");

static const s32 KI_PROTOFILTER_MEMID = 0x70666c74; // pflt
static const s32 KI_PROTOFILTER_REQUEST_COUNT = 2;

extern "C" ProtoFilterRefT* ProtoFilterCreate(void)
{
    const s32 iMemGroup = DirtyMemGroupQuery();
    ProtoFilterRefT* pFilter = (ProtoFilterRefT*)DirtyMemAlloc(
        sizeof(*pFilter), KI_PROTOFILTER_MEMID, iMemGroup);
    if (pFilter == NULL)
    {
        NetPrintfCode("ProtoFilter: could not allocate module state\n");
        return NULL;
    }

    memset(pFilter, 0, sizeof(*pFilter));
    pFilter->iMemGroup = iMemGroup;
    return pFilter;
}

extern "C" s32 ProtoFilterRequest(ProtoFilterRefT* pFilter, const char* pText,
                                    s32* pRequest)
{
    for (s32 iIndex = 0; iIndex < KI_PROTOFILTER_REQUEST_COUNT; ++iIndex)
    {
        ProtoFilterSlotT* pSlot = &pFilter->aRequests[iIndex];
        if (pSlot->bPending != 0)
        {
            continue;
        }

        // ARTIST widens each source byte by sign-extending it before storing the
        // UTF-16 code unit. The XDK STRING_DATA points into this slot-owned buffer.
        const u8* pSource = (const u8*)pText;
        WCHAR* pTarget = pSlot->aString;
        s32 iChar = *pSource;
        while (iChar != 0)
        {
            *pTarget++ = (WCHAR)(s16)(s8)iChar;
            iChar = *++pSource;
        }
        *pTarget = 0;

        pSlot->StringData.wStringSize = (WORD)wcslen((const wchar_t*)pSlot->aString);
        pSlot->StringData.pszString = pSlot->aString;
        pSlot->uResultsSize = 12;
        memset(&pSlot->Results, 0, 12);
        pSlot->Results.pStringResult = &pSlot->iStringResult;

        const DWORD uResult = XStringVerify(
            0, "en-us", 1, &pSlot->StringData, pSlot->uResultsSize,
            &pSlot->Results, &pSlot->Overlapped);
        if (uResult == ERROR_IO_PENDING)
        {
            pSlot->bPending = 1;
            *pRequest = iIndex;
            return -1;
        }
    }
    return -2;
}

extern "C" s32 ProtoFilterRequestCancel(ProtoFilterRefT* pFilter, s32 iRequest)
{
    s32 iResult = (s32)pFilter;
    if (iRequest == -1)
    {
        for (s32 iIndex = 0; iIndex < KI_PROTOFILTER_REQUEST_COUNT; ++iIndex)
        {
            ProtoFilterSlotT* pSlot = &pFilter->aRequests[iIndex];
            if (pSlot->bPending != 0)
            {
                iResult = (s32)XCancelOverlapped(&pSlot->Overlapped);
                if (iResult != 0)
                {
                    iResult = NetPrintfCode(
                        "protofilter: error cancelling overlapped operatation\n");
                }
                pSlot->bPending = 0;
            }
        }
    }
    else if (iRequest < KI_PROTOFILTER_REQUEST_COUNT)
    {
        ProtoFilterSlotT* pSlot = &pFilter->aRequests[iRequest];
        if (pSlot->bPending != 0)
        {
            iResult = (s32)XCancelOverlapped(&pSlot->Overlapped);
            if (iResult != 0)
            {
                iResult = NetPrintfCode(
                    "protofilter: error cancelling overlapped operatation\n");
            }
            pSlot->bPending = 0;
        }
    }
    return iResult;
}

extern "C" s32 ProtoFilterResult(ProtoFilterRefT* pFilter, u32 uRequest)
{
    if (uRequest >= KI_PROTOFILTER_REQUEST_COUNT)
    {
        return -3;
    }

    ProtoFilterSlotT* pSlot = &pFilter->aRequests[uRequest];
    if (pSlot->bPending == 0)
    {
        return -3;
    }
    if (XGetOverlappedExtendedError(&pSlot->Overlapped) == ERROR_IO_INCOMPLETE)
    {
        return -1;
    }

    pSlot->bPending = 0;
    return (pSlot->iStringResult != 0) ? 1 : 0;
}

extern "C" s32 ProtoFilterApplyFilter(ProtoFilterRefT* pFilter, s32 iAction,
                                        char* pText)
{
    if (iAction == 1)
    {
        char* pChar = pText;
        while (*pChar != '\0')
        {
            if (*pChar != ' ')
            {
                *pChar = '*';
            }
            ++pChar;
        }
        return (s32)pFilter;
    }
    if (iAction == 2)
    {
        if (pFilter->strProfane[0] != '\0')
        {
            ds_strnzcpy(pText, pFilter->strProfane, 32);
        }
        else
        {
            ds_strnzcpy(pText, "@/&!", 5);
        }
        return (s32)pText;
    }
    return (s32)pFilter;
}

extern "C" s32 ProtoFilterControl(ProtoFilterRefT* pFilter, s32 iSelect,
                                    s32 iValue, s32 iValue2, const char* pValue)
{
    (void)iValue;
    (void)iValue2;
    if ((u32)iSelect != 0x70737472) // pstr
    {
        return -1;
    }
    ds_strnzcpy(pFilter->strProfane, pValue, sizeof(pFilter->strProfane));
    return 0;
}

extern "C" void ProtoFilterDestroy(ProtoFilterRefT* pFilter)
{
    ProtoFilterRequestCancel(pFilter, -1);
    DirtyMemFree(pFilter, KI_PROTOFILTER_MEMID, pFilter->iMemGroup);
}
