#ifndef CGS_SPLICER_RESOURCE_TYPE_H
#define CGS_SPLICER_RESOURCE_TYPE_H

#include "types.hpp"

namespace CgsResource
{
class SplicerResourceType
{
public:
    int GetTypeID();
};

inline int SplicerResourceType::GetTypeID()
{
    return 40997;
}
}

#endif
