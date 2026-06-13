#ifndef CGS_SNAPSHOT_DATA_RESOURCE_TYPE_H
#define CGS_SNAPSHOT_DATA_RESOURCE_TYPE_H

#include "types.hpp"

namespace CgsResource
{
class SnapshotDataResourceType
{
public:
    int GetTypeID();
};

inline int SnapshotDataResourceType::GetTypeID()
{
    return 41001;
}
}

#endif
