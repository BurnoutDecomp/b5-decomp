#ifndef BRN_TRAFFIC_GRAPHICS_STUB_RESOURCE_TYPE_H
#define BRN_TRAFFIC_GRAPHICS_STUB_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"
#include "SharedClasses/Traffic/BrnTrafficGraphicsStub.h"   // BrnTraffic::GraphicsStub (the record this type handles)

namespace BrnTraffic
{
    // Resource-type handler for the traffic car's graphics stub, type 65557
    // (0x10015). Registered by CgsResourceTypeRegistration.cpp in the console's
    // own slot -- RegisterResourceTypes @0x82667EA8 registers "GraphicsStubResourceType"
    // immediately after "TrafficDataResourceType" and before "LoopModelResourceType".
    //
    // The six virtuals below are exactly the six the DecFIGS DWARF declares
    // (SharedClasses/Traffic/BrnTrafficGraphicsStubResourceType.h:38) and the six
    // the leaked Feb-2007 header declares; DeSerialise / PostFixUp / ReBase /
    // CanDefrag / DebugValidate / GetDebugResourceCategory are inherited from the
    // non-pure CgsResource::Type base, unoverridden, on both.
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
