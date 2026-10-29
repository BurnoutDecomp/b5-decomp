// CgsDeviceManager.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   GetDeviceManager        0x8264AD88   ctor                    0x828F0C90
//   InitInternal            0x828E9788   InitializeDeviceManager 0x828F91E0
// (AddPhysicalDevice 0x828F9320 + PhysicalDeviceThread 0x828F1EA8 land in the worker step.)

#include "GameShared/GameClasses/System/FileSystem/CgsDeviceManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "rw/rwcore_structs.h"

#include <cstring>   // strcmp / strncpy / strlen

// Forked-vendor EAThread X360-ABI sleep (pointer-arg; the worker polls 15ms when the queue is
// empty). Declared locally rather than via the vendor eathread.h (whose ThreadSleep takes a
// ThreadTime); the body lives in vendor/EAThread/source/pc/eathread_x360align.cpp.
namespace EA { namespace Thread { unsigned int ThreadSleep(const unsigned int* pMilliseconds); } }

namespace CgsFileSystem
{
    // ---- statics ----
    // X360 globals: dword_830EA400 (singleton), off_830EA404 (allocator).
    DeviceManager* DeviceManager::mpDeviceManager = 0;
    void*          DeviceManager::mpAllocator     = 0;

    // CgsFileSystem::GetDeviceManager @ 0x8264AD88
    DeviceManager* GetDeviceManager()
    {
        CGS_ASSERT(DeviceManager::mpDeviceManager != 0, "Device manager not initialized\n");
        return DeviceManager::mpDeviceManager;
    }

    // Native allocation uses the resource allocator installed before new is called.
    void* DeviceManager::operator new(size_t luSize)
    {
        rw::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size = luSize;
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 16;
        return static_cast<rw::IResourceAllocator*>(mpAllocator)->DoAllocate(lDescriptor, 0).m_baseResources[0];
    }
    void DeviceManager::operator delete(void* lpBlock)
    {
        rw::Resource lResource;
        lResource.m_baseResources[0] = lpBlock;
        static_cast<rw::IResourceAllocator*>(mpAllocator)->DoFree(lResource);
    }

    // ctor @ 0x828F0C90.
    // The X360 ctor, per physical slot, RtlInitializeCriticalSection'd the OperationPool lock,
    // default-constructed its Semaphore and Thread, then initialised the device-list futex. Here
    // the slot's Semaphore + Thread and the device-list Futex are constructed by their own C++
    // member ctors; the remaining per-slot OperationPool bring-up (lock + slot table) is done by
    // OperationPool::Construct.
    DeviceManager::DeviceManager()
        : miInitializationThread(0)
        , mbInitialized(false)
        , mbReleasing(false)
    {
        for (s32 liIndex = 0; liIndex < KI_MAX_PHYSICAL_DEVICES; ++liIndex)
            maPhysicalDevices[liIndex].mOperations.Construct();
        macDefaultPath[0] = 0;
    }

    // InitInternal @ 0x828E9788 — under the device-list lock, mark every physical + virtual slot
    // free (mpDevice = 0) and flag the manager initialised.
    void DeviceManager::InitInternal()
    {
        mDeviceListFutex.Lock();

        for (s32 liIndex = 0; liIndex < KI_MAX_PHYSICAL_DEVICES; ++liIndex)
            maPhysicalDevices[liIndex].mpDevice = 0;
        for (s32 liIndex = 0; liIndex < KI_MAX_VIRTUAL_DEVICES; ++liIndex)
            maVirtualDevices[liIndex].mpDevice = 0;

        mbInitialized = true;
        mbReleasing   = false;

        mDeviceListFutex.Unlock();
    }

    // InitializeDeviceManager @ 0x828F91E0 — create + init the singleton. Asserts it does not
    // already exist and that an allocator was supplied; stores the allocator, allocates +
    // constructs the DeviceManager, then runs InitInternal.
    bool DeviceManager::InitializeDeviceManager(void* lpAllocator)
    {
        CGS_ASSERT(mpDeviceManager == 0, "Device manager already exists\n");
        CGS_ASSERT(lpAllocator != 0, "Allocator required\n");

        mpAllocator     = lpAllocator;
        mpDeviceManager = new DeviceManager();
        if (mpDeviceManager)
            mpDeviceManager->InitInternal();
        return mpDeviceManager != 0;
    }

