// CgsDevice.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsFileSystem::Device::CallErrorCallback @ 0x828D65F0
//   CgsFileSystem::Device::GetFileSize       @ 0x828DDE50
//   CgsFileSystem::Device::OpenDirectory     @ 0x828DDEE0
//   CgsFileSystem::Device::CloseDirectory    @ 0x828DDF70
//   CgsFileSystem::Device::ReadDirectory     @ 0x828DE000
//
// Out-of-line bodies for the CgsFileSystem::Device base. The four directory/size
// entry points are the base "Not implemented" defaults: each fires the assert
// machinery and returns -2 (a device that does not support the operation reports a
// generic failure). CallErrorCallback runs the registered error callback and then
// yields the calling thread for 5ms.
#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\filesystem\\CgsDevice.cpp"
#include "GameShared/GameClasses/System/FileSystem/Devices/CgsDevice.h"
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"

namespace CgsFileSystem
{
    // ---- base "Not implemented" defaults for the worker-dispatched op interface --------
    // A device that does not support an op inherits these (assert + generic -2 failure). The
    // X360 base supplied this same default-body shape (e.g. GetFileSize @0x828DDE50,
    // CgsDevice.cpp:91); concrete devices override the ops they implement.
    int Device::Connect()
    {
        return 0;   // worker-start / op8 hook: base no-op (a device with no connect step)
    }

    int Device::Open(const char* /*lpcPath*/, u32 /*luMode*/, Handle::DeviceHandle* /*lppOutHandle*/)
    {
        CGS_ASSERT(false, "Not implemented\n");
        return -2;
    }

    int Device::Close(Handle::DeviceHandle /*lpHandle*/)
    {
        CGS_ASSERT(false, "Not implemented\n");
        return -2;
    }

    int Device::Read(Handle::DeviceHandle /*lpHandle*/, void* /*lpBuffer*/, u32 /*luSize*/, u32* /*lpuOutResult*/)
    {
        CGS_ASSERT(false, "Not implemented\n");
        return -2;
    }

    int Device::Write(Handle::DeviceHandle /*lpHandle*/, const void* /*lpBuffer*/, u32 /*luSize*/, u32* /*lpuOutResult*/)
    {
        CGS_ASSERT(false, "Not implemented\n");
        return -2;
    }

    int Device::Seek(Handle::DeviceHandle /*lpHandle*/, u64 /*lu64Offset*/, u64* /*lpu64OutPosition*/)
    {
        CGS_ASSERT(false, "Not implemented\n");
        return -2;
    }

    int Device::GetFileSize(Handle::DeviceHandle /*lpHandle*/, u64* /*lpu64OutSize*/)   // @0x828DDE50 (default)
    {
        CGS_ASSERT(false, CGS_ASSERT_STREAM_MESSAGE_AT("Not implemented\n", CGS_ARTIST_FILE, 91));
        return -2;
    }

    // ---- directory enumeration "Not implemented" defaults (@0x828DDEE0/F70/000) ----
    int Device::OpenDirectory(const char* /*lpcPath*/, void* /*lpEntryBuffer*/, u32 /*luMaxEntries*/,
                              u32* /*lpuOutCount*/, Handle::DeviceHandle* /*lppOutHandle*/)  // @0x828DDEE0
    {
        CGS_ASSERT(false, CGS_ASSERT_STREAM_MESSAGE_AT("Not implemented\n", CGS_ARTIST_FILE, 103));
        return -2;
    }

    int Device::CloseDirectory(Handle::DeviceHandle /*lpHandle*/)   // @0x828DDF70
    {
        CGS_ASSERT(false, CGS_ASSERT_STREAM_MESSAGE_AT("Not implemented\n", CGS_ARTIST_FILE, 111));
        return -2;
    }

    int Device::ReadDirectory(Handle::DeviceHandle /*lpHandle*/, void* /*lpEntryBuffer*/,
                              u32 /*luMaxEntries*/, u32* /*lpuOutCount*/)  // @0x828DE000
    {
        CGS_ASSERT(false, CGS_ASSERT_STREAM_MESSAGE_AT("Not implemented\n", CGS_ARTIST_FILE, 122));
        return -2;
    }

    int Device::Shutdown()
    {
        return 0;   // worker-exit hook: base no-op
    }

} // namespace CgsFileSystem

#undef CGS_ARTIST_FILE
