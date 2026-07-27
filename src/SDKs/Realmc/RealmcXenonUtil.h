#pragma once

// ===========================================================================
// RealmcIface::XenonUtil -- the Xenon (Xbox 360) platform back-end for the
// Realmc memory-card SDK (the RealmcIface family in BURNOUT_X360_ARTIST.XEX;
// sibling to RealmcIface::MemcardInterface / MemcardInterfaceImpl / CardData /
// LoadEntryInfo). It is a collection of STATIC helpers that the memory-card
// worker tasks (RealmcIface::BootupCheckTask / SaveTask / LoadTask /
// XenonRunnableTask) call to talk to the X360 content system (XContent*),
// the device-selector UI (XShowDeviceSelectorUI / XNotifyGetNext) and Win32
// file I/O (CreateFileA / ReadFile / WriteFile).
//
// No Feb-2007 leak source and no DWARF exist for this TU, so the SHAPE below is
// reconstructed purely from the X360 pseudocode + asm. `Realmc` is a vendor
// library boundary, so its identifiers are preserved verbatim per the naming
// convention.
//
// The helpers operate on a shared State object (the memory-card interface's
// internal Xenon state) passed by pointer as their first argument. Byte offsets
// in the State comments are X360-AUTHORITATIVE (32-bit target): they are the
// exact displacements the ASM loads/stores. This project compiles host x64
// (8-byte pointers/HANDLE), so the realised host offsets are NOT byte-identical
// to the 32-bit X360 ones -- the comments document which member each ASM
// displacement names (semantic parity by named members, not byte offsets); the
// interior gaps the helpers never touch are reserved with explicit padding.
//
// CardResultToCardStatus @ 0x82B53AD8 is a jump-table dispatch; its 16-byte
// offset table `byte_82148638` was recovered from the IDA database (dump:
// scratchpad/waveD/XenonUtil.spec.md in the workflow repo), so the switch is
// rodata-attested, not guessed.
// ===========================================================================

#include "types.hpp"

#include <windows.h>   // HANDLE / DWORD / WCHAR (the X360 wraps Win32 file/handle types; host Win32 on PC)

#include "SDKs/Realmc/RealmcCardData.h"       // RealmcIface::CardData (embedded in State at +0x08)
#include "SDKs/Realmc/RealmcLoadEntryInfo.h"  // RealmcIface::LoadEntryInfo (the 48-byte save entry)

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// XCONTENT_DATA -- the X360 content-package descriptor the XContent* APIs take
// (DeviceID + content type + wide display name + ascii file name). This is a
// platform (Xbox SDK) type reproduced here so XenonUtil::OpenContainer can build
// one field-by-field and the container helpers can pass it by pointer. FLAGGED:
// Xbox-SDK type; only the fields the X360 code touches are modelled, laid out at
// their attested byte offsets (0x08 wide name, 0x108 ascii name).
// ---------------------------------------------------------------------------
struct XCONTENT_DATA
{
    DWORD DeviceID;             // +0x000  target device id
    DWORD dwContentType;        // +0x004  content type (3 == saved game, else 1)
    WCHAR szDisplayName[128];   // +0x008  wide display name (wcsncpy 127)
    CHAR  szFileName[42];       // +0x108  ascii file name (strncpy 41)
};

// ---------------------------------------------------------------------------
// XOVERLAPPED -- the X360 async-op descriptor the device-selector UI / content
// APIs post their completion into. FLAGGED: Xbox-SDK type; only InternalLow (the
// status word compared against ERROR_IO_INCOMPLETE == 997) is read by name, the
// rest is reserved so the object can be handed to the APIs by address.
// ---------------------------------------------------------------------------
struct XOVERLAPPED
{
    DWORD  InternalLow;         // +0x00  status (997 == still pending)
    DWORD  InternalHigh;        // +0x04
    DWORD  InternalContext;     // +0x08
    HANDLE hEvent;              // +0x0C
    void*  pCompletionRoutine;  // +0x10
    DWORD  dwCompletionContext; // +0x14
    DWORD  dwExtendedError;     // +0x18
};

// ---------------------------------------------------------------------------
// ThumbnailData -- the { pointer, byte-count } thumbnail descriptor the save
// path hands to XContentSetThumbnail (X360 reads *a3 == data pointer,
// *(a3+4) == byte count).
// ---------------------------------------------------------------------------
struct ThumbnailData
{
    const void* mpbData;  // +0x00  thumbnail bytes
    DWORD       mcbData;  // +0x04  thumbnail size (0 == none)
};

