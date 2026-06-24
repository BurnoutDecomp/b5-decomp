#include "SharedClasses/Traffic/BrnTrafficGraphicsStubResourceType.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/rwcore_structs.h"

#include <cstddef>

namespace BrnTraffic
{
    namespace BrnResource
    {
        enum EResourceTypeId
        {
            E_BRN_TRAFFIC_GFX_STUB_RESOURCE_TYPE = 65557
        };
    }

    uint32_t GraphicsStubResourceType::GetTypeID() const
    {
        return BrnResource::E_BRN_TRAFFIC_GFX_STUB_RESOURCE_TYPE;
    }

    // GetSerialisedResourceDescriptor @ 0x82760708 (store-for-store). A constant
    // descriptor: the stub serialises into one block of {size = 8, align = 4} (the
    // `std` at +0 packs {size = 8, align = 4}); entries 1..4 are {0,1}. The size is
    // the X360 serialised footprint — two 4-byte (load-relative) graphics pointers —
    // so it is the literal 8, NOT host sizeof(GraphicsStub) (16 with 64-bit pointers).
    CgsResource::ResourceDescriptor GraphicsStubResourceType::GetSerialisedResourceDescriptor(const void*) const
    {
        CgsResource::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = 8u;   // entry0 size  (two 4-byte pointers)
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 4u;   // entry0 align
        for (uint32_t luBlock = 1; luBlock < 5u; ++luBlock)
        {
            lDescriptor.m_baseResourceDescriptors[luBlock].m_size      = 0u;   // entry1..4 {0,1}
            lDescriptor.m_baseResourceDescriptors[luBlock].m_alignment = 1u;
        }
        return lDescriptor;
    }

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

    void GraphicsStubResourceType::GetImportPointer(const void* lpResource, uint32_t luImportIndex, uint32_t* lpuOutOffset, const void** lppOutResource) const
    {
        const GraphicsStub* lpStub = static_cast<const GraphicsStub*>(lpResource);

        switch (luImportIndex)
        {
            case 0:
                *lppOutResource = lpStub->mpVehicleGraphics;
                *lpuOutOffset = static_cast<uint32_t>(offsetof(GraphicsStub, mpVehicleGraphics));
                break;

            case 1:
                *lppOutResource = lpStub->mpWheelGraphics;
                *lpuOutOffset = static_cast<uint32_t>(offsetof(GraphicsStub, mpWheelGraphics));
                break;

            default:
                CGS_ASSERT(false, "Tried to access out-of-range import on TrafficGraphicsStub resource");
                break;
        }
    }
}
