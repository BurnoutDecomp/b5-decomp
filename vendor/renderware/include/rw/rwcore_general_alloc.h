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
        // Adopts the resource regions immediately (X360 ctor @0x82BC0AA8: same math as Initialize()
        // below, just invoked in-place during construction rather than after).
        GeneralResourceAllocator(const Resource& lrResource, const ResourceDescriptor& lrCapacity);
        ~GeneralResourceAllocator() override;

        // Compute the heap descriptor required to back a usage descriptor: the X360 asm
        // (GetResourceDescriptor @0x82BC0E28) copies the input 5-entry descriptor through, then
        // accumulates a synthetic entry-0 requirement of { size = round_up(m_alignment, 2616),
        // alignment = m_alignment } via the committed rw::BaseResourceDescriptors<5>::operator+=
        // (2616 = the EA::Allocator::GeneralAllocator in-heap bookkeeping overhead the X360 reserves
        // out of the front of the adopted core). Entries 1..4 of the synthetic operand are the
        // additive identity { size = 0, alignment = 1 } so they pass through unchanged.
        static ::rw::BaseResourceDescriptors<5>* GetResourceDescriptor(::rw::BaseResourceDescriptors<5>* lpOut, const ::rw::BaseResourceDescriptors<5>* lpIn);

        // Adopt the resource regions: main heap over [resource[0], +capacity[0]); physical heap over
        // [resource[2], +capacity[2]) if that capacity is non-zero (X360 ctor math @0x82BC0AA8).
        void  Initialize(const Resource& lrResource, const ResourceDescriptor& lrCapacity);
        // Allocate luSize bytes (luAlignment-aligned) for pool luType (graphics pools route to the
        // physical heap when present; everything else to the main heap).
        void* Alloc(uint32_t luType, uint32_t luSize, uint32_t luAlignment);
        void  Free(void* lpBlock);

        // DoAllocate @0x82BC0C08 -- the virtual carve consumers reach through the +0 interface
        // base (the GUI modules store GeneralResourceAllocator as an IResourceAllocator*
        // and dispatch DoAllocate: the boost-bar/billboard vertex carves, the texture-state
        // resource carves...). The X360 body walks all five descriptor entries,
        // carving each non-empty pool from the matching heap (index 2 through
        // m_physicalAllocator, the rest through m_mainAllocator).
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
