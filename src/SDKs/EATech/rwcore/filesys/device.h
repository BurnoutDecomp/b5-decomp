#pragma once

#include "types.hpp"

#include "SDKs/EATech/rwcore/filesys/asyncop.h"          // AsyncOp / AsyncOpList / OpStream
#include "SDKs/EATech/rwcore/filesys/list.h"             // detail::ListSingle (the registered-device list)
#include "eathread/eathread_thread.h"                     // EA::Thread::Thread / ThreadParameters / ThreadSleep
#include "eathread/eathread_mutex.h"                      // EA::Thread::Mutex
#include "eathread/eathread_condition.h"                  // EA::Thread::Condition

// =====================================================================================
// rw::core::filesys::Device -- the RenderWare core filesystem device scheduler.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for this TU. Members are modelled by
// name at the X360 offsets proven by the asm; the embedded threading primitives are the
// EAThread library's own types (two mutexes, two conditions, the worker thread).
//
//   rw::core::filesys::Device::Device                @0x82BBE170  (ctor)
//   rw::core::filesys::Device::~Device               @0x82BBE208  (dtor)
//   rw::core::filesys::Device::`scalar deleting dtor' @0x82BBE3B0
//   rw::core::filesys::Device::Start                 @0x82BBF608
//   rw::core::filesys::Device::ThreadEntry           @0x82BBF3E8
//   rw::core::filesys::Device::InsertOp              @0x82BBF6B0
//   rw::core::filesys::Device::ChangeOpPriority      @0x82BBF7E8
//   rw::core::filesys::Device::CheckForOptimalReadOp @0x82BBED28
//   rw::core::filesys::Device::GetInstance           @0x82BBEAC0
// =====================================================================================

namespace rw
{
    namespace core
    {
        namespace filesys
        {
            class Device;  // the scheduler defined below; Manager links a list of these

            // The device driver a Device schedules IO onto: an abstract class whose
            // concrete drivers (the platform file driver, the null driver, the game's own
            // drivers) override the IO entry points. The base class supplies the defaults
            // below; Open / Close / Read / Seek / GetSize are pure. Object layout:
            // { vtable, mDeviceName }.
            struct DeviceDriver
            {
                // The record FindBegin / FindNext fill in.
                struct FindData
                {
                    unsigned int mFlags;
                    uint64_t     mCreationTime;
                    uint64_t     mAccessTime;
                    uint64_t     mModificationTime;
                    uint64_t     mSize;
                    char         mName[256];
                };

                // devicedriver.cpp: copy pName into mDeviceName.
                DeviceDriver(const char* pName);

                virtual ~DeviceDriver() {}

                // Bring the driver up (Device::Start) / shut it down (~Device).
                virtual bool Init() { return true; }
                virtual void Restore() {}

                // Open a file; returns the driver's open-file object (null on failure).
                // *ppHandle may be set to an already-open Handle the file belongs to.
                virtual void* Open(const char* pPath, unsigned int uFlags, Handle** ppHandle) = 0;
                virtual void  Close(void* pFile) = 0;
                virtual unsigned int Read(void* pFile, void* pBuffer, unsigned int uSize,
                                          DeviceDriver* pDriver, void* pContext) = 0;
                virtual unsigned int Write(void* pFile, const void* pBuffer, unsigned int uSize,
                                           DeviceDriver* pDriver, void* pContext) { return 0; }
                virtual uint64_t Seek(void* pFile, uint64_t uPosition, int iOrigin,
                                      DeviceDriver* pDriver, void* pContext) = 0;
                virtual uint64_t GetSize(void* pFile) = 0;
                virtual bool     Resize(void* pFile, uint64_t uSize) { return false; }

                // The on-media location of an open file; the scheduler sorts reads by it.
                virtual uint64_t QueryLocation(void* pFile) { return 0; }
                virtual unsigned int GetMaxReadSize() { return 0xFFFFFFFFu; }

                virtual bool  Delete(const char* pPath) { return false; }
                virtual bool  Move(const char* pFrom, const char* pTo) { return false; }
                virtual void* FindBegin(const char* pPath, FindData* pData) { return nullptr; }
                virtual bool  FindNext(void* pFind, FindData* pData) { return false; }
                virtual bool  FindEnd(void* pFind) { return false; }
                virtual bool  DirectoryCreate(const char* pPath) { return false; }
                virtual bool  DirectoryRemove(const char* pPath) { return false; }

                // The scheme name Device::GetInstance matches a path's "scheme:" against.
                const char* GetName() const { return mDeviceName; }

            protected:
                char mDeviceName[16];
            };

