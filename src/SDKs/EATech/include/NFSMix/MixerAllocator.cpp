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

// The global singleton. FLAG: the X360 installs the real mixer system here via
// Nicotine::IDynamicMixer::CreateInstance @0x82B44AD0; the PC build uses this leaf
// allocator instance directly.
static MixerAllocator g_MixerAllocatorPC;
MixerAllocator* g_pMixerAllocator = &g_MixerAllocatorPC;
