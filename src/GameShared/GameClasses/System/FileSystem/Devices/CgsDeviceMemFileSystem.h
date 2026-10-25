#pragma once

#include "types.hpp"
#include "SDKs/EATech/rwcore/filesys/device.h"
#include <cstddef>

namespace CgsFileSystem
{
    // ARTIST828F0670..6B4 and820F7BB8; DecFIGS Drivers/CgsDeviceMemFileSystem.h.
    // This is a RenderWare filesys driver, separate from the game's Device engine.
    class DeviceMemFileSystem : public rw::core::filesys::DeviceDriver
    {
    public:
        struct MemFile
        {
            char macName[128];
            u64 muSize;
            void* mpData;
        };
        static const u32 KU_MAX_MEM_FILES = 64;

        explicit DeviceMemFileSystem(const char* lpcDeviceName);
        ~DeviceMemFileSystem() override;
        // ARTIST828E0FF8: deleting tail uses free; the destructor has no owned heap.
        static void operator delete(void* lpAddress) noexcept;

        void Construct();
        bool RegisterMemFile(const char* lpcFileName, void* lpData, u64 luSize);
        bool UnregisterMemFile(const char* lpcFileName);

        void* Open(const char* lpcFileName, unsigned int luFlags,
                   rw::core::filesys::Handle** lppHandle) override;
        void Close(void* lpFile) override;
        unsigned int Read(void* lpFile, void* lpBuffer, unsigned int luSize,
                          rw::core::filesys::DeviceDriver* lpDriver, void* lpContext) override;
        unsigned int Write(void* lpFile, const void* lpBuffer, unsigned int luSize,
                           rw::core::filesys::DeviceDriver* lpDriver, void* lpContext) override;
        u64 Seek(void* lpFile, u64 luPosition, int liWhence,
                 rw::core::filesys::DeviceDriver* lpDriver, void* lpContext) override;
        u64 GetSize(void* lpFile) override;
        bool Resize(void* lpFile, u64 luSize) override;
        bool Delete(const char* lpcFileName) override;
        bool Move(const char* lpcSource, const char* lpcDestination) override;
        void* FindBegin(const char* lpcPattern,
                        rw::core::filesys::DeviceDriver::FindData* lpFindData) override;
        bool FindNext(void* lpFind,
                      rw::core::filesys::DeviceDriver::FindData* lpFindData) override;
        bool FindEnd(void* lpFind) override;
        bool DirectoryCreate(const char* lpcDirectory) override;
        bool DirectoryRemove(const char* lpcDirectory) override;

    private:
        bool mbConstructed;
        MemFile maMemFiles[KU_MAX_MEM_FILES];
    };
}
