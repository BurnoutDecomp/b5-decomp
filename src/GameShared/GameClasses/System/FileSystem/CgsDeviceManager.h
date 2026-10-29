#pragma once

// CgsDeviceManager.h — CgsFileSystem::DeviceManager, the resource file-I/O device registry.
// Reconstructed from BURNOUT_X360_ARTIST.XEX + DecFIGS DWARF (CgsDeviceManager.h).
//
// The DeviceManager owns up to 8 physical devices (each with its own OperationPool + worker
// thread) and 32 virtual devices (name aliases onto a physical device). A producer issues an
// async op (Open/Read/Write/Close/...) to a device's OperationPool; the device's worker thread
// (PhysicalDeviceThread) drains it and dispatches to the device's virtual op.
//
// SOURCES (X360 ARTIST):
//   GetDeviceManager           0x8264AD88   ctor                 0x828F0C90
//   InitInternal               0x828E9788   InitializeDeviceManager 0x828F91E0
//   AddPhysicalDevice          0x828F9320   PhysicalDeviceThread 0x828F1EA8 (worker)
//
// LAYOUT NOTE: the X360 object is 161712 bytes (8 PhysicalDeviceSlots, stride 0x4E90, from
// +8; 32 VirtualDeviceSlots from +160904; the device-list futex at +161416). The slot layout
// is reconstructed faithfully in field order with x64 widths (NOT byte-matched): each physical
// slot = { macPrefix[9]; Device* mpDevice (free when null); OperationPool mOperations;
// EA::Thread::Thread mWorkerThread }. Per the eathread-strategy decision the threading members
// are the (forked) vendor EAThread types.

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT
#include "GameShared/GameClasses/System/FileSystem/CgsDeviceOperationPool.h"    // OperationPool
#include "GameShared/GameClasses/System/FileSystem/Devices/CgsDevice.h"         // Device, ErrorCallback
#include "eathread/eathread_thread.h"                                           // EA::Thread::Thread
#include "eathread/eathread_futex.h"                                            // EA::Thread::Futex

namespace CgsFileSystem
{
    // Completion callback the async device ops invoke when an operation finishes: the result code,
    // the file handle, the byte count, and the producer's context pointer (the issuing AsyncOp).
    typedef OperationCallback AsyncCompletionCallback;

    struct DeviceManager
    {
        // ---- static integral constants (DWARF CgsDeviceManager.h:50-54) ----
        static const s32 KI_MAX_PREFIX_LENGTH       = 8;
        static const s32 KI_MAX_PHYSICAL_DEVICES    = 8;
        static const s32 KI_MAX_VIRTUAL_DEVICES     = 32;
        static const s32 KI_MAX_DEFAULT_PATH_LENGTH = 256;

        // A physical device: a real backing store with its own async-op queue + worker thread.
        struct PhysicalDeviceSlot
        {
            char               macPrefix[KI_MAX_PREFIX_LENGTH + 1];  // X360 slot+0  (free when mpDevice == 0)
            Device*            mpDevice;                             // X360 slot+12
            OperationPool      mOperations;                         // X360 slot+16
            EA::Thread::Thread mWorkerThread;                       // X360 slot+0x4E88
        };

        // A virtual device: a name alias mapping onto a physical device.
        struct VirtualDeviceSlot
        {
            char    macPrefix[KI_MAX_PREFIX_LENGTH + 1];
            Device* mpDevice;
        };

        // ---- lifecycle ----
        DeviceManager();                                       // 0x828F0C90
        void InitInternal();                                   // 0x828E9788
        static bool InitializeDeviceManager(void* lpAllocator);// 0x828F91E0

        // The singleton if already created (null otherwise) — lets a one-time bring-up (e.g.
        // FileSystem::Prepare) avoid re-initialising without tripping GetDeviceManager's assert.
        static DeviceManager* GetIfInitialized() { return mpDeviceManager; }

        // ---- device registry ----
        // Return the physical/virtual device registered under lpcPrefix, or null.
        Device* FindDevice(const char* lpcPrefix);

        // Register a physical device under lpcPrefix and spawn its worker thread (step 2).
        bool AddPhysicalDevice(Device* lpDevice, const char* lpcPrefix, ErrorCallback lpfErrorCallback); // 0x828F9320

