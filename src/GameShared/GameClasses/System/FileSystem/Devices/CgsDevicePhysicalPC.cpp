// CgsDevicePhysicalPC.cpp — Win32-backed concrete physical device (the PC IO leaf).
//
// The X360 PhysicalX360Device used the XDK CreateFileA/ReadFile against the DVD/HDD; this is
// the host-API reconstruction of that leaf. The async machinery above it (DeviceManager
// OperationPool + per-device worker thread) is the faithful X360 engine — these methods run ON
// the device's worker thread and perform the actual byte transfer. DeviceHandle is the native-
// width opaque Win32 HANDLE itself; the table below exists only for validation and shutdown.

#include "GameShared/GameClasses/System/FileSystem/Devices/CgsDevicePhysicalPC.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>   // CreateFileA / ReadFile / WriteFile / SetFilePointerEx / GetFileSizeEx / CloseHandle

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
            return -1;
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
            CloseHandle(lhFile);
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
        return lbOk ? 0 : -1;
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
            return -1;
        }
        if (lpu64OutPosition) *lpu64OutPosition = static_cast<u64>(lNew.QuadPart);
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
