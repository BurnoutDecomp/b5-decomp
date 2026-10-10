#pragma once
#include "types.hpp"
#include <cmath>

namespace renderengine {
// FLAG PC-platform leaf: the native packet carries pointers to the original
// dispatch-bin constant snapshots; it never refers to the update thread's live
// matrices. The matrix/index arrays have the original five-entry capacity.
struct WorldInstanceDrawPC
{
    const float* mpMatrices;
    const float* mpIndices;
    const float* mpViewProjection;
    u32 muCount;

    const float* Matrix(u32 index) const
    {
        const float selected = mpIndices ? mpIndices[index * 4 + 3] : static_cast<float>(index);
        return std::isfinite(selected) && selected >= 0 && selected < 5
            ? mpMatrices + static_cast<u32>(selected) * 16 : nullptr;
    }
};
inline void WorldInstanceWvpPC(const float* world, const float* viewProjection, float* output)
{
    for (u32 row = 0; row < 4; ++row)
        for (u32 column = 0; column < 4; ++column)
            output[row * 4 + column] = world[row * 4] * viewProjection[column]
                + world[row * 4 + 1] * viewProjection[4 + column]
                + world[row * 4 + 2] * viewProjection[8 + column]
                + (row == 3 ? 1.0f : world[row * 4 + 3]) * viewProjection[12 + column];
}
bool WorldDraw_TryInstancedPC(u32 primitive, u32 baseVertex, u32 startIndex,
                              u32 indexCount, const WorldInstanceDrawPC& instances);
void WorldDraw_IndexedUP(u32 primitive, u32 baseVertex, u32 startIndex, u32 indexCount);
}
