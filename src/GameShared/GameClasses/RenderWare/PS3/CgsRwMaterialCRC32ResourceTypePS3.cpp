#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x828A80E8
//   (CgsResource::RwMaterialCRC32ResourceType::GetTypeID)  ->  return 11;

namespace CgsResource
{
    class RwMaterialCRC32ResourceType
    {
    public:
        int GetTypeID();
    };

    static const int KI_RW_MATERIAL_CRC32_RESOURCE_TYPE_ID = 11;

    int RwMaterialCRC32ResourceType::GetTypeID()
    {
        return KI_RW_MATERIAL_CRC32_RESOURCE_TYPE_ID;
    }
}
