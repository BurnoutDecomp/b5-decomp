#ifndef AI_SECTIONS_RESOURCE_TYPE_H
#define AI_SECTIONS_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace BrnAI
{
struct AISectionsData
{
    int FixUp(int liDelta);
    int FixDown(int liDelta);
};

class AISectionsResourceType : public CgsResource::Type
{
public:
    uint32_t                        GetTypeID() const override;
    void                            FixDown(void* lpResource, const rw::Resource& lrResource) const override;
    void                            FixUp(void* lpResource, const rw::Resource& lrResource) const override;
    CgsResource::ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const override;
};
}

#endif
