// ===========================================================================
// RealmcIface::XenonUtil -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// The Xenon (Xbox 360) platform back-end of the Realmc memory-card SDK: content
// package (XContent*) management, the device-selector UI (XShowDeviceSelectorUI /
// XNotifyGetNext) and Win32 file I/O. No leak source / no DWARF: SHAPE and
// BODIES both come from the X360 pseudocode + asm. See RealmcXenonUtil.h for the
// class/State layout.
//
// Result codes materialised by the asm and reproduced verbatim (no attested enum
// to name them):
//   0 ok | 2 card removed/not present | 7 wrong device | 8 generic I/O error
//   9 not enough space | 10 truncated read | 13 not signed in | 14 cancelled
//   15 still busy.
// ===========================================================================

#define _CRT_SECURE_NO_WARNINGS   // wcsncpy / strncpy / strcpy (the X360 inlines these)

#include "SDKs/Realmc/RealmcXenonUtil.h"

#include "SDKs/EATech/eathread/BrnEAThreadX360.h"   // EA::Thread::ThreadSleep

#include <cstring>   // memset / strncpy / strcpy / strcat
#include <cwchar>    // wcsncpy

// ARTIST Crc32 table at 0x82FA7C88. All 256 words were read from the X360 image
// and match the non-reflected 0x04C11DB7 polynomial table.
namespace
{
    const u32 kCrc32Table[256] =
    {
        0x00000000u, 0x04C11DB7u, 0x09823B6Eu, 0x0D4326D9u,
        0x130476DCu, 0x17C56B6Bu, 0x1A864DB2u, 0x1E475005u,
        0x2608EDB8u, 0x22C9F00Fu, 0x2F8AD6D6u, 0x2B4BCB61u,
        0x350C9B64u, 0x31CD86D3u, 0x3C8EA00Au, 0x384FBDBDu,
        0x4C11DB70u, 0x48D0C6C7u, 0x4593E01Eu, 0x4152FDA9u,
        0x5F15ADACu, 0x5BD4B01Bu, 0x569796C2u, 0x52568B75u,
        0x6A1936C8u, 0x6ED82B7Fu, 0x639B0DA6u, 0x675A1011u,
        0x791D4014u, 0x7DDC5DA3u, 0x709F7B7Au, 0x745E66CDu,
        0x9823B6E0u, 0x9CE2AB57u, 0x91A18D8Eu, 0x95609039u,
        0x8B27C03Cu, 0x8FE6DD8Bu, 0x82A5FB52u, 0x8664E6E5u,
        0xBE2B5B58u, 0xBAEA46EFu, 0xB7A96036u, 0xB3687D81u,
        0xAD2F2D84u, 0xA9EE3033u, 0xA4AD16EAu, 0xA06C0B5Du,
        0xD4326D90u, 0xD0F37027u, 0xDDB056FEu, 0xD9714B49u,
        0xC7361B4Cu, 0xC3F706FBu, 0xCEB42022u, 0xCA753D95u,
        0xF23A8028u, 0xF6FB9D9Fu, 0xFBB8BB46u, 0xFF79A6F1u,
        0xE13EF6F4u, 0xE5FFEB43u, 0xE8BCCD9Au, 0xEC7DD02Du,
        0x34867077u, 0x30476DC0u, 0x3D044B19u, 0x39C556AEu,
        0x278206ABu, 0x23431B1Cu, 0x2E003DC5u, 0x2AC12072u,
        0x128E9DCFu, 0x164F8078u, 0x1B0CA6A1u, 0x1FCDBB16u,
        0x018AEB13u, 0x054BF6A4u, 0x0808D07Du, 0x0CC9CDCAu,
        0x7897AB07u, 0x7C56B6B0u, 0x71159069u, 0x75D48DDEu,
        0x6B93DDDBu, 0x6F52C06Cu, 0x6211E6B5u, 0x66D0FB02u,
        0x5E9F46BFu, 0x5A5E5B08u, 0x571D7DD1u, 0x53DC6066u,
        0x4D9B3063u, 0x495A2DD4u, 0x44190B0Du, 0x40D816BAu,
        0xACA5C697u, 0xA864DB20u, 0xA527FDF9u, 0xA1E6E04Eu,
        0xBFA1B04Bu, 0xBB60ADFCu, 0xB6238B25u, 0xB2E29692u,
        0x8AAD2B2Fu, 0x8E6C3698u, 0x832F1041u, 0x87EE0DF6u,
        0x99A95DF3u, 0x9D684044u, 0x902B669Du, 0x94EA7B2Au,
        0xE0B41DE7u, 0xE4750050u, 0xE9362689u, 0xEDF73B3Eu,
        0xF3B06B3Bu, 0xF771768Cu, 0xFA325055u, 0xFEF34DE2u,
        0xC6BCF05Fu, 0xC27DEDE8u, 0xCF3ECB31u, 0xCBFFD686u,
        0xD5B88683u, 0xD1799B34u, 0xDC3ABDEDu, 0xD8FBA05Au,
        0x690CE0EEu, 0x6DCDFD59u, 0x608EDB80u, 0x644FC637u,
        0x7A089632u, 0x7EC98B85u, 0x738AAD5Cu, 0x774BB0EBu,
        0x4F040D56u, 0x4BC510E1u, 0x46863638u, 0x42472B8Fu,
        0x5C007B8Au, 0x58C1663Du, 0x558240E4u, 0x51435D53u,
        0x251D3B9Eu, 0x21DC2629u, 0x2C9F00F0u, 0x285E1D47u,
        0x36194D42u, 0x32D850F5u, 0x3F9B762Cu, 0x3B5A6B9Bu,
        0x0315D626u, 0x07D4CB91u, 0x0A97ED48u, 0x0E56F0FFu,
        0x1011A0FAu, 0x14D0BD4Du, 0x19939B94u, 0x1D528623u,
        0xF12F560Eu, 0xF5EE4BB9u, 0xF8AD6D60u, 0xFC6C70D7u,
        0xE22B20D2u, 0xE6EA3D65u, 0xEBA91BBCu, 0xEF68060Bu,
        0xD727BBB6u, 0xD3E6A601u, 0xDEA580D8u, 0xDA649D6Fu,
        0xC423CD6Au, 0xC0E2D0DDu, 0xCDA1F604u, 0xC960EBB3u,
        0xBD3E8D7Eu, 0xB9FF90C9u, 0xB4BCB610u, 0xB07DABA7u,
        0xAE3AFBA2u, 0xAAFBE615u, 0xA7B8C0CCu, 0xA379DD7Bu,
        0x9B3660C6u, 0x9FF77D71u, 0x92B45BA8u, 0x9675461Fu,
        0x8832161Au, 0x8CF30BADu, 0x81B02D74u, 0x857130C3u,
        0x5D8A9099u, 0x594B8D2Eu, 0x5408ABF7u, 0x50C9B640u,
        0x4E8EE645u, 0x4A4FFBF2u, 0x470CDD2Bu, 0x43CDC09Cu,
        0x7B827D21u, 0x7F436096u, 0x7200464Fu, 0x76C15BF8u,
        0x68860BFDu, 0x6C47164Au, 0x61043093u, 0x65C52D24u,
        0x119B4BE9u, 0x155A565Eu, 0x18197087u, 0x1CD86D30u,
        0x029F3D35u, 0x065E2082u, 0x0B1D065Bu, 0x0FDC1BECu,
        0x3793A651u, 0x3352BBE6u, 0x3E119D3Fu, 0x3AD08088u,
        0x2497D08Du, 0x2056CD3Au, 0x2D15EBE3u, 0x29D4F654u,
        0xC5A92679u, 0xC1683BCEu, 0xCC2B1D17u, 0xC8EA00A0u,
        0xD6AD50A5u, 0xD26C4D12u, 0xDF2F6BCBu, 0xDBEE767Cu,
        0xE3A1CBC1u, 0xE760D676u, 0xEA23F0AFu, 0xEEE2ED18u,
        0xF0A5BD1Du, 0xF464A0AAu, 0xF9278673u, 0xFDE69BC4u,
        0x89B8FD09u, 0x8D79E0BEu, 0x803AC667u, 0x84FBDBD0u,
        0x9ABC8BD5u, 0x9E7D9662u, 0x933EB0BBu, 0x97FFAD0Cu,
        0xAFB010B1u, 0xAB710D06u, 0xA6322BDFu, 0xA2F33668u,
        0xBCB4666Du, 0xB8757BDAu, 0xB5365D03u, 0xB1F740B4u,
    };
}

