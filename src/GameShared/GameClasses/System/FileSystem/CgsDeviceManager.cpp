// CgsDeviceManager.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   GetDeviceManager        0x8264AD88   ctor                    0x828F0C90
//   InitInternal            0x828E9788   InitializeDeviceManager 0x828F91E0
// (AddPhysicalDevice 0x828F9320 + PhysicalDeviceThread 0x828F1EA8 land in the worker step.)

#include "GameShared/GameClasses/System/FileSystem/CgsDeviceManager.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include <new>
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstdlib>   // malloc / free
#include <cstring>   // strcmp / strncpy / strlen
#include <cstdio>    // snprintf

// Forked-vendor EAThread X360-ABI sleep (pointer-arg; the worker polls 15ms when the queue is
// empty). Declared locally rather than via the vendor eathread.h (whose ThreadSleep takes a
// ThreadTime); the body lives in vendor/EAThread/source/pc/eathread_x360align.cpp.
namespace EA { namespace Thread { unsigned int ThreadSleep(const unsigned int* pMilliseconds); } }

namespace CgsFileSystem
{
namespace
{
    // Original CgsStringUtils.h:65 copy expansion. Preserve the assert and
    // terminator-inclusive copy; no truncation or alternate path is introduced.
    void CopyConstructionString(char* lpcDest, u32 luBytes, const char* lpcSource)
    {
        const size_t luLength = std::strlen(lpcSource);
        if (luLength >= luBytes)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::MessageBufferPC lBuffer;
            CgsDev::StrStream lStream(lBuffer.GetBuffer(), lBuffer.GetSize());
            lStream << "String " << lpcSource << " is too long. Buffer size = "
                    << luBytes << ", string length = " << static_cast<u32>(luLength) << "\n";
            CgsDev::Assert::FireAssert(lBuffer.GetBuffer(),
                "..\\..\\..\\GameShared\\GameClasses\\Core/CgsStringUtils.h", 65);
            CgsDev::Assert::EndAssert();
        }
        std::memcpy(lpcDest, lpcSource, luLength + 1);
    }

