#pragma once

#include "types.hpp"

#include "SDKs/EATech/rwcore/filesys/asyncop.h"          // AsyncOp / AsyncOpList / OpStream
#include "SDKs/EATech/eathread/eathread_condition.h"      // EA::Thread::Condition
#include "SDKs/EATech/eathread/BrnEAThreadX360.h"          // EA::Thread::Thread / ThreadParameters / ThreadSleep

#include <windows.h>  // CRITICAL_SECTION -- the host mapping of the X360 EA::Thread::Mutex

// =====================================================================================
// rw::core::filesys::Device -- the RenderWare core filesystem device scheduler.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for this TU. Members are modelled by
// name at the X360 offsets proven by the asm; the embedded threading primitives are the
// project's homed EA::Thread types, with the X360 EA::Thread::Mutex modelled as a host
// Win32 CRITICAL_SECTION exactly as the homed eathread_condition.cpp does for its own
// unblock-lock (the X360 disassembly even labels the +0x18/+0xB0 fields "CriticalSection";
// Lock/Unlock map to Enter/LeaveCriticalSection, ctor/dtor to Init/DeleteCriticalSection).
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

            // The device driver the scheduler dispatches through. The X360 object is
            // { mpVTable @+0x00, char macName[] @+0x04 } -- exactly what the DeviceDriver
            // ctor TU builds (devicedriver.cpp installs gDeviceDriverVTable at +0 and copies
            // the scheme name into the inline buffer at +4). The Device scheduler reaches the
            // driver only through three vtable slots plus the name:
            //   vtable +0x04 (slot 1)  Open         : (*(driver+4))(driver)   -- bring it up
            //   vtable +0x08 (slot 2)  Close        : (*(driver+8))(driver)   -- shut it down
            //   vtable +0x28 (slot 10) GetBlockSize : (driver, key) -> u32 cost estimate
            //   data   +0x04 macName                : scheme name (GetInstance matches it)
            //
            // The vtable is modelled as a named function-pointer table (NOT a C++ virtual
            // class), so the on-disk { pointer-to-table, name } layout is reproduced exactly
            // and every slot is called by name -- no raw `*(*driver+N)` offset arithmetic.
            // The untouched slots are reserved entries so the touched ones sit at the
            // observed indices.
            class DeviceDriver;

            struct DeviceDriverVTable
            {
                void* mpfnSlot0;                                   // +0x00
                int  (*mpfnOpen)(DeviceDriver* lpDriver);          // +0x04 (slot 1)
                int  (*mpfnClose)(DeviceDriver* lpDriver);         // +0x08 (slot 2)
                void* mapfnReserved0C[7];                          // +0x0C .. +0x24 (slots 3..9)
                u32  (*mpfnGetBlockSize)(DeviceDriver* lpDriver, void* lpDriverKey); // +0x28 (slot 10)
            };

            class DeviceDriver
            {
            public:
                // The ctor (devicedriver.cpp @0x82BBD548) installs gDeviceDriverVTable and
                // copies pName into macName. Declared here so the type has one definition.
                DeviceDriver(const char* pName);

                // Named slot wrappers so the scheduler dispatches by name.
                int Open()                       { return mpVTable->mpfnOpen(this); }
                int Close()                      { return mpVTable->mpfnClose(this); }
                u32 GetBlockSize(void* lpKey)    { return mpVTable->mpfnGetBlockSize(this, lpKey); }

                const DeviceDriverVTable* mpVTable;   // +0x00
                char                      macName[1];  // +0x04  scheme name
            };

            // The process-wide filesys Manager singleton (X360 off_8327F078). The Device
            // TUs read these fields of it, all by name:
            //   X360 +0x00 mpDeviceListHead  : head of the registered-Device list
            //   X360 +0x0C mThreadParameters : params handed to EA::Thread::Thread::Begin
            //   X360 +0x28 mpDefaultDevice   : device returned when a path has no scheme
            //   X360 +0x30 mpCurrentDir      : current working-directory prefix string
            //   X360 +0x40 mbSortByPosition  : when == 1, read ops are reordered by position
            // The X360 byte offsets are not reproduced (host pointer / ThreadParameters
            // sizes differ); members keep the X360 ORDER and are accessed purely by name.
            // The full Manager is owned by its own (not-yet-homed) TU -- this is a forward
            // shape exposing only what the device scheduler/path-resolver touch.
            struct Manager
            {
                Device*                       mpDeviceListHead;   // X360 +0x00
                EA::Thread::ThreadParameters  mThreadParameters;   // X360 +0x0C
                Device*                       mpDefaultDevice;     // X360 +0x28
                const char*                   mpCurrentDir;        // X360 +0x30
                s32                           mbSortByPosition;    // X360 +0x40
            };

            // X360 off_8327F078 -- the global filesys Manager. Owned by the Manager TU;
            // declared here so the scheduler can read its sort flag / thread params.
            extern Manager* gpFileSysManager;

            // The RenderWare core filesystem device scheduler. A worker thread drains a
            // priority-ordered list of AsyncOps, optionally coalescing adjacent read ops
            // to honour the driver's optimal-read budget.
            //
            // The X360 object lays its members at fixed offsets (op list +0x08, op-list
            // mutex +0x18, work condition +0x48, worker thread +0xA8, completion mutex
            // +0xB0, completion condition +0xE0, driver +0x140, read budget +0x148). Those
            // offsets are NOT reproduced here: the host EA::Thread::Condition / ::Thread and
            // the host CRITICAL_SECTION have different sizes than the X360 primitives, and
            // every access in this TU is by member NAME (never `this+N`), so the layout is
            // free to follow the host ABI. Member ORDER is preserved to mirror the X360
            // construction/destruction order.
            class Device
            {
            public:
                // @0x82BBE170 -- wire up the empty op list, build the two mutexes and two
                // conditions and the (unstarted) worker thread, latch the external-thread
                // flag (lbExternalThread & 1) and the driver pointer.
                Device(DeviceDriver* lpDriver, bool lbExternalThread);

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

            private:
                Device*               mpNextRegistered;  // X360 +0x000 (Manager device-list link)
                u8                    mbThreadRunning;    // X360 +0x004
                u8                    mbStopRequested;    // X360 +0x005
                u8                    mbExternalThread;   // X360 +0x006
                AsyncOpList           mOpList;            // X360 +0x008 (head/tail/count)
                CRITICAL_SECTION      mLock;              // X360 +0x018 (EA::Thread::Mutex)
                EA::Thread::Condition mWork;              // X360 +0x048 ("work pending")
                EA::Thread::Thread    mThread;            // X360 +0x0A8 (worker thread)
                CRITICAL_SECTION      mCompletionLock;    // X360 +0x0B0 (EA::Thread::Mutex)
                EA::Thread::Condition mCompletion;        // X360 +0x0E0 ("op completed")
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
