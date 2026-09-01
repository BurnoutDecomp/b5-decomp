// =====================================================================================
// rw::DefaultSystemAllocatorInitializer -- the one-shot installer for the process-wide
// default resource allocator.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for this TU.
//
//   rw::DefaultSystemAllocatorInitializer::DefaultSystemAllocatorInitializer @0x82BBD160
//
// The ctor is a Meyers-style lazy initialiser invoked by
// rw::ResourceAllocatorRegistry::GetDefaultAllocator. The X360 body (store-for-store):
//
//   if ((dword_8327F068 & 1) == 0) {          // once guard, bit 0
//       dword_8327F068 |= 1;
//       dword_8327F064 = off_82181118;        // install the X360 native allocator vtable
//       atexit(sub_82C75AB0);                 // register teardown
//   }
//   if (!rw::ResourceAllocatorRegistry::s_defaultAllocator)
//       rw::ResourceAllocatorRegistry::s_defaultAllocator = &dword_8327F064;
//
// dword_8327F064 is a single-word object whose only field is a vptr -- i.e. the static
// X360 native allocator instance (off_82181118 == its vtable). The PC build selects
// its own native system allocator behind the same Paradise IResourceAllocator contract.
// The guard + atexit pair is the compiler's local-static initialisation
// fence; std::call_once-free here because the registry call site is single-threaded
// bring-up, matching the X360's non-atomic bit test.
// =====================================================================================

#include "rw/rwcore_structs.h"

namespace rw {

// The platform-native allocator has static storage, matching the ARTIST lifetime.
#if defined(D_PLATFORM_X360)
static Xbox2NativeSystemAllocator s_systemAllocator;
#else
static SystemAllocatorGeneric s_systemAllocator;
#endif

// X360 0x82BBD160 -- the lazy installer. a1 (r3) is the `this` of the
// DefaultSystemAllocatorInitializer; the ctor returns it unchanged.
DefaultSystemAllocatorInitializer::DefaultSystemAllocatorInitializer()
{
    if (!ResourceAllocatorRegistry::s_defaultAllocator)
    {
        ResourceAllocatorRegistry::s_defaultAllocator = &s_systemAllocator;
    }
}

// Storage for the additive static slot grown into ResourceAllocatorRegistry.
IResourceAllocator* ResourceAllocatorRegistry::s_defaultAllocator = nullptr;

}  // namespace rw
