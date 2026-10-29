#pragma once

#include "SDKs/EATech/rwcore/filesys/device.h"

namespace rw { namespace core { namespace filesys {
    // The genuine null fallback atARTIST8327F098. Its19-slot table821811E0
    // has successful Init and constant-zero IO capabilities, not an OS driver.
    struct NullFileDriver : DeviceDriver
    {
        explicit NullFileDriver(const char* lpcName) : DeviceDriver(lpcName) {}
        virtual ~NullFileDriver() {}
        virtual void* Open(const char*, unsigned int, Handle**) { return 0; }
        virtual void Close(void*) {}
        virtual unsigned int Read(void*, void*, unsigned int, DeviceDriver*, void*) { return 0; }
        virtual u64 Seek(void*, u64, int, DeviceDriver*, void*) { return 0; }
        virtual u64 GetSize(void*) { return 0; }
    };

    // ARTIST82181230: the actual19-slot Xenon Win32/XDK file provider.
    // Driver objects contain only the20-byte named base; each opened handle is
    // separately allocated through Manager's installed native resource allocator.
    struct XenonFileDeviceDriver : DeviceDriver
    {
        explicit XenonFileDeviceDriver(const char* lpcName) : DeviceDriver(lpcName) {}
        virtual ~XenonFileDeviceDriver() {}
        virtual void* Open(const char*, unsigned int, Handle**);
        virtual void Close(void*);
        virtual unsigned int Read(void*, void*, unsigned int, DeviceDriver*, void*);
        virtual unsigned int Write(void*, const void*, unsigned int, DeviceDriver*, void*);
        virtual u64 Seek(void*, u64, int, DeviceDriver*, void*);
        virtual u64 GetSize(void*);
        virtual bool Resize(void*, u64);
        virtual u64 QueryLocation(void*);
        virtual bool Delete(const char*);
        virtual bool Move(const char*, const char*);
        virtual void* FindBegin(const char*, FindData*);
        virtual bool FindNext(void*, FindData*);
        virtual bool FindEnd(void*);
        virtual bool DirectoryCreate(const char*);
        virtual bool DirectoryRemove(const char*);
    private:
        struct File
        {
            void* mpHandle;
            u64 muPosition;
        };
        static void AssertNativeLayout();
    };

    // Original CRT thunks82C73EE0/3F28/3F70/3FB8 construct these in this order.
    extern NullFileDriver gNullFileDriver;             //8327F098, "null:"
    extern XenonFileDeviceDriver gDVDFileDriver;       //8327F084, "D:"
    extern XenonFileDeviceDriver gCacheFileDriver;     //8327F0AC, "CACHE:"
    extern XenonFileDeviceDriver gNetworkFileDriver;   //8327F0C0, "NET:"
    extern DeviceDriver* const gaBuiltinDeviceDrivers[3]; //82F91A3C
}}}
