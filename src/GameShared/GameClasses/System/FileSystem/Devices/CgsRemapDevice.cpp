#include "types.hpp"
#include "GameShared/GameClasses/System/FileSystem/Devices/CgsDevice.h"   // canonical CgsFileSystem::Device

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsFileSystem::RemapDevice::Close       @ 0x828DE920
//   CgsFileSystem::RemapDevice::Init        @ 0x828DE658
//   CgsFileSystem::RemapDevice::Read        @ 0x828DE938
//   CgsFileSystem::RemapDevice::Seek        @ 0x828DE968
//   CgsFileSystem::RemapDevice::Write       @ 0x828DE950
//   CgsFileSystem::RemapDevice::GetFileSize @ 0x828DE980
//
// A pass-through file device that forwards every operation to the device it wraps
// (mpWrappedDevice). Init only prints a debug trace. The forwarders are plain
// virtual dispatches in the original (the X360 build inlines them to vtable
// thunks, e.g. mpWrappedDevice->vtable[2] == Close). GetFileSize forwards through
// the wrapped device's vtable slot +0x18 (asm 0x828DE988: lwz r11, 0x18(r11)).
//
// Derives from the single canonical CgsFileSystem::Device (CgsDevice.h): it overrides
// the five pure per-operation virtuals plus GetFileSize, and inherits the base's
// default Open/Close/ReadDirectory "Not implemented" bodies. mpWrappedDevice is
// accessed by name, so its exact in-object offset is immaterial here.

namespace CgsDev
{
    namespace Message { extern u64 gxMessageFilterFlags; }   // defined in Development/Log/CgsLog.cpp (u64 -- matches CgsLog.h / CgsMessage.h FilterFlag)
    namespace Log { void DebugPrint(const char* pcText, int liArg); void DebugPrint(const char*, int) { __debugbreak(); } }
}

namespace CgsFileSystem
{
    class RemapDevice : public Device
    {
    public:
        // Init @0x828DE658 mapped to the worker-start Connect() hook (prints a debug trace).
        int Connect() override;
        // The forwarders pass straight through to the wrapped device (X360: vtable thunks,
        // e.g. mpWrappedDevice->vtable[2] == Close; GetFileSize via the wrapped vtable +0x18).
        int Close(Handle::DeviceHandle lpHandle) override
            { return mpWrappedDevice->Close(lpHandle); }
        int Read(Handle::DeviceHandle lpHandle, void* lpBuffer, u32 luSize, u32* lpuOutResult) override
            { return mpWrappedDevice->Read(lpHandle, lpBuffer, luSize, lpuOutResult); }
        int Write(Handle::DeviceHandle lpHandle, const void* lpBuffer, u32 luSize, u32* lpuOutResult) override
            { return mpWrappedDevice->Write(lpHandle, lpBuffer, luSize, lpuOutResult); }
        int Seek(Handle::DeviceHandle lpHandle, u64 lu64Offset, u64* lpu64OutPosition) override
            { return mpWrappedDevice->Seek(lpHandle, lu64Offset, lpu64OutPosition); }
        int GetFileSize(Handle::DeviceHandle lpHandle, u64* lpu64OutSize) override
            { return mpWrappedDevice->GetFileSize(lpHandle, lpu64OutSize); }

    private:
        Device* mpWrappedDevice;
    };

    int RemapDevice::Connect()
    {
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            CgsDev::Log::DebugPrint("Remap device initialized\n", 0);
        return 0;
    }
}
