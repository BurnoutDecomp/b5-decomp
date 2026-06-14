#include "GameShared/GameClasses/World/Resources/CgsWorldPainter2DResourceType.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826659D8
//   (CgsResource::WorldPainter2DResourceType::GetTypeID)  ->  return 48;
// Virtual override of CgsResource::Type::GetTypeID (via BinaryFileResourceType).

namespace CgsResource
{
    static const uint32_t KU_WORLD_PAINTER_2D_RESOURCE_TYPE_ID = 48;

    uint32_t WorldPainter2DResourceType::GetTypeID() const
    {
        return KU_WORLD_PAINTER_2D_RESOURCE_TYPE_ID;
    }
}
