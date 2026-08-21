#include "SharedClasses/Traffic/BrnTrafficGraphicsStubResourceType.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/rwcore_structs.h"

#include <cstddef>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnTraffic::GraphicsStubResourceType::GetTypeID                        @ 0x82752770
//   BrnTraffic::GraphicsStubResourceType::GetSerialisedResourceDescriptor  @ 0x82760708
//   BrnTraffic::GraphicsStubResourceType::GetImportPointer                 @ 0x82752780
// FixDown / FixUp / GetImportCount are ICF-folded on the console (empty bodies and a
// `return 2` fold into shared thunks, so neither carries a distinct export). Their shapes
// come from the DWARF (BrnTrafficGraphicsStubResourceType.h:38): both Fix* are empty and
// GetImportCount returns 2, which the shipped bundles corroborate (importCount 2 with
// offsets {0x0, 0x4} in 42/42 retail VEH_T*_GR.BIN).

namespace BrnResource
{
    // The one enumerator this handler needs out of BrnResource's resource-type id enum.
    // PARK: that enum has no reconstructed home in this tree yet, so it lives here as a
    // file-local constant, the same way every other handler carries its id.
    enum { E_BRN_TRAFFIC_GFX_STUB_RESOURCE_TYPE = 65557 };   // 0x10015
}

namespace BrnTraffic
{
    // GetTypeID @ 0x82752770 -- `return 65557;`
    uint32_t GraphicsStubResourceType::GetTypeID() const
    {
        return BrnResource::E_BRN_TRAFFIC_GFX_STUB_RESOURCE_TYPE;
    }

    // GetSerialisedResourceDescriptor @ 0x82760708. A constant descriptor: the X360
    // stores the 64-bit literal 0x800000004 at +0 ({size 8, alignment 4}), then writes
    // {0, 1} into entries 1..4. The size is the serialised footprint of two 4-byte
    // slots; host sizeof(GraphicsStub) is also 8, pinned by GraphicsStub::_AssertLayout.
    CgsResource::ResourceDescriptor GraphicsStubResourceType::GetSerialisedResourceDescriptor(const void*) const
    {
        CgsResource::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = 8u;   // entry0 size  (two 4-byte slots)
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 4u;   // entry0 align
        for (uint32_t luBlock = 1; luBlock < 5u; ++luBlock)
        {
            lDescriptor.m_baseResourceDescriptors[luBlock].m_size      = 0u;   // entry1..4 {0,1}
            lDescriptor.m_baseResourceDescriptors[luBlock].m_alignment = 1u;
        }
        return lDescriptor;
    }

    // Empty on the console: both of this resource's words are import slots, which
    // CgsResource::Pool::ResolveImportForEntry overwrites outright, so there is
    // nothing to relocate or byte-swap.
    void GraphicsStubResourceType::FixDown(void*, const rw::Resource&) const
    {
    }

    void GraphicsStubResourceType::FixUp(void*, const rw::Resource&) const
    {
    }

    uint32_t GraphicsStubResourceType::GetImportCount(const void*) const
    {
        return 2;
    }

    // GetImportPointer @ 0x82752780. The console constant-folds the two offsets to the
    // literals 0 and 4; the offsetof form here matches only because GraphicsStub's slots
    // are Ptr32<T>, which GraphicsStub::_AssertLayout keeps true.
    void GraphicsStubResourceType::GetImportPointer(const void* lpResource, uint32_t luImportIndex, uint32_t* lpuOutOffset, const void** lppOutResource) const
    {
        const GraphicsStub* lpStub = static_cast<const GraphicsStub*>(lpResource);

        switch (luImportIndex)
        {
            case 0:
                *lppOutResource = lpStub->mpVehicleGraphics;
                *lpuOutOffset   = static_cast<uint32_t>(offsetof(GraphicsStub, mpVehicleGraphics));   // == 0
                break;

            case 1:
                *lppOutResource = lpStub->mpWheelGraphics;
                *lpuOutOffset   = static_cast<uint32_t>(offsetof(GraphicsStub, mpWheelGraphics));     // == 4
                break;

            default:
                // Baked at BrnTrafficGraphicsStubResourceType.cpp:175.
                CGS_ASSERT(false, "Tried to access out-of-range import on TrafficGraphicsStub resource");
                break;
        }
    }
}
