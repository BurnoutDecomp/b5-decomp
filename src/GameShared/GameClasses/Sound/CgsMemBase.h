#ifndef CGS_SOUND_CGSMEMBASE_H
#define CGS_SOUND_CGSMEMBASE_H

#include "types.hpp"
#include "rw/rwcore_structs.h"   // rw::IResourceAllocator (the operators are inline)

namespace CgsSound
{
// ARTIST826AB548/826AB5C8; DecFIGS CgsMemBase.h68..434.
// Sound objects share the allocator installed by Logic::Module::Prepare.
class MemBase
{
public:
    MemBase() {}
    // DWARF CgsMemBase.h:83, inline: ARTIST 0x826AB548 (and every effect's deleting destructor)
    // stores the MemBase vtable without a call.
    virtual ~MemBase() {}

    // DWARF CgsMemBase.h:259 / :277, inline. The XDK keeps the second out of line like ARTIST
    // (0x826AB5C8, called by every CreateObject): the allocator's AllocateMemoryResource
    // (DecFIGS resourceallocator.h:178) with 16-byte alignment (li r11,0x10 at 0x826AB5D4).
    static void* operator new(size_t luSize, const char* lpcName) { return operator new(luSize, lpcName, false); }
    static void* operator new(size_t luSize, const char* lpcName, bool /*lbPermanent*/)
    {
        return mpAllocator->AllocateMemoryResource(static_cast<u32>(luSize), 16, lpcName);
    }

    // DWARF CgsMemBase.h:310, inline: ARTIST has no out-of-line copy; 0x826AB548 builds the
    // five-lane resource (lane 0 = the block) on its stack and calls the allocator's DoFree, i.e. the
    // allocator's FreeMemoryResource (DecFIGS resourceallocator.h:188).
    static void operator delete(void* lpMemory)
    {
        mpAllocator->FreeMemoryResource(lpMemory);
    }
    // DWARF CgsMemBase.h:343 / :361 (inline).
    static void operator delete(void* lpMemory, const char* /*lpcName*/) { operator delete(lpMemory); }
    static void operator delete(void* lpMemory, const char* /*lpcName*/, bool /*lbPermanent*/) { operator delete(lpMemory); }

    static void SetAllocator(rw::IResourceAllocator* lpAllocator) { mpAllocator = lpAllocator; }
    static rw::IResourceAllocator* GetAllocator() { return mpAllocator; }

protected:
    static rw::IResourceAllocator* mpAllocator; // ARTIST82FFB954
};
}
#endif
