#include "GameShared/GameClasses/System/FileSystem/Devices/CgsDeviceMemFileSystem.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "rw/core/stdc/stdc.h"
#include <cstring>
#include <cstddef>
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"
// SOURCE LINES: ARTIST's asserts in this file (file string @0x820F9748) pass the PS3 DecFIGS lines
// unchanged (RegisterMemFile 107 .. FindEnd 482); each assert sits on that line. The two
// registration asserts are plain messages, every other one streams its message (ARTIST builds the
// StrStream over the shared assert buffer), as in the original.
#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\filesystem\\Drivers/CgsDeviceMemFileSystem.cpp"

namespace CgsFileSystem
{
    DeviceMemFileSystem::~DeviceMemFileSystem() {}

















































































    // ARTIST828DD378: first empty data slot, bounded name copy, then data and size.
    bool DeviceMemFileSystem::RegisterMemFile(const char* lpcName, void* lpData, u64 luSize)
    {
        CGS_ASSERT_AT(mbConstructed, "mbConstructed", CGS_ARTIST_FILE, 107);
        for (u32 lu = 0; lu < KU_MAX_MEM_FILES; ++lu)
        {
            if (maMemFiles[lu].mpData) continue;
            strncpy(maMemFiles[lu].macName, lpcName, sizeof(maMemFiles[lu].macName));
            maMemFiles[lu].mpData = lpData;
            maMemFiles[lu].muSize = luSize;
            return true;
        }
        return false;
    }













    // ARTIST828DD428. Registration names use exact-case comparison here.
    bool DeviceMemFileSystem::UnregisterMemFile(const char* lpcName)
    {
        CGS_ASSERT_AT(mbConstructed, "mbConstructed", CGS_ARTIST_FILE, 134);
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
    // flags 1/2 reject writes/creates, matching the original early returns.
    void* DeviceMemFileSystem::Open(const char* lpcName, unsigned int luFlags, rw::core::filesys::Handle**)
    {
        if (luFlags & 1)
        {


            CGS_FAIL_STREAM("Can not open mem files for write\n");
            return 0;
        }
        if (luFlags & 2)
        {


            CGS_FAIL_STREAM("Can not open mem files for create\n");
            return 0;
        }
        char lacName[256];
        if (*lpcName == '/' || *lpcName == '\\')
        {
            rw::core::stdc::StringCopy(lacName, mDeviceName);
            rw::core::stdc::StringCat(lacName, lpcName);
        }
        else rw::core::stdc::StringCopy(lacName, lpcName);
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

    u64 DeviceMemFileSystem::GetSize(void* lpFile)
    { return static_cast<OpenMemFile*>(lpFile)->muSize; }






































































    // ARTIST828DD780 performs no cursor update; Seek owns muPosition.
    unsigned int DeviceMemFileSystem::Read(void* lpFH, void* lpBuffer, unsigned int luBufferSize, rw::core::filesys::DeviceDriver*, void*)
    {
        OpenMemFile* lp = static_cast<OpenMemFile*>(lpFH);
        u32 luRead;
        if (lp->muPosition + luBufferSize > lp->muSize)
        {
            luRead = static_cast<u32>(lp->muSize) - static_cast<u32>(lp->muPosition);
        }
        else luRead = luBufferSize;
        CGS_ASSERT_STREAM_AT(lpBuffer != 0, "No buffer passed in!\n", CGS_ARTIST_FILE, 286);
        memcpy(lpBuffer, static_cast<char*>(lp->mpData) + lp->muPosition, luRead);
        return luRead;
    }









    unsigned int DeviceMemFileSystem::Write(void*, const void*, unsigned int, rw::core::filesys::DeviceDriver*, void*)
    {


        CGS_FAIL_STREAM_AT("Mem device does not support writes\n", CGS_ARTIST_FILE, 303);
        return 0;
    }














    // ARTIST828DD8F8: r5 is a full 64-bit position; r6 is whence.
    u64 DeviceMemFileSystem::Seek(void* lpFH, u64 luOffset, int liWhence, rw::core::filesys::DeviceDriver*, void*)
    {
        OpenMemFile* lp = static_cast<OpenMemFile*>(lpFH);
        if (liWhence == 0 && luOffset == lp->muPosition) return luOffset;
        switch (static_cast<u32>(liWhence))
        {
        case 0:
            lp->muPosition = luOffset;
            break;
        case 1:
            lp->muPosition += luOffset;
            break;
        case 2:
            lp->muPosition = lp->muSize - luOffset;
            break;
        default:
















            CGS_FAIL_STREAM("Invalid 'whence' value\n");
            break;
        }
        return lp->muPosition;
    }




























    bool DeviceMemFileSystem::Resize(void*, u64)
    {
        CGS_FAIL_STREAM_AT("Mem device does not support resizes\n", CGS_ARTIST_FILE, 388);
        return false;
    }









    bool DeviceMemFileSystem::Delete(const char*)
    {
        CGS_FAIL_STREAM_AT("Mem device does not support deletes\n", CGS_ARTIST_FILE, 402);
        return false;
    }









    bool DeviceMemFileSystem::Move(const char*, const char*)
    {
        CGS_FAIL_STREAM_AT("Mem device does not support moves\n", CGS_ARTIST_FILE, 416);
        return false;
    }









    void* DeviceMemFileSystem::FindBegin(const char*, FindData*)
    {
        CGS_FAIL_STREAM_AT("Mem device does not support searches\n", CGS_ARTIST_FILE, 430);
        return 0;
    }








    bool DeviceMemFileSystem::FindNext(void*, FindData*)
    {
        CGS_FAIL_STREAM_AT("Mem device does not support searches\n", CGS_ARTIST_FILE, 443);
        return false;
    }








    bool DeviceMemFileSystem::DirectoryCreate(const char*)
    {
        CGS_FAIL_STREAM_AT("Mem device does not support directory creation\n", CGS_ARTIST_FILE, 456);
        return false;
    }








    bool DeviceMemFileSystem::DirectoryRemove(const char*)
    {
        CGS_FAIL_STREAM_AT("Mem device does not support directory removal\n", CGS_ARTIST_FILE, 469);
        return false;
    }








    bool DeviceMemFileSystem::FindEnd(void*)
    {
        CGS_FAIL_STREAM_AT("Mem device does not support searches\n", CGS_ARTIST_FILE, 482);
        return false;
    }















    void DeviceMemFileSystem::AssertNativeLayout()
    {
        CGS_STATIC_ASSERT(sizeof(MemFile) == 0x90, "ARTIST memory-file entry");
        CGS_STATIC_ASSERT(sizeof(OpenMemFile) == 0x18, "ARTIST open memory-file handle");
        CGS_STATIC_ASSERT(offsetof(OpenMemFile, muSize) == 8, "ARTIST memory-file size");
        CGS_STATIC_ASSERT(offsetof(OpenMemFile, muPosition) == 16, "ARTIST memory-file position");
        CGS_STATIC_ASSERT(sizeof(DeviceMemFileSystem) == 0x2418, "ARTIST memory-file device");
    }
}

#undef CGS_ARTIST_FILE
