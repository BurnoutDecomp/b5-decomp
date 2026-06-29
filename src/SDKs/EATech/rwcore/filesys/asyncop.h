#pragma once

#include "types.hpp"

// rw::core::filesys -- async-op / handle primitives of the RenderWare core filesystem.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for these TUs. Members are modelled
// by name at the observed X360 offsets (no raw-offset casts at the call sites).
//
//   rw::core::filesys::AsyncOpList::InsertAfter @0x82BBDE70
//   rw::core::filesys::AsyncOpList::Remove      @0x82BBDED0
//   rw::core::filesys::GetSize                  @0x82BBD700
//   rw::core::filesys::Handle::~Handle          @0x82BBD6A0
//   rw::core::filesys::Handle::~Handle (scalar deleting) @0x82BBDFA8

namespace rw
{
    namespace core
    {
        namespace filesys
        {
            // Forward decl of the device a Handle binds to. Its vtable slot +0x10
            // (5th entry) is the close/release method ~Handle invokes when open.
            class DeviceBase;

            // An outstanding async filesystem operation. Fields are named at the X360
            // offsets proven by the asm of the AsyncOp-list helpers AND of the Device
            // scheduler TU (Device::InsertOp / CheckForOptimalReadOp / ThreadEntry, which
            // walk this object by `this+N`). Offsets verified from BURNOUT_X360_ARTIST.XEX:
            //   +0x00 mpNext      : intrusive forward link (AsyncOpList node link)
            //   +0x04 miResult    : DoIo result/status (Device::ThreadEntry stores it here)
            //   +0x09 mbIsReadOp  : non-zero for read-class ops the scheduler may reorder
            //   +0x0C miPriority  : queue priority (Device sorts the op list by this)
            //   +0x10 mpStream    : owning stream/handle; the scheduler reads (*mpStream)+8
            //   +0x18 (union)     : 64-bit size (GetSize) OR the completion callback ptr
            //                       the scheduler invokes once the op's DoIo finishes
            //   +0x20 mu64Position: 64-bit byte position used by the read-coalescing math
            //   +0x44 mpfnDoIo    : the op's transfer entry point (Device calls it)
            //
            // The +0x18 slot is modelled as a union: GetSize @0x82BBD700 reads the 64-bit
            // word there (`ld 0x18`) while the Device scheduler reads a callback pointer at
            // the same offset (`lwz 0x18`); both views are byte-for-byte the X360 object.
            struct AsyncOp;
            typedef void (*CompletionCallback)(AsyncOp* lpOp);

            // The object AsyncOp::mpStream points at. The Device scheduler only reads the
            // word at +0x08 (`*(*(op+0x10)+8)`) -- the handle/key it hands to the device
            // driver's block-size vtable method -- so only that field is named here.
            struct OpStream
            {
                u8    macReserved0[8]; // +0x00 .. +0x07
                void* mpDriverKey;     // +0x08  key passed to DeviceDriver::GetBlockSize
            };

            struct AsyncOp
            {
                AsyncOp*  mpNext;          // +0x00  forward link in the AsyncOpList
                s32       miResult;        // +0x04  DoIo result / status
                u8        mbReserved08;    // +0x08
                u8        mbIsReadOp;       // +0x09  non-zero for read-class ops
                u8        macReserved0A[2]; // +0x0A .. +0x0B
                s32       miPriority;      // +0x0C  scheduling priority
                OpStream* mpStream;        // +0x10  owning stream/handle (scheduler reads +8)
                u8        macReserved14[4]; // +0x14 .. +0x17
                union                      // +0x18
                {
                    u64               mu64Size;       // GetSize: `ld 0x18`
                    CompletionCallback mpfnComplete;  // scheduler: `lwz 0x18`, then call
                };
                u64       mu64Position;    // +0x20  byte position (read-coalescing math)
                u8        macReserved28[0x1C]; // +0x28 .. +0x43
                s32       (*mpfnDoIo)(AsyncOp* lpOp); // +0x44  transfer entry point
            };

            // GetSize @0x82BBD700:  ld r3, 0x18(r3)  -- return the 64-bit size at +0x18.
            u64 GetSize(const AsyncOp* lpOp);

            // rw::core::filesys::IntrusiveList<AsyncOp> -- the singly-linked op list head.
            //   +0x00 mpHead  : first node
            //   +0x04 mpTail  : last node
            //   +0x08 muCount : node count
            // Nodes link forward via AsyncOp::mpNext (node+0). The TU id carries the
            // trailing '>' of the template instantiation rw::core::filesys::...<AsyncOp>.
            struct AsyncOpList
            {
                AsyncOp*  mpHead;   // +0x00
                AsyncOp*  mpTail;   // +0x04
                u32       muCount;  // +0x08

                // InsertAfter @0x82BBDE70: insert lpNode after lpAfter (after == null ->
                // push front). Maintains mpTail when inserting at the end and bumps muCount.
                AsyncOpList* InsertAfter(AsyncOp* lpAfter, AsyncOp* lpNode);

                // Remove @0x82BBDED0: unlink lpNode. lpFrom is an optional predecessor node
                // to start the forward scan from (null -> scan from mpHead). Returns 1 if
                // removed (and clears lpNode->mpNext), 0 otherwise.
                int Remove(AsyncOp* lpNode, AsyncOp* lpFrom);
            };

            // An open filesystem handle. Named fields at the X360 offsets:
            //   +0x00 mField0
            //   +0x04 mField1
            //   +0x08 mbIsOpen : non-zero while bound to a live device handle
            //   +0x0C mField3
            //   +0x10 mpDevice : the owning device (its vtable +0x10 == close/release)
            struct Handle
            {
                ~Handle();

                u32         mField0;    // +0x00
                u32         mField1;    // +0x04
                u32         mbIsOpen;   // +0x08
                u32         mField3;    // +0x0C
                DeviceBase* mpDevice;   // +0x10
            };

            // The device base the Handle releases through on close. Modelled as a single
            // vtable with the close/release method at slot +0x10 (the 5th pointer), matching
            // the X360 indirect call `(*(*mpDevice + 0x10))(mpDevice)` in ~Handle.
            class DeviceBase
            {
            public:
                virtual void Slot0() = 0;
                virtual void Slot1() = 0;
                virtual void Slot2() = 0;
                virtual void Slot3() = 0;
                virtual void Release() = 0;  // slot +0x10 (5th entry)
            };

            // The process-wide filesys allocator (X360 off_8327F07C). The scalar deleting
            // destructor frees a heap Handle through its vtable slot +0xC -- the 4th pointer
            // == Free(this, 0). Modelled faithful to that indirect-call shape; the concrete
            // object is installed by the filesys bring-up (null until then).
            class Allocator
            {
            public:
                virtual void  Slot0() = 0;          // +0x00
                virtual void* Alloc(unsigned int luSize) = 0;  // +0x08 (2nd entry)
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
