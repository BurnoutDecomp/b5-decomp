#ifndef STREAMED_DEFORMATION_SPEC_RESOURCE_TYPE_H
#define STREAMED_DEFORMATION_SPEC_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace BrnResource
{
class StreamedDeformationSpecResourceType : public CgsResource::Type
{
public:
    uint32_t                        GetTypeID() const ;
    CgsResource::ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const ;
    void                            FixUp(void* lpResource, const rw::Resource& lrResource) const ;
    virtual void*                   Serialise(const void* lpResource, const rw::Resource& lrDest) const;
};
}

#endif
