#ifndef BRN_PROP_PHYSICS_LIST_RESOURCE_TYPE_H
#define BRN_PROP_PHYSICS_LIST_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace BrnPhysics
{
namespace Props
{
// Resource-type handler for a serialised PropPhysicsDataHeader (resource type 0x1000F /
// 65551 -- PROPS/PROPPHYSICS.BUNDLE), deriving from CgsResource::Type. FixUp/FixDown forward
// straight to the header's own FixUp/FixDown (the resource payload IS a
// PropPhysicsDataHeader). Base, virtual set and vtable order recovered from the DecFIGS
// DWARF (BrnPropPhysicsListResourceType.h declares exactly these four overrides, in this
// order) and the X360 pseudocode; registered as "PropPhysicsResourceType" by
// BrnResource::GameDataModule::RegisterResourceTypes @0x82667EA8.
class PropPhysicsResourceType : public CgsResource::Type
{
public:
    uint32_t                   GetTypeID() const override;
    CgsResource::ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
    void                       FixDown(void* lpResource, const rw::Resource& lrResource) const override;
    void                       FixUp(void* lpResource, const rw::Resource& lrResource) const override;

    // Compile-time-only pin of the serialised layout this handler fixes up (see the .cpp).
    static void _AssertWireContract();
};
}
}

#endif
