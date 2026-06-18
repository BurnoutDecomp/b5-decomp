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

    CgsResource::ResourceDescriptor GraphicsStubResourceType::GetSerialisedResourceDescriptor(const void*) const
    {
        CgsResource::ResourceDescriptor lDescriptor = {};
        lDescriptor.m_baseResourceDescriptors[0].m_size = sizeof(GraphicsStub);
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 4;
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
