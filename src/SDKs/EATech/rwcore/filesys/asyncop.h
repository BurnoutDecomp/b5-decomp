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

            // An outstanding async filesystem operation. Only the fields touched by the
            // group's TUs are named; the rest of the X360 object is reserved space so the
            // named members land at their observed offsets.
            //   +0x00 mpNext  : intrusive forward link (AsyncOpList node link)
            //   +0x18 mu64Size : 64-bit size queried by GetSize
            struct AsyncOp
            {
                AsyncOp*  mpNext;          // +0x00  forward link in the AsyncOpList
                u8        macReserved0[0x14]; // +0x04 .. +0x17 (op state: not in this group)
                u64       mu64Size;        // +0x18  byte size of the operation/transfer
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
