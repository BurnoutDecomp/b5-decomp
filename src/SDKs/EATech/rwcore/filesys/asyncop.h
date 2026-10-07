#pragma once

#include "types.hpp"
#include "SDKs/EATech/rwcore/filesys/list.h"   // detail::ListSingle (the op list)
#include "eathread/eathread.h"                  // EA::Thread::ThreadTime (GetStatus / Device::Wait timeout)

// rw::core::filesys -- async-op / handle primitives of the RenderWare core filesystem.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for these TUs. Members are modelled
// by name at the observed X360 offsets (no raw-offset casts at the call sites).
//
//   rw::core::filesys::GetSize                  @0x82BBD700
//   rw::core::filesys::Handle::~Handle          @0x82BBD6A0
//   rw::core::filesys::Handle::~Handle (scalar deleting) @0x82BBDFA8
//
// The rw::core::filesys::AsyncOp class itself (ctor/dtor + Open/Read/Write/Close + the
// DoOpen/DoRead/DoWrite/DoClose transfer callbacks + the result/status accessors) is
// reconstructed in asyncop.cpp; its full 0x48-byte object layout is named here, grown
// from the prior forward shape, with every field grounded by an asm store/load.

namespace rw
{
    namespace core
    {
        namespace filesys
        {
            // The device driver a Handle's file was opened through (device.h). ~Handle
            // closes the file through it.
            struct DeviceDriver;

            // The scheduler that owns the op-queue (homed in device.cpp). AsyncOp's
            // Open/Read/Write/Close hand the op to Device::InsertOp, and the result
            // accessors block on Device::Wait. Forward-declared to break the include
            // cycle (device.h includes this header).
            class Device;

            // An outstanding async filesystem operation. Fields are named at the X360
            // offsets proven by the asm of the AsyncOp class methods (ctor @0x82BBD3E0,
            // Open/Read/Write/Close, DoOpen/DoRead/DoWrite/DoClose) AND of the Device
            // scheduler TU (Device::InsertOp / CheckForOptimalReadOp / ThreadEntry, which
            // walk this object by `this+N`). Offsets verified from BURNOUT_X360_ARTIST.XEX:
            //   +0x00 mpNext        : intrusive forward link (AsyncOpList node link)
            //   +0x04 miResult      : DoIo result/status (ctor inits -3; ThreadEntry stores it)
            //   +0x08 mbReserved08  : byte cleared by ctor/Open/Read/Write/Close
            //   +0x09 mbIsReadOp    : non-zero for read-class ops the scheduler may reorder
            //   +0x0C miPriority    : queue priority (Device sorts the op list by this)
            //   +0x10 mpStream      : the owning Stream/File (Open/Read/Write store it; the
            //                         DoIo callbacks walk it; DoOpen stores the new Handle)
            //   +0x14 mUserData     : opaque user word carried through to the callback
            //   +0x18 mpfnComplete  : completion callback (ctor/Open default it to STUB)
            //   +0x20 mu64Position  : 64-bit file offset (read-coalescing math; a4 of R/W)
            //   +0x28 mu64ByteCount : 64-bit byte count requested (a7 of Read/Write)
            //   +0x30 mpBuffer      : transfer buffer (R/W a3; Open's allocated path copy)
            //   +0x38 mu64BytesDone : 64-bit running transferred-bytes accumulator
            //   +0x40 mpDevice      : the Device this op was queued on
            //   +0x44 mpfnDoIo      : the op's transfer entry point (Device calls it)
            //
            // The +0x18 slot is the completion callback (a 32-bit function pointer in the
            // asm: `stw r5,0x18`). The free GetSize @0x82BBD700 reads the 64-bit word at
            // +0x18 of its argument (`ld 0x18`); that accessor is a separate free function
            // (not an AsyncOp method) and is modelled via the mu64Size union view below so
            // both the byte layout and both call shapes are reproduced exactly.
            struct AsyncOp;

            // The object AsyncOp::mpStream points at, as the Device scheduler sees it. The
            // scheduler only reads the word at +0x08 (`*(*(op+0x10)+8)`) -- the key it hands
            // to the device driver's QueryLocation. The full Stream object (the
            // AsyncOp DoIo callbacks' richer view of the same bytes) is `Stream` below.
            struct OpStream
            {
                u8    macReserved0[8]; // +0x00 .. +0x07
                void* mpDriverKey;     // +0x08  key passed to DeviceDriver::QueryLocation
            };

            // The op completion callback the scheduler fires once DoIo finishes.
            typedef void (*CompletionCallback)(AsyncOp* lpOp);

            // The op's transfer entry point (DoOpen/DoRead/DoWrite/DoClose). Returns
            // non-zero when the op has completed (the scheduler then fires mpfnComplete).
            typedef s32 (*DoIoCallback)(AsyncOp* lpOp);

