#ifndef CGS_GINSU_WAVE_CONTENT_RESOURCE_TYPE_H
#define CGS_GINSU_WAVE_CONTENT_RESOURCE_TYPE_H

#include "types.hpp"

namespace CgsResource
{
class GinsuWaveContentResourceType
{
public:
    int GetTypeID();
};

inline int GinsuWaveContentResourceType::GetTypeID()
{
    return 40993;
}
}

#endif
