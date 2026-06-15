#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsFileSystem::RemapDevice::Close @ 0x828DE920
//   CgsFileSystem::RemapDevice::Init  @ 0x828DE658
//   CgsFileSystem::RemapDevice::Read  @ 0x828DE938
//   CgsFileSystem::RemapDevice::Seek  @ 0x828DE968
//   CgsFileSystem::RemapDevice::Write @ 0x828DE950
//
// A pass-through file device that forwards every operation to the device it wraps
// (mpWrappedDevice). Init only prints a debug trace. The forwarders are plain
// virtual dispatches in the original (the X360 build inlines them to vtable
// thunks, e.g. mpWrappedDevice->vtable[2] == Close).

namespace CgsDev
{
    namespace Message { extern u32 gxMessageFilterFlags; }   // defined in Development/Log/CgsLog.cpp
    namespace Log { void DebugPrint(const char* pcText, int liArg); void DebugPrint(const char*, int) { __debugbreak(); } }
}

namespace CgsFileSystem
{
    // Abstract file device. The X360 vtable order is: Close(2), Read(3), Write(4),
    // Seek(5), so they are declared in that slot order here.
    class Device
    {
    public:
        virtual ~Device() {}
        virtual int Init(int liMode, int liArg) = 0;
        virtual int Close() = 0;
        virtual int Read() = 0;
        virtual int Write() = 0;
        virtual int Seek() = 0;
    };

    class RemapDevice : public Device
    {
    public:
        int Init(int liMode, int liArg) override;
        int Close() override { return mpWrappedDevice->Close(); }
        int Read()  override { return mpWrappedDevice->Read(); }
        int Write() override { return mpWrappedDevice->Write(); }
        int Seek()  override { return mpWrappedDevice->Seek(); }

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
