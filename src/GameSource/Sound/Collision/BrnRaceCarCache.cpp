#include "GameSource/Sound/Collision/BrnRaceCarCache.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// =============================================================================
// BrnSound::Logic::Collision::RaceCarCache -- out-of-line body.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnRaceCarCache.h for the
// RaceCarCacheNode layout (Vector3 + DataPoint<bool> + DataPoint<Matrix44Affine>
// + Vector3 + Vector3, X360-attested 192-byte stride) and the X360-32-bit-vs-
// host-64-bit offset note.
//
// This TU's recon'd function set is ONE entry:
//   RaceCarCache::GetRaceCar  @ 0x82683068
// (RaceCarCache::Update is declared-only / DEFERRED in the header -- it touches the
//  un-homed VehicleInterface / DeformationState surface; mirrors BrnHingeStateCache.)
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Collision
{

// ---------------------------------------------------------------------------
// RaceCarCache::GetRaceCar(u32 luIndex) const  @ 0x82683068
//   DWARF (BrnCollisionStateManager.h:154):
//     const RaceCarCacheNode* GetRaceCar(uint32_t) const;
//
//   cmplwi cr6, r31, 8 ; blt loc_826830AC     ; if (luIndex >= 8) { assert }
//     -> FireAssert("luIndex >= 0 && luIndex < BrnPhysics::Vehicle::ku8MaxNumRaceCars")
//                                                ; NON-GATING (no return)
//   loc_826830AC:
//     slwi r11,r31,1 ; add r11,r31,r11 ; slwi r11,r11,6   ; luIndex*3*64 == luIndex*192
//     add  r3,r11,r30                                     ; this + luIndex*192
//     return                                              ; == &maRaceCars[luIndex]
//
// Bounds-checks luIndex against the 8-entry maRaceCars[] and returns
// &maRaceCars[luIndex]. The X360 codegen's flat `luIndex*192` is the materialisation
// of the RaceCarCacheNode array stride (sizeof(RaceCarCacheNode) == 192 on the X360
// 32-bit ABI); reproduced BY NAME as an array subscript, not a raw offset walk, so the
// host sizeof drives the stride (no 192 static_assert across the 32/64 ABI). The assert
// is NON-GATING (no early return): the indexed pointer is returned on the out-of-range
// path too, matching the X360 ordering.
//
// CONDITION uses the local KU_MAX_NUM_RACE_CARS (== 8) -- the symbol
// BrnPhysics::Vehicle::ku8MaxNumRaceCars is NOT declared anywhere in-tree (it appears
// only inside assert MESSAGE strings), so it cannot be used in the compiled condition;
// the fully-qualified name is preserved VERBATIM in the message string exactly as the
// X360 rodata carries it (no trailing '\n').
// ---------------------------------------------------------------------------
const RaceCarCache::RaceCarCacheNode* RaceCarCache::GetRaceCar( u32 luIndex ) const
{
    CGS_ASSERT( luIndex < KU_MAX_NUM_RACE_CARS,
                "luIndex >= 0 && luIndex < BrnPhysics::Vehicle::ku8MaxNumRaceCars" );
    return &maRaceCars[luIndex];
}

} // namespace Collision
} // namespace Logic
} // namespace BrnSound
