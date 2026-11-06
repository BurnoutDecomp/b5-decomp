// CgsDeviceAsyncOp.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsFileSystem::AsyncOp::AsyncOp @ 0x828DE2F8
//   CgsFileSystem::AsyncOp::GetLastOperationResult @ 0x82661160
//
// Out-of-line bodies for the AsyncOp result accessors. The original code checks
// the +0x28 outstanding flag, streams the assertion message into the shared
// assert buffer, then returns the cached handle/result on continuation.

#include "GameShared/GameClasses/System/FileSystem/Devices/CgsDeviceAsyncOp.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"

namespace CgsFileSystem
{
    // ARTIST 0x828DE2F8. Construct the real zero-count semaphore and clear only
    // the operating flag; result, handle and size are not initialized here.
    AsyncOp::AsyncOp()
        : mSemaphore(0)
        , mbOperating(false)
    {
    }

    // ARTIST 0x826610A8. Assertion continuation still returns the copied handle.
    Handle AsyncOp::GetLastOperationHandle()
    {
        if (mbOperating)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lAssertMessage(
                CgsDev::Assert::gpcMessageBuffer,
                CgsDev::Assert::giMessageBufferSize);
            lAssertMessage << "Last operation has not been waited before\n";
            CgsDev::Assert::FireAssert(
                CgsDev::Assert::gpcMessageBuffer,
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\filesystem\\CgsDeviceAsyncOp.h",
                152);
            CgsDev::Assert::EndAssert();
        }
        return mHandle;
    }

    // ARTIST 0x82661160. Assertion continuation still returns the stored result.
    s32 AsyncOp::GetLastOperationResult()
    {
        if (mbOperating)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lAssertMessage(
                CgsDev::Assert::gpcMessageBuffer,
                CgsDev::Assert::giMessageBufferSize);
            lAssertMessage << "Last operation has not been waited before\n";
            CgsDev::Assert::FireAssert(
                CgsDev::Assert::gpcMessageBuffer,
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\filesystem\\CgsDeviceAsyncOp.h",
                159);
            CgsDev::Assert::EndAssert();
        }
        return miResult;
    }

} // namespace CgsFileSystem
