#include "GameShared/GameClasses/Sound/CgsMemBase.h"
#include "rw/rwcore_structs.h"

namespace CgsSound
{
rw::IResourceAllocator* MemBase::mpAllocator = 0;

MemBase::~MemBase() {}

void* MemBase::operator new(size_t luSize, const char* lpcName)
{
    return operator new(luSize, lpcName, false);
}

// ARTIST826AB5C8: both allocation flavours use the same five-lane descriptor.
// The bool is present in the original declaration but unused by this build.
void* MemBase::operator new(size_t luSize, const char* lpcName, bool /*lbPermanent*/)
{
    rw::ResourceDescriptor lDescriptor;
    lDescriptor.m_baseResourceDescriptors[0].m_size = static_cast<u32>(luSize);
    lDescriptor.m_baseResourceDescriptors[0].m_alignment = 16;
    rw::Resource lResource = mpAllocator->DoAllocate(lDescriptor, lpcName);
    return lResource.m_baseResources[0];
}

// The deleting destructor826AB548 passes one client pointer and four null lanes.
void MemBase::operator delete(void* lpMemory)
{
    rw::Resource lResource;
    lResource.m_baseResources[0] = lpMemory;
    mpAllocator->DoFree(lResource);
}

void MemBase::operator delete(void* lpMemory, const char* /*lpcName*/)
{
    operator delete(lpMemory);
}

void MemBase::operator delete(void* lpMemory, const char* /*lpcName*/, bool /*lbPermanent*/)
{
    operator delete(lpMemory);
}
}
