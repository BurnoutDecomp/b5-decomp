// CgsDeviceAsyncOp.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX
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
    // @0x82661160 (DWARF CgsDeviceAsyncOp.h:159)
    int AsyncOp::GetLastOperationResult()
    {
        CGS_ASSERT(!mbOperationOutstanding,
                   "Last operation has not been waited before\n");
        return miLastOperationResult;
    }

} // namespace CgsFileSystem