// Crc32 @ 0x82C44850. The X360 returns zero for lengths below one word, seeds
// from the complemented first big-endian word, then updates once per remaining
// byte and complements the final state. This is the checksum used by the
// Realmc file header and payload paths below.
u32 Crc32(const void* lpData, u32 luLength)
{
    if (luLength < 4)
        return 0;

    const u8* lpBytes = static_cast<const u8*>(lpData);
    u32 luState = (static_cast<u32>(lpBytes[0]) << 24)
                | (static_cast<u32>(lpBytes[1]) << 16)
                | (static_cast<u32>(lpBytes[2]) << 8)
                |  static_cast<u32>(lpBytes[3]);
    luState = ~luState;

    for (u32 luIndex = 4; luIndex < luLength; ++luIndex)
    {
        const u32 luTableIndex = luState >> 24;
        luState = ((luState << 8) | static_cast<u32>(lpBytes[luIndex]))
                ^ kCrc32Table[luTableIndex];
    }

    return ~luState;
}

namespace RealmcIface
{

namespace
{
    // The X360 file-scope memory-card globals this TU owns. dword_82F8797C is the
    // signed-in user index (-1 == none); byte_83250194 caches the last observed
    // storage-device-present state. FLAGGED: modelled as internal-linkage statics;
    // the not-yet-homed XenonRunnableTask TUs share the same X360 globals and must,
    // when landed, promote these to a shared home so the symbol is single.
    const u32 KU_NO_USER = 0xFFFFFFFFu;   // dword_82F8797C sentinel (X360 cmpwi -1)