    // FindDevice — return the device registered under lpcPrefix (physical first, then virtual),
    // or null. Used by AddPhysicalDevice (duplicate-name guard) and the open path.
    Device* DeviceManager::FindDevice(const char* lpcPrefix)
    {
        if (!lpcPrefix)
            return 0;

        for (s32 liIndex = 0; liIndex < KI_MAX_PHYSICAL_DEVICES; ++liIndex)
            if (maPhysicalDevices[liIndex].mpDevice &&
                strcmp(maPhysicalDevices[liIndex].macPrefix, lpcPrefix) == 0)
                return maPhysicalDevices[liIndex].mpDevice;

        for (s32 liIndex = 0; liIndex < KI_MAX_VIRTUAL_DEVICES; ++liIndex)
            if (maVirtualDevices[liIndex].mpDevice &&
                strcmp(maVirtualDevices[liIndex].macPrefix, lpcPrefix) == 0)
                return maVirtualDevices[liIndex].mpDevice;

        return 0;
    }

    // AddPhysicalDevice @0x828F9320 — register lpDevice under lpcPrefix (asserting the name is
    // free and a slot is available), install its error callback, then spawn the worker thread.
    bool DeviceManager::AddPhysicalDevice(Device* lpDevice, const char* lpcPrefix, ErrorCallback lpfErrorCallback)
    {
        mDeviceListFutex.Lock();

        CGS_ASSERT(FindDevice(lpcPrefix) == 0, "Device name already in use\n");

        s32 liSlot = -1;
        for (s32 liIndex = 0; liIndex < KI_MAX_PHYSICAL_DEVICES; ++liIndex)
            if (!maPhysicalDevices[liIndex].mpDevice) { liSlot = liIndex; break; }
        CGS_ASSERT(liSlot >= 0, "No free physical devices\n");
        if (liSlot < 0) { mDeviceListFutex.Unlock(); return false; }

        PhysicalDeviceSlot& lrSlot = maPhysicalDevices[liSlot];

        CGS_ASSERT(strlen(lpcPrefix) <= static_cast<size_t>(KI_MAX_PREFIX_LENGTH), "Device prefix too long\n");
        strncpy(lrSlot.macPrefix, lpcPrefix, KI_MAX_PREFIX_LENGTH);
        lrSlot.macPrefix[KI_MAX_PREFIX_LENGTH] = 0;

        // Install the device + its error callback (X360: *(dev+8)=callback, *(dev+4)=0). The
        // slot's OperationPool was already brought up by the DeviceManager ctor.
        lrSlot.mpDevice = lpDevice;
        lpDevice->SetParent(0);
        lpDevice->SetErrorCallback(lpfErrorCallback);

        mDeviceListFutex.Unlock();

        // Spawn the per-device worker, named "CgsFileSystem:<prefix>" (X360 SPrintf + Thread::Begin).
        char lacThreadName[64];
        CgsCore::SPrintf(lacThreadName, sizeof(lacThreadName), "CgsFileSystem:%s", lpcPrefix);
        EA::Thread::ThreadParameters lParams;
        lParams.mpName = lacThreadName;
        lParams.mnPriority = 350;
        lParams.mnProcessor = 0;
        lParams.mbSuspended = false;
        lrSlot.mWorkerThread.Begin(PhysicalDeviceThread, &lrSlot, &lParams,
            EA::Thread::Thread::GetGlobalRunnableFunctionUserWrapper());
        return true;
    }

