#pragma once

#include "types.hpp"

namespace renderengine
{
    // FLAG PC-platform leaf: a registered debug control for the prop reflection
    // LOD, which the original RenderModel path hard-codes to LOD2.
    inline s32& PropEnvironmentMapLodPC() { static s32 siLod = 2; return siLod; }
}
