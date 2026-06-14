#ifndef CGS_SNAPSHOT_DATA_RESOURCE_TYPE_H
#define CGS_SNAPSHOT_DATA_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsBinaryFileResource.h"

namespace CgsResource
{
class SnapshotDataResourceType : public BinaryFileResourceType
{
public:
    uint32_t GetTypeID() const override;
};
}

#endif
