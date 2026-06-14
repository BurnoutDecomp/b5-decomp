#ifndef CGS_GINSU_WAVE_CONTENT_RESOURCE_TYPE_H
#define CGS_GINSU_WAVE_CONTENT_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsBinaryFileResource.h"

namespace CgsResource
{
class GinsuWaveContentResourceType : public BinaryFileResourceType
{
public:
    uint32_t GetTypeID() const override;
};
}

#endif