    // ARTIST828F0CF8..828F11B4 (raw assembly; export hole).
    void DeviceManager::AddVirtualDevice(Device* lpDevice, const char* lpcPrefix,
                                         const char* lpcParent, ErrorCallback lpfErrorCallback)
    {
        mDeviceListFutex.Lock();
        CGS_ASSERT(FindDevice(lpcPrefix) == 0, "Device name already in use\n");
        Device* lpParent = FindDevice(lpcParent);
        CGS_ASSERT(lpParent != 0, "Parent device not found\n");
        s32 liSlot = 0;
        while (liSlot < KI_MAX_VIRTUAL_DEVICES && maVirtualDevices[liSlot].mpDevice) ++liSlot;
        CGS_ASSERT(liSlot < KI_MAX_VIRTUAL_DEVICES, "No free virtual devices\n");
        CgsCore::StrCpy(maVirtualDevices[liSlot].macPrefix,
                       sizeof(maVirtualDevices[liSlot].macPrefix), lpcPrefix);
        maVirtualDevices[liSlot].mpDevice = lpDevice;
        lpDevice->SetParent(lpParent);
        lpDevice->SetErrorCallback(lpfErrorCallback);
        Device* lpPhysical = lpDevice->GetPhysicalParent();
        s32 liPhysical = 0;
        while (liPhysical < KI_MAX_PHYSICAL_DEVICES &&
               maPhysicalDevices[liPhysical].mpDevice != lpPhysical) ++liPhysical;
        CGS_ASSERT(liPhysical < KI_MAX_PHYSICAL_DEVICES, "Physical device not found\n");
        Operation lOperation = Operation();
        lOperation.miType = E_OP_DISCONNECT;
        lOperation.mHandle = Handle::Make(lpDevice, 0);
        lOperation.miPriority = 0x7fffffff;
        maPhysicalDevices[liPhysical].mOperations.AddOperation(&lOperation);
        mDeviceListFutex.Unlock();
    }

    // ARTIST828F2260: shutdown is asynchronous; the worker retires after op9.
    void DeviceManager::Shutdown()
    {
        mDeviceListFutex.Lock();
        for (s32 li = 0; li < KI_MAX_PHYSICAL_DEVICES; ++li)
        {
            if (!maPhysicalDevices[li].mpDevice) continue;
            Operation lOperation = Operation();
            lOperation.miType = E_OP_SHUTDOWN;
            lOperation.mHandle = Handle::Make(maPhysicalDevices[li].mpDevice, 0);
            lOperation.miPriority = 0x7fffffff;
            maPhysicalDevices[li].mOperations.AddOperation(&lOperation);
        }
        mDeviceListFutex.Unlock();
    }

    void DeviceManager::SetDefaultPath(const char* lpcPath)
    {
        CGS_ASSERT(lpcPath != 0, "Default path required\n");
        if (!lpcPath)
            return;
        const size_t luLength = strlen(lpcPath);
        CGS_ASSERT(luLength < sizeof(macDefaultPath), "Default path is too long\n");
        strncpy(macDefaultPath, lpcPath, sizeof(macDefaultPath) - 1);
        macDefaultPath[sizeof(macDefaultPath) - 1] = 0;
    }

