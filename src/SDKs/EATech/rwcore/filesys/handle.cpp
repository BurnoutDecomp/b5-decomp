// =====================================================================================
// rw::core::filesys::Handle -- destructor + scalar deleting destructor.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for these TUs.
//
//   rw::core::filesys::Handle::~Handle                 @0x82BBD6A0
//   rw::core::filesys::Handle::`scalar deleting destructor' @0x82BBDFA8
//
// ~Handle: if the handle is open (mbIsOpen != 0), release it through the owning device's
// vtable (slot +0x10, the 5th entry == the device's close/release), then zero the object
// in the X360's exact store order: mField1, mbIsOpen, mField3, mpDevice, then mField0.
//
// The scalar deleting destructor is the compiler thunk MSVC emits for `delete`: run
// ~Handle, then if (flags & 1) free the storage through the global filesys allocator
// (off_8327F07C) via its vtable slot +0xC -- Free(allocator, this, 0). Modelled with a
// faithful allocator-shaped indirect call; the global allocator object is owned by the
// device-driver TU (extern here).
// =====================================================================================

#include "asyncop.h"

namespace rw
{
    namespace core
    {
        namespace filesys
        {
            // X360 off_8327F07C -- the global filesys allocator. Installed by filesys
            // bring-up; null until then. Single definition lives here.
            Allocator* gpFileSysAllocator = nullptr;

            // ~Handle @0x82BBD6A0.
            //   if (mbIsOpen)                      // lwz r4,8(r31); cmplwi; beq
            //       mpDevice->vtable[+0x10]();      // (*(*mpDevice+0x10))(mpDevice)
            //   mField1 = 0; mbIsOpen = 0; mField3 = 0; mpDevice = 0; mField0 = 0;
            Handle::~Handle()
            {
                if (mbIsOpen)
                    mpDevice->Release();

                // Zero in the X360 store order: +4, +8, +0xC, +0x10, then +0.
                mField1  = 0;
                mbIsOpen = 0;
                mField3  = 0;
                mpDevice = nullptr;
                mField0  = 0;
            }

            // Handle::`scalar deleting destructor' @0x82BBDFA8.
            //   ~Handle(this);
            //   if (flags & 1)
            //       (*(*gpFileSysAllocator + 0xC))(gpFileSysAllocator, this, 0);  // Free
            //   return this;
            Handle* HandleScalarDeletingDtor(Handle* lpHandle, char lcFlags)
            {
                lpHandle->~Handle();
                if (lcFlags & 1)
                    gpFileSysAllocator->Free(lpHandle, 0);
                return lpHandle;
            }
        }
    }
}
