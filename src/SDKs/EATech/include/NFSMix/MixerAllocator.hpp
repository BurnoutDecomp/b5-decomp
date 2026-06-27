#pragma once

// ===========================================================================
//  MixerAllocator -- the EATech mixer-system allocator (the global off_83250004).
//  The X360 audio mixer dispatches Allocate via vtable slot 2 (+0x08) and Free via
//  slot 3 (+0x0C). It is the single allocation source for the whole NFS mix +
//  Nicotine layer (NFSMixMap's mixer-memory blocks, the NFSMixMap object itself,
//  the IDynamicMixer internals -- see Nicotine::IDynamicMixer::CreateInstance
//  @0x82B44AD0, which installs the system into off_83250004 and allocates through it).
//
//  Modelled as a minimal interface matching that vtable shape (slots 0/1 are the
//  dtor + an unmodelled hook; Allocate is slot 2, Free is slot 3). FLAG: the X360
//  backing is a dedicated mixer heap; the PC backing mallocs at the leaf (the
//  established "faithful shape, PC allocation at the boundary" pattern).
// ===========================================================================

class MixerAllocator
{
public:
    virtual ~MixerAllocator() {}                    // vtable slot 0
    virtual void Reserved1() {}                      // vtable slot 1 (unmodelled)
    // slot 2 (+0x08): Allocate(size, alignment, debug-name) -> block, or null.
    virtual void* Allocate(unsigned int luSize, unsigned int luAlign, const char* lpcName);
    // slot 3 (+0x0C): Free(block, flag).
    virtual void  Free(void* lpPtr, int liFlag);
};

// off_83250004 -- the process-wide mixer-system allocator singleton.
extern MixerAllocator* g_pMixerAllocator;
