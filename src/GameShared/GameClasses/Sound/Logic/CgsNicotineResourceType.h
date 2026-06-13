#ifndef CGS_NICOTINE_RESOURCE_TYPE_H
#define CGS_NICOTINE_RESOURCE_TYPE_H

#include "types.hpp"

namespace CgsResource
{
class NicotineResourceType
{
public:
    int GetTypeID();
};

inline int NicotineResourceType::GetTypeID()
{
    return 40996;
}
}

#endif
