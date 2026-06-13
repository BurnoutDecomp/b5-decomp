#ifndef CGS_GENERIC_RWAC_WAVE_CONTENT_RESOURCE_TYPE_H
#define CGS_GENERIC_RWAC_WAVE_CONTENT_RESOURCE_TYPE_H

#include "types.hpp"

namespace CgsResource
{
class GenericRwacWaveContentResourceType
{
public:
    int GetTypeID();
};

inline int GenericRwacWaveContentResourceType::GetTypeID()
{
    return 40992;
}
}

#endif
