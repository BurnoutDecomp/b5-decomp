#include "SDKs/EATech/rwcore/filesys/xenonfiledriver.h"
#include <xtl.h>
#include <cstring>
#include <cstddef>

namespace rw { namespace core { namespace filesys {
    // These are genuine native CRT-owned driver instances, not per-Manager
    // adapters. The pointer table registers DVD, cache, then network after null.
    NullFileDriver gNullFileDriver("null:");
    XenonFileDeviceDriver gDVDFileDriver("D:");
    XenonFileDeviceDriver gCacheFileDriver("CACHE:");
    XenonFileDeviceDriver gNetworkFileDriver("NET:");
    DeviceDriver* const gaBuiltinDeviceDrivers[3] =
        {&gDVDFileDriver, &gCacheFileDriver, &gNetworkFileDriver};

    namespace
    {
        // Inlined throughout the original Xenon driver. Preserve named source
        // bytes while converting the separator; the original has no length guard.
        void CopyPath(char* lpDest, const char* lpcSource)
        {
            if (!lpcSource) lpcSource = lpDest;
            char lc;
            do
            {
                lc = *lpcSource++;
                *lpDest++ = lc == '/' ? '\\' : lc;
            } while (lc);
        }
        void CopyFindData(DeviceDriver::FindData* lpOut, const WIN32_FIND_DATAA& lrData)
        {
            // ARTIST82BBE958..978 copies the native FILETIME bit patterns;
            // it combines the separate high/low size words explicitly.
            memcpy(&lpOut->mCreationTime, &lrData.ftCreationTime, sizeof(u64));
            memcpy(&lpOut->mAccessTime, &lrData.ftLastAccessTime, sizeof(u64));
            memcpy(&lpOut->mModificationTime, &lrData.ftLastWriteTime, sizeof(u64));
            lpOut->mSize = (static_cast<u64>(lrData.nFileSizeHigh) << 32) | lrData.nFileSizeLow;
            strncpy(lpOut->mName, lrData.cFileName, sizeof(lpOut->mName));
            lpOut->mName[255] = 0;
            lpOut->mFlags = 0;
            if (lrData.dwFileAttributes & FILE_ATTRIBUTE_READONLY) lpOut->mFlags = 1;
            if (lrData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) lpOut->mFlags |= 2;
        }
    }

    // ARTIST82BBE558. Flags select access/disposition/cache semantics; only a
    // separator-led path gets this provider's scheme prepended before conversion.
    void* XenonFileDeviceDriver::Open(const char* lpcPath, unsigned int luFlags, Handle**)
    {
        DWORD luAccess = GENERIC_READ;
        if (luFlags & 1) luAccess |= GENERIC_WRITE;
        DWORD luDisposition = OPEN_EXISTING;
        switch (luFlags & 6)
        {
        case 2: luDisposition = CREATE_NEW; break;
        case 4: luDisposition = TRUNCATE_EXISTING; break;
        case 6: luDisposition = CREATE_ALWAYS; break;
        }
        DWORD luAttributes = FILE_ATTRIBUTE_NORMAL;
        if (luFlags & 0x10) luAttributes |= FILE_FLAG_NO_BUFFERING;
        if (luFlags & 8) luAttributes |= FILE_FLAG_SEQUENTIAL_SCAN;
        char lacPath[256];
        if (*lpcPath == '/' || *lpcPath == '\\')
        {
            strcpy(lacPath, mDeviceName);
            strcat(lacPath, lpcPath);
            CopyPath(lacPath, lacPath);
        }
        else CopyPath(lacPath, lpcPath);
        HANDLE lHandle = CreateFileA(lacPath, luAccess, FILE_SHARE_READ, 0,
                                      luDisposition, luAttributes, 0);
        if (lHandle == INVALID_HANDLE_VALUE) return 0;
        File* lpFile = static_cast<File*>(Manager::Allocate(sizeof(File)));
        if (!lpFile) return 0;
        lpFile->mpHandle = lHandle;
        lpFile->muPosition = 0;
        return lpFile;
    }
    void XenonFileDeviceDriver::Close(void* lpFile)
    {
        CloseHandle(static_cast<File*>(lpFile)->mpHandle);
        Manager::Free(lpFile);
    }

    // ARTIST82BBDAB8: failure is routed through the Manager's actual callback;
    // false stops indefinitely. The transfer count still updates the cached cursor.
    unsigned int XenonFileDeviceDriver::Read(void* lpFile, void* lpBuffer, unsigned int luSize,
                                             DeviceDriver*, void*)
    {
        File* lp = static_cast<File*>(lpFile);
        DWORD luRead;
        if (!ReadFile(lp->mpHandle, lpBuffer, luSize, &luRead, 0))
        {
            if (!gpFileSysManager->mpErrorHandler(GetLastError())) Sleep(INFINITE);
        }
        lp->muPosition += luRead;
        return luRead;
    }
    unsigned int XenonFileDeviceDriver::Write(void* lpFile, const void* lpBuffer,
                                              unsigned int luSize, DeviceDriver*, void*)
    {
        File* lp = static_cast<File*>(lpFile);
        DWORD luWritten;
        WriteFile(lp->mpHandle, lpBuffer, luSize, &luWritten, 0);
        lp->muPosition += luWritten;
        return luWritten;
    }

