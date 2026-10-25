#include "GameShared/GameClasses/System/FileSystem/Devices/CgsRemapDevice.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstring>

namespace CgsFileSystem
{
namespace
{
    // Original CgsStringUtils.h:65 copy expansion. Preserve the assert and
    // terminator-inclusive copy; no truncation or alternate path is introduced.
    void CopyConstructionString(char* lpcDest, u32 luBytes, const char* lpcSource)
    {
        const size_t luLength = std::strlen(lpcSource);
        if (luLength >= luBytes)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::MessageBufferPC lBuffer;
            CgsDev::StrStream lStream(lBuffer.GetBuffer(), lBuffer.GetSize());
            lStream << "String " << lpcSource << " is too long. Buffer size = "
                    << luBytes << ", string length = " << static_cast<u32>(luLength) << "\n";
            CgsDev::Assert::FireAssert(lBuffer.GetBuffer(),
                "..\\..\\..\\GameShared\\GameClasses\\Core/CgsStringUtils.h", 65);
            CgsDev::Assert::EndAssert();
        }
        std::memcpy(lpcDest, lpcSource, luLength + 1);
    }

}
    // ARTIST828DE4C0: exact constructor path copy, including its original assert.
    void RemapDevice::Construct(const char* lpcPath)
    {
        CopyConstructionString(macRemapPath, KI_MAX_REMAP_NAME_LENGTH, lpcPath);
    }

    // ARTIST828DE658; Connect is the existing native name of the Init virtual.
    int RemapDevice::Connect()
    {
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "Remap device initialized\n";
        return 0;
    }

    // ARTIST828DE6B0: only r4/r5/r6 are saved and restored. The apparent r7
    // pseudocode input is not a formal argument and is not added to this API.
    int RemapDevice::Open(const char* lpcPath, u32 luMode, Handle::DeviceHandle* lppOutHandle)
    {
        char lacPhysicalName[256];
        CopyConstructionString(lacPhysicalName, sizeof(lacPhysicalName), macRemapPath);
        const size_t luPrefix = std::strlen(lacPhysicalName);
        const size_t luPath = std::strlen(lpcPath);
        if (luPrefix + luPath >= sizeof(lacPhysicalName) - 1)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("(strlen(lpcSource)+strlen(lpcDest))<luBytes - 1",
                "..\\..\\..\\GameShared\\GameClasses\\Core/CgsStringUtils.h", 75);
            CgsDev::Assert::EndAssert();
        }
        std::memcpy(lacPhysicalName + luPrefix, lpcPath, luPath + 1);
        return GetParent()->Open(lacPhysicalName, luMode, lppOutHandle);
    }
    int RemapDevice::Close(Handle::DeviceHandle lpHandle)
    { return GetParent()->Close(lpHandle); }
    int RemapDevice::Read(Handle::DeviceHandle lpHandle, void* lpBuffer, u32 luSize, u32* lpuOutResult)
    { return GetParent()->Read(lpHandle, lpBuffer, luSize, lpuOutResult); }
    int RemapDevice::Write(Handle::DeviceHandle lpHandle, const void* lpBuffer, u32 luSize, u32* lpuOutResult)
    { return GetParent()->Write(lpHandle, lpBuffer, luSize, lpuOutResult); }
    int RemapDevice::Seek(Handle::DeviceHandle lpHandle, u64 luOffset, u64* lpuOutPosition)
    { return GetParent()->Seek(lpHandle, luOffset, lpuOutPosition); }
    int RemapDevice::GetFileSize(Handle::DeviceHandle lpHandle, u64* lpuOutSize)
    { return GetParent()->GetFileSize(lpHandle, lpuOutSize); }

}
