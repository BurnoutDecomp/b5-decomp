#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsResource
{
class Pool;

// FLAG PC-platform leaf: the GUI's synchronous loader owns separate host storage
// while GameData uses the module scheduler. Publish that storage under the original
// resource-pool id so both transports resolve the same resident entries. The
// original Pool::FindResource still controls ids, status and reference counts.
namespace PCPoolBindings
{
    constexpr s32 KI_MAX_POOLS = 128;

    inline Pool** GetBindings()
    {
        static Pool* sapPools[KI_MAX_POOLS] = {};
        return sapPools;
    }

    inline void Publish(s32 liPoolId, Pool* lpPool)
    {
        CGS_ASSERT(liPoolId >= 0 && liPoolId < KI_MAX_POOLS, "Invalid pool id");
        if (liPoolId >= 0 && liPoolId < KI_MAX_POOLS)
            GetBindings()[liPoolId] = lpPool;
    }

    inline Pool* Find(s32 liPoolId)
    {
        return liPoolId >= 0 && liPoolId < KI_MAX_POOLS
            ? GetBindings()[liPoolId] : nullptr;
    }
}
}
