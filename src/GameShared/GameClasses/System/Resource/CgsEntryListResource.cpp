#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x828D8430
//   (CgsResource::EntryListResourceType::GetTypeID)  ->  return 29;

namespace CgsResource
{
    class EntryListResourceType
    {
    public:
        int GetTypeID();
    };

    static const int KI_ENTRY_LIST_RESOURCE_TYPE_ID = 29;

    int EntryListResourceType::GetTypeID()
    {
        return KI_ENTRY_LIST_RESOURCE_TYPE_ID;
    }
}
