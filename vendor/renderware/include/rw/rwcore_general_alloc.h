#pragma once

#include "rw/rwcore_structs.h"               // rw::Resource / ResourceDescriptor
#include "ppmalloc/EAGeneralAllocator.h"     // the two embedded EA GeneralAllocators

// rw::core::GeneralResourceAllocator - the general-purpose resource allocator (supports per-block
// Free, unlike the bump LinearResourceAllocator). The X360 (rw::core::GeneralResourceAllocator ctor
// 0x82BC0AA8 / dtor 0x82BC0A50 / scalar deleting dtor 0x82BC0BB8 / Initialize 0x82BC0E00 /
// GetResourceDescriptor 0x82BC0E28 / DoAllocate 0x82BC0C08 / DoFree 0x82BC0DA8 /
// GetPhysicalGeneralAllocator 0x82BC0AA0)
// wraps TWO EA::Allocator::GeneralAllocators: a main heap over the resource's first region and an
// optional physical/graphics heap over a second region. [PC-LEAF, user-approved 2026-06-20]:
// faithful structure (two EA GeneralAllocators + a has-physical flag), bodies backed by the working
// PC GeneralAllocator engine; the bit-exact rwcore.lib chunk layout is elided (behaviourally
// invisible). Uses the rw::Resource/ResourceDescriptor (4-pool) typedefs for consistency with the
// AllocatorList + LinearResourceAllocator, EXCEPT GetResourceDescriptor which the X360 asm proves
// operates on the serialised 5-entry rw::BaseResourceDescriptors<5> (memcpy(40) + a call into the
// already-committed rw::BaseResourceDescriptors<5>::operator+=) -- see body for the derivation.
//
// HAS-A rw::IResourceAllocator (matches the sibling LinearResourceAllocator's composition pattern,
// `field_0x0`, rather than public inheritance): on X360 this is a real polymorphic base (the ctor asm
// stores the class's own vtable at offset 0, `*a1 = off_821814C0`; the dtor asm restores
// IResourceAllocator's vtable, `off_8200F5B4`, before returning to the base-class teardown -- standard
// compiler codegen for a class with a vptr). Composition means GeneralResourceAllocator is NOT
// polymorphic on PC; member construction/destruction of field_0x0 / m_mainAllocator /
// m_physicalAllocator (reverse order) still happens automatically via the ordinary ctor/dtor below, but
// there is no vtable to poke and no scalar-deleting-destructor thunk to reconstruct (see below).
namespace rw
{
namespace core
{
    struct GeneralResourceAllocator
    {
        GeneralResourceAllocator();
        // Adopts the resource regions immediately (X360 ctor @0x82BC0AA8: same math as Initialize()
        // below, just invoked in-place during construction rather than after).
        GeneralResourceAllocator(const Resource& lrResource, const ResourceDescriptor& lrCapacity);
        ~GeneralResourceAllocator();

        // NOT RECONSTRUCTED (`scalar deleting destructor' @0x82BC0BB8): the X360 thunk the compiler
        // emits for polymorphic `delete` through an IResourceAllocator* (dtor, then conditionally
        // operator delete(this)). Composition (field_0x0, matching the sibling LinearResourceAllocator
        // pattern above) makes GeneralResourceAllocator non-polymorphic on PC, so this thunk has no
        // C++-language counterpart here; nothing in the committed tree deletes a GeneralResourceAllocator
        // through a base IResourceAllocator* today. Revisit if/when a caller needs that.

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
        // subobject (the GUI modules store &GeneralResourceAllocator as an IResourceAllocator*
        // and dispatch DoAllocate: the boost-bar/billboard vertex carves, the texture-state
        // resource carves...). The X360 body walks all FIVE serialised descriptor entries,
        // carving each non-empty pool from the matching heap (index 2 through
        // m_physicalAllocator, the rest through m_mainAllocator). On the PC's narrowed 4-pool
        // rw::Resource form the same walk runs over the four pools through Alloc() (which
        // already routes graphics pools to the physical heap) -- see ResourceAllocatorBridge
        // below. (The old note here left the slot on the DEFAULT IResourceAllocator::DoAllocate
        // -- an EMPTY-Resource stub -- which made every virtual carve through this allocator
        // silently return null bases; the boost-bar mount was the first caller to notice.)
        // DoFree @0x82BC0DA8 performs the inverse operation in reverse lane order: lane 2 is
        // returned to m_physicalAllocator and all other populated lanes to m_mainAllocator.
        // ResourceAllocatorBridge below preserves that dispatch on the PC's four-lane Resource.

        EA::Allocator::GeneralAllocator& GetPhysicalGeneralAllocator() { return m_physicalAllocator; }

        // The +0 interface subobject, now the REAL virtual bridge: it sits at offset 0, so the
        // owning GeneralResourceAllocator is `this` reinterpreted -- exactly the X360 layout,
        // where the class IS-A IResourceAllocator and DoAllocate is its override. sizeof is
        // unchanged (one vptr).
        struct ResourceAllocatorBridge : ::rw::IResourceAllocator
        {
            ::rw::Resource DoAllocate(const ::rw::ResourceDescriptor& lrDescriptor,
                                      const char* lpcName) override;
            void DoFree(const ::rw::Resource& lrResource) override;
        };

        ResourceAllocatorBridge         field_0x0;            // X360 +0    vptr (IS-A IResourceAllocator)
        EA::Allocator::GeneralAllocator m_mainAllocator;       // X360 +4    main resource heap
        EA::Allocator::GeneralAllocator m_physicalAllocator;   // X360 +1308 physical/graphics heap
        bool                            m_hasPhysical;         // X360 +2612
    };
}
}
