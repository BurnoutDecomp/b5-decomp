#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826659D8
//   (CgsResource::WorldPainter2DResourceType::GetTypeID)  ->  return 48;

namespace CgsResource
{
    class WorldPainter2DResourceType
    {
    public:
        int GetTypeID();
    };

    static const int KI_WORLD_PAINTER_2D_RESOURCE_TYPE_ID = 48;

    int WorldPainter2DResourceType::GetTypeID()
    {
        return KI_WORLD_PAINTER_2D_RESOURCE_TYPE_ID;
    }
}
