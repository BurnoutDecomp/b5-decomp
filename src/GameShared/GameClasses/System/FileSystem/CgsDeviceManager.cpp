// CgsDeviceManager.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   GetDeviceManager        0x8264AD88   ctor                    0x828F0C90
//   InitInternal            0x828E9788   InitializeDeviceManager 0x828F91E0
// (AddPhysicalDevice 0x828F9320 + PhysicalDeviceThread 0x828F1EA8 land in the worker step.)

#include "GameShared/GameClasses/System/FileSystem/CgsDeviceManager.h"

#include <cstdlib>   // malloc / free
#include <cstring>   // strcmp / strncpy / strlen
#include <cstdio>    // snprintf

// Forked-vendor EAThread X360-ABI sleep (pointer-arg; the worker polls 15ms when the queue is
// empty). Declared locally rather than via the vendor eathread.h (whose ThreadSleep takes a
// ThreadTime); the body lives in vendor/EAThread/source/pc/eathread_x360align.cpp.
namespace EA { namespace Thread { unsigned int ThreadSleep(const unsigned int* pMilliseconds); } }

namespace CgsFileSystem
{
    // ---- statics ----
    // X360 globals: dword_830EA400 (singleton), off_830EA404 (allocator).
    DeviceManager* DeviceManager::mpDeviceManager = nullptr;
    void*          DeviceManager::mpAllocator     = nullptr;

    // CgsFileSystem::GetDeviceManager @ 0x8264AD88
    DeviceManager* GetDeviceManager()
    {
        CGS_ASSERT(DeviceManager::mpDeviceManager != nullptr, "Device manager not initialized\n");
        return DeviceManager::mpDeviceManager;
    }

    // Allocation. The X360 allocated the (161712-byte) DeviceManager through the GameData
    // allocator via a custom operator new; on PC this is a marked malloc leaf (single ~160KB
    // allocation). mpAllocator is captured by InitializeDeviceManager for parity / future use.
    void* DeviceManager::operator new(size_t luSize) { return malloc(luSize); }      // PC IO leaf
    void  DeviceManager::operator delete(void* lpBlock) { free(lpBlock); }

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
            maPhysicalDevices[liIndex].mpDevice = nullptr;
        for (s32 liIndex = 0; liIndex < KI_MAX_VIRTUAL_DEVICES; ++liIndex)
            maVirtualDevices[liIndex].mpDevice = nullptr;

        mbInitialized = true;
        mbReleasing   = false;

