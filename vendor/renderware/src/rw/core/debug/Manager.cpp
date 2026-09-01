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
// ManagerAllocator.h). dword_8327F048 is the singleton IFormatter instance pointer. The
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
#include "rw/core/debug/ManagerAllocator.h"
#include <new>

namespace rw { namespace core { namespace debug {

    namespace detail {

        // X360 off_8327F044 -- the resource allocator the debug subsystem carves through.
        // Defined here (Manager owns it); IFormatter reads it via ManagerAllocatorSlot().
        ::rw::IResourceAllocator*& ManagerAllocatorSlot()
        {
            static ::rw::IResourceAllocator* spAllocator = nullptr;
            return spAllocator;
        }

        // X360 dword_8327F048 -- the singleton IFormatter instance published by CreateInstance.
        ::rw::core::debug::IFormatter* spFormatterInstance = nullptr;

    }  // namespace detail

    // X360 0x82BBD380 -- record the allocator, carve + publish the singleton formatter.
    int Manager::CreateInstance(::rw::IResourceAllocator* lpAllocator)
    {
        detail::ManagerAllocatorSlot() = lpAllocator;

        // operator new carves one IFormatter (4 bytes on the X360 build = one vptr) through
        // the subsystem allocator.
        void* lpStorage = IFormatter::operator new(sizeof(Formatter));

        if (lpStorage)
        {
            detail::spFormatterInstance = ::new (lpStorage) Formatter;
        }
        else
        {
            detail::spFormatterInstance = nullptr;
        }

        return 0;
    }

}}}  // namespace rw::core::debug