    // Common expansion of the original manager's formatted assertions.
    void AssertDeviceConstruction(bool lbValid, const char* lpcMessage, s32 liLine)
    {
        if (!lbValid)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::MessageBufferPC lBuffer;
            CgsDev::StrStream lStream(lBuffer.GetBuffer(), lBuffer.GetSize());
            lStream << lpcMessage;
            CgsDev::Assert::FireAssert(lBuffer.GetBuffer(),
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\filesystem\\CgsDeviceManager.cpp",
                liLine);
            CgsDev::Assert::EndAssert();
        }
    }

}
    // ARTIST828F0CF8..11B4. Register the logical device on its actual physical
    // worker, then queue opcode8 (the existing worker's Connect/Init case).
    void DeviceManager::AddVirtualDevice(Device* lpDevice, const char* lpcPrefix,
                                          const char* lpcParent, ErrorCallback lpfErrorCallback)
    {
        mDeviceListFutex.Lock();
        const auto AssertDevice = [](bool lbValid, const char* lpcMessage, s32 liLine)
        {
            if (!lbValid)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::MessageBufferPC lBuffer;
                CgsDev::StrStream lStream(lBuffer.GetBuffer(), lBuffer.GetSize());
                lStream << lpcMessage;
                CgsDev::Assert::FireAssert(lBuffer.GetBuffer(),
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\filesystem\\CgsDeviceManager.cpp",
                    liLine);
                CgsDev::Assert::EndAssert();
            }
        };
        AssertDevice(FindDevice(lpcPrefix) == nullptr, "Device name already in use\n", 196);
        Device* const lpParent = FindDevice(lpcParent);
        AssertDevice(lpParent != nullptr, "Parent device not found\n", 200);
        s32 liSlot = 0;
        while (liSlot < KI_MAX_VIRTUAL_DEVICES && maVirtualDevices[liSlot].mpDevice != nullptr)
            ++liSlot;
        AssertDevice(liSlot < KI_MAX_VIRTUAL_DEVICES, "No free virtual devices\n", 211);
        CopyConstructionString(maVirtualDevices[liSlot].macPrefix,
                               KI_MAX_PREFIX_LENGTH + 1, lpcPrefix);
        maVirtualDevices[liSlot].mpDevice = lpDevice;
        lpDevice->SetParent(lpParent);
        lpDevice->SetErrorCallback(lpfErrorCallback);
        Device* const lpPhysical = lpParent ? lpParent->GetPhysicalParent() : lpDevice;
        s32 liPhysical = 0;
        while (liPhysical < KI_MAX_PHYSICAL_DEVICES
            && maPhysicalDevices[liPhysical].mpDevice != lpPhysical)
            ++liPhysical;
        AssertDevice(liPhysical < KI_MAX_PHYSICAL_DEVICES, "Could not find physical device\n", 232);
        Operation lOperation;
        lOperation.miType = E_OP_DISCONNECT; // existing name; ARTIST opcode8 initializes the logical device
        lOperation.macPath[0] = '\0';
        lOperation.muFlags = 0;
        lOperation.mHandle = Handle::Make(lpDevice, nullptr);
        lOperation.mu64Offset = 0;
        lOperation.mpReadBuffer = nullptr;
        lOperation.mpWriteBuffer = nullptr;
        lOperation.muSize = 0;
        lOperation.mpfCallback = nullptr;
        lOperation.mpContext = nullptr;
        lOperation.miPriority = 0x7FFFFFFF;
        maPhysicalDevices[liPhysical].mOperations.AddOperation(&lOperation);
        mDeviceListFutex.Unlock();
    }

    // ---- statics ----
    // X360 globals: dword_830EA400 (singleton), off_830EA404 (allocator).
    DeviceManager* DeviceManager::mpDeviceManager = nullptr;
    rw::IResourceAllocator* DeviceManager::mpAllocator = nullptr;

    // CgsFileSystem::GetDeviceManager @ 0x8264AD88
    DeviceManager* GetDeviceManager()
    {
        CGS_ASSERT(DeviceManager::mpDeviceManager != nullptr, "Device manager not initialized\n");
        return DeviceManager::mpDeviceManager;
    }

    // ARTIST828EB9C8: lane zero, native object size, alignment16, null name.
    void* DeviceManager::operator new(size_t luSize)
    {
        return mpAllocator->AllocateMemoryResource(static_cast<u32>(luSize), 16, nullptr);
    }

    // FLAG PC-platform leaf: retain the existing native malloc cleanup only.
    // The original manager owner is arena-lived; its release/delete chain is not activated here.
    void DeviceManager::operator delete(void* lpBlock) { free(lpBlock); }

    // ARTIST828F0C90. OperationPool constructs its lock then its deferred semaphore;
    // the eight Thread subobjects and final device-list Futex construct themselves.
    DeviceManager::DeviceManager()
    {
    }

    // ARTIST828E9788: clear registrations and the first default-path byte under the lock.
    void DeviceManager::InitInternal()
    {
        mDeviceListFutex.Lock();
        for (s32 liIndex = 0; liIndex < KI_MAX_PHYSICAL_DEVICES; ++liIndex)
            maPhysicalDevices[liIndex].mpDevice = nullptr;
        for (s32 liIndex = 0; liIndex < KI_MAX_VIRTUAL_DEVICES; ++liIndex)
            maVirtualDevices[liIndex].mpDevice = nullptr;
        mbInitialized = true;
        macDefaultPath[0] = '\0';
        mDeviceListFutex.Unlock();
    }

    // ARTIST828F91E0; DecFIGS confirms void and the real RW allocator type.
    void DeviceManager::InitializeDeviceManager(rw::IResourceAllocator* lpAllocator)
    {
        AssertDeviceConstruction(mpDeviceManager == nullptr, "Device manager already exists\n", 44);
        AssertDeviceConstruction(lpAllocator != nullptr, "Allocator required\n", 45);
        mpAllocator = lpAllocator;
        void* const lpMemory = DeviceManager::operator new(sizeof(DeviceManager));
        mpDeviceManager = lpMemory ? ::new(lpMemory) DeviceManager() : nullptr;
        // The original deliberately proceeds to InitInternal even after failed allocation.
        mpDeviceManager->InitInternal();
    }

    // FLAG PC-platform leaf: existing native singleton ownership is malloc/process lifetime.
    // Keep its actual queue initialization and scalar values before publishing the object;
    // no fabricated RW allocator is supplied to the original initialization interface.
    bool DeviceManager::InitializeDeviceManagerNativePC()
    {
        CGS_ASSERT(mpDeviceManager == nullptr, "Device manager already exists\n");
        void* const lpMemory = malloc(sizeof(DeviceManager));
        DeviceManager* const lpManager = lpMemory ? ::new(lpMemory) DeviceManager() : nullptr;
        if (lpManager)
        {
            lpManager->miInitializationThread = 0;
            lpManager->mbInitialized = false;
            lpManager->mbReleasing = false;
            for (s32 liIndex = 0; liIndex < KI_MAX_PHYSICAL_DEVICES; ++liIndex)
                lpManager->maPhysicalDevices[liIndex].mOperations.ConstructNativePC();
            lpManager->macDefaultPath[0] = '\0';
        }
        mpDeviceManager = lpManager;
        if (mpDeviceManager)
            mpDeviceManager->InitInternal();
        return mpDeviceManager != nullptr;
    }

    // FindDevice — return the device registered under lpcPrefix (physical first, then virtual),
    // or null. Used by AddPhysicalDevice (duplicate-name guard) and the open path.
    Device* DeviceManager::FindDevice(const char* lpcPrefix)
    {
        if (!lpcPrefix)
            return nullptr;

        for (s32 liIndex = 0; liIndex < KI_MAX_PHYSICAL_DEVICES; ++liIndex)
            if (maPhysicalDevices[liIndex].mpDevice &&
                strcmp(maPhysicalDevices[liIndex].macPrefix, lpcPrefix) == 0)
                return maPhysicalDevices[liIndex].mpDevice;

        for (s32 liIndex = 0; liIndex < KI_MAX_VIRTUAL_DEVICES; ++liIndex)
            if (maVirtualDevices[liIndex].mpDevice &&
                strcmp(maVirtualDevices[liIndex].macPrefix, lpcPrefix) == 0)
                return maVirtualDevices[liIndex].mpDevice;

        return nullptr;
    }

    // AddPhysicalDevice @0x828F9320 — register lpDevice under lpcPrefix (asserting the name is
    // free and a slot is available), install its error callback, then spawn the worker thread.
    // ARTIST828F9320..9720. Initialize this physical queue at registration, bind its
    // actual device, and start its owning worker with the original thread parameters.
    void DeviceManager::AddPhysicalDevice(Device* lpDevice, const char* lpcPrefix,
                                          ErrorCallback lpfErrorCallback)
    {
        mDeviceListFutex.Lock();
        AssertDeviceConstruction(FindDevice(lpcPrefix) == nullptr, "Device name already in use\n", 143);
        s32 liSlot = 0;
        while (liSlot < KI_MAX_PHYSICAL_DEVICES && maPhysicalDevices[liSlot].mpDevice != nullptr)
            ++liSlot;
        AssertDeviceConstruction(liSlot < KI_MAX_PHYSICAL_DEVICES, "No free physical devices\n", 154);
        PhysicalDeviceSlot& lrSlot = maPhysicalDevices[liSlot];
        CopyConstructionString(lrSlot.macPrefix, KI_MAX_PREFIX_LENGTH + 1, lpcPrefix);
        lrSlot.mOperations.Construct();
        lrSlot.mpDevice = lpDevice;
        lpDevice->SetErrorCallback(lpfErrorCallback);
        lpDevice->SetParent(nullptr);
        mDeviceListFutex.Unlock();

        char lacThreadName[32];
        CgsCore::SPrintf(lacThreadName, sizeof(lacThreadName), "CgsFileSystem:%s", lpcPrefix);
        EA::Thread::ThreadParameters lParams;
        lParams.mnPriority = 350;
        lParams.mnProcessor = 0;
        lParams.mbDisablePriorityBoost = false;
        lParams.mpName = lacThreadName;
        lrSlot.mWorkerThread.Begin(PhysicalDeviceThread, &lrSlot, &lParams,
            EA::Thread::Thread::GetGlobalRunnableFunctionUserWrapper());
    }

    // FLAG PC-platform leaf: preserve the existing guarded native registration.
    bool DeviceManager::AddPhysicalDeviceNativePC(Device* lpDevice, const char* lpcPrefix, ErrorCallback lpfErrorCallback)
    {
        mDeviceListFutex.Lock();

        CGS_ASSERT(FindDevice(lpcPrefix) == nullptr, "Device name already in use\n");

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
        lpDevice->SetErrorCallback(lpfErrorCallback);

        mDeviceListFutex.Unlock();

        // Spawn the per-device worker, named "CgsFileSystem:<prefix>" (X360 SPrintf + Thread::Begin).
        char lacThreadName[64];
        snprintf(lacThreadName, sizeof(lacThreadName), "CgsFileSystem:%s", lpcPrefix);
        EA::Thread::ThreadParameters lParams;
        lParams.mpName = lacThreadName;
        lrSlot.mWorkerThread.Begin(PhysicalDeviceThread, &lrSlot, &lParams);
        return true;
    }

    void DeviceManager::SetDefaultPath(const char* lpcPath)
    {
        CGS_ASSERT(lpcPath != nullptr, "Default path required\n");
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
        CGS_ASSERT(lpcFileName != nullptr, "File name required\n");

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
        CGS_ASSERT(lpDevice != nullptr, "Could not find device\n");

        Device* lpPhysicalDevice = lpDevice ? lpDevice->GetPhysicalParent() : nullptr;
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
            lOperation.mHandle = Handle::Make(lpDevice, nullptr);
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
        Device* lpPhysicalDevice = lpDevice ? lpDevice->GetPhysicalParent() : nullptr;
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
        Device* lpPhysicalDevice = lpDevice ? lpDevice->GetPhysicalParent() : nullptr;
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
        Device* lpPhysicalDevice = lpDevice ? lpDevice->GetPhysicalParent() : nullptr;
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
                Handle::DeviceHandle lpDeviceHandle = nullptr;
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

    // ---- ReadFileSync — PC synchronous convenience over the async device engine ----------
    namespace
    {
        // A per-op completion record: the worker fires SyncOpCallback, which records the result
        // + handle and posts the count-0 semaphore the waiting caller blocks on.
        struct SyncCompletion
        {
            EA::Thread::Semaphore* mpDone;
            s32                    miResult;
            Handle                 mHandle;
            u64                    muSize;
        };

        void SyncOpCallback(s32 liResult, Handle lHandle, u64 luSize, void* lpContext)
        {
            SyncCompletion* lpComp = static_cast<SyncCompletion*>(lpContext);
            lpComp->miResult = liResult;
            lpComp->mHandle  = lHandle;
            lpComp->muSize   = luSize;
            lpComp->mpDone->Post(1);
        }

        // Fill in the op's completion callback/context, enqueue it on the device's pool, and block
        // until the worker thread posts completion; returns the op result.
        int EnqueueAndWait(OperationPool* lpPool, Operation* lpOp, SyncCompletion* lpComp)
        {
            lpOp->mpfCallback = SyncOpCallback;
            lpOp->mpContext   = lpComp;
            lpPool->AddOperation(lpOp);
            CgsDev::Assert::WaitForWorkerPC(*lpComp->mpDone);
            return lpComp->miResult;
        }
    }

    s32 DeviceManager::ReadFileSync(const char* lpcPath, void* lpBuffer, u32 luMaxSize)
    {
        // Route through the first registered physical device.
        PhysicalDeviceSlot* lpSlot = nullptr;
        for (s32 liIndex = 0; liIndex < KI_MAX_PHYSICAL_DEVICES; ++liIndex)
            if (maPhysicalDevices[liIndex].mpDevice) { lpSlot = &maPhysicalDevices[liIndex]; break; }
        if (!lpSlot)
            return -1;

        Device*        lpDevice = lpSlot->mpDevice;
        OperationPool* lpPool   = &lpSlot->mOperations;

        EA::Thread::Semaphore lDone;   // count 0 (default ctor)
        SyncCompletion lComp;
        lComp.mpDone   = &lDone;
        lComp.miResult = 0;
        lComp.mHandle.Clear();
        lComp.muSize   = 0;

        // OPEN (read mode == 1). The worker delivers the file handle in the callback's result.
        Operation lOp;
        memset(&lOp, 0, sizeof(lOp));
        lOp.miType   = E_OP_OPEN;
        lOp.mHandle = Handle::Make(lpDevice, nullptr);
        strncpy(lOp.macPath, lpcPath, sizeof(lOp.macPath) - 1);
        lOp.macPath[sizeof(lOp.macPath) - 1] = 0;
        lOp.muFlags     = 1;
        lOp.mpfCallback = SyncOpCallback;
        lOp.mpContext   = &lComp;
        lpPool->AddOperation(&lOp);
        CgsDev::Assert::WaitForWorkerPC(lDone);
        const Handle lHandle = lComp.mHandle;
        if (lComp.miResult != 0 || lHandle.IsNull())
            return -1;

        // READ the first luMaxSize bytes.
        memset(&lOp, 0, sizeof(lOp));
        lOp.miType     = E_OP_READ;
        lOp.mHandle    = lHandle;
        lOp.mu64Offset = 0;
        lOp.muSize     = luMaxSize;
        lOp.mpReadBuffer = lpBuffer;
        lOp.mpfCallback = SyncOpCallback;
        lOp.mpContext   = &lComp;
        lpPool->AddOperation(&lOp);
        CgsDev::Assert::WaitForWorkerPC(lDone);
        const int liBytes = lComp.miResult == 0 ? static_cast<int>(lComp.muSize) : -1;

        // CLOSE.
        memset(&lOp, 0, sizeof(lOp));
        lOp.miType      = E_OP_CLOSE;
        lOp.mHandle     = lHandle;
        lOp.mpfCallback = SyncOpCallback;
        lOp.mpContext   = &lComp;
        lpPool->AddOperation(&lOp);
        CgsDev::Assert::WaitForWorkerPC(lDone);

        return liBytes;
    }

    void* DeviceManager::ReadWholeFile(const char* lpcPath, u32* lpuOutSize)
    {
        if (lpuOutSize) *lpuOutSize = 0;

        // Route through the first registered physical device.
        PhysicalDeviceSlot* lpSlot = nullptr;
        for (s32 liIndex = 0; liIndex < KI_MAX_PHYSICAL_DEVICES; ++liIndex)
            if (maPhysicalDevices[liIndex].mpDevice) { lpSlot = &maPhysicalDevices[liIndex]; break; }
        if (!lpSlot)
            return nullptr;

        Device*        lpDevice = lpSlot->mpDevice;
        OperationPool* lpPool   = &lpSlot->mOperations;

        EA::Thread::Semaphore lDone;
        SyncCompletion lComp;
        lComp.mpDone   = &lDone;
        lComp.miResult = 0;
        lComp.mHandle.Clear();
        lComp.muSize   = 0;

        Operation lOp;

        // OPEN (read mode == 1). The worker delivers the file handle in the callback's result.
        memset(&lOp, 0, sizeof(lOp));
        lOp.miType   = E_OP_OPEN;
        lOp.mHandle = Handle::Make(lpDevice, nullptr);
        strncpy(lOp.macPath, lpcPath, sizeof(lOp.macPath) - 1);
        lOp.macPath[sizeof(lOp.macPath) - 1] = 0;
        lOp.muFlags = 1;
        const int liOpenResult = EnqueueAndWait(lpPool, &lOp, &lComp);
        const Handle lHandle = lComp.mHandle;
        if (liOpenResult != 0 || lHandle.IsNull())
            return nullptr;

        // GETFILESIZE (the worker passes the size as the callback result).
        memset(&lOp, 0, sizeof(lOp));
        lOp.miType   = E_OP_GETFILESIZE;
        lOp.mHandle = lHandle;
        const int liSizeResult = EnqueueAndWait(lpPool, &lOp, &lComp);
        const int liSize = liSizeResult == 0 ? static_cast<int>(lComp.muSize) : -1;

        void* lpBuffer = nullptr;
        u32   luBytes  = 0;
        if (liSize > 0)
        {
            lpBuffer = malloc(static_cast<size_t>(liSize));   // PC IO leaf; caller free()s
            if (lpBuffer)
            {
                // READ the whole file.
                memset(&lOp, 0, sizeof(lOp));
                lOp.miType     = E_OP_READ;
        lOp.mHandle    = lHandle;
                lOp.mu64Offset = 0;
                lOp.muSize     = static_cast<u32>(liSize);
        lOp.mpReadBuffer = lpBuffer;
                const int liReadResult = EnqueueAndWait(lpPool, &lOp, &lComp);
                const int liRead = liReadResult == 0 ? static_cast<int>(lComp.muSize) : -1;
                if (liRead == liSize)
                {
                    luBytes = static_cast<u32>(liSize);
                }
                else
                {
                    free(lpBuffer);   // short / failed read -> the whole-file read failed
                    lpBuffer = nullptr;
                }
            }
        }

        // CLOSE.
        memset(&lOp, 0, sizeof(lOp));
        lOp.miType   = E_OP_CLOSE;
        lOp.mHandle = lHandle;
        EnqueueAndWait(lpPool, &lOp, &lComp);

        if (lpBuffer && lpuOutSize)
            *lpuOutSize = luBytes;
        return lpBuffer;
    }

} // namespace CgsFileSystem
