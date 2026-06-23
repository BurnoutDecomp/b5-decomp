#ifndef BRN_ENVIRONMENT_DICTIONARY_RESOURCE_TYPE_H
#define BRN_ENVIRONMENT_DICTIONARY_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace BrnWorld
{
namespace EnvironmentSettings
{
// Resource-type handler for the environment-settings Dictionary resource (0x10014).
// GetSerialisedResourceDescriptor / DeSerialise / etc. are inherited from the
// non-pure CgsResource::Type base (deferred — not part of this TU's recovered
// slice: only GetTypeID/FixUp/FixDown executed/were recovered). The base virtuals
// are concrete, so overriding just these three keeps the type concrete.
struct DictionaryResourceType : public CgsResource::Type
{
    uint32_t                       GetTypeID() const override;
    // ADDITIVE: the type's serialised-size descriptor. The X360 ARTIST build emits
    // this override (GetSerialised... @ 0x8267D310) returning a five-entry
    // BaseResourceDescriptors<5> whose first entry's size is derived from two header
    // fields of the resource (muField4 @+4, muField12 @+12); entries 1..4 are {0,1}.
    // Body in the .cpp.
    CgsResource::ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
    void                           FixUp(void* lpResource, const rw::Resource& lrResource) const override;
    void                           FixDown(void* lpResource, const rw::Resource& lrResource) const override;
};
}
}

#endif
