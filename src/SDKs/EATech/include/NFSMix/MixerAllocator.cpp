#include "SDKs/EATech/include/NFSMix/MixerAllocator.hpp"
#include <malloc.h> // _aligned_malloc / _aligned_free (MSVC)

// ===========================================================================
//  MixerAllocator -- PC backing for the EATech mixer-system allocator (off_83250004).
//  FLAG (PC boundary): the X360 routes a dedicated mixer heap; on PC we back it with
//  aligned malloc at the leaf. Allocate honours the requested alignment; the debug name
//  is ignored. This is the established "faithful shape, PC allocation at the leaf" pattern.
// ===========================================================================

void* MixerAllocator::Allocate(unsigned int luSize, unsigned int luAlign, const char* /*lpcName*/)
{
    if (luAlign < sizeof(void*))
        luAlign = sizeof(void*);
    return _aligned_malloc(luSize, luAlign);
}

void MixerAllocator::Free(void* lpPtr, int /*liFlag*/)
{
    _aligned_free(lpPtr);
}
