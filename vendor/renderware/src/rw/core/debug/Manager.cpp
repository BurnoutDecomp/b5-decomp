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
// [PC MODEL] rwcore_structs.h models rw::core::debug::IFormatter / Formatter as
// layout-faithful POD-with-raw-vptr structs (a single `void* __vftable` field), not as a
// C++ polymorphic hierarchy. The Format vtable slot belongs to Formatter, whose Format body
// is a separate (not-yet-reconstructed) TU. We therefore carve and publish the singleton
// exactly as the binary does, but leave the concrete vtable pointer unresolved: off_82181110
// is rodata we do not have, and the rules forbid fabricating it. The vptr is left null
// (flagged below); wiring it is the Formatter TU's job, which owns ??_7Formatter@...@.
// =====================================================================================

#include "rw/rwcore_structs.h"  // rw::core::debug::Manager / IFormatter, rw::IResourceAllocator
#include "rw/core/debug/ManagerAllocator.h"

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
        IFormatter* lpFormatter =
            static_cast<IFormatter*>(IFormatter::operator new(sizeof(IFormatter)));

        if (lpFormatter)
        {
            // X360: *v1 = off_82181110 -- install the concrete Formatter vtable into the
            // carved storage. off_82181110 (??_7Formatter@debug@core@rw@@6B@) is rodata we do
            // not have; the rules forbid fabricating a vtable pointer, and Formatter::Format
            // (the slot it points at) is a separate TU. Left as a flagged placeholder.
            lpFormatter->__vftable = nullptr;  // FLAGGED: unresolved off_82181110 (Formatter vtable)
            detail::spFormatterInstance = lpFormatter;
        }
        else
        {
            detail::spFormatterInstance = nullptr;
        }

        return 0;
    }

}}}  // namespace rw::core::debug
