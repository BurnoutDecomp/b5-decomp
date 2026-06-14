#include "GameShared/GameClasses/Sound/Logic/CgsSnapshotDataResourceType.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826659C8
//   (CgsResource::SnapshotDataResourceType::GetTypeID)  ->  return 41001;
// Virtual override of CgsResource::Type::GetTypeID (via BinaryFileResourceType).

namespace CgsResource
{
    static const uint32_t KU_SNAPSHOT_DATA_RESOURCE_TYPE_ID = 41001;

    uint32_t SnapshotDataResourceType::GetTypeID() const
    {
        return KU_SNAPSHOT_DATA_RESOURCE_TYPE_ID;
    }
}
