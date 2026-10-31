#pragma once

#include "BrnCommonTypes.h"

namespace BrnTraffic
{
    // DecFIGS BrnTrafficSharedMaths.h:81; ARTIST 0x823610B8 receives
    // the packed position and Y rotation by value in v1.
    Matrix44Affine ExpandPosPlusYRotToTransform(Vector3Plus lPosPlusYRot);
}
