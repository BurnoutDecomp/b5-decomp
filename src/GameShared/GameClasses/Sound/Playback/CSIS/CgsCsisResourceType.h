#ifndef CGS_CSIS_RESOURCE_TYPE_H
#define CGS_CSIS_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsBinaryFileResource.h"

namespace CgsResource
{
class CsisResourceType : public BinaryFileResourceType
{
public:
    uint32_t GetTypeID() const override;
};
}

#endif
