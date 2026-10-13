// CgsDevicePhysicalPC.cpp — Win32-backed concrete physical device (the PC IO leaf).
//
// The X360 PhysicalX360Device used the XDK CreateFileA/ReadFile against the DVD/HDD; this is
// the host-API reconstruction of that leaf. The async machinery above it (DeviceManager
// OperationPool + per-device worker thread) is the faithful X360 engine — these methods run ON
// the device's worker thread and perform the actual byte transfer. DeviceHandle is the native-
// width opaque Win32 HANDLE itself; the table below exists only for validation and shutdown.
// The ARTIST-attested Open/Read/Seek/Close failures invoke the registered callback, yield 5ms,
// then block indefinitely if it returns false. The current RWDiskErrorHandler latches the disk
// error and returns false. A direct Open path-not-found therefore can stall this worker; PC
// optional-asset probe compatibility remains unverified. Local invalid-token and slot-exhaustion
// paths below remain separate from those native I/O failure sites.

#include "GameShared/GameClasses/System/FileSystem/Devices/CgsDevicePhysicalPC.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>   // CreateFileA / ReadFile / WriteFile / SetFilePointerEx / GetFileSizeEx / CloseHandle
#include <string>

namespace
{
    // ARTIST writes a 256-byte name followed by a 32-bit type at +0x100.
    struct PCDirectoryEntry
    {
        char macName[256];
        s32  miType;
    };

    void CopyDirectoryEntry(const WIN32_FIND_DATAA& lFindData, PCDirectoryEntry& lEntry)
    {
        u32 luNameLength = 0;
        while (lFindData.cFileName[luNameLength])
            ++luNameLength;
        CGS_ASSERT(luNameLength < sizeof(lEntry.macName),
                   "String is too long. Buffer size = 256\n");

        for (u32 luIndex = 0; luIndex < luNameLength; ++luIndex)
            lEntry.macName[luIndex] = lFindData.cFileName[luIndex];
        lEntry.macName[luNameLength] = '\0';
        lEntry.miType = ((lFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) + 1;
    }
}

namespace CgsFileSystem
{
    DevicePhysicalPC::DevicePhysicalPC()
    {
        mpfErrorCallback = nullptr;
        for (int liIndex = 0; liIndex < KI_MAX_OPEN_FILES; ++liIndex)
            mapHandles[liIndex] = INVALID_HANDLE_VALUE;
    }

    DevicePhysicalPC::~DevicePhysicalPC()
    {
        Shutdown();
    }

    void* DevicePhysicalPC::HandleFor(Handle::DeviceHandle lpHandle) const
    {
        for (int liIndex = 0; liIndex < KI_MAX_OPEN_FILES; ++liIndex)
            if (mapHandles[liIndex] == lpHandle)
                return lpHandle;
        return INVALID_HANDLE_VALUE;
    }

    // Worker-start hook: nothing to bring up for a Win32 file device.
    int DevicePhysicalPC::Connect()
    {
        return 0;
    }

    // Open: claim a tracking slot, CreateFileA, and return the native HANDLE through the opaque
    // DeviceHandle out-param. The operation status is independently returned as zero on success.
    int DevicePhysicalPC::Open(const char* lpcPath, u32 luMode, Handle::DeviceHandle* lppOutHandle)
    {
        int liSlot = -1;
        for (int liIndex = 0; liIndex < KI_MAX_OPEN_FILES; ++liIndex)
            if (mapHandles[liIndex] == INVALID_HANDLE_VALUE) { liSlot = liIndex; break; }
        if (liSlot < 0)
        {
            if (lppOutHandle) *lppOutHandle = nullptr;
            return -1;
        }

        // luMode follows FileAccess (CgsFile.h): 2 == WRITE, else READ.
        const DWORD ldwAccess = (luMode == 2) ? GENERIC_WRITE : GENERIC_READ;
        const DWORD ldwShare  = FILE_SHARE_READ;
        const DWORD ldwDisp   = (luMode == 2) ? CREATE_ALWAYS : OPEN_EXISTING;

        HANDLE lhFile = CreateFileA(lpcPath, ldwAccess, ldwShare, NULL, ldwDisp, FILE_ATTRIBUTE_NORMAL, NULL);
        if (lhFile == INVALID_HANDLE_VALUE)
        {
            if (lppOutHandle) *lppOutHandle = nullptr;
            if (!CallErrorCallback(E_DEVICEERROR_OPEN))
                Sleep(static_cast<DWORD>(-1));
            return -2;
        }

        mapHandles[liSlot] = lhFile;
        if (lppOutHandle) *lppOutHandle = lhFile;
        return 0;
    }

