#ifndef CGS_AEMS_BANK_RESOURCE_TYPE_H
#define CGS_AEMS_BANK_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsBinaryFileResource.h"

namespace CgsResource
{
class AemsBankResourceType : public BinaryFileResourceType
{
public:
    uint32_t GetTypeID() const override;
};
}

#endif
