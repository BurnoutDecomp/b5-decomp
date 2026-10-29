#pragma once

#include "rw/rwcore_structs.h"               // rw::Resource / ResourceDescriptor
#include "ppmalloc/EAGeneralAllocator.h"     // the two embedded EA GeneralAllocators

// rw::core::GeneralResourceAllocator - the general-purpose resource allocator (supports per-block
// Free, unlike the bump LinearResourceAllocator). The X360 (rw::core::GeneralResourceAllocator ctor
// 0x82BC0AA8 / dtor 0x82BC0A50 / scalar deleting dtor 0x82BC0BB8 / Initialize 0x82BC0E00 /
// GetResourceDescriptor 0x82BC0E28 / DoAllocate 0x82BC0C08 / DoFree 0x82BC0DA8 /
// GetPhysicalGeneralAllocator 0x82BC0AA0)
// wraps TWO EA::Allocator::GeneralAllocators: a main heap over the resource's first region and an
// optional physical/graphics heap over lane two. ARTIST's vtable stores prove this is-a
// IResourceAllocator. The embedded heaps use the original EA native implementation.
namespace rw
{
namespace core
{
    struct GeneralResourceAllocator : ::rw::IResourceAllocator
    {
        // The resource includes the object reserve returned by GetResourceDescriptor.
        // ARTIST 0x82BC0AA8 starts the main heap after that reserve, keeping the
        // requested capacity intact. The native reserve is 0xA38 bytes, aligned
        // to the lane-zero capacity alignment without imposing another minimum.
        GeneralResourceAllocator(const Resource& lrResource, const ResourceDescriptor& lrCapacity);
        ~GeneralResourceAllocator() ;

        // ARTIST 0x82BC0E28 adds the aligned allocator-object reserve to lane zero.
        static ::rw::BaseResourceDescriptors<5>* GetResourceDescriptor(::rw::BaseResourceDescriptors<5>* lpOut, const ::rw::BaseResourceDescriptors<5>* lpIn);

        // ARTIST 0x82BC0E00: placement-construct at resource lane zero, or return null.
        // Backing must satisfy GetResourceDescriptor(capacity), including object alignment.
        static GeneralResourceAllocator* Initialize(const Resource& lrResource,
                                                     const ResourceDescriptor& lrCapacity);

        // DoAllocate @0x82BC0C08 -- the virtual carve consumers reach through the +0 interface
        // base (the GUI modules store GeneralResourceAllocator as an IResourceAllocator*
        // and dispatch DoAllocate: the boost-bar/billboard vertex carves, the texture-state
        // resource carves...). The X360 body walks all five descriptor entries,
        // carving lanes 0/1/3/4 from the main heap before lane 2 from the physical
        // heap. Any failure frees the partial resource and returns all-null lanes.
        // DoFree @0x82BC0DA8 performs the inverse operation in reverse lane order: lane 2 is
        // returned to m_physicalAllocator and all other populated lanes to m_mainAllocator.
        ::rw::Resource DoAllocate(const ::rw::ResourceDescriptor& lrDescriptor,
                                  const char* lpcName) ;
        void DoFree(const ::rw::Resource& lrResource) ;

        EA::Allocator::GeneralAllocator& GetPhysicalGeneralAllocator() { return m_physicalAllocator; }

        EA::Allocator::GeneralAllocator m_mainAllocator;
        EA::Allocator::GeneralAllocator m_physicalAllocator;
        bool                            m_hasPhysical;
    };
    CGS_STATIC_ASSERT(sizeof(GeneralResourceAllocator) == 0xA38, "ARTIST resource allocator size");
    CGS_STATIC_ASSERT(offsetof(GeneralResourceAllocator, m_mainAllocator) == 0x4, "ARTIST main heap offset");
    CGS_STATIC_ASSERT(offsetof(GeneralResourceAllocator, m_physicalAllocator) == 0x51C, "ARTIST physical heap offset");
    CGS_STATIC_ASSERT(offsetof(GeneralResourceAllocator, m_hasPhysical) == 0xA34, "ARTIST physical heap flag");
}
}
