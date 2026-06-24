#pragma once

#include "types.hpp"

// CgsFileSystem::Operation — one queued file-system operation. A producer
// (DeviceManager::Open/Read/Write/Close/...) fills an Operation and hands it to the target
// physical device's OperationPool; that device's worker thread (DeviceManager::Physical-
// DeviceThread @0x828F1EA8) drains it, dispatches on miType to the device's virtual op, and
// fires the completion callback. The X360 stored this as a 304-byte (0x130) record and the
// scheduler read the priority at +0x12C; here it is a named struct (faithful field order, x64
// widths — NOT byte-matched) whose GetPriority() returns the named priority field so the
// OperationPool scheduler no longer needs the raw +0x12C offset.
//
// Reconstructed from the worker dispatch in PhysicalDeviceThread (BURNOUT_X360_ARTIST.XEX):
// it reads the op type, a path buffer (for opens), the device pointer + handle, an offset,
// sizes, a completion callback + its context, and the priority.

namespace CgsFileSystem
{
    class Device;

    // The worker fires this after an op completes: callback(device, handle, result, context).
    // (X360: the `v22(HIDWORD(v17), v17, result, v23)` indirect call in PhysicalDeviceThread.)
    // The context was a 32-bit value on the X360; here it is a void* (x64 callers pass an
    // object pointer, e.g. a sync-wait completion record) — marked PC divergence.
    typedef void (*OperationCallback)(Device* lpDevice, s32 liHandle, s32 liResult, void* lpContext);

    // miType values — the worker's switch(opcode) arms (PhysicalDeviceThread cases 0..9).
    enum OperationType
    {
        E_OP_READ        = 0,  // device->Read  (after a pre-check)
        E_OP_WRITE       = 1,  // device->Write (after a pre-check)
        E_OP_OPEN        = 2,  // device->Open  (uses macPath)
        E_OP_CLOSE       = 3,  // device->Close
        E_OP_GETFILESIZE = 4,  // device->GetFileSize
        E_OP_OPENEX      = 5,  // device->OpenEx (uses macPath)
        E_OP_SEEK        = 6,  // device->Seek
        E_OP_OP7         = 7,  // device->Op7
        E_OP_DISCONNECT  = 8,  // device->OnDisconnect (no callback)
        E_OP_SHUTDOWN    = 9   // device->Shutdown then the worker thread exits
    };

    struct Operation
    {
        s32               miType;        // opcode (E_OP_*)            (X360 Operation+0)
        char              macPath[256];  // path/filename for opens    (X360 Operation+4)
        Device*           mpDevice;      // target device
        s32               miHandle;      // file handle
        u64               mu64Offset;    // byte offset
        u32               muSize;        // byte count
        void*             mpBuffer;      // read destination / write source
        OperationCallback mpfCallback;   // completion callback
        void*             mpContext;     // callback context (x64 pointer; X360 was a 32-bit value)
        s32               miPriority;    // scheduler priority (X360 Operation+0x12C)

        // The OperationPool scheduler picks the highest-priority queued op.
        s32 GetPriority() const { return miPriority; }
    };
}