    // Open @0x828F11B8. Resolve the optional "prefix:" selector (or prepend the configured
    // default path), retain the selected logical device in the operation, and enqueue it on
    // the root physical device's worker queue.
    void DeviceManager::Open(const char* lpcFileName, u32 luFlags,
                             AsyncCompletionCallback lpfCallback, void* lpContext,
                             s32 liPriority)
    {
        CGS_ASSERT(lpcFileName != 0, "File name required\n");

        const char* lpcColon = strchr(lpcFileName, ':');
        if (!lpcColon)
        {
            char lacFullPath[256];
            const size_t luDefaultLength = strlen(macDefaultPath);
            const size_t luNameLength = strlen(lpcFileName);
            CGS_ASSERT(luDefaultLength + luNameLength < sizeof(lacFullPath) - 1,
                       "Default path and file name are too long\n");
            memcpy(lacFullPath, macDefaultPath, luDefaultLength);
            memcpy(lacFullPath + luDefaultLength, lpcFileName, luNameLength + 1);
            Open(lacFullPath, luFlags, lpfCallback, lpContext, liPriority);
            return;
        }

        mDeviceListFutex.Lock();

        const size_t luNameLength = strlen(lpcFileName);
        CGS_ASSERT(luNameLength < 256, "File name is too long\n");

        char lacFileName[256];
        memcpy(lacFileName, lpcFileName, luNameLength + 1);

        const size_t luPrefixLength = static_cast<size_t>(lpcColon - lpcFileName);
        lacFileName[luPrefixLength] = 0;
        Device* lpDevice = FindDevice(lacFileName);
        CGS_ASSERT(lpDevice != 0, "Could not find device\n");

        Device* lpPhysicalDevice = lpDevice ? lpDevice->GetPhysicalParent() : 0;
        s32 liDeviceIndex = 0;
        while (liDeviceIndex < KI_MAX_PHYSICAL_DEVICES &&
               maPhysicalDevices[liDeviceIndex].mpDevice != lpPhysicalDevice)
            ++liDeviceIndex;
        CGS_ASSERT(liDeviceIndex < KI_MAX_PHYSICAL_DEVICES,
                   "Could not find physical device\n");

        if (lpDevice && liDeviceIndex < KI_MAX_PHYSICAL_DEVICES)
        {
            Operation lOperation = {};
            lOperation.miType = E_OP_OPEN;
            strncpy(lOperation.macPath, lacFileName + luPrefixLength + 1,
                    sizeof(lOperation.macPath) - 1);
            lOperation.macPath[sizeof(lOperation.macPath) - 1] = 0;
            lOperation.muFlags = luFlags;
            lOperation.mHandle = Handle::Make(lpDevice, 0);
            lOperation.mpfCallback = lpfCallback;
            lOperation.mpContext = lpContext;
            lOperation.miPriority = liPriority;
            maPhysicalDevices[liDeviceIndex].mOperations.AddOperation(&lOperation);
        }

        mDeviceListFutex.Unlock();
    }

    // Read/Write/Close @0x828F1AA0/0x828F1BF8/0x828F1D58. The compound Handle selects
    // both the logical device receiving the op and the physical queue that services it.
    void DeviceManager::Read(Handle lHandle, u64 luPosition, void* lpBuffer, u32 luBufferSize,
                             AsyncCompletionCallback lpfCallback, void* lpContext,
                             s32 liPriority)
    {
        mDeviceListFutex.Lock();
        Device* lpDevice = lHandle.GetDevice();
        Device* lpPhysicalDevice = lpDevice ? lpDevice->GetPhysicalParent() : 0;
        s32 liDeviceIndex = 0;
        while (liDeviceIndex < KI_MAX_PHYSICAL_DEVICES &&
               maPhysicalDevices[liDeviceIndex].mpDevice != lpPhysicalDevice)
            ++liDeviceIndex;
        CGS_ASSERT(liDeviceIndex < KI_MAX_PHYSICAL_DEVICES,
                   "Could not find physical device\n");

        if (lpDevice && liDeviceIndex < KI_MAX_PHYSICAL_DEVICES)
        {
            Operation lOperation = {};
            lOperation.miType = E_OP_READ;
            lOperation.mHandle = lHandle;
            lOperation.mu64Offset = luPosition;
            lOperation.muSize = luBufferSize;
            lOperation.mpReadBuffer = lpBuffer;
            lOperation.mpfCallback = lpfCallback;
            lOperation.mpContext = lpContext;
            lOperation.miPriority = liPriority;
            maPhysicalDevices[liDeviceIndex].mOperations.AddOperation(&lOperation);
        }
        mDeviceListFutex.Unlock();
    }

