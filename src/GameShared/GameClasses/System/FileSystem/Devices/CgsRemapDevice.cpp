#include "GameShared/GameClasses/System/FileSystem/Devices/CgsRemapDevice.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
namespace CgsFileSystem
{
    void RemapDevice::Construct(const char* lpcPath)
    { CgsCore::StrCpy(macRemapPath, sizeof(macRemapPath), lpcPath); }
    int RemapDevice::Connect()
    {
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "Remap device initialized\n";
        return 0;
    }
    int RemapDevice::Open(const char* lpcPath, u32 luMode, Handle::DeviceHandle* lppHandle)
    {
        char lacPath[256];
        CgsCore::StrCpy(lacPath, sizeof(lacPath), macRemapPath);
        CgsCore::StrCat(lacPath, sizeof(lacPath), lpcPath);
        return mpParent->Open(lacPath, luMode, lppHandle);
    }
    int RemapDevice::Close(Handle::DeviceHandle lpHandle) { return mpParent->Close(lpHandle); }
    int RemapDevice::Read(Handle::DeviceHandle lpHandle, void* lpBuffer, u32 luSize, u32* lpRead)
    { return mpParent->Read(lpHandle, lpBuffer, luSize, lpRead); }
    int RemapDevice::Write(Handle::DeviceHandle lpHandle, const void* lpBuffer, u32 luSize, u32* lpWritten)
    { return mpParent->Write(lpHandle, lpBuffer, luSize, lpWritten); }
    int RemapDevice::Seek(Handle::DeviceHandle lpHandle, u64 luPosition, u64* lpPosition)
    { return mpParent->Seek(lpHandle, luPosition, lpPosition); }
    int RemapDevice::GetFileSize(Handle::DeviceHandle lpHandle, u64* lpSize)
    { return mpParent->GetFileSize(lpHandle, lpSize); }
}
