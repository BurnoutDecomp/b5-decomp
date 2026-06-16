#include "SharedClasses/Traffic/BrnTrafficLightTrigger.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"           // rw::math::vpu::IsValid (per-lane NaN test) -- canonical home

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnTraffic::LightTriggerStartData::GetStartPosition  @ 0x8231BB50
//   BrnTraffic::LightTriggerStartData::GetStartDirection @ 0x8231BC68
//
// Both functions are the classic SIMD-return-by-value pattern: the hidden sret
// pointer, _savegprlr/_restgprlr, and lvx128/stvx128 are compiler artifacts and are
// dropped; the three-lane vspltw+vcmpeqfp cascade is the inlined RwMath::IsValid NaN
// test on lanes x/y/z, reversed to a single RwMath::IsValid(...) call. The bounds/NaN
// guards use the project CGS_ASSERT macro (X360-baked file/line discarded per convention).

namespace BrnTraffic
{

// GetStartPosition (X360 @ 0x8231BB50) - spawn position for start-grid slot luIndex.
// R31 = 16*luIndex + this -> &maStartingPositions[luIndex] (array at +0).
Vector3 LightTriggerStartData::GetStartPosition(u32 luIndex) const
{
    CGS_ASSERT(muNumStartingPositions <= KU_MAX_START_POSITIONS, "muNumStartingPositions <= KU_MAX_START_POSITIONS");
    CGS_ASSERT(luIndex < muNumStartingPositions, "luIndex < muNumStartingPositions");
    CGS_ASSERT(rw::math::vpu::IsValid(maStartingPositions[luIndex]), "RwMath::IsValid( maStartingPositions[luIndex] )");
    return maStartingPositions[luIndex];
}

// GetStartDirection (X360 @ 0x8231BC68) - facing for start-grid slot luIndex.
// R31 = 16*(luIndex + 8) + this -> &maStartingDirections[luIndex] (the directions array
// follows the 8-entry positions array, base +128).
Vector3 LightTriggerStartData::GetStartDirection(u32 luIndex) const
{
    CGS_ASSERT(muNumStartingPositions <= KU_MAX_START_POSITIONS, "muNumStartingPositions <= KU_MAX_START_POSITIONS");
    CGS_ASSERT(luIndex < muNumStartingPositions, "luIndex < muNumStartingPositions");
    CGS_ASSERT(rw::math::vpu::IsValid(maStartingDirections[luIndex]), "RwMath::IsValid( maStartingDirections[luIndex] )");
    return maStartingDirections[luIndex];
}

}
