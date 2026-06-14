#ifndef CGS_WORLD_PAINTER_2D_RESOURCE_TYPE_H
#define CGS_WORLD_PAINTER_2D_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsBinaryFileResource.h"

namespace CgsResource
{
class WorldPainter2DResourceType : public BinaryFileResourceType
{
public:
    uint32_t GetTypeID() const override;
};
}

#endif
