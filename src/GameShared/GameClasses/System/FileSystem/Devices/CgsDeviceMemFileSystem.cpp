#include "GameShared/GameClasses/System/FileSystem/Devices/CgsDeviceMemFileSystem.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/core/stdc/stdc.h"
#include <cstring>
#include <cstddef>

namespace CgsFileSystem
{
    DeviceMemFileSystem::DeviceMemFileSystem(const char* lpcName)
        : rw::core::filesys::DeviceDriver(lpcName), mbConstructed(false) {}
    DeviceMemFileSystem::~DeviceMemFileSystem() {}

    // Inlined ARTIST828F065C..66C: table clear precedes constructed publication.
    void DeviceMemFileSystem::Construct()
    {
        memset(maMemFiles, 0, sizeof(maMemFiles));
        mbConstructed = true;
    }

    // ARTIST828DD428. Registration names use exact-case comparison here.
    bool DeviceMemFileSystem::UnregisterMemFile(const char* lpcName)
    {
        CGS_ASSERT(mbConstructed, "mbConstructed");
        for (u32 lu = 0; lu < KU_MAX_MEM_FILES; ++lu)
        {
            if (strcmp(maMemFiles[lu].macName, lpcName) != 0) continue;
            maMemFiles[lu].macName[0] = 0;
            maMemFiles[lu].mpData = 0;
            maMemFiles[lu].muSize = 0;
            return true;
        }
        return false;
    }

    // ARTIST828DD500. Strip the device component after normalizing separators;
    // flags1/2 reject writes/creates, matching the original early returns.
    void* DeviceMemFileSystem::Open(const char* lpcPath, unsigned int luFlags,
                                    rw::core::filesys::Handle**)
    {
        if (luFlags & 1) { CGS_ASSERT(false, "Can not open mem files for write\n"); return 0; }
        if (luFlags & 2) { CGS_ASSERT(false, "Can not open mem files for create\n"); return 0; }
        char lacName[256];
        if (*lpcPath == '/' || *lpcPath == '\\')
        {
            rw::core::stdc::StringCopy(lacName, mDeviceName);
            rw::core::stdc::StringCat(lacName, lpcPath);
        }
        else rw::core::stdc::StringCopy(lacName, lpcPath);
        for (char* lp = lacName; *lp; ++lp) if (*lp == '/') *lp = '\\';
        const char* lpName = lacName;
        while (*lpName) { const char lc = *lpName++; if (lc == '\\') break; }
        for (u32 lu = 0; lu < KU_MAX_MEM_FILES; ++lu)
        {
            if (!maMemFiles[lu].mpData ||
                rw::core::stdc::StringNoCaseCompare(maMemFiles[lu].macName, lpName)) continue;
            OpenMemFile* lpFile = static_cast<OpenMemFile*>(
                rw::core::filesys::gpFileSysAllocator->Alloc(sizeof(OpenMemFile), 0, 0, 4, 0));
            if (!lpFile) return 0;
            lpFile->muPosition = 0;
            lpFile->muSize = maMemFiles[lu].muSize;
            lpFile->mpData = maMemFiles[lu].mpData;
            return lpFile;
        }
        return 0;
    }

    void DeviceMemFileSystem::Close(void* lpFile)
    { rw::core::filesys::gpFileSysAllocator->Free(lpFile, 0); }

    // ARTIST828DD780 performs no cursor update; Seek owns muPosition.
    unsigned int DeviceMemFileSystem::Read(void* lpFile, void* lpBuffer, unsigned int luSize,
                                          rw::core::filesys::DeviceDriver*, void*)
    {
        OpenMemFile* lp = static_cast<OpenMemFile*>(lpFile);
        const u32 luRead = lp->muPosition + luSize <= lp->muSize ? luSize
            : static_cast<u32>(lp->muSize) - static_cast<u32>(lp->muPosition);
        CGS_ASSERT(lpBuffer != 0, "No buffer passed in!\n");
        memcpy(lpBuffer, static_cast<char*>(lp->mpData) + lp->muPosition, luRead);
        return luRead;
    }
    unsigned int DeviceMemFileSystem::Write(void*, const void*, unsigned int,
                                           rw::core::filesys::DeviceDriver*, void*)
    { CGS_ASSERT(false, "Mem device does not support writes\n"); return 0; }

    // ARTIST828DD8F8: r5 is a full64-bit position; r6 is whence.
    u64 DeviceMemFileSystem::Seek(void* lpFile, u64 luPosition, int liWhence,
                                  rw::core::filesys::DeviceDriver*, void*)
    {
        OpenMemFile* lp = static_cast<OpenMemFile*>(lpFile);
        if (liWhence == 0 && luPosition == lp->muPosition) return luPosition;
        switch (static_cast<u32>(liWhence))
        {
        case 0: lp->muPosition = luPosition; break;
        case 1: lp->muPosition += luPosition; break;
        case 2: lp->muPosition = lp->muSize - luPosition; break;
        default: CGS_ASSERT(false, "Invalid 'whence' value\n"); break;
        }
        return lp->muPosition;
    }
    u64 DeviceMemFileSystem::GetSize(void* lpFile)
    { return static_cast<OpenMemFile*>(lpFile)->muSize; }
    bool DeviceMemFileSystem::Resize(void*, u64)
    { CGS_ASSERT(false, "Mem device does not support resizes\n"); return false; }
    bool DeviceMemFileSystem::Delete(const char*)
    { CGS_ASSERT(false, "Mem device does not support deletes\n"); return false; }
    bool DeviceMemFileSystem::Move(const char*, const char*)
    { CGS_ASSERT(false, "Mem device does not support moves\n"); return false; }
    void* DeviceMemFileSystem::FindBegin(const char*, FindData*)
    { CGS_ASSERT(false, "Mem device does not support searches\n"); return 0; }
    bool DeviceMemFileSystem::FindNext(void*, FindData*)
    { CGS_ASSERT(false, "Mem device does not support searches\n"); return false; }
    bool DeviceMemFileSystem::FindEnd(void*)
    { CGS_ASSERT(false, "Mem device does not support searches\n"); return false; }
    bool DeviceMemFileSystem::DirectoryCreate(const char*)
    { CGS_ASSERT(false, "Mem device does not support directory creation\n"); return false; }
    bool DeviceMemFileSystem::DirectoryRemove(const char*)
    { CGS_ASSERT(false, "Mem device does not support directory removal\n"); return false; }

    void DeviceMemFileSystem::AssertNativeLayout()
    {
        CGS_STATIC_ASSERT(sizeof(MemFile) == 0x90, "ARTIST memory-file entry");
        CGS_STATIC_ASSERT(sizeof(OpenMemFile) == 0x18, "ARTIST open memory-file handle");
        CGS_STATIC_ASSERT(offsetof(OpenMemFile, muSize) == 8, "ARTIST memory-file size");
        CGS_STATIC_ASSERT(offsetof(OpenMemFile, muPosition) == 16, "ARTIST memory-file position");
        CGS_STATIC_ASSERT(sizeof(DeviceMemFileSystem) == 0x2418, "ARTIST memory-file device");
    }
}
