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

#include "GameShared/GameClasses/System/FileSystem/Devices/CgsDevice.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// X360 EA::Thread facade (home: src/SDKs/EATech/eathread/BrnEAThreadX360.cpp).
// The X360 ABI is ThreadSleep(const u32* pMilliseconds) @0x82B42610 — it takes a
// POINTER to a millisecond count (0 -> yield, else SleepEx), which is what the asm
// at 0x828D6630 calls (it stores 5 into a stack slot and passes its address). This
// is declared locally rather than #including the divergent vendor eathread.h, whose
// ThreadSleep(const ThreadTime&) signature does not match the X360 build.
namespace EA
{
namespace Thread
{
    u32 ThreadSleep(const u32* lpuMilliseconds);
}
}

namespace CgsFileSystem
{
    // @0x828D65F0
    int Device::CallErrorCallback(int liError)
    {
        int liResult = 0;
        if (mpfErrorCallback)
            liResult = mpfErrorCallback(liError);

        // Yield 5ms before the failing caller retries (asm stores 5 and passes &it).
        u32 luSleepMs = 5;
        EA::Thread::ThreadSleep(&luSleepMs);
        return liResult;
    }

    // ---- base "Not implemented" defaults for the worker-dispatched op interface --------
    // A device that does not support an op inherits these (assert + generic -2 failure). The
    // X360 base supplied this same default-body shape (e.g. GetFileSize @0x828DDE50,
    // CgsDevice.cpp:91); concrete devices override the ops they implement.
    int Device::Connect()
    {
        return 0;   // worker-start / op8 hook: base no-op (a device with no connect step)
    }

    int Device::Open(const char* /*lpcPath*/, int /*liMode*/, int* /*lpiOutHandle*/)
    {
        CGS_ASSERT(false, "Not implemented\n");
        return -2;
    }

    int Device::Close(int /*liHandle*/)
    {
        CGS_ASSERT(false, "Not implemented\n");
        return -2;
    }

    int Device::Read(int /*liHandle*/, u64 /*lu64Offset*/, u32 /*luSize*/, void* /*lpBuffer*/, int* /*lpiOutResult*/)
    {
        CGS_ASSERT(false, "Not implemented\n");
        return -2;
    }

    int Device::Write(int /*liHandle*/, u64 /*lu64Offset*/, u32 /*luSize*/, const void* /*lpBuffer*/, int* /*lpiOutResult*/)
    {
        CGS_ASSERT(false, "Not implemented\n");
        return -2;
    }

    int Device::CheckOp(int /*liHandle*/, u64 /*lu64Offset*/, void* /*lpOut*/)
    {
        return 0;   // default pre-check: OK to proceed
    }

    int Device::GetFileSize(int /*liHandle*/, u64* /*lpu64OutSize*/)   // @0x828DDE50 (default)
    {
        CGS_ASSERT(false, "Not implemented\n");
        return -2;
    }

    int Device::OpenEx(const char* /*lpcPath*/, u32 /*luA*/, u32 /*luB*/, int* /*lpiOutC*/, int* /*lpiOutResult*/)
    {
        CGS_ASSERT(false, "Not implemented\n");
        return -2;
    }

    int Device::Op7(int /*liHandle*/)
    {
        CGS_ASSERT(false, "Not implemented\n");
        return -2;
    }

    int Device::Seek(int /*liHandle*/, u64 /*lu64Offset*/, int* /*lpiOutResult*/)
    {
        CGS_ASSERT(false, "Not implemented\n");
        return -2;
    }

    int Device::Shutdown()
    {
        return 0;   // worker-exit hook: base no-op
    }

    // ---- directory enumeration "Not implemented" defaults (@0x828DDEE0/F70/000) ----
    int Device::OpenDirectory(const char* /*lpcPath*/, void* /*lpEntryBuffer*/, int /*liMaxEntries*/, int* /*lpiOutCount*/, int* /*lpiOutHandle*/)  // @0x828DDEE0
    {
        CGS_ASSERT(false, "Not implemented\n");
        return -2;
    }

    int Device::CloseDirectory(int /*liHandle*/)   // @0x828DDF70
    {
        CGS_ASSERT(false, "Not implemented\n");
        return -2;
    }

    int Device::ReadDirectory(int /*liHandle*/, void* /*lpEntryBuffer*/, int /*liMaxEntries*/, int* /*lpiOutCount*/)  // @0x828DE000
    {
        CGS_ASSERT(false, "Not implemented\n");
        return -2;
    }

} // namespace CgsFileSystem
