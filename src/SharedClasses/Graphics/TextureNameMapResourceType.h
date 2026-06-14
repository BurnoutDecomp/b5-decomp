#ifndef TEXTURE_NAME_MAP_RESOURCE_TYPE_H
#define TEXTURE_NAME_MAP_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace BrnParticle
{
class TextureNameMapResourceType : public CgsResource::Type
{
public:
    uint32_t      GetTypeID() const override;
    void          FixDown(void* lpResource, const rw::Resource& lrResource) const override;
    void          FixUp(void* lpResource, const rw::Resource& lrResource) const override;
    virtual void* Serialise(const void* lpResource, const rw::Resource& lrDest) const;
};
}

#endif
