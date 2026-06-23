#ifndef BRN_ENVIRONMENT_KEYFRAME_RESOURCE_TYPE_H
#define BRN_ENVIRONMENT_KEYFRAME_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace BrnWorld
{
namespace EnvironmentSettings
{
class KeyframeResourceType : public CgsResource::Type
{
public:
    uint32_t                       GetTypeID() const override;
    // ADDITIVE: the type's serialised-size descriptor. The X360 ARTIST build emits
    // this override (GetSerialised... @ 0x8267D220) returning a five-entry
    // BaseResourceDescriptors<5> with a fixed first-entry size of 0x240 bytes
    // (entries 1..4 are {0,1}). Body in the .cpp.
    CgsResource::ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
    void                           FixUp(void* lpResource, const rw::Resource& lrResource) const override;
};
}
}

#endif
