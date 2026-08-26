#include "SharedClasses/Traffic/BrnTrafficLightTrigger.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"           // rw::math::vpu::IsValid (per-lane NaN test) -- canonical home
#include "SharedClasses/Traffic/Junctions/BrnTrafficLightCollection.h" // BrnTraffic::ExpandPosPlusYRotToTransform (its current home)

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

// =============================================================================================
// LightTrigger::GetTransform -- DWARF BrnTrafficLightTrigger.h:94. No standalone X360 symbol.
//
// [stuntrace waveD, agent D1] Recovered from its single fold site, the one function in the image
// that turns a light trigger into a world volume --
// BrnTraffic::TrafficEntityModule::ManageTriggers @0x82747518:
//   0x827477F0  lvx128 v1, r31, r23          ; r23 == 16 -> &mPosPlusYRot
//   0x827477F4  addi   r3, r1, var_F0        ; the sret slot for the returned Matrix44Affine
//   0x82747800  bl     BrnTraffic::ExpandPosPlusYRotToTransform
// i.e. the accessor IS that call, with the record's own packed lane as the argument. The callee
// (a rotation about Y by the packed w, with the packed xyz as the translation row) is
// reconstructed in Junctions/BrnTrafficLightCollection.cpp -- read its banner before touching the
// row assembly; the DWARF home for BOTH is SharedClasses/Traffic/BrnTrafficSharedMaths.h:81,
// which this tree has no mirror of yet.
// =============================================================================================
Matrix44Affine LightTrigger::GetTransform() const
{
    return ExpandPosPlusYRotToTransform(mPosPlusYRot);
}

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

// GetNumStartPositions - the start-grid slot count. NO standalone X360 symbol (the compiler
// folded it into every caller), so it is recovered from the folds, all of which agree that it is
// the zero-extended byte at this+0x190:
//   * ModeManager::SetStartingGrid @0x82328608 -- the OFFLINE grid seater, and the fold that
//     names it, at 0x823286D8: `lbz r11, 0x190(r29)` / `cmplw cr6, r11, r23` / `bge` / assert
//     "lpStartData->GetNumStartPositions() >= static_cast<uint32_t>( liCarCount )"
//     (BrnModeManager.cpp:4156, `li r5, 0x103C`) -- i.e. the very predicate this accessor feeds
//     is the one the console spells with this function's name.
//   * the two exported accessors above read the SAME byte for their own bound and spell it in
//     their assert text: "muNumStartingPositions <= KU_MAX_START_POSITIONS"
//     (0x8231BB68 / 0x8231BC80, both `lbz r11, 0x190(r31)`).
// +0x190 == 400 == muNumStartingPositions in the DWARF member run below the two Vector3[8]
// arrays (+0, +128), the CgsID[16] destination table (+256) and the u8[16] difficulty table
// (+384). The member is u8; the accessor's DWARF return type is u32, hence the widening.
//
// Landed 2026-08-26 (stuntrace waveB mount closure): SetStartingGrid is in the exe now, so this
// symbol is a hard LNK2019 rather than a shape-coherence declaration.
u32 LightTriggerStartData::GetNumStartPositions() const
{
    return muNumStartingPositions;
}

}
