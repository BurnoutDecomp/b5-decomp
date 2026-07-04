// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/RaceCarEntityModule/
//   BrnRaceCarEntityModule_GlassFracture.cpp
//
// X360 0x822BD280 -- BrnWorld::SetGlassFractureConstants(...): the free helper the
// race-car (and traffic) render paths call once per glass-fracture draw to stamp the
// three glass-fracture shader constants (indices 30/31/32) into the global
// ShaderConstantTable. Only caller in this TU family: BrnWorld::RaceCarEntityModule::
// RenderRaceCar.
//
// DWARF-authoritative signature (BrnRaceCarEntityModule.cpp:4 /
// _compile/BrnEntityModuleUnity.cpp:19091):
//   void BrnWorld::SetGlassFractureConstants(float32_t lfFractureStrength,
//                                            float32_t lfEqualisationFactor,
//                                            const Vector2Template<float>& lvUVScale,
//                                            Vector4 lvUVOffsets);
//   (IDA rendered the two float args as doubles a1/a2 and the by-value Vector4 as the
//    r3/r4 int pair; the fdivs/fmuls/fsubs are single-precision.)
//
// Store-for-store to the X360 body:
//   * constant 30: {(1-strength)*inv, inv, 0, 0} where inv = strength>0 ? 1/strength : 0
//     (0x822BD2A0..0x822BD300; flt_82001CC0 = 0.0f, flt_82001C98 = 1.0f).
//   * constant 31: lvUVOffsets verbatim (the incoming Vector4, held in v127).
//   * constant 32: {uv.x, uv.x*equalisation, uv.y, uv.y*equalisation}
//     (0x822BD314..0x822BD344; uv = lvUVScale).
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h"
#include "GameShared/GameClasses/Graphics/CgsShaderConstants.h"   // ShaderConstantTable
#include "BrnCommonTypes.h"                                       // Vector2, Vector4

// The global runtime shader-constant register (X360 symbol mShaderConstantTable; bodied
// by the CgsShaderConstants TU). Mirrors the committed extern in CgsDispatcherCommands.cpp.
extern ShaderConstantTable mShaderConstantTable;

namespace BrnWorld
{

void SetGlassFractureConstants(float lfFractureStrength,
                               float lfEqualisationFactor,
                               const Vector2& lvUVScale,
                               Vector4 lvUVOffsets)
{
    // inv = strength > 0 ? 1/strength : 0  (0x822BD2B4 fcmpu vs 0.0f -> fdivs / fmr 0.0f)
    const float lfInverseStrength =
        (lfFractureStrength > 0.0f) ? (1.0f / lfFractureStrength) : 0.0f;

    // Constant 30: {(1 - strength) * inv, inv, 0, 0}.
    const Vector4 lv4Fracture{ (1.0f - lfFractureStrength) * lfInverseStrength,
                               lfInverseStrength,
                               0.0f,
                               0.0f };
    mShaderConstantTable.SetShaderConstantData(30, lv4Fracture);

    // Constant 31: the incoming UV-offsets vector, verbatim (held in v127 across the call).
    mShaderConstantTable.SetShaderConstantData(31, lvUVOffsets);

    // Constant 32: {uv.x, uv.x * equalisation, uv.y, uv.y * equalisation}.
    const Vector4 lv4UVScale{ lvUVScale.x,
                              lvUVScale.x * lfEqualisationFactor,
                              lvUVScale.y,
                              lvUVScale.y * lfEqualisationFactor };
    mShaderConstantTable.SetShaderConstantData(32, lv4UVScale);
}

}   // namespace BrnWorld
