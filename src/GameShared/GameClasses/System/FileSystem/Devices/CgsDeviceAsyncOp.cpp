// CgsDeviceAsyncOp.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsFileSystem::AsyncOp::AsyncOp @ 0x828DE2F8
//   CgsFileSystem::AsyncOp::GetLastOperationResult @ 0x82661160
//
// Out-of-line body for the AsyncOp result accessor. Reading the result of an
// operation that was never waited on is a programming error, so the original
// asserts in that case (the asm guards on the +0x28 "outstanding" flag and fires
// the assert machinery against CgsDeviceAsyncOp.h:159) before returning the cached
// +0x18 result.

#include "GameShared/GameClasses/System/FileSystem/Devices/CgsDeviceAsyncOp.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsFileSystem
{
    // ARTIST 0x828DE2F8. Construct the real zero-count semaphore and clear only
    // the operating flag; result, handle and size are not initialized here.
    AsyncOp::AsyncOp()
        : mSemaphore(0)
        , mbOperating(false)
    {
    }

    // ARTIST 0x82661160. Assertion continuation still returns the stored result.
    s32 AsyncOp::GetLastOperationResult()
    {
        if (mbOperating)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "Last operation has not been waited before\n",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\filesystem\\CgsDeviceAsyncOp.h",
                159);
            CgsDev::Assert::EndAssert();
        }
        return miResult;
    }

} // namespace CgsFileSystem
