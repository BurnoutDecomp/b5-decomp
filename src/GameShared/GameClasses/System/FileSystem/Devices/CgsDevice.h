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
#include "GameShared/GameClasses/System/FileSystem/CgsDeviceOperation.h"

namespace CgsFileSystem
{
    // A device-level error callback: receives the device error code, returns the (possibly
    // remapped) result the failing op should report.
    typedef int (*ErrorCallback)(int liError);

    class Device
    {
    public:
        Device() : mpParent(nullptr), mpfErrorCallback(nullptr) {}
        virtual ~Device() {}

        // ---- the worker-dispatched async op interface (X360 vtable order) -------------
        // Base defaults are "Not implemented" (assert + -2); concrete devices override.
        virtual int Connect();                                                       // op8 + worker start
        virtual int Open(const char* lpcPath, u32 luMode, Handle::DeviceHandle* lppOutHandle); // op2
        virtual int Close(Handle::DeviceHandle lpHandle);                            // op3
        virtual int Read(Handle::DeviceHandle lpHandle, void* lpBuffer, u32 luSize,
                         u32* lpuOutResult);                                          // op0
        virtual int Write(Handle::DeviceHandle lpHandle, const void* lpBuffer, u32 luSize,
                          u32* lpuOutResult);                                         // op1
        virtual int Seek(Handle::DeviceHandle lpHandle, u64 lu64Offset,
                         u64* lpu64OutPosition);                                      // op0/op1 pre-seek
        virtual int GetFileSize(Handle::DeviceHandle lpHandle, u64* lpu64OutSize);   // op4

        // ---- directory enumeration interface (X360 base defaults @0x828DDEE0/F70/000) ----
        // Not part of the worker async-op opcode table; the directory subsystem calls these on
        // the device directly. The base supplies "Not implemented" defaults (assert + -2);
        // concrete devices that can walk a directory (the physical disc device) override them.
        // luMaxEntries is the caller's entry-buffer capacity; *lpiOutCount returns how many were
        // written. lpiOutHandle (Open) yields a find-handle Read/CloseDirectory then use.
        virtual int OpenDirectory(const char* lpcPath, void* lpEntryBuffer, u32 luMaxEntries,
                                  u32* lpuOutCount, Handle::DeviceHandle* lppOutHandle); // op5
        virtual int CloseDirectory(Handle::DeviceHandle lpHandle);                    // op7
        virtual int ReadDirectory(Handle::DeviceHandle lpHandle, void* lpEntryBuffer,
                                  u32 luMaxEntries, u32* lpuOutCount);                 // op6
        virtual int Shutdown();                                                       // op9

        // ---- non-virtual helpers ----
        // @0x828D65F0. If an error callback is registered, invoke it with liError and remember
        // its result; then yield the calling thread 5ms (so a hammering retry loop does not
        // starve the I/O thread) and return the callback's result (0 when none is set).
        int CallErrorCallback(int liError);

        // DeviceManager::AddPhysicalDevice installs the device's error callback (X360: *(dev+8)
        // = callback, *(dev+4) = 0).
        void SetErrorCallback(ErrorCallback lpfErrorCallback) { mpfErrorCallback = lpfErrorCallback; }

        // CgsDevice.h:113/127 (DecFIGS). Virtual devices form a short parent chain;
        // DeviceManager queues their work on the root physical device's worker while retaining
        // the logical device in Handle so its overrides still receive the operation.
        Device* GetParent() const { return mpParent; }
        Device* GetPhysicalParent()
        {
            Device* lpPhysical = this;
            while (lpPhysical->mpParent)
                lpPhysical = lpPhysical->mpParent;
            return lpPhysical;
        }
        void SetParent(Device* lpParent) { mpParent = lpParent; }

    protected:
        Device*       mpParent;         // X360 this+4 (32-bit build)
        ErrorCallback mpfErrorCallback;  // asm 8(this) in CallErrorCallback; 0 when none installed
    };

} // namespace CgsFileSystem
