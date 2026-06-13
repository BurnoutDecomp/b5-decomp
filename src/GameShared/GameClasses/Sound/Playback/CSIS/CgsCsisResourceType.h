#ifndef CGS_CSIS_RESOURCE_TYPE_H
#define CGS_CSIS_RESOURCE_TYPE_H

#include "types.hpp"

namespace CgsResource
{
class CsisResourceType
{
public:
    int GetTypeID();
};

inline int CsisResourceType::GetTypeID()
{
    return 40995;
}
}

#endif
