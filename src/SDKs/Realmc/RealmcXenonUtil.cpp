// ===========================================================================
// RealmcIface::XenonUtil -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// The Xenon (Xbox 360) platform back-end of the Realmc memory-card SDK: content
// package (XContent*) management, the device-selector UI (XShowDeviceSelectorUI /
// XNotifyGetNext) and Win32 file I/O. No leak source / no DWARF: SHAPE and
// BODIES both come from the X360 pseudocode + asm. See RealmcXenonUtil.h for the
// class/State layout and the list of the (blocked) functions not reconstructed
// here.
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

// ---------------------------------------------------------------------------
// Xbox 360 content / notification / sign-in platform APIs. Declared locally as
// extern "C" prototypes (the established idiom for X360 kernel calls in this
// tree -- see the X360 network managers). The content-data / overlapped /
// thumbnail arguments are register-level pointers, so they are declared void*
// here rather than pinning the Xbox-SDK struct layouts. Return values are the
// Win32/XDK error codes the asm branches on.
// ---------------------------------------------------------------------------
extern "C"
{
    DWORD XContentGetDeviceState(DWORD dwDeviceID, void* pOverlapped);
    DWORD XContentGetDeviceData(DWORD dwDeviceID, void* pDeviceData);
    DWORD XContentClose(const char* szRootName, void* pOverlapped);
    DWORD XContentFlush(const char* szRootName, void* pOverlapped);
    DWORD XContentDelete(DWORD dwUserIndex, const void* pContentData, void* pOverlapped);
    DWORD XContentSetThumbnail(DWORD dwUserIndex, const void* pContentData,
                               const void* pbThumbnail, DWORD cbThumbnail, void* pOverlapped);
    DWORD XContentCreate(DWORD dwUserIndex, const char* szRootName, void* pContentData,
                         DWORD dwContentFlags, DWORD* pdwDisposition, DWORD* pdwLicenseMask,
                         void* pOverlapped);
    DWORD XShowDeviceSelectorUI(DWORD dwUserIndex, DWORD dwContentType, DWORD dwContentFlags,
                                const void* pBytesRequested, DWORD* pdwDeviceID, void* pOverlapped);
    DWORD XGetOverlappedExtendedError(void* pOverlapped);
    int   XNotifyGetNext(HANDLE hNotification, DWORD dwMsgFilter, DWORD* pdwId, ULONG_PTR* pParam);
    s32   XUserGetSigninState(u32 dwUserIndex);
}

// The Crc32 checksum utility used by the file-header/integrity checks (external
// engine helper; declared for the compile gate -- its body lives in its own TU).
u32 Crc32(const void* lpData, u32 luLength);

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
int XenonUtil::CardExists(const u32* lpuDeviceId)
{
    u32 luDeviceId = *lpuDeviceId;
    if (luDeviceId == 0)
        return 2;
    if (XContentGetDeviceState(luDeviceId, nullptr) == 0)
        return 0;
    return 2;
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
    lpState->mOverlapped = XOVERLAPPED{};        // zero the 28-byte async descriptor
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
    u32 luDeviceId = rCard.maCurrent.muState;    // *(a1+8)
    if (luDeviceId != 0)
    {
        DWORD luErr = XContentGetDeviceData(luDeviceId, &rCard);
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
int XenonUtil::DeviceSelectorShow(State* lpState, const void* lpBytesRequested, int liMode)
{
    int liResult = CheckState(lpState);
    if (liResult != 0)
        return liResult;

    DWORD dwContentType = (liMode == 1) ? 3 : 1;
    DWORD luDeviceId = 0;                          // the selected device id (BYREF)
    lpState->mOverlapped = XOVERLAPPED{};          // zero the async descriptor

    // Kick the selector UI until it starts (returns ERROR_IO_PENDING == 997) or
    // the operation is cancelled.
    DWORD luShow;
    do
    {
        luShow = XShowDeviceSelectorUI(sDwUserIndex, dwContentType, lpState->muContentFlags,
                                       lpBytesRequested, &luDeviceId, &lpState->mOverlapped);
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
        DWORD luErr = XContentGetDeviceData(luDeviceId, &lpState->mCardData);
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

    lpContentData->DeviceID = lpState->mCardData.maCurrent.muState;   // *a5 = *(a1+8)
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
                                 &luDisposition, nullptr, nullptr);
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
    XContentClose(KPSZ_XENON_ROOT, nullptr);
    lpState->mbContainerOpen = 0;

    int liResult = CheckState(lpState);
    if (liResult == 0)
    {
        DWORD luSize = lpThumbnail->mcbData;
        if (luSize != 0)
        {
            DWORD luErr = XContentSetThumbnail(sDwUserIndex, lpContentData,
                                               lpThumbnail->mpbData, luSize, nullptr);
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
        DWORD luErr = XContentDelete(sDwUserIndex, lpContentData, nullptr);
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
        DWORD luErr = XContentFlush(KPSZ_XENON_ROOT, nullptr);
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
                                       lpThumbnail->mpbData, luSize, nullptr);
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

    HANDLE lHandle = CreateFileA(acPath, dwAccess, 0, nullptr, dwCreationDisposition,
                                 FILE_ATTRIBUTE_NORMAL, nullptr);
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
    if (::ReadFile(lHandle, lpBuffer, luBytes, &luRead, nullptr))
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
    if (::WriteFile(lHandle, lpBuffer, luBytes, &luWritten, nullptr))
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