        mDeviceListFutex.Unlock();
    }

    // InitializeDeviceManager @ 0x828F91E0 — create + init the singleton. Asserts it does not
    // already exist and that an allocator was supplied; stores the allocator, allocates +
    // constructs the DeviceManager, then runs InitInternal.
    bool DeviceManager::InitializeDeviceManager(void* lpAllocator)
    {
        CGS_ASSERT(mpDeviceManager == nullptr, "Device manager already exists\n");
        CGS_ASSERT(lpAllocator != nullptr, "Allocator required\n");

        mpAllocator     = lpAllocator;
        mpDeviceManager = new DeviceManager();
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
    bool DeviceManager::AddPhysicalDevice(Device* lpDevice, const char* lpcPrefix, ErrorCallback lpfErrorCallback)
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

            Device* lpDevice = lOp.mpDevice;
            switch (lOp.miType)
            {
            case E_OP_READ:
            {
                int  liResult = 0;
                char lacCheck[8] = { 0 };
                if (lpDevice->CheckOp(lOp.miHandle, lOp.mu64Offset, lacCheck) == 0)
                    lpDevice->Read(lOp.miHandle, lOp.mu64Offset, lOp.muSize, lOp.mpBuffer, &liResult);
                if (lOp.mpfCallback) lOp.mpfCallback(lpDevice, lOp.miHandle, liResult, lOp.mpContext);
                break;
            }
            case E_OP_WRITE:
            {
                int  liResult = 0;
                char lacCheck[8] = { 0 };
                if (lpDevice->CheckOp(lOp.miHandle, lOp.mu64Offset, lacCheck) == 0)
                    lpDevice->Write(lOp.miHandle, lOp.mu64Offset, lOp.muSize, lOp.mpBuffer, &liResult);
                if (lOp.mpfCallback) lOp.mpfCallback(lpDevice, lOp.miHandle, liResult, lOp.mpContext);
                break;
            }
            case E_OP_OPEN:
            {
                int liResult = 0;
                lpDevice->Open(lOp.macPath, lOp.miHandle, &liResult);
                if (lOp.mpfCallback) lOp.mpfCallback(lpDevice, lOp.miHandle, liResult, lOp.mpContext);
                break;
            }
            case E_OP_CLOSE:
                lpDevice->Close(lOp.miHandle);
                if (lOp.mpfCallback) lOp.mpfCallback(lpDevice, lOp.miHandle, 0, lOp.mpContext);
                break;
            case E_OP_GETFILESIZE:
            {
                u64 lu64Size = 0;
                lpDevice->GetFileSize(lOp.miHandle, &lu64Size);
                if (lOp.mpfCallback) lOp.mpfCallback(lpDevice, lOp.miHandle, static_cast<int>(lu64Size), lOp.mpContext);
                break;
            }
            case E_OP_OPENEX:
            {
                int liResult = 0, liC = 0;
                lpDevice->OpenEx(lOp.macPath, lOp.muSize, static_cast<u32>(lOp.miHandle), &liC, &liResult);
                if (lOp.mpfCallback) lOp.mpfCallback(lpDevice, lOp.miHandle, liResult, lOp.mpContext);
                break;
            }
            case E_OP_SEEK:
            {
                int liResult = 0;
                lpDevice->Seek(lOp.miHandle, lOp.mu64Offset, &liResult);
                if (lOp.mpfCallback) lOp.mpfCallback(lpDevice, lOp.miHandle, liResult, lOp.mpContext);
                break;
            }
            case E_OP_OP7:
                lpDevice->Op7(lOp.miHandle);
                if (lOp.mpfCallback) lOp.mpfCallback(lpDevice, lOp.miHandle, 0, lOp.mpContext);
                break;
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
            int                    miResult;
            int                    miHandle;
        };

        void SyncOpCallback(Device* /*lpDevice*/, s32 liHandle, s32 liResult, void* lpContext)
        {
            SyncCompletion* lpComp = static_cast<SyncCompletion*>(lpContext);
            lpComp->miResult = liResult;
            lpComp->miHandle = liHandle;
            lpComp->mpDone->Post(1);
        }

        // Fill in the op's completion callback/context, enqueue it on the device's pool, and block
        // until the worker thread posts completion; returns the op result.
        int EnqueueAndWait(OperationPool* lpPool, Operation* lpOp, SyncCompletion* lpComp)
        {
            lpOp->mpfCallback = SyncOpCallback;
            lpOp->mpContext   = lpComp;
            lpPool->AddOperation(lpOp);
            const u32 luWaitForever = 0xFFFFFFFFu;
            lpComp->mpDone->Wait(&luWaitForever);
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
        lComp.miHandle = -1;
        const u32 luWaitForever = 0xFFFFFFFFu;

        // OPEN (read mode == 1). The worker delivers the file handle in the callback's result.
        Operation lOp;
        memset(&lOp, 0, sizeof(lOp));
        lOp.miType   = E_OP_OPEN;
        lOp.mpDevice = lpDevice;
        strncpy(lOp.macPath, lpcPath, sizeof(lOp.macPath) - 1);
        lOp.macPath[sizeof(lOp.macPath) - 1] = 0;
        lOp.miHandle    = 1;
        lOp.mpfCallback = SyncOpCallback;
        lOp.mpContext   = &lComp;
        lpPool->AddOperation(&lOp);
        lDone.Wait(&luWaitForever);
        const int liHandle = lComp.miResult;
        if (liHandle < 0)
            return -1;

        // READ the first luMaxSize bytes.
        memset(&lOp, 0, sizeof(lOp));
        lOp.miType     = E_OP_READ;
        lOp.mpDevice   = lpDevice;
        lOp.miHandle   = liHandle;
        lOp.mu64Offset = 0;
        lOp.muSize     = luMaxSize;
        lOp.mpBuffer   = lpBuffer;
        lOp.mpfCallback = SyncOpCallback;
        lOp.mpContext   = &lComp;
        lpPool->AddOperation(&lOp);
        lDone.Wait(&luWaitForever);
        const int liBytes = lComp.miResult;

        // CLOSE.
        memset(&lOp, 0, sizeof(lOp));
        lOp.miType      = E_OP_CLOSE;
        lOp.mpDevice    = lpDevice;
        lOp.miHandle    = liHandle;
        lOp.mpfCallback = SyncOpCallback;
        lOp.mpContext   = &lComp;
        lpPool->AddOperation(&lOp);
        lDone.Wait(&luWaitForever);

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
        lComp.miHandle = -1;

        Operation lOp;

        // OPEN (read mode == 1). The worker delivers the file handle in the callback's result.
        memset(&lOp, 0, sizeof(lOp));
        lOp.miType   = E_OP_OPEN;
        lOp.mpDevice = lpDevice;
        strncpy(lOp.macPath, lpcPath, sizeof(lOp.macPath) - 1);
        lOp.macPath[sizeof(lOp.macPath) - 1] = 0;
        lOp.miHandle = 1;
        const int liHandle = EnqueueAndWait(lpPool, &lOp, &lComp);
        if (liHandle < 0)
            return nullptr;

        // GETFILESIZE (the worker passes the size as the callback result).
        memset(&lOp, 0, sizeof(lOp));
        lOp.miType   = E_OP_GETFILESIZE;
        lOp.mpDevice = lpDevice;
        lOp.miHandle = liHandle;
        const int liSize = EnqueueAndWait(lpPool, &lOp, &lComp);

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
                lOp.mpDevice   = lpDevice;
                lOp.miHandle   = liHandle;
                lOp.mu64Offset = 0;
                lOp.muSize     = static_cast<u32>(liSize);
                lOp.mpBuffer   = lpBuffer;
                const int liRead = EnqueueAndWait(lpPool, &lOp, &lComp);
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
        lOp.mpDevice = lpDevice;
        lOp.miHandle = liHandle;
        EnqueueAndWait(lpPool, &lOp, &lComp);

        if (lpBuffer && lpuOutSize)
            *lpuOutSize = luBytes;
        return lpBuffer;
    }

} // namespace CgsFileSystem
