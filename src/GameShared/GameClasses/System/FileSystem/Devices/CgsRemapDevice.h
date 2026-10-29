#pragma once
#include "GameShared/GameClasses/System/FileSystem/Devices/CgsDevice.h"
namespace CgsFileSystem
{
    // DecFIGS names macRemapPath; ARTIST828DE4C0 stores it at+12.
    class RemapDevice : public Device
    {
    public:
        void Construct(const char* lpcPath);
        virtual int Connect();
        virtual int Open(const char*, u32, Handle::DeviceHandle*);
        virtual int Close(Handle::DeviceHandle);
        virtual int Read(Handle::DeviceHandle, void*, u32, u32*);
        virtual int Write(Handle::DeviceHandle, const void*, u32, u32*);
        virtual int Seek(Handle::DeviceHandle, u64, u64*);
        virtual int GetFileSize(Handle::DeviceHandle, u64*);
    private:
        char macRemapPath[64];
    };
}
