#ifndef CGS_GENERIC_RWAC_REVERB_IR_CONTENT_RESOURCE_TYPE_H
#define CGS_GENERIC_RWAC_REVERB_IR_CONTENT_RESOURCE_TYPE_H

#include "types.hpp"

namespace CgsResource
{
class GenericRwacReverbIRContentResourceType
{
public:
    int GetTypeID();
};

inline int GenericRwacReverbIRContentResourceType::GetTypeID()
{
    return 41000;
}
}

#endif
