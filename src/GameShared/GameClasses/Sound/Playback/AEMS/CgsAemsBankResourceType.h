#ifndef CGS_AEMS_BANK_RESOURCE_TYPE_H
#define CGS_AEMS_BANK_RESOURCE_TYPE_H

#include "types.hpp"

namespace CgsResource
{
class AemsBankResourceType
{
public:
    int GetTypeID();
};

inline int AemsBankResourceType::GetTypeID()
{
    return 40994;
}
}

#endif
