#include "renderstates.h"

namespace renderengine
{
MeshHelper::MeshData* MeshHelper::Initialize(const u32* lpaParameters)
{
    MeshData* lpData = mpData;
    u32 luPrimaryCount = 0;

    if (lpaParameters[0] != 0)
    {
        luPrimaryCount = 1;
    }
    lpData->muPrimaryCount = luPrimaryCount;

    u32 luSecondaryCount = 0;
    while (luSecondaryCount < 16 && lpaParameters[1 + luSecondaryCount] != 0)
    {
        ++luSecondaryCount;
    }
    lpData->muSecondaryCount = luSecondaryCount;

    u32 luWriteIndex = 0;
    for (; luWriteIndex < lpData->muPrimaryCount; ++luWriteIndex)
    {
        lpData->maValues[luWriteIndex] = lpaParameters[luWriteIndex];
    }

    for (u32 luIndex = 0; luIndex < lpData->muSecondaryCount; ++luIndex)
    {
        lpData->maValues[luWriteIndex + luIndex] = lpaParameters[1 + luIndex];
    }

    return lpData;
}
}
