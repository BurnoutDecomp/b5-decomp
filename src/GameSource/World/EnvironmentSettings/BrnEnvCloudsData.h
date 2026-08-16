#ifndef BRN_ENV_CLOUDS_DATA_H
#define BRN_ENV_CLOUDS_DATA_H

#include "types.hpp"

namespace BrnWorld
{
namespace EnvironmentSettings
{
// Reconstructed from BURNOUT_X360_ARTIST.XEX.
// Default clouds keyframe: two per-layer colour arrays copied from the module's
// static default template, followed by explicit per-layer float-pair parameters.
// Field names/types per burnout.wiki (Environment Keyframe ->
// BrnWorld::EnvironmentSettings::CloudsData); offsets verified against the X360
// pseudocode.
class CloudsData
{
public:
    void Construct();

    // ---- the two reads EnvironmentManager::Update @0x827D6060 makes -----------------
    // The console reads them straight out of its embedded mClouds block
    // (manager+0x6D8 == mClouds+0x58 == mafLayerSpeed[0]; manager+0x6E8 == mClouds+0x68
    // == mfDirectionAngle) to scroll the cloud UVs and to build the wind velocity it
    // publishes for the effects module. Exposed as const getters so the manager reaches
    // them BY NAME rather than by offset into a foreign class. (envblend, step 9)
    f32 GetLayerSpeed( u32 luLayer ) const { return mafLayerSpeed[ luLayer ]; }
    f32 GetDirectionAngle() const          { return mfDirectionAngle; }

    // Blend keyframes into this one (element-wise weighted sum over the 27-float layout).
    // 4-way @ 0x82675FC0.  The 2-way form below has NO body in
    // BURNOUT_X360_ARTIST.XEX (envfix round 2026-08-16: a name scan of all 30,095
    // exported functions returns exactly three EnvironmentSettings SetToBlend
    // symbols -- CloudsData 0x82675FC0, ScatteringData 0x827AF468, LightingData
    // 0x827AFAA8, all 4-way). It is kept as a DECLARATION ONLY, matching the
    // DWARF/Feb-2007 class shape; nothing in the tree calls it, so it never
    // reaches the linker. Do not "fix" it with an invented body.
    void SetToBlend( const CloudsData& lValueA, float lfWeightA,
                     const CloudsData& lValueB, float lfWeightB );
    void SetToBlend( const CloudsData& lValueA0, float lfWeightA0,
                     const CloudsData& lValueA1, float lfWeightA1,
                     const CloudsData& lValueB0, float lfWeightB0,
                     const CloudsData& lValueB1, float lfWeightB1 );

    // PUBLIC per the DWARF (BrnEnvironmentData.h:187-194 are struct members with no access
    // specifier); EnvironmentManager::GenerateShaderConstants @0x827D0098 reads them by name.
public:
    float mav3LayerLiteColour[2][4]; // 0x00 (Vector3[2])
    float mav3LayerDarkColour[2][4]; // 0x20 (Vector3[2])
    float mafLayerDensity[2];        // 0x40
    float mafLayerFeathering[2];     // 0x48
    float mafLayerOpacity[2];        // 0x50
    float mafLayerSpeed[2];          // 0x58
    float mafLayerScale[2];          // 0x60
    float mfDirectionAngle;          // 0x68
};
}
}

#endif
