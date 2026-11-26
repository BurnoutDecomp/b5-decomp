// XDK size optimization: ARTIST compiled RenderWare's rwcore for size (record-form cr0
// tests, as in the rwcore filesys sources);
// debug::Manager::CreateInstance 0x82BBD380 matches only so.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif
// =====================================================================================
// rw::core::debug::Manager -- bring-up of the RenderWare core debug subsystem.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for this TU.
//
//   rw::core::debug::Manager::CreateInstance  @0x82BBD380
//
// X360 store-for-store (@0x82BBD380):
//
//   off_8327F044  = a1;                              // record the subsystem allocator
//   v1 = rw::core::debug::IFormatter::operator new(4);
//   if (v1) {                                        // r3 != 0
//       *v1 = off_82181110;                          // install the concrete formatter vtable
//       dword_8327F048 = v1;                         // publish the singleton
//   } else {
//       dword_8327F048 = 0;
//   }
//   return 0;
//
// off_8327F044 is the subsystem resource allocator slot (shared with IFormatter, see
// Manager::sAllocator). dword_8327F048 is the singleton IFormatter instance pointer. The
// IFormatter operator new carves four bytes (one vptr on the X360's 32-bit build) through
// that allocator; CreateInstance then stores the concrete formatter vtable (off_82181110 =
// ??_7Formatter@debug@core@rw@@6B@, the rw::core::debug::Formatter implementation) into the
// carved storage's vptr slot -- i.e. it constructs the concrete formatter in place. The
// return value is always 0 (a success sentinel; callers read the singleton back through the
// global rather than through the return).
//
// The host class hierarchy is polymorphic, so placement construction of Formatter performs
// the same vtable installation as the explicit X360 store at 0x82BBD3B0.
// =====================================================================================

#include "rw/rwcore_structs.h"  // rw::core::debug::Manager / IFormatter, rw::IResourceAllocator
#include <new>

namespace rw { namespace core { namespace debug {

    // X360 0x8327F044 / 0x8327F048.
    ::rw::IResourceAllocator* Manager::sAllocator = 0;
    IFormatter* Manager::sFormatter = 0;

    // DecFIGS manager.h:284; ARTIST inlines it into the IFormatter/Formatter scalar deleting
    // destructors 0x82BBCB78/0x82BBCBC8, emitted with the Manager code.
    void IFormatter::operator delete(void* lpBlock)
    {
        // ARTIST 0x82BBCB78: Free through Manager::sAllocator, no null tests.
        Manager::sAllocator->Free(lpBlock);
    }

    // X360 0x82BBD380 -- record the allocator, carve + publish the singleton formatter.
    int Manager::CreateInstance(::rw::IResourceAllocator* lpAllocator)
    {
        sAllocator = lpAllocator;

        // operator new carves one IFormatter (4 bytes on the X360 build = one vptr) through
        // the subsystem allocator.
        void* lpStorage = IFormatter::operator new(sizeof(Formatter));

        if (lpStorage)
        {
            sFormatter = ::new (lpStorage) Formatter;
        }
        else
        {
            sFormatter = 0;
        }

        return 0;
    }

}}}  // namespace rw::core::debug
