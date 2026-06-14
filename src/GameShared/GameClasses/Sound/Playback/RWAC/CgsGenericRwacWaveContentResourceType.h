#ifndef CGS_GENERIC_RWAC_WAVE_CONTENT_RESOURCE_TYPE_H
#define CGS_GENERIC_RWAC_WAVE_CONTENT_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsBinaryFileResource.h"

namespace CgsResource
{
class GenericRwacWaveContentResourceType : public BinaryFileResourceType
{
public:
    uint32_t GetTypeID() const override;
};
}

#endif