    int DevicePhysicalPC::Close(Handle::DeviceHandle lpHandle)
    {
        HANDLE lhFile = HandleFor(lpHandle);
        if (lhFile != INVALID_HANDLE_VALUE)
        {
            if (!CloseHandle(lhFile))
            {
                if (!CallErrorCallback(E_DEVICEERROR_CLOSE))
                    Sleep(static_cast<DWORD>(-1));
                return -2;
            }
            for (int liIndex = 0; liIndex < KI_MAX_OPEN_FILES; ++liIndex)
                if (mapHandles[liIndex] == lpHandle)
                {
                    mapHandles[liIndex] = INVALID_HANDLE_VALUE;
                    break;
                }
        }
        return 0;
    }

    int DevicePhysicalPC::Read(Handle::DeviceHandle lpHandle, void* lpBuffer, u32 luSize,
                               u32* lpuOutResult)
    {
        HANDLE lhFile = HandleFor(lpHandle);
        if (lhFile == INVALID_HANDLE_VALUE)
        {
            if (lpuOutResult) *lpuOutResult = 0;
            return -1;
        }

        DWORD ldwRead = 0;
        const BOOL lbOk = ReadFile(lhFile, lpBuffer, luSize, &ldwRead, NULL);
        if (lpuOutResult) *lpuOutResult = static_cast<u32>(ldwRead);
        if (!lbOk)
        {
            if (!CallErrorCallback(E_DEVICEERROR_READ))
                Sleep(static_cast<DWORD>(-1));
            return -2;
        }
        return 0;
    }

    int DevicePhysicalPC::Write(Handle::DeviceHandle lpHandle, const void* lpBuffer, u32 luSize,
                                u32* lpuOutResult)
    {
        HANDLE lhFile = HandleFor(lpHandle);
        if (lhFile == INVALID_HANDLE_VALUE)
        {
            if (lpuOutResult) *lpuOutResult = 0;
            return -1;
        }

        DWORD ldwWritten = 0;
        const BOOL lbOk = WriteFile(lhFile, lpBuffer, luSize, &ldwWritten, NULL);
        if (lpuOutResult) *lpuOutResult = static_cast<u32>(ldwWritten);
        return lbOk ? 0 : -1;
    }

    int DevicePhysicalPC::GetFileSize(Handle::DeviceHandle lpHandle, u64* lpu64OutSize)
    {
        HANDLE lhFile = HandleFor(lpHandle);
        if (lhFile == INVALID_HANDLE_VALUE)
            return -1;

        LARGE_INTEGER lSize;
        if (!GetFileSizeEx(lhFile, &lSize))
            return -1;
        if (lpu64OutSize) *lpu64OutSize = static_cast<u64>(lSize.QuadPart);
        return 0;
    }

    int DevicePhysicalPC::Seek(Handle::DeviceHandle lpHandle, u64 lu64Offset,
                               u64* lpu64OutPosition)
    {
        HANDLE lhFile = HandleFor(lpHandle);
        if (lhFile == INVALID_HANDLE_VALUE)
        {
            if (lpu64OutPosition) *lpu64OutPosition = 0;
            return -1;
        }

        LARGE_INTEGER lPos, lNew;
        lPos.QuadPart = static_cast<LONGLONG>(lu64Offset);
        if (!SetFilePointerEx(lhFile, lPos, &lNew, FILE_BEGIN))
        {
            if (lpu64OutPosition) *lpu64OutPosition = 0;
            if (!CallErrorCallback(E_DEVICEERROR_SEEK))
                Sleep(static_cast<DWORD>(-1));
            return -2;
        }
        if (lpu64OutPosition) *lpu64OutPosition = static_cast<u64>(lNew.QuadPart);
        return 0;
    }