    void DeviceManager::Write(Handle lHandle, u64 luPosition, const void* lpBuffer, u32 luBufferSize,
                              AsyncCompletionCallback lpfCallback, void* lpContext,
                              s32 liPriority)
    {
        mDeviceListFutex.Lock();
        Device* lpDevice = lHandle.GetDevice();
        Device* lpPhysicalDevice = lpDevice ? lpDevice->GetPhysicalParent() : 0;
        s32 liDeviceIndex = 0;
        while (liDeviceIndex < KI_MAX_PHYSICAL_DEVICES &&
               maPhysicalDevices[liDeviceIndex].mpDevice != lpPhysicalDevice)
            ++liDeviceIndex;
        CGS_ASSERT(liDeviceIndex < KI_MAX_PHYSICAL_DEVICES,
                   "Could not find physical device\n");

        if (lpDevice && liDeviceIndex < KI_MAX_PHYSICAL_DEVICES)
        {
            Operation lOperation = {};
            lOperation.miType = E_OP_WRITE;
            lOperation.mHandle = lHandle;
            lOperation.mu64Offset = luPosition;
            lOperation.muSize = luBufferSize;
            lOperation.mpWriteBuffer = lpBuffer;
            lOperation.mpfCallback = lpfCallback;
            lOperation.mpContext = lpContext;
            lOperation.miPriority = liPriority;
            maPhysicalDevices[liDeviceIndex].mOperations.AddOperation(&lOperation);
        }
        mDeviceListFutex.Unlock();
    }

    void DeviceManager::Close(Handle lHandle, AsyncCompletionCallback lpfCallback,
                              void* lpContext, s32 liPriority)
    {
        mDeviceListFutex.Lock();
        Device* lpDevice = lHandle.GetDevice();
        Device* lpPhysicalDevice = lpDevice ? lpDevice->GetPhysicalParent() : 0;
        s32 liDeviceIndex = 0;
        while (liDeviceIndex < KI_MAX_PHYSICAL_DEVICES &&
               maPhysicalDevices[liDeviceIndex].mpDevice != lpPhysicalDevice)
            ++liDeviceIndex;
        CGS_ASSERT(liDeviceIndex < KI_MAX_PHYSICAL_DEVICES,
                   "Could not find physical device\n");

        if (lpDevice && liDeviceIndex < KI_MAX_PHYSICAL_DEVICES)
        {
            Operation lOperation = {};
            lOperation.miType = E_OP_CLOSE;
            lOperation.mHandle = lHandle;
            lOperation.mpfCallback = lpfCallback;
            lOperation.mpContext = lpContext;
            lOperation.miPriority = liPriority;
            maPhysicalDevices[liDeviceIndex].mOperations.AddOperation(&lOperation);
        }
        mDeviceListFutex.Unlock();
    }

