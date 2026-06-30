// CgsDevicePhysicalX360.cpp — the Xbox 360 physical file device (XDK Win32 file leaf).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Init           @ 0x828DE998   Open           @ 0x828F9728   Close          @ 0x828F9D90
//   Read           @ 0x828DE9F0   Seek           @ 0x828DEB98
//   OpenDirectory  @ 0x828F9E40   ReadDirectory  @ 0x828DEC30   CloseDirectory @ 0x828FA8B0
//
// The device owns an embedded byte-indexed IndexedPool<FileHandleRecord,N,s8> (mHandlePool, this
// +0x10): Open claims a slot and stores the open HANDLE + seek cursor + resolved path there;
// Close returns the slot. The op methods receive an int handle == the pool index and resolve it
// back to the FileHandleRecord through the pool BY NAME (the asm passes the resolved record
// pointer; here operator[] does that resolution). The failure path on every op runs the device's
// error callback and yields 5ms (Device::CallErrorCallback), then retries — the X360 did this
// inline; routed through the base helper here, faithful to the same effect.
//
// The byte transfer itself is the platform leaf: on X360 these were XDK calls against the DVD/HDD;
// reconstructed here with the host Win32 file API (marked), exactly as the PC sibling does.

#include "GameShared/GameClasses/System/FileSystem/Devices/CgsDevicePhysicalX360.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>   // CreateFileA / ReadFile / SetFilePointerEx / FindFirstFileA / CloseHandle

// CgsDev debug-trace facade (Init prints a one-line trace when the message filter allows). Matches
// the RemapDevice sibling's local declaration; bodies live in Development/Log/CgsLog.cpp.
namespace CgsDev
{
    namespace Message { extern u64 gxMessageFilterFlags; }
    namespace Log { void DebugPrint(const char* pcText, int liArg); }
}

namespace CgsFileSystem
{
    // Init @0x828DE998, mapped to the worker-start Connect() hook: when the message filter's bit 0
    // is set, print a one-line init trace; always return 0.
    int DevicePhysicalX360::Connect()
    {
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            CgsDev::Log::DebugPrint("Physical X360 device initialized\n", 0);
        return 0;
    }