// ---------------------------------------------------------------------------
// XenonUtil -- static memory-card platform helpers. Every method returns a
// Realmc card result code (0 == success; 2 == card removed / not present;
// 7 == wrong device; 8 == generic I/O error; 9 == not enough space;
// 10 == truncated read; 13 == not signed in; 14 == cancelled;
// 15 == still busy). The codes are reproduced as the numeric literals the asm
// materialises (no attested enum exists to name them).
// ---------------------------------------------------------------------------
class XenonUtil
{
public:
    // The shared Xenon state the helpers read/write. Embedded (later) in the
    // memory-card interface impl; a pointer to it is the helpers' `this`-like
    // first argument.
    struct State
    {
        u8       mbCancel;             // +0x000  nonzero aborts the selector poll loops
        u8       maPad001[3];          // +0x001
        s32      miResult;             // +0x004  last CheckState result code
        CardData mCardData;            // +0x008  active card data (0xA0); doubles as the
                                       //         XContentGetDeviceData buffer
        DWORD    muContentFlags;       // +0x0A8  dwContentFlags for XShowDeviceSelectorUI
        DWORD    muNextContentFlags;   // +0x0AC  pending flags (promoted into +0x0A8 on success)
        u8       mbDeviceMounted;      // +0x0B0  device-present flag
        u8       maPad0B1[3];          // +0x0B1
        HANDLE   mhNotification;       // +0x0B4  XNotify listener / open file handle (-1 == none)
        XOVERLAPPED mOverlapped;       // +0x0B8  device-selector async op (X360 28 bytes)
        u8       maPad0D4[5];          // +0x0D4  .. +0x0D8 (X360 reserved; untouched here)
        u8       mbContainerOpen;      // +0x0D9  a content container is currently open
    };

    // @ 0x82B53A18 -- 0 if the device *lpuDeviceId names is present (XContentGetDeviceState
    //                 succeeds), 2 if the id is 0 or the device is gone.
    static int CardExists(const u32* lpuDeviceId);

    // @ 0x82B53AD8 -- map an internal card RESULT code (the values above) to the
    //                 public card STATUS code the worker tasks report. A 16-entry
    //                 byte jump table (byte_82148638, rodata-attested); results 2/14
    //                 consult State::mbDeviceMounted (asm reads 0xB0(r4) -- the
    //                 State* second argument Hex-Rays' one-arg prototype hides;
    //                 every caller does `lwz r4, 0x10(r31)` before the call).
    static int CardResultToCardStatus(u32 luResult, const State* lpState);

    // @ 0x82B53460 -- 0 if a user is signed in (dwUserIndex valid and XUserGetSigninState
    //                 non-zero), else 13; stashes the code in State::miResult.
    static int CheckState(State* lpState);

    // @ 0x82B53400 -- close the open notification/file handle, reset the device/overlapped
    //                 state, clear the signed-in user. Returns the CloseHandle BOOL.
    static BOOL CloseState(State* lpState);

    // @ 0x82B53C0 family -- device-info / container / file helpers (see the .cpp for the
    //                       per-function contracts recovered from the asm).
    static int UpdateDeviceInfo(State* lpState);
    static int DeviceSelectorUpdate(State* lpState);
    static int DeviceSelectorShow(State* lpState, const void* lpBytesRequested, int liMode);

    static int OpenContainer(State* lpState, const void* lpName, s16 lsFlags, int liOpenMode,
                             XCONTENT_DATA* lpContentData, bool* lpbCreatedNew);
    static int CloseContainer(State* lpState, const XCONTENT_DATA* lpContentData,
                              const ThumbnailData* lpThumbnail);
    static int DeleteContainer(State* lpState, const XCONTENT_DATA* lpContentData);
    static int FlushContainer(State* lpState);
    static int SetThumbnail(State* lpState, const XCONTENT_DATA* lpContentData,
                            const ThumbnailData* lpThumbnail);

    // File-level helpers. lpHandleOut receives the open Win32 handle (or -1).
    static int OpenFile(State* lpState, const u8* lpFileName, s16 lsFlags, HANDLE* lpHandleOut);
    static int CloseFile(State* lpState, HANDLE* lpHandle);
    static int ReadFile(HANDLE lHandle, void* lpBuffer, u32 luBytes, DWORD* lpuBytesRead);
    static int WriteFile(HANDLE lHandle, const void* lpBuffer, DWORD luBytes, DWORD* lpuBytesWritten);

    // Layered load/save (header + integrity checks; iterate the entry list).
    static int ReadFileLayer2(HANDLE lHandle, const LoadEntryInfo* lpEntry);
    static int SaveDataRegular(HANDLE lHandle, const LoadEntryInfo* lpEntry);
    static int LoadFiles(State* lpState, s16 lsMode, int liCount, const LoadEntryInfo* lpEntries);
    static int SaveFiles(State* lpState, const XCONTENT_DATA* lpContentData,
                         const ThumbnailData* lpThumbnail, s16 lsMode, int liCount,
                         const LoadEntryInfo* lpEntries);

    // Header / integrity verification of the on-disk "MC02" file records.
    static int VerifyFileHeader(const u32* lpHeader, u32 luFileSize);
    static int VerifyDataIntegrity(const u32* lpHeader, u32 luDataSize, u32 luDataCrc);
};

} // namespace RealmcIface
