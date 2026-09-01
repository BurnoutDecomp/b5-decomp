// =====================================================================================
// rw::ResourceAllocatorRegistry -- accessors for the process-wide default resource
// allocator slot.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for this TU.
//
//   rw::ResourceAllocatorRegistry::GetDefaultAllocator  @0x82BBD1E0
//   rw::ResourceAllocatorRegistry::SetDefaultAllocator  @0x82BBC490
//
// GetDefaultAllocator (store-for-store, @0x82BBD1E0):
//
//   if ((dword_8327F070 & 1) == 0) {                 // once guard, bit 0
//       dword_8327F070 |= 1;
//       rw::DefaultSystemAllocatorInitializer::DefaultSystemAllocatorInitializer(
//           &unk_8327F06C);                          // run the lazy installer once
//   }
//   return rw::ResourceAllocatorRegistry::s_defaultAllocator;
//
// unk_8327F06C is the static DefaultSystemAllocatorInitializer instance (sizeof 1) and
// dword_8327F070 is its compiler once-guard. Running its ctor points s_defaultAllocator
// at the static platform-native system allocator the first time, if still null (see
// DefaultSystemAllocatorInitializer.cpp). The guard is the compiler's local-static
// initialisation fence; the bit test is non-atomic, matching the X360's single-threaded
// bring-up call site. s_defaultAllocator itself is defined in
// DefaultSystemAllocatorInitializer.cpp.
//
// SetDefaultAllocator (store-for-store, @0x82BBC490): a plain store of the incoming
// pointer into the static slot.
// =====================================================================================

#include "rw/rwcore_structs.h"  // rw::ResourceAllocatorRegistry, DefaultSystemAllocatorInitializer

namespace rw {

// X360 0x82BBD1E0 -- lazy-init the default allocator (once) then return the slot.
IResourceAllocator* ResourceAllocatorRegistry::GetDefaultAllocator()
{
    // unk_8327F06C: the static DefaultSystemAllocatorInitializer; dword_8327F070: its
    // once-guard. A function-local static gives the same one-shot construction fence the
    // X360 open-coded with the guard word.
    static DefaultSystemAllocatorInitializer s_initializer;
    (void)s_initializer;

    return s_defaultAllocator;
}

// X360 0x82BBC490 -- overwrite the default-allocator slot.
void ResourceAllocatorRegistry::SetDefaultAllocator(IResourceAllocator* lpAllocator)
{
    s_defaultAllocator = lpAllocator;
}

}  // namespace rw