    // Open @0x828F9728. The name must start with '/' or '\' (it carries a leading separator before
    // the drive token). Strip it, normalize '/'->'\\', then split the first path component as the
    // drive and rebuild "<drive>:\\<rest>". Decode the access + disposition out of liMode and
    // CreateFileA. On success claim a pool slot and fill its FileHandleRecord. On failure run the
    // error callback (5ms yield) and, if it does not absorb the error, block.
    int DevicePhysicalX360::Open(const char* lpcPath, int liMode, int* lpiOutHandle)
    {
        const char lcFirst = lpcPath[0];
        CGS_ASSERT(lcFirst == '/' || lcFirst == '\\', "Name is invalid format\n");

        const char* lpcName = lpcPath + 1;             // past the leading separator
        size_t luLen = 0;
        while (lpcName[luLen]) ++luLen;
        CGS_ASSERT(luLen < 256, "String is too long. Buffer size = 256\n");

        // Copy and normalize forward slashes to backslashes.
        char lacNormalized[256];
        {
            size_t li = 0;
            for (; lpcName[li]; ++li)
                lacNormalized[li] = lpcName[li];
            lacNormalized[li] = '\0';
        }
        for (char* lpc = lacNormalized; *lpc; ++lpc)
            if (*lpc == '/') *lpc = '\\';

        // Locate the first '\\' that splits the drive token from the remainder; rebuild
        // "<drive>:\\<rest>" (the X360 SPrintf "%s:\\%s" form).
        char lacFullPath[256];
        {
            s32 liSplit = 0;
            const char* lpcScan = lacNormalized;
            while (*lpcScan && *lpcScan != '\\') { ++lpcScan; ++liSplit; }
            if (*lpcScan == '\\')
            {
                // lacNormalized[0..liSplit) is the drive; the rest (past the '\\') is the path.
                lacNormalized[liSplit] = '\0';
                CgsCore::SPrintf(lacFullPath, sizeof(lacFullPath), "%s:\\%s",
                                 lacNormalized, &lacNormalized[liSplit + 1]);
            }
            else
            {
                // No separator after the drive token: use the normalized name as-is.
                size_t li = 0;
                for (; lacNormalized[li]; ++li) lacFullPath[li] = lacNormalized[li];
                lacFullPath[li] = '\0';
            }
        }

        // Decode access + disposition from the open flags (X360 immediates).
        const DWORD ldwAccess = (liMode & KU_X360_OPEN_FLAG_WRITE) ? (GENERIC_READ | GENERIC_WRITE)
                                                                   : GENERIC_READ;
        DWORD ldwDisposition;
        switch (static_cast<u32>(liMode) & KU_X360_OPEN_DISPOSITION_MASK)
        {
            case 0x0: ldwDisposition = OPEN_EXISTING;     break;  // 3
            case 0x4: ldwDisposition = CREATE_NEW;        break;  // 1
            case 0x8: ldwDisposition = TRUNCATE_EXISTING; break;  // 5
            case 0xC: ldwDisposition = CREATE_ALWAYS;     break;  // 2
            default:  ldwDisposition = OPEN_EXISTING;     break;
        }

        HANDLE lhFile = CreateFileA(lacFullPath, ldwAccess, FILE_SHARE_READ, NULL,
                                    ldwDisposition, FILE_ATTRIBUTE_NORMAL, NULL);
        if (lhFile != INVALID_HANDLE_VALUE)
        {
            const s8 lsSlot = mHandlePool.Allocate();
            CGS_ASSERT(lsSlot != -1, "Out of handles\n");

            FileHandleRecord& lRecord = mHandlePool[lsSlot];
            lRecord.mu64Position = 0;
            lRecord.mpHandle     = lhFile;
            lRecord.miFlags      = liMode;
            {
                size_t li = 0;
                for (; lacFullPath[li]; ++li) lRecord.macPath[li] = lacFullPath[li];
                lRecord.macPath[li] = '\0';
            }
            *lpiOutHandle = lsSlot;
            return 0;
        }

        // Open failed: run the error callback (it yields 5ms); block if it does not absorb it.
        if (!CallErrorCallback(2))
            Sleep(static_cast<DWORD>(-1));
        return -2;
    }

    // Close @0x828F9D90. CloseHandle the record's HANDLE; on success return the slot to the pool.
    int DevicePhysicalX360::Close(int liHandle)
    {
        FileHandleRecord& lRecord = mHandlePool[static_cast<s8>(liHandle)];
        if (CloseHandle(lRecord.mpHandle))
        {
            const s8 lsIndex = mHandlePool.GetObjectIndex(&lRecord);
            mHandlePool.PushIndex(lsIndex);
            return 0;
        }

        if (!CallErrorCallback(4))
            Sleep(static_cast<DWORD>(-1));
        return -2;
    }

    // Read @0x828DE9F0. ReadFile at the record's HANDLE into lpBuffer; on success advance the
    // record's running position by the byte count and report it.
    int DevicePhysicalX360::Read(int liHandle, u64 /*lu64Offset*/, u32 luSize, void* lpBuffer, int* lpiOutResult)
    {
        FileHandleRecord& lRecord = mHandlePool[static_cast<s8>(liHandle)];

        DWORD ldwRead = 0;
        if (ReadFile(lRecord.mpHandle, lpBuffer, luSize, &ldwRead, NULL))
        {
            lRecord.mu64Position += ldwRead;
            *lpiOutResult = static_cast<int>(ldwRead);
            return 0;
        }

        if (!CallErrorCallback(0))
            Sleep(static_cast<DWORD>(-1));
        return -2;
    }

