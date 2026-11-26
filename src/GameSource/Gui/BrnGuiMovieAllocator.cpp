#include "GameSource/Gui/BrnGuiMovieAllocator.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

namespace BrnGui
{
    // (the destructor is the implicit one, see the header)

    // MovieAllocator::Construct is header-inline (ARTIST has no out-of-line copy;
    // MovieManager::Construct @0x824F95F0..95F4 resets the contained linear
    // allocator directly). These lines keep this file's line count for the
    // __LINE__ of its asserts.
    //

    // ARTIST 824F9780, DWARF: both five-lane arguments are passed by value.
    bool MovieAllocator::Prepare(rw::Resource lResource, rw::ResourceDescriptor lDescriptor)
    {
        mMainAllocator.Construct(lResource.m_baseResources[0],
            static_cast<s32>(lDescriptor.m_baseResourceDescriptors[0].m_size));
        miMainAlignment = static_cast<s32>(lDescriptor.m_baseResourceDescriptors[0].m_alignment);
        // ARTIST824F97C8..97DC always creates the graphics arena, including
        // a zero-length region, before setting its requested alignment.
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
        // ARTIST's asserts name BrnGuiMovieManager.cpp:1129/1134 (DoFree's original home) and
        // compare signed (cmpwi): the original read rw::Resource's inline
        // GetPhysicalMemoryResource()/GetMemoryResource(), which the tree's rw::Resource lacks.
        CGS_ASSERT_AT(lrResource.m_baseResources[2] == 0, "lResource.GetPhysicalMemoryResource() == 0",
                      "..\\..\\..\\GameSource\\Gui/BrnGuiMovieManager.cpp", 1129);
        CGS_ASSERT_AT(lrResource.m_baseResources[0] != 0, "lResource.GetMemoryResource()",
                      "..\\..\\..\\GameSource\\Gui/BrnGuiMovieManager.cpp", 1134);
        mMainAllocator.Free(lrResource.m_baseResources[0]);
    }

    void MovieAllocator::DoFreeDisposable(rw::Resource&)
    {
        CGS_FAIL_AT(
            "Not implemented",
                "..\\..\\..\\GameSource\\Gui/BrnGuiMovieManager.cpp", 1151 /* ARTIST 0x824F1914 */); // ARTIST 824F1900.

    }
}
