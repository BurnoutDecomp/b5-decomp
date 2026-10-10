// =====================================================================================
// vp6 -- allocator facade of the VP6 video decoder used by the movie player.
//
// These three functions are not the codec itself; they are the namespace-level allocation
// shim the decoder routes its heap traffic through (duck_malloc / duck_mallocAlign /
// duck_free / duck_freeAlign call vp6::Alloc / vp6::Free), and CgsGraphics::MoviePlayer
// installs and removes the allocator with SetAllocator around the decoder's lifetime.
//
// The installed object is the movie player's MoviePlayerCoreAllocator, an
// EA::Allocator::ICoreAllocator: Alloc dispatches to its plain Alloc(size, name, flags) with
// the "VP6" tag and flag 1, Free to Free(block, 0). With no allocator installed the CRT heap
// serves both.
// =====================================================================================

#include "SDKs/EATech/include/Common/vp6/vp6_decoder.h"
#include "coreallocator/icoreallocator_interface.h"

#include <cstdlib> // malloc / free fallback

namespace vp6
{
    // The process-wide VP6 allocator (null until SetAllocator installs one).
    static EA::Allocator::ICoreAllocator* spAllocator = 0;

    // Store the supplied allocator (null removes it).
    void SetAllocator(EA::Allocator::ICoreAllocator* lpAllocator)
    {
        spAllocator = lpAllocator;
    }

    // Route through the installed allocator, or fall back to malloc when none is installed.
    void* Alloc(unsigned int luSize)
    {
        if (spAllocator)
        {
            return spAllocator->Alloc(luSize, "VP6", 1);
        }
        return malloc(luSize);
    }

    // Route through the installed allocator, or fall back to free when none is installed.
    void Free(void* lpBlock)
    {
        if (spAllocator)
        {
            spAllocator->Free(lpBlock, 0);
        }
        else
        {
            free(lpBlock);
        }
    }
}