    // Seek @0x828DEB98. If the request equals the record's current position, report it without a
    // syscall; otherwise SetFilePointerEx (FILE_BEGIN) and store the new position back into the
    // record. The asm compares against the record's position word at +0 and writes the result to
    // both the record and the out param.
    int DevicePhysicalX360::Seek(int liHandle, u64 lu64Offset, int* lpiOutResult)
    {
        FileHandleRecord& lRecord = mHandlePool[static_cast<s8>(liHandle)];

        if (lu64Offset == lRecord.mu64Position)
        {
            *lpiOutResult = static_cast<int>(lRecord.mu64Position);
            return 0;
        }

        LARGE_INTEGER lDistance, lNew;
        lDistance.QuadPart = static_cast<LONGLONG>(lu64Offset);
        if (SetFilePointerEx(lRecord.mpHandle, lDistance, &lNew, FILE_BEGIN))
        {
            lRecord.mu64Position = static_cast<u64>(lNew.QuadPart);
            *lpiOutResult = static_cast<int>(lNew.QuadPart);
            return 0;
        }

        if (!CallErrorCallback(1))
            Sleep(static_cast<DWORD>(-1));
        return -2;
    }

    // OpenDirectory @0x828F9E40. Validate the name format and the entry buffer; resolve the path
    // exactly as Open (normalize, "<drive>:\\<rest>"), append "\\*.*", FindFirstFileA, then fill up
    // to liMaxEntries DirectoryEntry records (FindNextFileA for the rest). Claim a pool slot to
    // hold the find HANDLE, hand back the entry count and the slot. The X360 builds the wildcard
    // path with CgsCore::AppendString and walks the results into the caller's entry array.
    int DevicePhysicalX360::OpenDirectory(const char* lpcPath, void* lpEntryBuffer, int liMaxEntries, int* lpiOutCount, int* lpiOutHandle)
    {
        const char lcFirst = lpcPath[0];
        CGS_ASSERT(lcFirst == '/' || lcFirst == '\\', "Name is invalid format\n");
        CGS_ASSERT(lpEntryBuffer && lpiOutCount && liMaxEntries >= 1,
                   "Must supply a buffer for at least 1 entry\n");

        const char* lpcName = lpcPath + 1;
        size_t luLen = 0;
        while (lpcName[luLen]) ++luLen;
        CGS_ASSERT(luLen < 256, "String is too long. Buffer size = 256\n");

        char lacNormalized[256];
        {
            size_t li = 0;
            for (; lpcName[li]; ++li) lacNormalized[li] = lpcName[li];
            lacNormalized[li] = '\0';
        }
        for (char* lpc = lacNormalized; *lpc; ++lpc)
            if (*lpc == '/') *lpc = '\\';

        char lacFullPath[256];
        {
            s32 liSplit = 0;
            const char* lpcScan = lacNormalized;
            while (*lpcScan && *lpcScan != '\\') { ++lpcScan; ++liSplit; }
            if (*lpcScan == '\\')
            {
                lacNormalized[liSplit] = '\0';
                CgsCore::SPrintf(lacFullPath, sizeof(lacFullPath), "%s:\\%s",
                                 lacNormalized, &lacNormalized[liSplit + 1]);
            }
            else
            {
                size_t li = 0;
                for (; lacNormalized[li]; ++li) lacFullPath[li] = lacNormalized[li];
                lacFullPath[li] = '\0';
            }
        }

        // Append the "\\*.*" wildcard (X360 CgsCore::AppendString; asserted not to overflow).
        {
            size_t li = 0;
            while (lacFullPath[li]) ++li;
            CGS_ASSERT(li + 4 < 0xFF, "(strlen(lpcSource)+strlen(lpcDest))<luBytes - 1\n");
            const char* lpcWild = "\\*.*";
            for (size_t lj = 0; lpcWild[lj]; ++lj) lacFullPath[li + lj] = lpcWild[lj];
            lacFullPath[li + 4] = '\0';
        }

        DirectoryEntry* lpEntries = static_cast<DirectoryEntry*>(lpEntryBuffer);
        s32 liCount = 0;

        WIN32_FIND_DATAA lFindData;
        HANDLE lhFind = FindFirstFileA(lacFullPath, &lFindData);
        if (lhFind != INVALID_HANDLE_VALUE)
        {
            // First entry.
            {
                size_t li = 0;
                for (; lFindData.cFileName[li]; ++li) lpEntries[0].macName[li] = lFindData.cFileName[li];
                lpEntries[0].macName[li] = '\0';
            }
            liCount = 1;
            lpEntries[0].miType = ((lFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) + 1;

            // Remaining entries up to liMaxEntries.
            while (liCount < liMaxEntries && FindNextFileA(lhFind, &lFindData))
            {
                DirectoryEntry& lEntry = lpEntries[liCount];
                size_t li = 0;
                for (; lFindData.cFileName[li]; ++li) lEntry.macName[li] = lFindData.cFileName[li];
                lEntry.macName[li] = '\0';
                lEntry.miType = ((lFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) + 1;
                ++liCount;
            }
        }

        // Claim a pool slot to carry the find handle (so ReadDirectory/CloseDirectory can reuse it).
        const s8 lsSlot = mHandlePool.Allocate();
        CGS_ASSERT(lsSlot != -1, "Out of handles\n");

        FileHandleRecord& lRecord = mHandlePool[lsSlot];
        {
            size_t li = 0;
            for (; lacFullPath[li]; ++li) lRecord.macPath[li] = lacFullPath[li];
            lRecord.macPath[li] = '\0';
        }
        lRecord.mpHandle     = lhFind;
        lRecord.miFlags      = 0;
        lRecord.mu64Position = 0;

        *lpiOutHandle = lsSlot;
        *lpiOutCount  = liCount;
        return 0;
    }

    // CloseDirectory @0x828FA8B0. FindClose the find handle (the X360 calls CloseHandle on it,
    // treating an INVALID handle as already closed) and return the slot to the pool.
    int DevicePhysicalX360::CloseDirectory(int liHandle)
    {
        FileHandleRecord& lRecord = mHandlePool[static_cast<s8>(liHandle)];
        if (lRecord.mpHandle == INVALID_HANDLE_VALUE || CloseHandle(lRecord.mpHandle))
        {
            const s8 lsIndex = mHandlePool.GetObjectIndex(&lRecord);
            mHandlePool.PushIndex(lsIndex);
            return 0;
        }

        if (!CallErrorCallback(4))
            Sleep(static_cast<DWORD>(-1));
        return -2;
    }

    // ReadDirectory @0x828DEC30. Continue a directory walk started by OpenDirectory: FindNextFileA
    // on the record's find handle, filling up to liMaxEntries DirectoryEntry records, and report
    // how many were written. Stops early when the find handle is exhausted or invalid.
    int DevicePhysicalX360::ReadDirectory(int liHandle, void* lpEntryBuffer, int liMaxEntries, int* lpiOutCount)
    {
        CGS_ASSERT(lpEntryBuffer && liMaxEntries && lpiOutCount, "Invalid params\n");

        FileHandleRecord& lRecord = mHandlePool[static_cast<s8>(liHandle)];
        DirectoryEntry* lpEntries = static_cast<DirectoryEntry*>(lpEntryBuffer);

        s32 liCount = 0;
        HANDLE lhFind = lRecord.mpHandle;
        if (lhFind != INVALID_HANDLE_VALUE && liMaxEntries > 0)
        {
            WIN32_FIND_DATAA lFindData;
            while (liCount < liMaxEntries && FindNextFileA(lhFind, &lFindData))
            {
                DirectoryEntry& lEntry = lpEntries[liCount];
                size_t li = 0;
                for (; lFindData.cFileName[li]; ++li) lEntry.macName[li] = lFindData.cFileName[li];
                lEntry.macName[li] = '\0';
                lEntry.miType = ((lFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) + 1;
                ++liCount;
            }
        }

        *lpiOutCount = liCount;
        return 0;
    }
}
