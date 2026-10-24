#pragma once
#include "GameShared/GameClasses/System/FileSystem/Devices/CgsDevice.h"

namespace CgsFileSystem
{
    struct RemapDevice : public Device
    {
    public:
        static const s32 KI_MAX_REMAP_NAME_LENGTH = 64;
        void Construct(const char* lpcPath);
        int Connect() override;
        int Open(const char* lpcPath, u32 luMode, Handle::DeviceHandle* lppOutHandle) override;
        int Close(Handle::DeviceHandle lpHandle) override;
        int Read(Handle::DeviceHandle lpHandle, void* lpBuffer, u32 luSize, u32* lpuOutResult) override;
        int Write(Handle::DeviceHandle lpHandle, const void* lpBuffer, u32 luSize, u32* lpuOutResult) override;
        int Seek(Handle::DeviceHandle lpHandle, u64 luOffset, u64* lpuOutPosition) override;
        int GetFileSize(Handle::DeviceHandle lpHandle, u64* lpuOutSize) override;

    private:
        char macRemapPath[KI_MAX_REMAP_NAME_LENGTH];
    };
}
