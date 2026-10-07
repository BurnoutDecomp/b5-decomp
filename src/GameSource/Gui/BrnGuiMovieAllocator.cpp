#include "GameSource/Gui/BrnGuiMovieAllocator.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnGui
{
    MovieAllocator::~MovieAllocator() = default;

    void MovieAllocator::Construct()
    {
        // ARTIST 824F95F0..95F4: only the contained linear allocator is reset.
        mGraphicsAllocator.Construct();
    }

    // ARTIST 824F9780, DWARF: both five-lane arguments are passed by value.
    bool MovieAllocator::Prepare(rw::Resource lResource, rw::ResourceDescriptor lDescriptor)
    {
        mMainAllocator.Construct(lResource.m_baseResources[0],
            static_cast<s32>(lDescriptor.m_baseResourceDescriptors[0].m_size));
        miMainAlignment = static_cast<s32>(lDescriptor.m_baseResourceDescriptors[0].m_alignment);
        mGraphicsAllocator.Create(lResource.m_baseResources[2],
            lDescriptor.m_baseResourceDescriptors[2].m_size);
        mGraphicsAllocator.SetAlignment(lDescriptor.m_baseResourceDescriptors[2].m_alignment);
        return true;
    }

    // The two original allocator teardowns are inlined at 82507BB4..BBC4.
    bool MovieAllocator::Release()
    {
        mMainAllocator.Destruct();
        mGraphicsAllocator.Destruct();
        return true;
    }

    void MovieAllocator::Destruct()
    {
        Release();
    }

    // ARTIST 824F9808: zero all five lanes, then allocate main and graphics.
    rw::Resource MovieAllocator::DoAllocate(const rw::ResourceDescriptor& lrDescriptor, const char*)
    {
        rw::Resource lResource;
        if (lrDescriptor.m_baseResourceDescriptors[0].m_size != 0)
            lResource.m_baseResources[0] = mMainAllocator.Malloc(
                static_cast<s32>(lrDescriptor.m_baseResourceDescriptors[0].m_size), miMainAlignment);
        if (lrDescriptor.m_baseResourceDescriptors[2].m_size != 0)
            lResource.m_baseResources[2] = mGraphicsAllocator.Malloc(
                lrDescriptor.m_baseResourceDescriptors[2].m_size);
        return lResource;
    }

    // ARTIST 824F9898: individual frees affect only main memory. The graphics
    // region, including movie texture storage, belongs to the enclosing arena.
    void MovieAllocator::DoFree(const rw::Resource& lrResource)
    {
        CGS_ASSERT(lrResource.m_baseResources[2] == nullptr,
                   "lResource.GetPhysicalMemoryResource() == NULL");
        CGS_ASSERT(lrResource.m_baseResources[0] != nullptr, "lResource.GetMemoryResource()");
        mMainAllocator.Free(lrResource.m_baseResources[0]);
    }

    void MovieAllocator::DoFreeDisposable(rw::Resource&)
    {
        CGS_ASSERT(false, "Not implemented"); // ARTIST 824F1900.
    }
}
