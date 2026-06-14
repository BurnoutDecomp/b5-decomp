#ifndef CGS_GENERIC_RWAC_REVERB_IR_CONTENT_RESOURCE_TYPE_H
#define CGS_GENERIC_RWAC_REVERB_IR_CONTENT_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsAlignedBinaryFileResource.h"

namespace CgsResource
{
class GenericRwacReverbIRContentResourceType : public AlignedBinaryFileResourceType
{
public:
    uint32_t GetTypeID() const override;
};
}

#endif
