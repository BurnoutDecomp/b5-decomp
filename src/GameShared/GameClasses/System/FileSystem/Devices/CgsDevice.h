#pragma once

// CgsDevice.h — CgsFileSystem::Device, the abstract base of every file device in the resource
// file-I/O subsystem (physical disk device, in-memory device, remap/pass-through device, ...).
//
// SOURCES (X360 ARTIST):
//   Device::CallErrorCallback @ 0x828D65F0  (run the registered error callback, then yield 5ms)
//   the per-op virtual interface is recovered from the DeviceManager worker thread
//   (PhysicalDeviceThread @0x828F1EA8), which dispatches each queued Operation to the device's
//   virtual op and fires the op's completion callback. The worker's opcode->vtable mapping is:
//     op0 READ  -> CheckOp then Read      op1 WRITE -> CheckOp then Write
//     op2 OPEN  -> Open                   op3 CLOSE -> Close
//     op4 GETFILESIZE -> GetFileSize      op5 OPENEX -> OpenEx
//     op6 SEEK  -> Seek                   op7 -> Op7
//     op8 -> Connect (also called once at worker start)   op9 -> Shutdown (worker exits)
//
// The X360 dispatched by raw vtable byte-offset (4-byte PPC slots); that is physically
// impossible on PC (8-byte x64 vtable slots), so the worker here dispatches by NAMED virtual
// and this interface declares those names in the X360 vtable order. Every op has a base default
// ("Not implemented": assert + return -2) so concrete devices override only what they support.
//
// LAYOUT NOTE (asm): CallErrorCallback reads the error-callback fn-ptr at this+8, so the base
// carries mpfErrorCallback (set by DeviceManager::AddPhysicalDevice). Members are by name.

#include "types.hpp"

namespace CgsFileSystem
{
    // A device-level error callback: receives the device error code, returns the (possibly
    // remapped) result the failing op should report.
    typedef int (*ErrorCallback)(int liError);

    class Device
    {
    public:
        virtual ~Device() {}

        // ---- the worker-dispatched async op interface (X360 vtable order) -------------
        // Base defaults are "Not implemented" (assert + -2); concrete devices override.
        virtual int Connect();                                                       // op8 + worker start
        virtual int Open(const char* lpcPath, int liMode, int* lpiOutHandle);        // op2
        virtual int Close(int liHandle);                                             // op3
        virtual int Read(int liHandle, u64 lu64Offset, u32 luSize, void* lpBuffer, int* lpiOutResult);        // op0
        virtual int Write(int liHandle, u64 lu64Offset, u32 luSize, const void* lpBuffer, int* lpiOutResult); // op1
        virtual int CheckOp(int liHandle, u64 lu64Offset, void* lpOut);              // op0/op1 pre-check
        virtual int GetFileSize(int liHandle, u64* lpu64OutSize);                    // op4
        virtual int OpenEx(const char* lpcPath, u32 luA, u32 luB, int* lpiOutC, int* lpiOutResult); // op5
        virtual int Op7(int liHandle);                                              // op7
        virtual int Seek(int liHandle, u64 lu64Offset, int* lpiOutResult);          // op6
        virtual int Shutdown();                                                     // op9

        // ---- non-virtual helpers ----
        // @0x828D65F0. If an error callback is registered, invoke it with liError and remember
        // its result; then yield the calling thread 5ms (so a hammering retry loop does not
        // starve the I/O thread) and return the callback's result (0 when none is set).
        int CallErrorCallback(int liError);

        // DeviceManager::AddPhysicalDevice installs the device's error callback (X360: *(dev+8)
        // = callback, *(dev+4) = 0).
        void SetErrorCallback(ErrorCallback lpfErrorCallback) { mpfErrorCallback = lpfErrorCallback; }

    protected:
        ErrorCallback mpfErrorCallback;  // asm 8(this) in CallErrorCallback; 0 when none installed
    };

} // namespace CgsFileSystem
