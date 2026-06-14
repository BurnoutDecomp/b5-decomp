#ifndef CGS_VIDEO_DATA_RESOURCE_H
#define CGS_VIDEO_DATA_RESOURCE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
// Resource-type handler for serialised video data. Derives from CgsResource::Type;
// GetTypeID/FixUp are virtual overrides. From DecFIGS DWARF.
class VideoDataResourceType : public Type
{
public:
    uint32_t GetTypeID() const override;
    void     FixUp(void* lpResource, const rw::Resource& lrResource) const override;
};
}

#endif