    // ARTIST82BBDBA0 ignores the generic origin argument and seeks absolutely.
    // On failure the32-bit current-position query is zero-extended into the cursor.
    u64 XenonFileDeviceDriver::Seek(void* lpFile, u64 luPosition, int, DeviceDriver*, void*)
    {
        File* lp = static_cast<File*>(lpFile);
        if (luPosition == lp->muPosition) return luPosition;
        LARGE_INTEGER lPosition, lResult;
        lPosition.QuadPart = static_cast<LONGLONG>(luPosition);
        u64 luResult;
        if (SetFilePointerEx(lp->mpHandle, lPosition, &lResult, FILE_BEGIN))
            luResult = static_cast<u64>(lResult.QuadPart);
        else luResult = static_cast<u32>(SetFilePointer(lp->mpHandle, 0, 0, FILE_CURRENT));
        lp->muPosition = luResult;
        return luResult;
    }
    u64 XenonFileDeviceDriver::GetSize(void* lpFile)
    {
        LARGE_INTEGER lSize;
        if (GetFileSizeEx(static_cast<File*>(lpFile)->mpHandle, &lSize) != TRUE) return 0;
        return static_cast<u64>(lSize.QuadPart);
    }
    bool XenonFileDeviceDriver::Resize(void* lpFile, u64 luSize)
    {
        File* lp = static_cast<File*>(lpFile);
        HANDLE lhFile = lp->mpHandle;
        LARGE_INTEGER lZero, lPrevious, lSize;
        lZero.QuadPart = 0;
        lSize.QuadPart = static_cast<LONGLONG>(luSize);
        if (!SetFilePointerEx(lhFile, lZero, &lPrevious, FILE_CURRENT) ||
            !SetFilePointerEx(lhFile, lSize, 0, FILE_BEGIN) ||
            !SetEndOfFile(lhFile) ||
            !SetFilePointerEx(lhFile, lPrevious, 0, FILE_BEGIN)) return false;
        lp->muPosition = static_cast<u64>(lPrevious.QuadPart);
        return true;
    }
    u64 XenonFileDeviceDriver::QueryLocation(void* lpFile)
    {
        const DWORD luKey = XGetFilePhysicalSortKey(static_cast<File*>(lpFile)->mpHandle);
        return luKey == 0xffffffffu ? 0 : static_cast<u64>(luKey) << 11;
    }
    bool XenonFileDeviceDriver::Delete(const char* lpcPath)
    {
        char lacPath[256]; CopyPath(lacPath, lpcPath);
        return DeleteFileA(lacPath) == TRUE;
    }
    bool XenonFileDeviceDriver::Move(const char* lpcFrom, const char* lpcTo)
    {
        char lacTo[256], lacFrom[256];
        CopyPath(lacTo, lpcTo); CopyPath(lacFrom, lpcFrom);
        return MoveFileA(lacFrom, lacTo) == TRUE;
    }
    void* XenonFileDeviceDriver::FindBegin(const char* lpcPath, FindData* lpData)
    {
        char lacPath[256]; CopyPath(lacPath, lpcPath);
        WIN32_FIND_DATAA lData;
        HANDLE lHandle = FindFirstFileA(lacPath, &lData);
        if (lHandle == INVALID_HANDLE_VALUE) return 0;
        File* lpFile = static_cast<File*>(Manager::Allocate(sizeof(File)));
        if (lpFile) { lpFile->mpHandle = lHandle; lpFile->muPosition = 0; }
        if (lpData) CopyFindData(lpData, lData);
        return lpFile;
    }
    bool XenonFileDeviceDriver::FindNext(void* lpFind, FindData* lpData)
    {
        WIN32_FIND_DATAA lData;
        const bool lbResult = FindNextFileA(static_cast<File*>(lpFind)->mpHandle, &lData) == TRUE;
        if (lbResult && lpData) CopyFindData(lpData, lData);
        return lbResult;
    }
    bool XenonFileDeviceDriver::FindEnd(void* lpFind)
    {
        const BOOL lbResult = CloseHandle(static_cast<File*>(lpFind)->mpHandle);
        Manager::Free(lpFind);
        return lbResult != FALSE;
    }
    bool XenonFileDeviceDriver::DirectoryCreate(const char* lpcPath)
    {
        char lacPath[256]; CopyPath(lacPath, lpcPath);
        return CreateDirectoryA(lacPath, 0) == TRUE;
    }
    bool XenonFileDeviceDriver::DirectoryRemove(const char* lpcPath)
    {
        char lacPath[256]; CopyPath(lacPath, lpcPath);
        return RemoveDirectoryA(lacPath) == TRUE;
    }
    void XenonFileDeviceDriver::AssertNativeLayout()
    {
        CGS_STATIC_ASSERT(sizeof(NullFileDriver) == 20, "ARTIST null driver span");
        CGS_STATIC_ASSERT(sizeof(XenonFileDeviceDriver) == 20, "ARTIST Xenon driver span");
        CGS_STATIC_ASSERT(sizeof(File) == 16, "ARTIST Xenon handle allocation");
        CGS_STATIC_ASSERT(offsetof(File, muPosition) == 8, "ARTIST Xenon cached cursor");
        CGS_STATIC_ASSERT(sizeof(FindData) == 296, "ARTIST filesystem find data");
        CGS_STATIC_ASSERT(offsetof(FindData, mName) == 40, "ARTIST filesystem find name");
    }
}}}