    u32  sDwUserIndex   = KU_NO_USER;   // dword_82F8797C
    bool sbDeviceChanged = false;       // byte_83250194

    // off_82F87978 -- the content mount root the XContent* APIs virtual-mount and
    // OpenFile prefixes onto its "<root>:\\<file>" path. IDA resolved the pointer's
    // value to the literal "rmcsave".
    const char* const KPSZ_XENON_ROOT = "rmcsave";
}

// ---------------------------------------------------------------------------
// CardExists @ 0x82B53A18 -- 0 if the named device is present, 2 otherwise.
// ---------------------------------------------------------------------------
int XenonUtil::CardExists(const XCONTENTDEVICEID* lpuDeviceId)
{
    u32 luDeviceId = *lpuDeviceId;
    if (luDeviceId == 0)
        return 2;
    if (XContentGetDeviceState(luDeviceId, 0) == 0)
        return 0;
    return 2;
}

// ---------------------------------------------------------------------------
// CardResultToCardStatus @ 0x82B53AD8 -- map an internal card RESULT code to
// the public card STATUS the worker tasks report. The X360 compiles this as a
// 16-entry byte jump table (byte_82148638 @ 0x82148638, rodata-attested:
// 00 40 08 40 40 40 20 28 28 30 40 00 30 38 08 40; +off from loc_82B53B08:
// 0x00=ret 0, 0x08=mbDeviceMounted?8:7, 0x20=ret 4, 0x28=ret 2, 0x30=ret 3,
// 0x38=ret 5, 0x40=ret 1). The State* second argument is real even though the
// pseudocode drops it: the 0x08 fragment reads 0xB0(r4) and every caller loads
// r4 from [task+0x10] before the call.
// ---------------------------------------------------------------------------
int XenonUtil::CardResultToCardStatus(u32 luResult, const State* lpState)
{
    switch (luResult)
    {
        case 0:                              // ok
        case 11:
            return 0;
        case 2:                              // card removed / not present
        case 14:                             // cancelled
            return lpState->mbDeviceMounted ? 8 : 7;
        case 6:
            return 4;
        case 7:                              // wrong device
        case 8:                              // generic I/O error
            return 2;
        case 9:                              // not enough space
        case 12:
            return 3;
        case 13:                             // not signed in
            return 5;
        default:                             // 1/3/4/5/10/15 and anything > 15
            return 1;
    }
}

// ---------------------------------------------------------------------------
// CheckState @ 0x82B53460 -- 0 if a user is signed in, else 13. Caches the code
// in State::miResult.
// ---------------------------------------------------------------------------
int XenonUtil::CheckState(State* lpState)
{
    int liResult;
    if (sDwUserIndex == KU_NO_USER || XUserGetSigninState(sDwUserIndex) == 0)
        liResult = 13;
    else
        liResult = 0;
    lpState->miResult = liResult;
    return liResult;
}

// ---------------------------------------------------------------------------
// CloseState @ 0x82B53400 -- close the notification/file handle and reset the
// device/overlapped/user state. Returns the CloseHandle BOOL.
// ---------------------------------------------------------------------------
BOOL XenonUtil::CloseState(State* lpState)
{
    HANDLE lHandle = lpState->mhNotification;
    lpState->mbCancel = 1;                       // *a1 = 1 (byte)
    BOOL lResult = CloseHandle(lHandle);
    lpState->mhNotification = INVALID_HANDLE_VALUE;   // -1
    std::memset(&lpState->mOverlapped, 0, sizeof(lpState->mOverlapped));        // zero the 28-byte async descriptor
    sDwUserIndex = KU_NO_USER;                    // dword_82F8797C = -1
    lpState->miResult = 0;
    return lResult;
}

// ---------------------------------------------------------------------------
// UpdateDeviceInfo @ 0x82B534C0 -- re-query the current device; on error reset
// the card data and return 14 (removed) / 1 (error). 0 if still present.
// ---------------------------------------------------------------------------
int XenonUtil::UpdateDeviceInfo(State* lpState)
{
    int liResult = 0;
    CardData& rCard = lpState->mCardData;
    u32 luDeviceId = rCard.maCurrent.DeviceID;    // *(a1+8)
    if (luDeviceId != 0)
    {
        DWORD luErr = XContentGetDeviceData(luDeviceId, &rCard.maCurrent);
        if (luErr == 0)
            return 0;
        liResult = (luErr == 1167) ? 14 : 1;
        rCard = CardData::Empty();
    }
    return liResult;
}

// ---------------------------------------------------------------------------
// DeviceSelectorUpdate @ 0x82B53548 -- poll the storage-device notification and
// the pending overlapped op. 15 while still busy, 14 on cancel, 1 on error, 0 ok.
// ---------------------------------------------------------------------------
int XenonUtil::DeviceSelectorUpdate(State* lpState)
{
    DWORD     luId = 0;
    ULONG_PTR luParam = 0;
    bool      lbDeviceChanged;
    if (XNotifyGetNext(lpState->mhNotification, 0, &luId, &luParam) && luId == 9)
    {
        lbDeviceChanged = (luParam != 0);
        sbDeviceChanged = lbDeviceChanged;
    }
    else
    {
        lbDeviceChanged = sbDeviceChanged;
    }

    if (lpState->mOverlapped.InternalLow == 997 || lbDeviceChanged)
        return 15;

    DWORD luErr = XGetOverlappedExtendedError(&lpState->mOverlapped);
    if (luErr == 0)
        return 0;
    if (luErr == 1223)
        return 14;
    return 1;
}

// ---------------------------------------------------------------------------
// DeviceSelectorShow @ 0x82B53CA8 -- run the device-selector UI, poll it to
// completion, then latch the chosen device's data into State::mCardData.
// ---------------------------------------------------------------------------
int XenonUtil::DeviceSelectorShow(State* lpState, u64 luBytesRequested, int liMode)
{
    int liResult = CheckState(lpState);
    if (liResult != 0)
        return liResult;

    DWORD dwContentType = (liMode == 1) ? 3 : 1;
    DWORD luDeviceId = 0;                          // the selected device id (BYREF)
    std::memset(&lpState->mOverlapped, 0, sizeof(lpState->mOverlapped));          // zero the async descriptor

    // Kick the selector UI until it starts (returns ERROR_IO_PENDING == 997) or
    // the operation is cancelled.
    ULARGE_INTEGER lBytesRequested;
    lBytesRequested.QuadPart = luBytesRequested;
    DWORD luShow;
    do
    {
        luShow = XShowDeviceSelectorUI(sDwUserIndex, dwContentType, lpState->muContentFlags,
                                       lBytesRequested, &luDeviceId, &lpState->mOverlapped);
    } while (!lpState->mbCancel && luShow != 997);

    // Pump it to completion (Update returns != 15) or until cancelled.
    int liStatus;
    do
    {
        liStatus = DeviceSelectorUpdate(lpState);
        u32 luSleepMs = 10;
        EA::Thread::ThreadSleep(&luSleepMs);
    } while (!lpState->mbCancel && liStatus == 15);

    if (luDeviceId == 0)
        liStatus = 14;                              // nothing chosen == cancelled
    if (liStatus == 0)
        liStatus = CheckState(lpState);

    lpState->mCardData = CardData::Empty();

    if (liStatus == 0)
    {
        lpState->muContentFlags = lpState->muNextContentFlags;
        DWORD luErr = XContentGetDeviceData(luDeviceId, &lpState->mCardData.maCurrent);
        if (luErr == 0)
            liStatus = 0;
        else if (luErr == 1167)
            liStatus = 14;
        else
            liStatus = 1;
    }
    return liStatus;
}

// ---------------------------------------------------------------------------
// OpenContainer @ 0x82B53DE8 -- build the XCONTENT_DATA and open/create the
// content package. Classifies the XContentCreate result into a card code.
// ---------------------------------------------------------------------------
int XenonUtil::OpenContainer(State* lpState, const void* lpName, s16 lsFlags, int liOpenMode,
                             XCONTENT_DATA* lpContentData, bool* lpbCreatedNew)
{
    std::memset(lpContentData, 0, 308);            // memset(a5, 0, 0x134)

    int liResult = CheckState(lpState);
    if (liResult != 0)
        return liResult;

    lpContentData->DeviceID = lpState->mCardData.maCurrent.DeviceID;   // *a5 = *(a1+8)
    lpContentData->dwContentType = (liOpenMode == 1) ? 3 : 1;

    std::wcsncpy(lpContentData->szDisplayName, static_cast<const wchar_t*>(lpName), 127);
    lpContentData->szDisplayName[127] = 0;                            // *(a5+262) = 0
    std::strncpy(lpContentData->szFileName,
                 static_cast<const char*>(lpName) + 256, 41);         // ascii name follows at +256
    lpContentData->szFileName[41] = 0;                               // *(a5+305) = 0

    DWORD dwContentFlags = (lsFlags & 2) ? 4u : 3u;   // CREATE_ALWAYS(open-always) vs OPEN_EXISTING-create
    if (lsFlags & 0x400)
        dwContentFlags |= 0x10u;

    DWORD luDisposition = 0;
    DWORD luErr = XContentCreate(sDwUserIndex, KPSZ_XENON_ROOT, lpContentData, dwContentFlags,
                                 &luDisposition, 0, 0);
    if (lpbCreatedNew)
        *lpbCreatedNew = (luDisposition == 2);

    // Classify the create result (mirrors the X360 fall-through ladder at 0x82B53ED8).
    if (luErr == 0)
    {
        liResult = 0;
        lpState->mbContainerOpen = 1;
    }
    else if (luErr == 3)
    {
        liResult = 9;
    }
    else if (luErr == 1110 || luErr == 15 || luErr == 20 || luErr == 21 ||
             luErr == 1112 || luErr == 1166 || luErr == 1167 ||
             luErr == 1617 || luErr == 1627)
    {
        liResult = 2;
    }
    else
    {
        liResult = 8;
    }
    return liResult;
}

// ---------------------------------------------------------------------------
// CloseContainer @ 0x82B53600 -- close the open package, clear the open flag,
// then re-set the thumbnail if one is supplied.
// ---------------------------------------------------------------------------
int XenonUtil::CloseContainer(State* lpState, const XCONTENT_DATA* lpContentData,
                              const ThumbnailData* lpThumbnail)
{
    XContentClose(KPSZ_XENON_ROOT, 0);
    lpState->mbContainerOpen = 0;

    int liResult = CheckState(lpState);
    if (liResult == 0)
    {
        DWORD luSize = lpThumbnail->mcbData;
        if (luSize != 0)
        {
            DWORD luErr = XContentSetThumbnail(sDwUserIndex, lpContentData,
                                               lpThumbnail->mpbData, luSize, 0);
            if (luErr == 1617 || luErr == 1110)
                liResult = 2;
            else
                liResult = (luErr != 0) ? 1 : 0;
        }
    }
    return liResult;
}

// ---------------------------------------------------------------------------
// DeleteContainer @ 0x82B53698 -- delete the package; map the delete error.
// ---------------------------------------------------------------------------
int XenonUtil::DeleteContainer(State* lpState, const XCONTENT_DATA* lpContentData)
{
    int liResult = CheckState(lpState);
    if (liResult == 0)
    {
        DWORD luErr = XContentDelete(sDwUserIndex, lpContentData, 0);
        if (luErr == 0)
            return 0;
        if (luErr == 2)
            return 9;
        if (luErr == 80 || luErr == 1110 || luErr == 1167 || luErr == 1617 || luErr == 1627)
            return 2;
        return 1;
    }
    return liResult;
}

// ---------------------------------------------------------------------------
// FlushContainer @ 0x82B53738 -- flush the package; map the flush error (same
// ladder as DeleteContainer).
// ---------------------------------------------------------------------------
int XenonUtil::FlushContainer(State* lpState)
{
    int liResult = CheckState(lpState);
    if (liResult == 0)
    {
        DWORD luErr = XContentFlush(KPSZ_XENON_ROOT, 0);
        if (luErr == 0)
            return 0;
        if (luErr == 2)
            return 9;
        if (luErr == 80 || luErr == 1110 || luErr == 1167 || luErr == 1617 || luErr == 1627)
            return 2;
        return 1;
    }
    return liResult;
}

// ---------------------------------------------------------------------------
// SetThumbnail @ 0x82B53B50 -- push the supplied thumbnail to the current
// content package (no-op when the descriptor is empty).
// ---------------------------------------------------------------------------
int XenonUtil::SetThumbnail(State* lpState, const XCONTENT_DATA* lpContentData,
                            const ThumbnailData* lpThumbnail)
{
    (void)lpState;   // the X360 body ignores its state argument
    DWORD luSize = lpThumbnail->mcbData;
    if (luSize == 0)
        return 0;

    DWORD luErr = XContentSetThumbnail(sDwUserIndex, lpContentData,
                                       lpThumbnail->mpbData, luSize, 0);
    if (luErr == 1617 || luErr == 1110)
        return 2;
    return (luErr != 0) ? 1 : 0;
}

// ---------------------------------------------------------------------------
// OpenFile @ 0x82B537C8 -- open/create "<root>:\\<name>" with the access/mode
// implied by lsFlags. Maps the CreateFileA failure to a card code.
// ---------------------------------------------------------------------------
int XenonUtil::OpenFile(State* lpState, const u8* lpFileName, s16 lsFlags, HANDLE* lpHandleOut)
{
    (void)lpState;   // the X360 body ignores its state argument
    int liResult = 0;
    *lpHandleOut = INVALID_HANDLE_VALUE;             // *a4 = -1

    if (lpFileName[0] == 0)
        return 1;

    DWORD dwAccess = 0;
    DWORD dwCreationDisposition;
    if (lsFlags & 0x200)
    {
        dwCreationDisposition = CREATE_ALWAYS;       // 2
        dwAccess = GENERIC_WRITE;                    // 0x40000000
    }
    else
    {
        dwCreationDisposition = OPEN_EXISTING;       // 3
        if (lsFlags & 1)
            dwAccess = GENERIC_READ;                 // 0x80000000
        if (lsFlags & 2)
            dwAccess = GENERIC_WRITE;                // 0x40000000
    }

    char acPath[304];
    std::strcpy(acPath, KPSZ_XENON_ROOT);            // "rmcsave"
    std::strcat(acPath, ":\\");
    std::strcat(acPath, reinterpret_cast<const char*>(lpFileName));

    HANDLE lHandle = CreateFileA(acPath, dwAccess, 0, 0, dwCreationDisposition,
                                 FILE_ATTRIBUTE_NORMAL, 0);
    *lpHandleOut = lHandle;
    if (lHandle == INVALID_HANDLE_VALUE)
    {
        DWORD luErr = GetLastError();
        liResult = 1;
        if ((luErr == 2 || luErr == 123) && dwCreationDisposition == OPEN_EXISTING)
            liResult = 8;
        else if (luErr == 1110 || luErr == 80)
            liResult = 2;
        else if (luErr == 1393)
            liResult = 7;
    }
    return liResult;
}

// ---------------------------------------------------------------------------
// CloseFile @ 0x82B53938 -- close the handle and invalidate it. 1 on failure.
// ---------------------------------------------------------------------------
int XenonUtil::CloseFile(State* lpState, HANDLE* lpHandle)
{
    (void)lpState;   // the X360 body ignores its state argument
    if (!CloseHandle(*lpHandle))
        return 1;
    *lpHandle = INVALID_HANDLE_VALUE;
    return 0;
}

// ---------------------------------------------------------------------------
// ReadFile @ 0x82B53BC0 -- read exactly luBytes; 10 on a short read, 2 on a
// removed-device error, 1 otherwise.
// ---------------------------------------------------------------------------
int XenonUtil::ReadFile(HANDLE lHandle, void* lpBuffer, u32 luBytes, DWORD* lpuBytesRead)
{
    int liResult = 1;
    DWORD luRead = 0;
    if (::ReadFile(lHandle, lpBuffer, luBytes, &luRead, 0))
    {
        liResult = (luRead != 0 && luRead == luBytes) ? 0 : 10;
    }
    else
    {
        DWORD luErr = GetLastError();
        if (luErr == 1110 || luErr == 1617)
            liResult = 2;
    }
    if (lpuBytesRead)
        *lpuBytesRead = luRead;
    return liResult;
}

// ---------------------------------------------------------------------------
// WriteFile @ 0x82B53990 -- write exactly luBytes; 2 removed / 7 read-only /
// 1 short-or-error.
// ---------------------------------------------------------------------------
int XenonUtil::WriteFile(HANDLE lHandle, const void* lpBuffer, DWORD luBytes, DWORD* lpuBytesWritten)
{
    int liResult = 1;
    DWORD luWritten = 0;
    if (::WriteFile(lHandle, lpBuffer, luBytes, &luWritten, 0))
    {
        liResult = (luWritten != luBytes) ? 1 : 0;
    }
    else
    {
        DWORD luErr = GetLastError();
        if (luErr == 1110 || luErr == 1617)
            liResult = 2;
        else if (luErr == 1393)
            liResult = 7;
    }
    if (lpuBytesWritten)
        *lpuBytesWritten = luWritten;
    return liResult;
}

// ---------------------------------------------------------------------------
// ReadFileLayer2 @ 0x82B540F0 -- read a "MC02" record from an open file into the
// entry's data buffer, verifying the header and the payload CRC.
//
//   1. read the 28-byte header, verify magic/size against the real file size;
//   2. confirm the header's stored data size matches the entry's expected size;
//   3. read exactly that many bytes into the entry's mpData buffer;
//   4. confirm the read length, then CRC the payload and verify it against the
//      header's stored data checksum.
// Any mismatch yields 8 (generic I/O error); ReadFile propagates its own codes.
// ---------------------------------------------------------------------------
int XenonUtil::ReadFileLayer2(HANDLE lHandle, const LoadEntryInfo* lpEntry)
{
    u32   laHeader[7] = {};   // the on-disk 28-byte "MC02" record (7 words)
    DWORD luBytesRead = 0;

    DWORD luFileSize = GetFileSize(lHandle, 0);

    int liResult = ReadFile(lHandle, laHeader, 28, &luBytesRead);
    if (liResult != 0)
        return liResult;

    liResult = VerifyFileHeader(laHeader, luFileSize);
    if (liResult != 0)
        return liResult;

    // The header's stored data size (word 3) must equal the entry's expected size.
    u32 luDataSize = lpEntry->muDataSize;
    if (laHeader[3] != luDataSize)
        return 8;

    liResult = ReadFile(lHandle, lpEntry->mpData, luDataSize, &luBytesRead);
    if (liResult != 0)
        return liResult;

    if (laHeader[3] != luBytesRead)
        return 8;

    u32 luDataCrc = Crc32(lpEntry->mpData, lpEntry->muDataSize);
    return VerifyDataIntegrity(laHeader, 0, luDataCrc);
}

// ---------------------------------------------------------------------------
// SaveDataRegular @ 0x82B53FF0 -- build a "MC02" record header for the entry's
// payload and write the header followed by the payload to the open file.
//
// The 28-byte header is: [0] 'MC02' magic, [1] total file size (payload + 28),
// [2] 0, [3] payload size, [4] the empty-input CRC seed (Crc32 of nothing),
// [5] the payload CRC, [6] the header CRC over the first 24 bytes. The write is
// a two-span loop (header span, then payload span) that stops on the first
// WriteFile error -- mirroring the X360 iovec walk.
// ---------------------------------------------------------------------------
int XenonUtil::SaveDataRegular(HANDLE lHandle, const LoadEntryInfo* lpEntry)
{
    const u32 KU_MAGIC_MC02 = 0x4D433032u;   // 'MC02'

    u32 laHeader[7] = {};
    laHeader[0] = KU_MAGIC_MC02;
    laHeader[3] = lpEntry->muDataSize;
    laHeader[1] = lpEntry->muDataSize + 28;             // header + payload
    laHeader[5] = Crc32(lpEntry->mpData, lpEntry->muDataSize);  // payload CRC
    laHeader[4] = Crc32(0, 0);                    // empty-input CRC seed
    laHeader[2] = 0;
    laHeader[6] = Crc32(laHeader, 24);                  // header CRC over words 0..5

    // Two write spans: the header, then the payload. WriteFile short-circuits the
    // loop on the first non-zero result code.
    struct WriteSpan { const void* mpBuffer; DWORD muSize; };
    const WriteSpan laSpans[2] =
    {
        { laHeader,        28 },
        { lpEntry->mpData, lpEntry->muDataSize },
    };

    int   liResult = 0;
    DWORD luWritten = 0;
    for (int i = 0; i < 2; ++i)
    {
        liResult = WriteFile(lHandle, laSpans[i].mpBuffer, laSpans[i].muSize, &luWritten);
        if (liResult != 0)
            break;
    }
    return liResult;
}

// ---------------------------------------------------------------------------
// LoadFiles @ 0x82B54260 -- iterate the entry list, opening each file, reading
// its layered payload and closing it; stop on the first error.
// ---------------------------------------------------------------------------
int XenonUtil::LoadFiles(State* lpState, s16 lsMode, int liCount, const LoadEntryInfo* lpEntries)
{
    int liResult = 0;
    const LoadEntryInfo* lpEntry = lpEntries;
    int i = 0;
    while (i < liCount)
    {
        HANDLE lHandle = INVALID_HANDLE_VALUE;
        liResult = OpenFile(lpState, reinterpret_cast<const u8*>(lpEntry), lsMode, &lHandle);
        if (liResult != 0)
            break;
        liResult = ReadFileLayer2(lHandle, lpEntry);
        int liClose = CloseFile(lpState, &lHandle);
        if (liResult != 0)
            break;
        liResult = liClose;
        ++i;
        ++lpEntry;
    }
    return liResult;
}

// ---------------------------------------------------------------------------
// SaveFiles @ 0x82B541D8 -- set the thumbnail once, then iterate the entry list
// writing each file. Unlike LoadFiles this keeps iterating past a failure
// (retaining the last error code).
// ---------------------------------------------------------------------------
int XenonUtil::SaveFiles(State* lpState, const XCONTENT_DATA* lpContentData,
                         const ThumbnailData* lpThumbnail, s16 lsMode, int liCount,
                         const LoadEntryInfo* lpEntries)
{
    int liResult = 0;
    SetThumbnail(lpState, lpContentData, lpThumbnail);

    const LoadEntryInfo* lpEntry = lpEntries;
    int liRemaining = liCount;
    while (liRemaining > 0)
    {
        HANDLE lHandle = INVALID_HANDLE_VALUE;
        liResult = OpenFile(lpState, reinterpret_cast<const u8*>(lpEntry), lsMode, &lHandle);
        if (liResult == 0)
        {
            liResult = SaveDataRegular(lHandle, lpEntry);
            int liClose = CloseFile(lpState, &lHandle);
            if (liResult == 0)
                liResult = liClose;
        }
        --liRemaining;
        ++lpEntry;
    }
    return liResult;
}

// ---------------------------------------------------------------------------
// VerifyFileHeader @ 0x82B53F60 -- validate the "MC02" record: size field must
// equal the file size, magic must match, and the header CRC must cover the
// first 24 bytes. 0 ok, 8 on any mismatch.
// ---------------------------------------------------------------------------
int XenonUtil::VerifyFileHeader(const u32* lpHeader, u32 luFileSize)
{
    const u32 KU_MAGIC_MC02 = 0x4D433032u;   // 'MC02'
    if (lpHeader[1] != luFileSize || lpHeader[0] != KU_MAGIC_MC02)
        return 8;

    u32 luCrc = Crc32(lpHeader, 24);

    // The stored header CRC is the 4 bytes at +0x18; compare it against the
    // freshly computed value byte-for-byte (mirrors the X360 compare loop).
    const u8* lpStored = reinterpret_cast<const u8*>(lpHeader) + 24;
    const u8* lpWant = reinterpret_cast<const u8*>(&luCrc);
    int liDiff = 0;
    for (int i = 0; i < 4; ++i)
    {
        liDiff = static_cast<int>(lpStored[i]) - static_cast<int>(lpWant[i]);
        if (liDiff != 0)
            break;
    }
    return (liDiff != 0) ? 8 : 0;
}

// ---------------------------------------------------------------------------
// VerifyDataIntegrity @ 0x82B53C48 -- when luDataSize is non-zero, confirm it
// matches the header's stored size and that the stored data CRC (4 bytes at
// +0x14) matches luDataCrc. A zero luDataSize short-circuits to success (the
// ReadFileLayer2 caller passes 0, making this a no-op there -- reproduced).
// ---------------------------------------------------------------------------
int XenonUtil::VerifyDataIntegrity(const u32* lpHeader, u32 luDataSize, u32 luDataCrc)
{
    if (luDataSize == 0)
        return 0;
    if (luDataSize != lpHeader[3])
        return 8;

    const u8* lpStored = reinterpret_cast<const u8*>(lpHeader) + 20;
    const u8* lpWant = reinterpret_cast<const u8*>(&luDataCrc);
    int liDiff = 0;
    for (int i = 0; i < 4; ++i)
    {
        liDiff = static_cast<int>(lpStored[i]) - static_cast<int>(lpWant[i]);
        if (liDiff != 0)
            break;
    }
    return (liDiff != 0) ? 8 : 0;
}

} // namespace RealmcIface
