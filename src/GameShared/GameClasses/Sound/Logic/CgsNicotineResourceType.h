#ifndef CGS_NICOTINE_RESOURCE_TYPE_H
#define CGS_NICOTINE_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsBinaryFileResource.h"

namespace CgsResource
{
class NicotineResourceType : public BinaryFileResourceType
{
public:
    uint32_t GetTypeID() const override;
};
}

#endif
