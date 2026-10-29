#ifndef CGS_SOUND_CGSMEMBASE_H
#define CGS_SOUND_CGSMEMBASE_H

#include "types.hpp"
namespace rw { struct IResourceAllocator; }

namespace CgsSound
{
// ARTIST826AB548/826AB5C8; DecFIGS CgsMemBase.h68..434.
// Sound objects share the allocator installed by Logic::Module::Prepare.
class MemBase
{
public:
    MemBase() {}
    virtual ~MemBase();

    static void* operator new(size_t luSize, const char* lpcName);
    static void* operator new(size_t luSize, const char* lpcName, bool lbPermanent);
    static void operator delete(void* lpMemory);
    static void operator delete(void* lpMemory, const char* lpcName);
    static void operator delete(void* lpMemory, const char* lpcName, bool lbPermanent);

    static void SetAllocator(rw::IResourceAllocator* lpAllocator) { mpAllocator = lpAllocator; }
    static rw::IResourceAllocator* GetAllocator() { return mpAllocator; }

protected:
    static rw::IResourceAllocator* mpAllocator; // ARTIST82FFB954
};
}
#endif
