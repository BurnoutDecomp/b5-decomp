#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826659C8
//   (CgsResource::SnapshotDataResourceType::GetTypeID)  ->  return 41001;

namespace CgsResource
{
    class SnapshotDataResourceType
    {
    public:
        int GetTypeID();
    };

    static const int KI_SNAPSHOT_DATA_RESOURCE_TYPE_ID = 41001;

    int SnapshotDataResourceType::GetTypeID()
    {
        return KI_SNAPSHOT_DATA_RESOURCE_TYPE_ID;
    }
}