            // The process-wide filesys Manager singleton (X360 off_8327F078). Homed by its
            // own TU (manager.cpp); the device scheduler/path-resolver read its fields by
            // name. The full 72-byte (0x48) X360 layout is reproduced in WORD order from the
            // Manager ctor/Init asm (every slot grounded by a store/load):
            //   +0x00 mDeviceList            : the registered-Device list (head/tail/count)
            //   X360 +0x0C mThreadParameters : params handed to EA::Thread::Thread::Begin
            //   X360 +0x24 miMaxReadSize     : per-read size ceiling, ctor-initialised to -1
            //                                   (read as an unsigned cap by AsyncOp::DoRead)
            //   X360 +0x28 mpDefaultDevice   : device returned when a path has no scheme
            //   X360 +0x2C mpScratchBuffer   : working path buffer (Init allocs muScratchSize)
            //   X360 +0x30 mpCurrentDir      : current working-directory prefix string
            //   X360 +0x34 muScratchSize     : byte size of mpScratchBuffer
            //   X360 +0x38 muMaxSearchPaths  : capacity of mpSearchPathTable (8 bytes/entry)
            //   X360 +0x3C mpSearchPathTable : array of { const char* path, Device* dev }
            //   X360 +0x40 mbSortByPosition  : when == 1, read ops are reordered by position
            //   X360 +0x44 muReserved44      : ctor-cleared trailing word
            // The X360 byte offsets are not reproduced literally (host pointer /
            // ThreadParameters sizes differ); members keep the X360 ORDER and are accessed
            // purely by name. _AssertLayout() is intentionally absent: the host
            // ThreadParameters size differs from the X360's, so the byte offsets are not
            // host-stable; only the field order/semantics are reconstructed.
            //
            // The CgsFileSystem config object handed to CreateInstance / the Manager ctor.
            // Only the three words those functions read are named (proven by the asm):
            //   +0x00 muMaxSearchPaths : search-path table capacity (ctor: a2[0])
            //   +0x04 muScratchSize    : working-buffer byte size   (ctor: a2[1])
            //   +0x08 mpAllocator      : the filesys allocator       (CreateInstance: a1[2])
            struct FileSystemConfig
            {
                u32        muMaxSearchPaths;  // +0x00
                u32        muScratchSize;     // +0x04
                Allocator* mpAllocator;       // +0x08
            };

            // A search-path table entry: { path string, resolved device }.
            struct SearchPathEntry
            {
                const char* mpcPath;  // +0x00
                Device*     mpDevice;  // +0x04
            };

            struct Manager
            {
                detail::ListSingle<Device>    mDeviceList;         // +0x00
                EA::Thread::ThreadParameters  mThreadParameters;   // X360 +0x0C
                s32                           miMaxReadSize;       // X360 +0x24 (init -1; read-size ceiling)
                Device*                       mpDefaultDevice;     // X360 +0x28
                char*                         mpScratchBuffer;     // X360 +0x2C
                const char*                   mpCurrentDir;        // X360 +0x30
                u32                           muScratchSize;       // X360 +0x34
                u32                           muMaxSearchPaths;    // X360 +0x38
                SearchPathEntry*              mpSearchPathTable;   // X360 +0x3C
                s32                           mbSortByPosition;    // X360 +0x40
                u32                           muReserved44;        // X360 +0x44

                // ----- rw::core::filesys::Manager methods (homed in manager.cpp) ----------

                // @0x82BBE418 (ctor) -- zero the lists, default-construct the thread params
                // (then bump their priority by 2), seed the handle id to -1, and latch the
                // search-path capacity (a2[0]) / scratch size (a2[1]) from the config.
                Manager(const FileSystemConfig* lpConfig);

                // @0x82BBF008 (dtor) -- free the search-path table and scratch buffer through
                // the global allocator, then unregister every device and clear the list.
                ~Manager();

                // @0x82BBF0B8 -- allocate the scratch buffer + search-path table, register
                // the default device and the static device table, set the default search path.
                int Init();

                // @0x82BBEEC8 -- (re)build the search-path table from a ';'-separated path
                // string, resolving each entry to a Device via Device::GetInstance.
                int SetSearchPath(const char* lpcSearchPath);

                // @0x82BBD640 -- allocate luSize bytes through the global filesys allocator.
                static void* Allocate(u32 luSize);

                // @0x82BBD678 -- free a block through the global filesys allocator.
                static void* Free(void* lpBlock);

                // @0x82BBD630 -- return the global Manager singleton (off_8327F078).
                static void* GetInstance();

                // @0x82BBF518 -- install the allocator from the config, allocate + construct
                // the singleton, Init it, and publish it. Returns the singleton.
                static void* CreateInstance(const FileSystemConfig* lpConfig);

                // @0x82BBF850 -- destroy + free the singleton and clear off_8327F078.
                static void* DestroyInstance();

                // Build a Device for lpDriver and append it to the global manager's device
                // list (luFlags bit 0: the device is pumped externally, no worker thread).
                Device* RegisterDevice(DeviceDriver* lpDriver, unsigned int luFlags) const;

                // Unlink lpDevice from the global manager's device list and destroy it.
                // False if it was not registered.
                bool    UnregisterDevice(Device* lpDevice) const;
            };

            // X360 off_8327F078 -- the global filesys Manager. Owned by manager.cpp;
            // declared here so the scheduler can read its sort flag / thread params.
            extern Manager* gpFileSysManager;

