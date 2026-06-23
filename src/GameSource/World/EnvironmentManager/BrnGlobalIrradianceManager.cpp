#include "GameSource/World/EnvironmentManager/BrnGlobalIrradianceManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnWorld::GlobalIrradianceManager::GetIrradianceMatrix @ 0x827B0500
//
// Semantic parity (not byte-matching). The X360 body asserts the colour channel is
// in range, then copies the requested 64-byte irradiance matrix out with four
// lvx128/stvx128 (16-byte vector) loads/stores. Those vector ops are a straight
// memory copy of a Matrix44 (four 16-byte rows) -- there is no vmaddfp/vsel/permute
// math -- so they lower faithfully to a scalar struct copy here. Called by
// BrnWorld::EnvironmentSettings::EnvironmentManager::GenerateShaderConstants.

namespace BrnWorld
{
    Matrix44& GlobalIrradianceManager::GetIrradianceMatrix(Matrix44& lrOutMatrix, u8 lu8Colour) const
    {
        // X360: if (lu8Colour >= 3u) Begin/Fire/EndAssert("lu8Colour < 3", "...h", 100);
        CGS_ASSERT(lu8Colour < E_NUM_COLOURS, "lu8Colour < 3");

        // r11 = (lu8Colour << 6) + this  ->  &maIrradianceMatrices[lu8Colour]
        // four lvx128/stvx128 == copy the 64-byte Matrix44 row-for-row.
        lrOutMatrix = maIrradianceMatrices[lu8Colour];
        return lrOutMatrix;
    }
}
