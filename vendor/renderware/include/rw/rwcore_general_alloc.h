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
// IResourceAllocator; the host keeps that inheritance and uses the PC GeneralAllocator engine
// only as the native platform implementation of the two heap members.
namespace rw
{
namespace core
{
    struct GeneralResourceAllocator : ::rw::IResourceAllocator
    {
        GeneralResourceAllocator();
        // The resource includes the object reserve returned by GetResourceDescriptor.
        // ARTIST 0x82BC0AA8 starts the main heap after that reserve, keeping the
        // requested capacity intact. The host reserve follows sizeof/alignof this type.
        GeneralResourceAllocator(const Resource& lrResource, const ResourceDescriptor& lrCapacity);
        ~GeneralResourceAllocator() override;

        // ARTIST 0x82BC0E28 adds the aligned allocator-object reserve to lane zero.
        // The original object's 2616 bytes widen with its real native EA heap members;
        // a console byte count cannot reserve storage for this host object.
        static ::rw::BaseResourceDescriptors<5>* GetResourceDescriptor(::rw::BaseResourceDescriptors<5>* lpOut, const ::rw::BaseResourceDescriptors<5>* lpIn);

        // ARTIST 0x82BC0E00: placement-construct at resource lane zero, or return null.
        // Backing must satisfy GetResourceDescriptor(capacity), including object alignment.
        static GeneralResourceAllocator* Initialize(const Resource& lrResource,
                                                     const ResourceDescriptor& lrCapacity);

        // Only lane two belongs to the physical heap. Disposable lanes one/four
        // use the main heap with the original allocation flag set.
        void* Alloc(uint32_t luType, uint32_t luSize, uint32_t luAlignment);
        void  Free(void* lpBlock);

        // DoAllocate @0x82BC0C08 -- the virtual carve consumers reach through the +0 interface
        // base (the GUI modules store GeneralResourceAllocator as an IResourceAllocator*
        // and dispatch DoAllocate: the boost-bar/billboard vertex carves, the texture-state
        // resource carves...). The X360 body walks all five descriptor entries,
        // carving lanes 0/1/3/4 from the main heap before lane 2 from the physical
        // heap. Any failure frees the partial resource and returns all-null lanes.
        // DoFree @0x82BC0DA8 performs the inverse operation in reverse lane order: lane 2 is
        // returned to m_physicalAllocator and all other populated lanes to m_mainAllocator.
        ::rw::Resource DoAllocate(const ::rw::ResourceDescriptor& lrDescriptor,
                                  const char* lpcName) override;
        void DoFree(const ::rw::Resource& lrResource) override;

        EA::Allocator::GeneralAllocator& GetPhysicalGeneralAllocator() { return m_physicalAllocator; }

        EA::Allocator::GeneralAllocator m_mainAllocator;
        EA::Allocator::GeneralAllocator m_physicalAllocator;
        bool                            m_hasPhysical;
    };
}
}
