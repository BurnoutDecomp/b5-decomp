#include "GameShared/GameClasses/System/FileSystem/Devices/CgsDeviceMemFileSystem.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "rw/core/stdc/stdc.h"
#include <cstdlib>
#include <cstring>

namespace CgsFileSystem
{
    namespace
    {
        const char KAC_MEM_DEVICE_SOURCE[] =
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\filesystem\\Drivers/CgsDeviceMemFileSystem.cpp";

        // Each caller retains the actual streamed assertion and original line.
        void AssertMemDeviceMessage(const char* lpcMessage, s32 liLine)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::MessageBufferPC lBuffer;
            CgsDev::StrStream lStream(lBuffer.GetBuffer(), lBuffer.GetSize());
            lStream << lpcMessage;
            CgsDev::Assert::FireAssert(lBuffer.GetBuffer(), KAC_MEM_DEVICE_SOURCE, liLine);
            CgsDev::Assert::EndAssert();
        }
    }

    // DecFIGS cpp30..76; ARTIST828DD6F8..754/828DD760/828DD780/828DD8F8.
    // This is the real separately allocated open-file object, not a table index.
    class MemDeviceFileHandle
    {
    public:
        static void* operator new(std::size_t luSize) noexcept
        {
            // Same filesys ICoreAllocator used by the original open and close.
            return rw::core::filesys::gpFileSysAllocator->Alloc(luSize, nullptr, 0, 4, 0);
        }
        static void operator delete(void* lpAddress) noexcept
        {
            rw::core::filesys::gpFileSysAllocator->Free(lpAddress, 0);
        }
        MemDeviceFileHandle(void* lpData, u64 luDataSize)
            : mpData(lpData), muDataSize(luDataSize), muCurrPos(0)
        {
        }
        void MovePointer(u64 luPosition) { muCurrPos += luPosition; }
        void SetPos(u64 luPosition) { muCurrPos = luPosition; }
        u64 GetPos() const { return muCurrPos; }
        u64 GetSize() const { return muDataSize; }
        void* GetDataPos() const { return static_cast<u8*>(mpData) + muCurrPos; }

    private:
        void* mpData;
        u64 muDataSize;
        u64 muCurrPos;
    };

    // Inlined828F0670..68C: base name copy, false flag, derived vtable.
    DeviceMemFileSystem::DeviceMemFileSystem(const char* lpcDeviceName)
        : rw::core::filesys::DeviceDriver(lpcDeviceName), mbConstructed(false)
    {
    }

    // ARTIST828E0FF8: no member destruction; compiler restores the base vtable.
    DeviceMemFileSystem::~DeviceMemFileSystem()
    {
    }

    void DeviceMemFileSystem::operator delete(void* lpAddress) noexcept
    {
        std::free(lpAddress);
    }

    // Inlined828F0698..6B4; clear all64 real records before setting the flag.
    void DeviceMemFileSystem::Construct()
    {
        std::memset(maMemFiles, 0, sizeof(maMemFiles));
        mbConstructed = true;
    }

    // ARTIST828DD378..420; no duplicate-name check or null-data special policy.
    bool DeviceMemFileSystem::RegisterMemFile(const char* lpcFileName, void* lpData, u64 luSize)
    {
        if (!mbConstructed)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("mbConstructed", KAC_MEM_DEVICE_SOURCE, 107);
            CgsDev::Assert::EndAssert();
        }
        u32 luIndex = 0;
        while (luIndex < KU_MAX_MEM_FILES && maMemFiles[luIndex].mpData)
            ++luIndex;
        if (luIndex == KU_MAX_MEM_FILES)
            return false;
        MemFile& lrFile = maMemFiles[luIndex];
        std::strncpy(lrFile.macName, lpcFileName, sizeof(lrFile.macName));
        lrFile.mpData = lpData;
        lrFile.muSize = luSize;
        return true;
    }

    // ARTIST828DD428: exact case-sensitive compare, no occupied-record filter.
    bool DeviceMemFileSystem::UnregisterMemFile(const char* lpcFileName)
    {
        if (!mbConstructed)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("mbConstructed", KAC_MEM_DEVICE_SOURCE, 134);
            CgsDev::Assert::EndAssert();
        }
        for (u32 luIndex = 0; luIndex < KU_MAX_MEM_FILES; ++luIndex)
        {
            MemFile& lrFile = maMemFiles[luIndex];
            if (std::strcmp(lrFile.macName, lpcFileName) == 0)
            {
                lrFile.macName[0] = 0;
                lrFile.mpData = nullptr;
                lrFile.muSize = 0;
                return true;
            }
        }
        return false;
    }

    // ARTIST828DD500. lppHandle is deliberately untouched by this driver.
    void* DeviceMemFileSystem::Open(const char* lpcFileName, unsigned int luFlags,
                                    rw::core::filesys::Handle** /*lppHandle*/)
    {
        if (luFlags & 1)
        {
            AssertMemDeviceMessage("Can not open mem files for write\n", 166);
            return nullptr;
        }
        if (luFlags & 2)
        {
            AssertMemDeviceMessage("Can not open mem files for create\n", 173);
            return nullptr;
        }
        char lacFileName[256];
        if (*lpcFileName == '/' || *lpcFileName == '\\')
        {
            rw::core::stdc::StringCopy(lacFileName, mDeviceName);
            rw::core::stdc::StringCat(lacFileName, lpcFileName);
        }
        else
            rw::core::stdc::StringCopy(lacFileName, lpcFileName);
        for (char* lpc = lacFileName; *lpc; ++lpc)
            if (*lpc == '/')
                *lpc = '\\';
        const char* lpcRelativeName = lacFileName;
        while (*lpcRelativeName)
        {
            const char lcCharacter = *lpcRelativeName++;
            if (lcCharacter == '\\')
                break;
        }
        for (u32 luIndex = 0; luIndex < KU_MAX_MEM_FILES; ++luIndex)
        {
            const MemFile& lrFile = maMemFiles[luIndex];
            if (lrFile.mpData && rw::core::stdc::StringNoCaseCompare(lrFile.macName, lpcRelativeName) == 0)
                return new MemDeviceFileHandle(lrFile.mpData, lrFile.muSize);
        }
        return nullptr;
    }

    // ARTIST828DD760: original filesys allocator free, no record mutation.
    void DeviceMemFileSystem::Close(void* lpFile)
    {
        MemDeviceFileHandle::operator delete(lpFile);
    }

    // ARTIST828DD780: a read does not advance the current file position.
    unsigned int DeviceMemFileSystem::Read(void* lpFile, void* lpBuffer, unsigned int luSize,
        rw::core::filesys::DeviceDriver* /*lpDriver*/, void* /*lpContext*/)
    {
        const MemDeviceFileHandle* const lpHandle = static_cast<MemDeviceFileHandle*>(lpFile);
        u32 luReadSize = luSize;
        if (lpHandle->GetPos() + static_cast<u64>(luSize) > lpHandle->GetSize())
            luReadSize = static_cast<u32>(lpHandle->GetSize()) - static_cast<u32>(lpHandle->GetPos());
        if (!lpBuffer)
            AssertMemDeviceMessage("No buffer passed in!\n", 286);
        std::memcpy(lpBuffer, lpHandle->GetDataPos(), luReadSize);
        return luReadSize;
    }

    // ARTIST828DD868: the original operation is unsupported and returns zero.
    unsigned int DeviceMemFileSystem::Write(void* /*lpFile*/, const void* /*lpBuffer*/,
        unsigned int /*luSize*/, rw::core::filesys::DeviceDriver* /*lpDriver*/, void* /*lpContext*/)
    {
        AssertMemDeviceMessage("Mem device does not support writes\n", 303);
        return 0;
    }

    // ARTIST828DD8F8: origin2 subtracts the unsigned offset from the file size.
    u64 DeviceMemFileSystem::Seek(void* lpFile, u64 luPosition, int liWhence,
        rw::core::filesys::DeviceDriver* /*lpDriver*/, void* /*lpContext*/)
    {
        MemDeviceFileHandle* const lpHandle = static_cast<MemDeviceFileHandle*>(lpFile);
        if (liWhence == 0 && luPosition == lpHandle->GetPos())
            return luPosition;
        switch (liWhence)
        {
        case 0: lpHandle->SetPos(luPosition); break;
        case 1: lpHandle->MovePointer(luPosition); break;
        case 2: lpHandle->SetPos(lpHandle->GetSize() - luPosition); break;
        default: AssertMemDeviceMessage("Invalid 'whence' value\n", 353); break;
        }
        return lpHandle->GetPos();
    }

    // ARTIST828DD9C8 reads the handle in r4; the pseudocode's this-only is wrong.
    u64 DeviceMemFileSystem::GetSize(void* lpFile)
    {
        return static_cast<MemDeviceFileHandle*>(lpFile)->GetSize();
    }

    bool DeviceMemFileSystem::Resize(void* /*lpFile*/, u64 /*luSize*/)
    {
        AssertMemDeviceMessage("Mem device does not support resizes\n", 388);
        return false;
    }
    bool DeviceMemFileSystem::Delete(const char* /*lpcFileName*/)
    {
        AssertMemDeviceMessage("Mem device does not support deletes\n", 402);
        return false;
    }
    bool DeviceMemFileSystem::Move(const char* /*lpcSource*/, const char* /*lpcDestination*/)
    {
        AssertMemDeviceMessage("Mem device does not support moves\n", 416);
        return false;
    }
    void* DeviceMemFileSystem::FindBegin(const char* /*lpcPattern*/,
        rw::core::filesys::DeviceDriver::FindData* /*lpFindData*/)
    {
        AssertMemDeviceMessage("Mem device does not support searches\n", 430);
        return nullptr;
    }
    bool DeviceMemFileSystem::FindNext(void* /*lpFind*/,
        rw::core::filesys::DeviceDriver::FindData* /*lpFindData*/)
    {
        AssertMemDeviceMessage("Mem device does not support searches\n", 443);
        return false;
    }
    bool DeviceMemFileSystem::FindEnd(void* /*lpFind*/)
    {
        AssertMemDeviceMessage("Mem device does not support searches\n", 482);
        return false;
    }
    bool DeviceMemFileSystem::DirectoryCreate(const char* /*lpcDirectory*/)
    {
        AssertMemDeviceMessage("Mem device does not support directory creation\n", 456);
        return false;
    }
    bool DeviceMemFileSystem::DirectoryRemove(const char* /*lpcDirectory*/)
    {
        AssertMemDeviceMessage("Mem device does not support directory removal\n", 469);
        return false;
    }
}