            // Manager::`scalar deleting destructor' @0x82BBF5A0: ~Manager, then if (flags & 1)
            // free `this` through gpFileSysAllocator. Returns lpManager (X360 r3 == this).
            Manager* ManagerScalarDeletingDtor(Manager* lpManager, char lcFlags);

            // The RenderWare core filesystem device scheduler. A worker thread drains a
            // priority-ordered list of AsyncOps, optionally coalescing adjacent read ops
            // to honour the driver's optimal-read budget.
            //
            // The X360 object lays its members at fixed offsets (op list +0x08, op-list
            // mutex +0x18, work condition +0x48, worker thread +0xA8, completion mutex
            // +0xB0, completion condition +0xE0, driver +0x140, read budget +0x148). Those
            // offsets are NOT reproduced here: the host EA::Thread::Mutex / ::Condition /
            // ::Thread have different sizes than the console primitives, and
            // every access in this TU is by member NAME (never `this+N`), so the layout is
            // free to follow the host ABI. Member ORDER is preserved to mirror the X360
            // construction/destruction order.
            class Device
            {
            public:
                // @0x82BBE170 -- wire up the empty op list, build the two mutexes and two
                // conditions and the (unstarted) worker thread, latch the external-thread
                // flag (luFlags & 1) and the driver pointer.
                Device(DeviceDriver* lpDriver, unsigned int luFlags);

                // @0x82BBE208 -- stop the worker (unless external), Close the driver, then
                // tear down the conditions/thread/mutexes and drop the op list.
                ~Device();

                // @0x82BBF608 -- Open the driver; if external just mark running, else spin
                // up the worker thread and block until it reports running.
                int Start();

                // @0x82BBF6B0 -- enqueue lpOp in priority order (auto-Start if idle) and
                // wake the worker.
                int InsertOp(AsyncOp* lpOp);

                // @0x82BBF7E8 -- re-prioritise an already-queued op: remove it, set the new
                // priority, and re-insert.
                int ChangeOpPriority(AsyncOp* lpOp, int liPriority);

                // @0x82BBED28 -- given the op the worker just popped, optionally swap in a
                // later same-stream read op when doing so stays inside the read budget;
                // returns the op the worker should actually service.
                AsyncOp* CheckForOptimalReadOp(AsyncOp* lpOp);

                // @0x82BBF3E8 -- the worker-thread body: drain the op list, dispatch each
                // op's DoIo, fire its completion callback under the completion lock, and
                // wait on new work until stop is requested. Static (EAThread entry point).
                static intptr_t ThreadEntry(void* lpContext);

                // @0x82BBEAC0 -- resolve a device-relative path (a1) against a search-path
                // working buffer (a2) to a registered Device, returning the matching
                // Device* (or the default device when the resolved path has no scheme).
                static Device* GetInstance(const char* lpcPath, char* lpScratch);

                // Block under the completion lock until lpOp has a result or the absolute
                // timeout has passed. Returns the completion-lock Unlock result.
                int Wait(AsyncOp* lpOp, const EA::Thread::ThreadTime& lTimeoutAbsolute);

                // Read accessor for the driver pointer; the AsyncOp DoIo callbacks hand the
                // driver to the Stream IO vtable slots as an opaque argument.
                DeviceDriver* GetDriver() const { return mpDriver; }

                // The "external thread" flag at +0x06. The AsyncOp queue/wait paths read it
                // (`lbz 6(device)`) to decide whether to operate on this device or redirect
                // to the Manager's default device's worker.
                bool IsExternalThread() const { return mbExternalThread != 0; }

            private:
                friend struct detail::ListSingle<Device>;   // links through mpNext
                friend struct Handle;                       // starts a search-path device

                Device*               mpNext;             // +0x000 (Manager device-list link)
                u8                    mbThreadRunning;    // +0x004
                u8                    mbStopRequested;    // +0x005
                u8                    mbExternalThread;   // +0x006
                AsyncOpList           mOpList;            // +0x008 (head/tail/count)
                EA::Thread::Mutex     mLock;              // +0x018
                EA::Thread::Condition mWork;              // +0x048 ("work pending")
                EA::Thread::Thread    mThread;            // +0x0A8 (worker thread)
                EA::Thread::Mutex     mCompletionLock;    // +0x0B0
                EA::Thread::Condition mCompletion;        // +0x0E0 ("op completed")
                DeviceDriver*         mpDriver;           // X360 +0x140
                u64                   mu64ReadBudget;     // X360 +0x148 (optimal-read budget)
            };

            // Device::`scalar deleting destructor' @0x82BBE3B0: the MSVC `delete` thunk --
            // run ~Device, then if (flags & 1) free the storage through the global filesys
            // allocator (off_8327F07C) via its vtable slot +0xC (Free(this, 0)). Returns
            // lpDevice (X360 r3 == this). Modelled as a free helper, mirroring the Handle
            // scalar deleting destructor in handle.cpp.
            Device* DeviceScalarDeletingDtor(Device* lpDevice, char lcFlags);
        }
    }
}