    // PhysicalDeviceThread @0x828F1EA8 — the per-physical-device worker. It drains the slot's
    // OperationPool (15ms poll-sleep when empty) and dispatches each op to the slot's device,
    // then fires the op's completion callback. The X360 dispatched by raw vtable byte-offset
    // (4-byte PPC slots) — impossible on PC (8-byte x64 vtable slots) — so this dispatches by
    // NAMED virtual (see CgsDevice.h for the opcode->method mapping).
    intptr_t DeviceManager::PhysicalDeviceThread(void* lpSlotContext)
    {
        PhysicalDeviceSlot* lpSlot = static_cast<PhysicalDeviceSlot*>(lpSlotContext);
        OperationPool*      lpPool = &lpSlot->mOperations;

        // Worker-start device hook (X360: (***(slot+12))(*(slot+12)) == mpDevice->vtable[0]).
        if (lpSlot->mpDevice)
            lpSlot->mpDevice->Connect();

        for (;;)
        {
            Operation lOp;
            while (!lpPool->ReadOperation(&lOp))
            {
                unsigned int luPollMs = 15;   // X360: HIDWORD(v5)=15; EA::Thread::ThreadSleep(&v5)
                EA::Thread::ThreadSleep(&luPollMs);
            }

            Device* lpDevice = lOp.mHandle.GetDevice();
            switch (lOp.miType)
            {
            case E_OP_READ:
            {
                u32 liResultSize = 0;
                u64 lu64Position = 0;
                int liResult = lpDevice->Seek(lOp.mHandle.GetDeviceHandle(), lOp.mu64Offset,
                                              &lu64Position);
                if (liResult == 0)
                    liResult = lpDevice->Read(lOp.mHandle.GetDeviceHandle(), lOp.mpReadBuffer,
                                              lOp.muSize, &liResultSize);
                if (lOp.mpfCallback)
                    lOp.mpfCallback(liResult, lOp.mHandle,
                                    static_cast<u64>(liResultSize), lOp.mpContext);
                break;
            }
            case E_OP_WRITE:
            {
                u32 liResultSize = 0;
                u64 lu64Position = 0;
                int liResult = lpDevice->Seek(lOp.mHandle.GetDeviceHandle(), lOp.mu64Offset,
                                              &lu64Position);
                if (liResult == 0)
                    liResult = lpDevice->Write(lOp.mHandle.GetDeviceHandle(), lOp.mpWriteBuffer,
                                               lOp.muSize, &liResultSize);
                if (lOp.mpfCallback)
                    lOp.mpfCallback(liResult, lOp.mHandle,
                                    static_cast<u64>(liResultSize), lOp.mpContext);
                break;
            }
            case E_OP_OPEN:
            {
                Handle::DeviceHandle lpDeviceHandle = 0;
                const int liResult = lpDevice->Open(lOp.macPath, lOp.muFlags, &lpDeviceHandle);
                if (lOp.mpfCallback)
                    lOp.mpfCallback(liResult, Handle::Make(lpDevice, lpDeviceHandle), 0,
                                    lOp.mpContext);
                break;
            }
            case E_OP_CLOSE:
            {
                const int liResult = lpDevice->Close(lOp.mHandle.GetDeviceHandle());
                Handle lClosedHandle;
                lClosedHandle.Clear();
                if (lOp.mpfCallback)
                    lOp.mpfCallback(liResult, lClosedHandle, 0,
                                    lOp.mpContext);
                break;
            }
            case E_OP_GETFILESIZE:
            {
                u64 lu64Size = 0;
                const int liResult = lpDevice->GetFileSize(lOp.mHandle.GetDeviceHandle(), &lu64Size);
                if (lOp.mpfCallback)
                    lOp.mpfCallback(liResult, lOp.mHandle, lu64Size,
                                    lOp.mpContext);
                break;
            }
            case E_OP_OPEN_DIRECTORY:
            {
                u32 luCount = 0;
                Handle::DeviceHandle lpDirectoryHandle = lOp.mHandle.GetDeviceHandle();
                const int liResult = lpDevice->OpenDirectory(lOp.macPath, lOp.mpReadBuffer,
                                                              lOp.muSize, &luCount,
                                                              &lpDirectoryHandle);
                if (lOp.mpfCallback)
                    lOp.mpfCallback(liResult, Handle::Make(lpDevice, lpDirectoryHandle),
                                    static_cast<u64>(luCount), lOp.mpContext);
                break;
            }
            case E_OP_READ_DIRECTORY:
            {
                u32 luCount = 0;
                const int liResult = lpDevice->ReadDirectory(lOp.mHandle.GetDeviceHandle(),
                                                              lOp.mpReadBuffer, lOp.muSize,
                                                              &luCount);
                if (lOp.mpfCallback)
                    lOp.mpfCallback(liResult, lOp.mHandle, static_cast<u64>(luCount),
                                    lOp.mpContext);
                break;
            }
            case E_OP_CLOSE_DIRECTORY:
            {
                const int liResult = lpDevice->CloseDirectory(lOp.mHandle.GetDeviceHandle());
                Handle lClosedHandle;
                lClosedHandle.Clear();
                if (lOp.mpfCallback)
                    lOp.mpfCallback(liResult, lClosedHandle, 0, lOp.mpContext);
                break;
            }
            case E_OP_DISCONNECT:
                lpDevice->Connect();   // X360 op8 -> vtable[0] (no completion callback)
                break;
            case E_OP_SHUTDOWN:
                lpDevice->Shutdown();
                return 0;              // the worker thread exits
            default:
                CGS_ASSERT(false, "Unrecognised operation\n");
                break;
            }
        }
    }

} // namespace CgsFileSystem