        void AddVirtualDevice(Device* lpDevice, const char* lpcPrefix,
                              const char* lpcParent, ErrorCallback lpfErrorCallback);
        void Shutdown(); // ARTIST828F2260: enqueue one shutdown per physical worker.

        // FileSystem::Prepare installs the search path used by unqualified opens.
        void SetDefaultPath(const char* lpcPath);

        // ---- async file operations (the AsyncOp producer API) ----
        // Queue an async open/read/write/close onto the target device's OperationPool; the device's
        // worker thread runs it and then invokes lpfCallback(result, handle, size, lpContext).
        // Recovered from the X360 AsyncOp call sites (Open 0x828F2300 / Read 0x828F23D8 /
        // Close 0x828F24C0). Declared-only here; the op-dispatch bodies land with the DeviceManager
        // reconstruction.
        void Open(const char* lpcFileName, u32 luFlags, AsyncCompletionCallback lpfCallback, void* lpContext, s32 liPriority);
        void Read(Handle lHandle, u64 luPosition, void* lpBuffer, u32 luBufferSize, AsyncCompletionCallback lpfCallback, void* lpContext, s32 liPriority);
        void Write(Handle lHandle, u64 luPosition, const void* lpBuffer, u32 luBufferSize, AsyncCompletionCallback lpfCallback, void* lpContext, s32 liPriority);
        void Close(Handle lHandle, AsyncCompletionCallback lpfCallback, void* lpContext, s32 liPriority);

        // The per-physical-device worker thread (step 2). Drains mOperations and dispatches
        // each op to the slot's device. lpSlotContext is the PhysicalDeviceSlot*.
        static intptr_t PhysicalDeviceThread(void* lpSlotContext); // 0x828F1EA8

        // ---- allocation ----
        // ARTIST828F91E0: allocate through the caller-supplied resource allocator.
        static void* operator new(size_t luSize);
        static void  operator delete(void* lpBlock);

        // ---- instance layout (faithful field order; x64 widths, NOT byte-matched) ----
        s32                miInitializationThread; // X360 +0 (ThreadId; kept for shape)
        bool               mbInitialized;          // X360 +4
        bool               mbReleasing;
        PhysicalDeviceSlot maPhysicalDevices[KI_MAX_PHYSICAL_DEVICES];  // X360 +8
        VirtualDeviceSlot  maVirtualDevices[KI_MAX_VIRTUAL_DEVICES];    // X360 +160904
        EA::Thread::Futex  mDeviceListFutex;                            // X360 +161416
        char               macDefaultPath[KI_MAX_DEFAULT_PATH_LENGTH + 1];

    private:
        static void AssertNativeLayout()
        {
            CGS_STATIC_ASSERT(sizeof(PhysicalDeviceSlot) == 0x4e90, "ARTIST physical slot stride");
            CGS_STATIC_ASSERT(offsetof(PhysicalDeviceSlot, mOperations) == 16, "ARTIST operation pool");
            CGS_STATIC_ASSERT(offsetof(PhysicalDeviceSlot, mWorkerThread) == 0x4e88, "ARTIST physical worker");
            CGS_STATIC_ASSERT(offsetof(DeviceManager, maVirtualDevices) == 160904, "ARTIST virtual device slots");
            CGS_STATIC_ASSERT(offsetof(DeviceManager, mDeviceListFutex) == 161416, "ARTIST device-list lock");
            CGS_STATIC_ASSERT(sizeof(DeviceManager) == 161712, "ARTIST device manager allocation");
        }
        // ---- statics (CgsDeviceManager.cpp) ----
        static DeviceManager* mpDeviceManager;   // X360 dword_830EA400 (singleton)
        static void*          mpAllocator;       // X360 off_830EA404

        // CgsFileSystem::GetDeviceManager() is a namespace-scope free function that reads the
        // private singleton (DWARF CgsDeviceManager.h:176).
        friend DeviceManager* GetDeviceManager();
    };

    // Namespace-scope free function (DWARF CgsDeviceManager.h:176).
    DeviceManager* GetDeviceManager();

} // namespace CgsFileSystem
