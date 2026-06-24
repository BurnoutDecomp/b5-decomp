// =====================================================================================
// vp6 -- allocator facade for the VP6 video decoder used by the movie player.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for this TU. These three functions
// are NOT the VP6 codec itself; they are the namespace-level allocation shim the decoder
// routes its heap traffic through (duck_mallocAlign / duck_free call into vp6::Alloc /
// vp6::Free; CgsGraphics::MoviePlayer installs/removes the allocator via SetAllocator).
//
//   vp6::Alloc         @0x82B58670
//   vp6::Free          @0x82B586A8
//   vp6::SetAllocator  @0x82B58660
//
// off_83270FD0 is the process-wide VP6 allocator object (a polymorphic allocator
// interface). When installed, Alloc/Free dispatch through its vtable; otherwise they fall
// back to the CRT malloc/free.
//   Alloc: r9=*allocator (vtable); call vtable+8 with (this, liSize, "VP6", 1).
//   Free : r10=*allocator (vtable); call vtable+0xC with (this, lpBlock, 0).
// The two leading vtable slots (offsets +0,+4) are exercised elsewhere in the codec and
// are declared here only as reserved virtuals so the called slots land at the asm-attested
// vtable offsets +8 (Allocate) and +0xC (Deallocate).
// =====================================================================================

#include "types.hpp"

#include <cstdlib> // malloc / free fallback

namespace vp6
{
    // Polymorphic allocator interface installed via SetAllocator. The vtable slot order
    // is fixed by the X360 dispatch offsets: Allocate is vtable+8, Deallocate is vtable+0xC.
    // Slots +0 and +4 are reserved (declared-only) so the called methods sit at the right
    // offsets; their exact identity is not attested by this TU.
    class IAllocator
    {
    public:
        virtual void  ReservedSlot0() = 0;                                   // vtable +0x00
        virtual void  ReservedSlot1() = 0;                                   // vtable +0x04
        virtual void* Allocate(u32 luSize, const char* lpcTag, int liFlag) = 0; // vtable +0x08
        virtual void  Deallocate(void* lpBlock, int liFlag) = 0;             // vtable +0x0C
    };

    // off_83270FD0 -- the process-wide VP6 allocator (null until SetAllocator installs one).
    static IAllocator* spAllocator = 0;

    // vp6::SetAllocator @0x82B58660 -- store the supplied allocator into the file static.
    void* SetAllocator(void* lpAllocator)
    {
        spAllocator = static_cast<IAllocator*>(lpAllocator);
        return lpAllocator;
    }

    // vp6::Alloc @0x82B58670 -- route through the installed allocator (tag "VP6", flag 1)
    // or fall back to malloc when none is installed.
    void* Alloc(u32 luSize)
    {
        if (spAllocator)
        {
            return spAllocator->Allocate(luSize, "VP6", 1);
        }
        return malloc(luSize);
    }

    // vp6::Free @0x82B586A8 -- route through the installed allocator (flag 0) or fall back
    // to free when none is installed.
    void Free(void* lpBlock)
    {
        if (spAllocator)
        {
            spAllocator->Deallocate(lpBlock, 0);
        }
        else
        {
            free(lpBlock);
        }
    }
}
