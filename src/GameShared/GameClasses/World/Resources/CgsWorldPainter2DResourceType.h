#ifndef CGS_WORLD_PAINTER_2D_RESOURCE_TYPE_H
#define CGS_WORLD_PAINTER_2D_RESOURCE_TYPE_H

#include "types.hpp"

namespace CgsResource
{
class WorldPainter2DResourceType
{
public:
    int GetTypeID();
};

inline int WorldPainter2DResourceType::GetTypeID()
{
    return 48;
}
}

#endif