    // FLAG PC-platform leaf: ARTIST's PhysicalX360Device uses the same Win32 find sequence,
    // but stores its search HANDLE inside the console's shared handle-pool record and translates
    // the console drive path. This host device follows its existing Open convention: it passes
    // the caller's PC path through and returns the native find HANDLE as the opaque token. It does
    // not translate XContent roots or discover DLC packages.
    int DevicePhysicalPC::OpenDirectory(const char* lpcPath, void* lpEntryBuffer,
                                        u32 luMaxEntries, u32* lpuOutCount,
                                        Handle::DeviceHandle* lppOutHandle)
    {
        CGS_ASSERT(lpEntryBuffer && lpuOutCount && luMaxEntries >= 1,
                   "Must supply a buffer for at least 1 entry\n");

        std::string lacSearchPath(lpcPath);
        lacSearchPath += "\\*.*";

        PCDirectoryEntry* lpEntries = static_cast<PCDirectoryEntry*>(lpEntryBuffer);
        s32 liCount = 0;
        WIN32_FIND_DATAA lFindData;
        HANDLE lhFind = FindFirstFileA(lacSearchPath.c_str(), &lFindData);
        if (lhFind != INVALID_HANDLE_VALUE)
        {
            CopyDirectoryEntry(lFindData, lpEntries[0]);
            liCount = 1;
            while (liCount < static_cast<s32>(luMaxEntries) &&
                   FindNextFileA(lhFind, &lFindData))
            {
                CopyDirectoryEntry(lFindData, lpEntries[liCount]);
                ++liCount;
            }
        }

        // ARTIST returns success and count zero when FindFirstFileA cannot open the search.
        // Preserve the invalid token so ReadDirectory/CloseDirectory follow their native empty
        // handle paths without turning an ordinary missing-directory probe into a disk error.
        *lppOutHandle = lhFind;
        *lpuOutCount = static_cast<u32>(liCount);
        return 0;
    }

    int DevicePhysicalPC::CloseDirectory(Handle::DeviceHandle lpHandle)
    {
        HANDLE lhFind = static_cast<HANDLE>(lpHandle);
        if (lhFind == INVALID_HANDLE_VALUE || FindClose(lhFind))
            return 0;

        // ARTIST reports only a real close failure through the registered CLOSE callback.
        if (!CallErrorCallback(E_DEVICEERROR_CLOSE))
            Sleep(static_cast<DWORD>(-1));
        return -2;
    }

    int DevicePhysicalPC::ReadDirectory(Handle::DeviceHandle lpHandle, void* lpEntryBuffer,
                                        u32 luMaxEntries, u32* lpuOutCount)
    {
        CGS_ASSERT(lpEntryBuffer && luMaxEntries && lpuOutCount, "Invalid params\n");

        PCDirectoryEntry* lpEntries = static_cast<PCDirectoryEntry*>(lpEntryBuffer);
        HANDLE lhFind = static_cast<HANDLE>(lpHandle);
        s32 liCount = 0;
        if (lhFind != INVALID_HANDLE_VALUE && luMaxEntries > 0)
        {
            WIN32_FIND_DATAA lFindData;
            while (liCount < static_cast<s32>(luMaxEntries) &&
                   FindNextFileA(lhFind, &lFindData))
            {
                CopyDirectoryEntry(lFindData, lpEntries[liCount]);
                ++liCount;
            }
        }

        // ARTIST treats FindNextFileA failure, including end-of-directory, as a successful page.
        *lpuOutCount = static_cast<u32>(liCount);
        return 0;
    }

    int DevicePhysicalPC::Shutdown()
    {
        for (int liIndex = 0; liIndex < KI_MAX_OPEN_FILES; ++liIndex)
            if (mapHandles[liIndex] != INVALID_HANDLE_VALUE)
            {
                CloseHandle(mapHandles[liIndex]);
                mapHandles[liIndex] = INVALID_HANDLE_VALUE;
            }
        return 0;
    }
}
