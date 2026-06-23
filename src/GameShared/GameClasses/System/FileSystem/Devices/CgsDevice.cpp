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

    // @0x828DDE50 (CgsDevice.cpp:91)
    int Device::GetFileSize()
    {
        CGS_ASSERT(false, "Not implemented\n");
        return -2;
    }

    // @0x828DDEE0 (CgsDevice.cpp:103)
    int Device::OpenDirectory()
    {
        CGS_ASSERT(false, "Not implemented\n");
        return -2;
    }

    // @0x828DDF70 (CgsDevice.cpp:111)
    int Device::CloseDirectory()
    {
        CGS_ASSERT(false, "Not implemented\n");
        return -2;
    }

    // @0x828DE000 (CgsDevice.cpp:122)
    int Device::ReadDirectory()
    {
        CGS_ASSERT(false, "Not implemented\n");
        return -2;
    }

} // namespace CgsFileSystem
