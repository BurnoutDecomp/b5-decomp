// On2 VP6 decoder: the allocation adapters for the decoder's configured heap.

#include "SDKs/EATech/include/Common/vp6/vp6_decoder.h"

extern "C"
{

void* duck_malloc(unsigned int size, int /*type*/)
{
    return vp6::Alloc(size);
}

// Over-allocate, align, and keep the block the allocator returned just below the aligned
// pointer so duck_freeAlign can hand it back.
void* duck_mallocAlign(unsigned int size, unsigned int align, int /*type*/)
{
    unsigned char* lpRaw = static_cast<unsigned char*>(vp6::Alloc(size + align + sizeof(void*)));
    unsigned char* lpAligned = reinterpret_cast<unsigned char*>(
        (reinterpret_cast<uintptr_t>(lpRaw) + align + sizeof(void*)) & ~static_cast<uintptr_t>(align - 1));
    reinterpret_cast<void**>(lpAligned)[-1] = lpRaw;
    return lpAligned;
}

void duck_free(void* ptr)
{
    vp6::Free(ptr);
}

void duck_freeAlign(void* ptr)
{
    vp6::Free(static_cast<void**>(ptr)[-1]);
}

}
