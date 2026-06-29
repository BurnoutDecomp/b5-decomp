// =====================================================================================
// rw::core::debug::IFormatter -- allocation hooks for the abstract debug formatter.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for this TU.
//
//   rw::core::debug::IFormatter::operator new            @0x82BBCBF0
//   rw::core::debug::IFormatter::`scalar deleting dtor'  @0x82BBCB50
//
// operator new (@0x82BBCBF0) carves `size` bytes through the debug subsystem's resource
// allocator (off_8327F044, shared with Manager -- see ManagerAllocator.h). It builds a
// resource descriptor on the stack: five {m_size, m_alignment} entries default-initialised
// to {0, 1}, then overwrites entry[0] with {m_size = size, m_alignment = 4}, and calls the
// allocator's DoAllocate(descriptor, /*name*/ 0). It returns the first base resource pointer
// of the carved Resource (Resource::m_baseResources[0]).
//
//   X360 init loop (v2 = 4; do { --v2; *p = 0; p[1] = 1; p += 2; } while (v2 >= 0)) writes
//   five descriptor entries; the QWORD store `HIDWORD(v4)=size; LODWORD(v4)=4; v6[0]=v4`
//   lands (big-endian) as entry[0] = { m_size = size, m_alignment = 4 }.
//
// The scalar deleting destructor (@0x82BBCB50) resets the vptr to the IFormatter base vtable
// (off_82181108) and, iff the delete flag (a2 bit 0) is set, releases the block through the
// same allocator: rw::IResourceAllocator::Free(off_8327F044, this). It is modelled here as
// the matching operator delete (the MSVC `delete p` lowering: run dtor, then free if owned).
//
// [PC MODEL] rwcore_structs.h models IFormatter / Formatter as raw-vptr POD structs (a
// single `void* __vftable`), not a C++ polymorphic hierarchy. The descriptor is the PC
// rw::ResourceDescriptor (four <4> entries) the modelled DoAllocate consumes -- the X360
// game build serialises the <5> form, but only entry[0] is non-identity, so the carve is
// equivalent. The Format vtable slot belongs to the concrete Formatter (a separate TU); the
// vptr-reset side effect of the destructor is a no-op here (we hold no vtable symbol).
// =====================================================================================

#include "rw/rwcore_structs.h"  // rw::core::debug::IFormatter, rw::IResourceAllocator, rw::ResourceDescriptor
#include "rw/core/debug/ManagerAllocator.h"

namespace rw { namespace core { namespace debug {

    // X360 0x82BBCBF0 -- carve `luSize` bytes through the subsystem resource allocator.
    void* IFormatter::operator new(size_t luSize)
    {
        // The asm (0x82BBCC40) loads off_8327F044 and dereferences it UNCONDITIONALLY -- there is
        // no null-test on the allocator (CreateInstance always installs it before operator new is
        // reachable), so we carve straight through it to match the binary's control flow.
        ::rw::IResourceAllocator* lpAllocator = detail::ManagerAllocatorSlot();

        // Build the carve descriptor: every entry defaults to the identity {m_size = 0,
        // m_alignment = 1} (rw::BaseResourceDescriptor's ctor), then entry[0] requests the
        // formatter's storage: { m_size = luSize, m_alignment = 4 }.
        ::rw::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size = static_cast<uint32_t>(luSize);
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 4u;

        // DoAllocate carves the Resource; the formatter lives in the first base region.
        ::rw::Resource lResource = lpAllocator->DoAllocate(lDescriptor, nullptr);
        return lResource.m_baseResources[0];
    }

    // X360 0x82BBCB50 (scalar deleting destructor, delete-flag path) -- release the block
    // through the same subsystem allocator. The vptr reset to the IFormatter base vtable is a
    // no-op in the raw-vptr PC model.
    void IFormatter::operator delete(void* lpBlock)
    {
        if (!lpBlock)
        {
            return;
        }

        ::rw::IResourceAllocator* lpAllocator = detail::ManagerAllocatorSlot();
        if (lpAllocator)
        {
            lpAllocator->Free(lpBlock);
        }
    }

}}}  // namespace rw::core::debug
