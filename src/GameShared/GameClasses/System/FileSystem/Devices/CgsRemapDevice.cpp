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
    namespace Message { extern u32 gxMessageFilterFlags; }   // defined in Development/Log/CgsLog.cpp
    namespace Log { void DebugPrint(const char* pcText, int liArg); void DebugPrint(const char*, int) { __debugbreak(); } }
}

namespace CgsFileSystem
{
    class RemapDevice : public Device
    {
    public:
        int Init(int liMode, int liArg) override;
        int Close()       override { return mpWrappedDevice->Close(); }
        int Read()        override { return mpWrappedDevice->Read(); }
        int Write()       override { return mpWrappedDevice->Write(); }
        int Seek()        override { return mpWrappedDevice->Seek(); }
        int GetFileSize() override { return mpWrappedDevice->GetFileSize(); }

    private:
        Device* mpWrappedDevice;
    };

    int RemapDevice::Init(int /*liMode*/, int liArg)
    {
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            CgsDev::Log::DebugPrint("Remap device initialized\n", liArg);
        return 0;
    }
}
