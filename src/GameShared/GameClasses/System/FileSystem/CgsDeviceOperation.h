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

    // CgsDeviceOperation.h:72 (DecFIGS): the public file handle carries both the logical
    // device and that device's private handle. ARTIST passes the pair in r4/r5 and stores it
    // as the 8-byte Operation field at +0x108. On the x64 target the pointer naturally widens.
    struct Handle
    {
        // DecFIGS CgsDeviceOperation.h:67. A device owns the concrete pointee; the
        // file-system layer deliberately treats it as an opaque native-width token.
        typedef void* DeviceHandle;

        void Clear() { mpDevice = nullptr; mDeviceHandle = nullptr; }
        bool IsNull() const { return mpDevice == nullptr; }

        static Handle Make(Device* lpDevice, DeviceHandle lpDeviceHandle)
        {
            Handle lHandle;
            lHandle.mpDevice = lpDevice;
            lHandle.mDeviceHandle = lpDeviceHandle;
            return lHandle;
        }

        Device* GetDevice() const { return mpDevice; }
        DeviceHandle GetDeviceHandle() const { return mDeviceHandle; }

    private:
        Device*      mpDevice;
        DeviceHandle mDeviceHandle;
    };

    // CgsDeviceOperation.h:104 (DecFIGS): result, compound handle, transferred size,
    // producer context. This is also the callback ABI visible in the ARTIST worker asm.
    typedef void (*OperationCallback)(s32 liResult, Handle lHandle, u64 luSize, void* lpContext);

    // miType values — the worker's switch(opcode) arms (PhysicalDeviceThread cases 0..9).
    enum OperationType
    {
        E_OP_READ        = 0,  // device->Read  (after a pre-check)
        E_OP_WRITE       = 1,  // device->Write (after a pre-check)
        E_OP_OPEN        = 2,  // device->Open  (uses macPath)
        E_OP_CLOSE       = 3,  // device->Close
        E_OP_GETFILESIZE     = 4,  // device->GetFileSize
        E_OP_OPEN_DIRECTORY  = 5,  // device->OpenDirectory (uses macPath)
        E_OP_READ_DIRECTORY  = 6,  // device->ReadDirectory
        E_OP_CLOSE_DIRECTORY = 7,  // device->CloseDirectory
        E_OP_DISCONNECT      = 8,  // device->OnDisconnect (no callback)
        E_OP_SHUTDOWN    = 9   // device->Shutdown then the worker thread exits
    };

    struct Operation
    {
        s32               miType;        // opcode (E_OP_*)            (X360 Operation+0)
        char              macPath[256];  // path/filename for opens    (X360 Operation+4)
        u32               muFlags;       // open flags
        Handle            mHandle;       // target device + opaque device handle
        u64               mu64Offset;    // byte offset
        void*             mpReadBuffer;  // read/directory destination
        const void*       mpWriteBuffer; // write source
        u32               muSize;        // byte count / directory capacity
        OperationCallback mpfCallback;   // completion callback
        void*             mpContext;     // callback context (x64 pointer; X360 was a 32-bit value)
        s32               miPriority;    // scheduler priority (X360 Operation+0x12C)

        // The OperationPool scheduler picks the highest-priority queued op.
        s32 GetPriority() const { return miPriority; }
    };
}
