#ifndef BRN_TRAFFIC_GRAPHICS_STUB_RESOURCE_TYPE_H
#define BRN_TRAFFIC_GRAPHICS_STUB_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace BrnTraffic
{
    struct GraphicsStub
    {
        const void* mpVehicleGraphics;
        const void* mpWheelGraphics;
    };

    class GraphicsStubResourceType : public CgsResource::Type
    {
    public:
        uint32_t GetTypeID() const override;
        CgsResource::ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
        void FixDown(void* lpResource, const rw::Resource& lrResource) const override;
        void FixUp(void* lpResource, const rw::Resource& lrResource) const override;
        uint32_t GetImportCount(const void* lpResource) const override;
        void GetImportPointer(const void* lpResource, uint32_t luImportIndex, uint32_t* lpuOutOffset, const void** lppOutResource) const override;
    };
}

#endif