            // -----------------------------------------------------------------------------
            // rw::core::filesys::Stream -- the file/stream object an AsyncOp transfers
            // through (the object Read/Write/Close receive and store at AsyncOp::mpStream).
            // Owned by the (not-yet-homed) Stream TU; this is the minimal shape the AsyncOp
            // DoIo callbacks walk, every field/slot grounded by the DoRead/DoWrite asm.
            //
            // The IO transfer slots are dispatched through Stream::mpIo->mpVTable. Slot
            // names below mirror the byte offsets the asm loads (`lwz r11,0xNN(vtable)`);
            // the role comments are the inferred meaning from the surrounding stores, but
            // the OFFSETS and the argument order are taken verbatim from the asm.
            //   vtable +0x14 mpfnSlot14 : DoRead  data transfer (buffer, count) -> bytes
            //   vtable +0x18 mpfnSlot18 : DoWrite data transfer (buffer, count) -> bytes
            //   vtable +0x1C mpfnSlot1C : DoRead/DoWrite leading position call -> 64-bit pos
            //   vtable +0x20 mpfnSlot20 : DoWrite trailing position fetch -> 64-bit pos
            //   vtable +0x2C mpfnSlot2C : DoRead size/granularity ceiling -> u32
            // -----------------------------------------------------------------------------
            struct StreamIoVTable
            {
                void* mapfnReserved00[5];                                   // +0x00 .. +0x10
                u64  (*mpfnSlot14)(void* lpIo, void* lpKey, void* lpBuffer,
                                   u32 luCount, void* lpDriver, void* lpCtxKey); // +0x14
                s32  (*mpfnSlot18)(void* lpIo, void* lpKey, void* lpBuffer,
                                   u64 lu64Count, void* lpDriver, void* lpCtxKey); // +0x18
                u64  (*mpfnSlot1C)(void* lpIo, void* lpKey, u64 lu64Position,
                                   u32 luFlags, void* lpDriver, void* lpCtxKey); // +0x1C
                u64  (*mpfnSlot20)(void* lpIo, void* lpKey);                // +0x20
                void* mpfnReserved24;                                       // +0x24
                void* mpfnReserved28;                                       // +0x28
                u32  (*mpfnSlot2C)(void* lpIo);                             // +0x2C
            };

            struct StreamIo
            {
                const StreamIoVTable* mpVTable; // +0x00
            };

            // The per-open context Stream::mpContext points at. DoRead/DoWrite read its
            // driver key (+0x08), its owning Device (+0x0C, whose +0x140 is the driver),
            // and stash the leading-position result back at +0x20.
            struct StreamContext
            {
                u8       macReserved00[8]; // +0x00 .. +0x07
                void*    mDriverKey;       // +0x08  key handed to the IO slots
                Device*  mpDevice;         // +0x0C  owning device (->mpDriver @ +0x140)
                u8       macReserved10[16]; // +0x10 .. +0x1F
                u64      mu64LeadPos;      // +0x20  leading-position result store
            };

            // NOTE: this is the low-level file-IO transfer object an AsyncOp reads/writes
            // through (AsyncOp::mpStream). It is NOT rw::core::filesys::Stream, the high-level
            // streaming reader (QueueFile/GetChunk/Kill, homed in stream.h/.cpp) -- that is a
            // different class; this one is named IoStream to avoid the collision.
            struct IoStream
            {
                u8             macReserved00[4]; // +0x00 .. +0x03
                StreamContext* mpContext;        // +0x04  per-open context
                void*          mDriverKey;       // +0x08  key handed to the IO slots
                Device*        mpDevice;         // +0x0C  owning Device (Read/Write latch)
                StreamIo*      mpIo;             // +0x10  IO interface (vtable dispatch)
                u8             macReserved14[4]; // +0x14
                u64            mu64WritePos;     // +0x18  DoWrite stashes slot-0x20 result here
            };

            struct AsyncOp
            {
                AsyncOp*           mpNext;          // +0x00  forward link in the AsyncOpList
                s32                miResult;        // +0x04  DoIo result / status (ctor: -3)
                u8                 mbReserved08;    // +0x08
                u8                 mbIsReadOp;      // +0x09  non-zero for read-class ops
                u8                 macReserved0A[2]; // +0x0A .. +0x0B
                s32                miPriority;      // +0x0C  scheduling priority
                OpStream*          mpStream;        // +0x10  owning Stream/File (or result Handle)
                uintptr_t          mUserData;       // +0x14  opaque user word for the callback
                                                    //        (pointer-width: the filesys Stream
                                                    //        stashes its engine pointer here and
                                                    //        recovers it in its op callbacks --
                                                    //        X360 stored a 32-bit word)
                union                               // +0x18
                {
                    CompletionCallback mpfnComplete; // scheduler: `stw 0x18`, then call
                    u64                mu64Size;     // free GetSize: `ld 0x18`
                };
                u32                muReserved1C;     // +0x1C  (tail of the +0x18 64-bit slot)
                u64                mu64Position;    // +0x20  64-bit file offset
                u64                mu64ByteCount;   // +0x28  64-bit byte count requested
                void*              mpBuffer;        // +0x30  transfer buffer
                u32                muReserved34;     // +0x34
                u64                mu64BytesDone;   // +0x38  running transferred bytes
                Device*            mpDevice;        // +0x40  the Device this op runs on
                DoIoCallback       mpfnDoIo;        // +0x44  transfer entry point

