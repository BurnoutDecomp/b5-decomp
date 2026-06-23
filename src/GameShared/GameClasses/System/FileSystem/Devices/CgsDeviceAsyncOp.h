#pragma once

// CgsDeviceAsyncOp.h — canonical home for CgsFileSystem::AsyncOp, the handle for a
// single outstanding asynchronous device operation (open/read/write/close issued
// against a CgsFileSystem::Device). A caller issues the op, later waits on it, then
// reads back the operation result.
//
// SOURCE (X360 ARTIST):
//   AsyncOp::GetLastOperationResult @ 0x82661160  (DWARF CgsDeviceAsyncOp.h:159)
//
// LAYOUT (proven offsets from the asm; the intervening members — the device
// handle, the completion event, the byte counts — are not exercised by this TU and
// are NOT reconstructed here, so this is a MINIMAL SLICE, not the full layout):
//   +0x18 (24)  miLastOperationResult — the int result of the last completed op
//                                       (asm: lwz r3, 0x18(this))
//   +0x28 (40)  mbOperationOutstanding — non-zero while an op has been issued but
//                                        not yet waited on (asm: lbz r11, 0x28(this))
// Future AsyncOp issue/wait work fleshes out the full layout IN THIS SAME FILE (add
// a correct sizeof check once present). Members are accessed strictly by name.

#include "types.hpp"

namespace CgsFileSystem
{
    class AsyncOp
    {
    public:
        // @0x82661160 (DWARF CgsDeviceAsyncOp.h:159). Returns the result of the last
        // completed operation. Asserts if the op was never waited on — reading the
        // result of an op still in flight is a programming error.
        int GetLastOperationResult();

    private:
        int  miLastOperationResult;   // +0x18 — last completed op result
        bool mbOperationOutstanding;  // +0x28 — set while an op is issued-but-unwaited
    };

} // namespace CgsFileSystem
