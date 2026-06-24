// CgsDevicePhysicalPC.cpp — Win32-backed concrete physical device (the PC IO leaf).
//
// The X360 PhysicalX360Device used the XDK CreateFileA/ReadFile against the DVD/HDD; this is
// the host-API reconstruction of that leaf. The async machinery above it (DeviceManager
// OperationPool + per-device worker thread) is the faithful X360 engine — these methods run ON
// the device's worker thread and perform the actual byte transfer. The integer file handle the
// worker passes is this device's handle-table index (Open hands one out; Read/Close use it).

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

    void* DevicePhysicalPC::HandleFor(int liHandle) const
    {
        if (liHandle < 0 || liHandle >= KI_MAX_OPEN_FILES)
            return INVALID_HANDLE_VALUE;
        return mapHandles[liHandle];
    }

    // Worker-start hook: nothing to bring up for a Win32 file device.
    int DevicePhysicalPC::Connect()
    {
        return 0;
    }

    // Open: claim a free table slot, CreateFileA, hand back the slot index in *lpiOutHandle
    // (the worker forwards that to the op's completion callback as the file handle).
    int DevicePhysicalPC::Open(const char* lpcPath, int liMode, int* lpiOutHandle)
    {
        int liSlot = -1;
        for (int liIndex = 0; liIndex < KI_MAX_OPEN_FILES; ++liIndex)
            if (mapHandles[liIndex] == INVALID_HANDLE_VALUE) { liSlot = liIndex; break; }
        if (liSlot < 0)
        {
            if (lpiOutHandle) *lpiOutHandle = -1;
            return -1;
        }

        // liMode follows FileAccess (CgsFile.h): 2 == WRITE, else READ.
        const DWORD ldwAccess = (liMode == 2) ? GENERIC_WRITE : GENERIC_READ;
        const DWORD ldwShare  = FILE_SHARE_READ;
        const DWORD ldwDisp   = (liMode == 2) ? CREATE_ALWAYS : OPEN_EXISTING;

        HANDLE lhFile = CreateFileA(lpcPath, ldwAccess, ldwShare, NULL, ldwDisp, FILE_ATTRIBUTE_NORMAL, NULL);
        if (lhFile == INVALID_HANDLE_VALUE)
        {
            if (lpiOutHandle) *lpiOutHandle = -1;
            return -1;
        }

        mapHandles[liSlot] = lhFile;
        if (lpiOutHandle) *lpiOutHandle = liSlot;
        return liSlot;
    }

    int DevicePhysicalPC::Close(int liHandle)
    {
        HANDLE lhFile = HandleFor(liHandle);
        if (lhFile != INVALID_HANDLE_VALUE)
        {
            CloseHandle(lhFile);
            mapHandles[liHandle] = INVALID_HANDLE_VALUE;
        }
        return 0;
    }

    int DevicePhysicalPC::Read(int liHandle, u64 lu64Offset, u32 luSize, void* lpBuffer, int* lpiOutResult)
    {
        HANDLE lhFile = HandleFor(liHandle);
        if (lhFile == INVALID_HANDLE_VALUE)
        {
            if (lpiOutResult) *lpiOutResult = -1;
            return -1;
        }

        LARGE_INTEGER lPos;
        lPos.QuadPart = static_cast<LONGLONG>(lu64Offset);
        SetFilePointerEx(lhFile, lPos, NULL, FILE_BEGIN);

        DWORD ldwRead = 0;
        const BOOL lbOk = ReadFile(lhFile, lpBuffer, luSize, &ldwRead, NULL);
        if (lpiOutResult) *lpiOutResult = static_cast<int>(ldwRead);
        return lbOk ? 0 : -1;
    }

    int DevicePhysicalPC::Write(int liHandle, u64 lu64Offset, u32 luSize, const void* lpBuffer, int* lpiOutResult)
    {
        HANDLE lhFile = HandleFor(liHandle);
        if (lhFile == INVALID_HANDLE_VALUE)
        {
            if (lpiOutResult) *lpiOutResult = -1;
            return -1;
        }

        LARGE_INTEGER lPos;
        lPos.QuadPart = static_cast<LONGLONG>(lu64Offset);
        SetFilePointerEx(lhFile, lPos, NULL, FILE_BEGIN);

        DWORD ldwWritten = 0;
        const BOOL lbOk = WriteFile(lhFile, lpBuffer, luSize, &ldwWritten, NULL);
        if (lpiOutResult) *lpiOutResult = static_cast<int>(ldwWritten);
        return lbOk ? 0 : -1;
    }

    int DevicePhysicalPC::GetFileSize(int liHandle, u64* lpu64OutSize)
    {
        HANDLE lhFile = HandleFor(liHandle);
        if (lhFile == INVALID_HANDLE_VALUE)
            return -1;

        LARGE_INTEGER lSize;
        if (!GetFileSizeEx(lhFile, &lSize))
            return -1;
        if (lpu64OutSize) *lpu64OutSize = static_cast<u64>(lSize.QuadPart);
        return 0;
    }

    int DevicePhysicalPC::Seek(int liHandle, u64 lu64Offset, int* lpiOutResult)
    {
        HANDLE lhFile = HandleFor(liHandle);
        if (lhFile == INVALID_HANDLE_VALUE)
        {
            if (lpiOutResult) *lpiOutResult = -1;
            return -1;
        }

        LARGE_INTEGER lPos, lNew;
        lPos.QuadPart = static_cast<LONGLONG>(lu64Offset);
        if (!SetFilePointerEx(lhFile, lPos, &lNew, FILE_BEGIN))
        {
            if (lpiOutResult) *lpiOutResult = -1;
            return -1;
        }
        if (lpiOutResult) *lpiOutResult = static_cast<int>(lNew.QuadPart);
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
