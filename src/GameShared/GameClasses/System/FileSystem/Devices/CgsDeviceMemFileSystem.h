#pragma once

#include "types.hpp"
#include "SDKs/EATech/rwcore/filesys/device.h"

namespace CgsFileSystem
{
    // ARTIST828F04E8 allocates0x2418 bytes: DeviceDriver20, constructed byte,
    // and64 naturally aligned144-byte MemFile records. DecFIGS names the fields.
    class DeviceMemFileSystem : public rw::core::filesys::DeviceDriver
    {
    public:
        static const u32 KU_MAX_MEM_FILES = 64;
        struct MemFile
        {
            char macName[128];
            u64 muSize;
            void* mpData;
        };
        explicit DeviceMemFileSystem(const char* lpcName);
        virtual ~DeviceMemFileSystem();
        void Construct();
        bool UnregisterMemFile(const char* lpcName);
        virtual void* Open(const char*, unsigned int, rw::core::filesys::Handle**);
        virtual void Close(void*);
        virtual unsigned int Read(void*, void*, unsigned int, rw::core::filesys::DeviceDriver*, void*);
        virtual unsigned int Write(void*, const void*, unsigned int, rw::core::filesys::DeviceDriver*, void*);
        virtual u64 Seek(void*, u64, int, rw::core::filesys::DeviceDriver*, void*);
        virtual u64 GetSize(void*);
        virtual bool Resize(void*, u64);
        virtual bool Delete(const char*);
        virtual bool Move(const char*, const char*);
        virtual void* FindBegin(const char*, FindData*);
        virtual bool FindNext(void*, FindData*);
        virtual bool FindEnd(void*);
        virtual bool DirectoryCreate(const char*);
        virtual bool DirectoryRemove(const char*);
    private:
        bool mbConstructed;
        MemFile maMemFiles[KU_MAX_MEM_FILES];
        // ARTIST828DD740..750: allocated handle contains data, size, position.
        struct OpenMemFile
        {
            void* mpData;
            u64 muSize;
            u64 muPosition;
        };
        static void AssertNativeLayout();
    };
}