                // AsyncOp::AsyncOp @0x82BBD3E0 -- zero/-3 init of the whole object.
                AsyncOp();

                // AsyncOp::~AsyncOp @0x82BBD430 -- lwsync, then zero +0x10/+0x14/+0x30/+0x40
                // and the link +0x00.
                ~AsyncOp();

                // The four request entry points (@0x82BC00C0 / @0x82BC01D0 / @0x82BBFCF0 /
                // @0x82BBFB48): fill the op, pick the Device, and Device::InsertOp it.
                s32 Open(const char* lpcPath, u32 luFlags, CompletionCallback lpfnComplete,
                         uintptr_t luUserData, s32 liPriority);
                s32 Read(void* lpStream, void* lpBuffer, u64 lu64Position, u64 lu64Count,
                         CompletionCallback lpfnComplete, uintptr_t luUserData, s32 liPriority);
                s32 Write(void* lpStream, void* lpBuffer, u64 lu64Position, u64 lu64Count,
                          CompletionCallback lpfnComplete, uintptr_t luUserData, s32 liPriority);
                s32 Close(void* lpStream, CompletionCallback lpfnComplete,
                          uintptr_t luUserData, s32 liPriority);

                // The transfer callbacks the scheduler invokes (@0x82BBFA70 / @0x82BBFBB8 /
                // @0x82BBD450 / @0x82BBE010). Static because mpfnDoIo points at them.
                static s32 DoOpen(AsyncOp* lpOp);
                static s32 DoRead(AsyncOp* lpOp);
                static s32 DoWrite(AsyncOp* lpOp);
                static s32 DoClose(AsyncOp* lpOp);

                // Blocking result accessors (@0x82BBE0C0 / @0x82BBE118 / @0x82BBE058).
                void* GetResultHandle();
                u64   GetResultSize();
                s32   GetStatus(const EA::Thread::ThreadTime& lTimeoutAbsolute);

                // SetPriority @0x82BBFD80 -- re-prioritise via Device::ChangeOpPriority.
                s32 SetPriority(s32 liPriority);
            };

            // GetSize @0x82BBD700:  ld r3, 0x18(r3)  -- return the 64-bit size at +0x18.
            u64 GetSize(const AsyncOp* lpOp);

            // The Device's priority-ordered op queue (nodes link through AsyncOp::mpNext).
            typedef detail::ListSingle<AsyncOp> AsyncOpList;

            // An open filesystem handle (0x28 bytes on the console; DoOpen allocates it).
            // Named fields at the console offsets:
            //   +0x00 mField0        : cleared by ctor/dtor
            //   +0x04 mpOwnerHandle  : this handle, or the handle the driver's Open linked
            //                          the file to (its out-parameter)
            //   +0x08 mpFile         : the driver's open-file object; null when the open failed
            //   +0x0C mpDevice       : the Device the file was found on
            //   +0x10 mpDriver       : that Device's driver (~Handle closes mpFile through it)
            //   +0x18 mu64Size       : the file size the driver reported at open
            //   +0x20 mu64Reserved20 : cleared by the ctor
            struct Handle
            {
                // Handle::Handle: open lpcPath through lpDevice's driver (walking
                // the Manager's search paths when lpDevice is the default device).
                Handle(const char* lpcPath, u32 luFlags, Device* lpDevice);

                ~Handle();

                u32           mField0;         // +0x00
                Handle*       mpOwnerHandle;   // +0x04
                void*         mpFile;          // +0x08
                Device*       mpDevice;        // +0x0C
                DeviceDriver* mpDriver;        // +0x10
                u64           mu64Size;        // +0x18  file size (Stream open/close callbacks
                                               //         copy it into StreamState::mu64FileSize)
                u64           mu64Reserved20;  // +0x20
            };

            // The process-wide filesys allocator (X360 off_8327F07C). The scalar deleting
            // destructor frees a heap Handle through its vtable slot +0xC -- the 4th pointer
            // == Free(this, 0). Modelled faithful to that indirect-call shape; the concrete
            // object is installed by the filesys bring-up (null until then).
            //
            // Alloc (vtable slot +0x08) is the full EA-allocator signature proven by the
            // Manager/AsyncOp allocation call sites:
            //   (*(*alloc + 4))(alloc, size, "name", flags, align, alignOffset)
            class Allocator
            {
            public:
                virtual void  Slot0() = 0;          // +0x00
                virtual void* Alloc(u32 luSize, const char* lpcName, u32 luFlags,
                                    u32 luAlign, u32 luAlignOffset) = 0;  // +0x08 (2nd entry)
                virtual void  Free(void* lpBlock, unsigned int luFlags) = 0;  // +0xC (4th entry)
            };

            // X360 off_8327F07C -- defined by the device-driver TU; null until installed.
            extern Allocator* gpFileSysAllocator;

            // Handle::`scalar deleting destructor' @0x82BBDFA8: ~Handle, then if (flags & 1)
            // free `this` through gpFileSysAllocator. Returns lpHandle (X360 r3 == this).
            Handle* HandleScalarDeletingDtor(Handle* lpHandle, char lcFlags);
        }
    }
}
