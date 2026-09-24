#include "GameSource/Sound/Collision/BrnRaceCarCache.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // RCEntityActiveRaceCarOutputInterface, RaceCarState
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationState.h"  // DeformationState, CarState

// =============================================================================
// BrnSound::Logic::Collision::RaceCarCache -- out-of-line body.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnRaceCarCache.h for the
// RaceCarCacheNode layout (Vector3 + DataPoint<bool> + DataPoint<Matrix44Affine>
// + Vector3 + Vector3, X360-attested 192-byte stride) and the X360-32-bit-vs-
// host-64-bit offset note.
//
// This TU's recon'd function set:
//   RaceCarCache::GetRaceCar  @ 0x82683068
//   RaceCarCache::Update      @ 0x826BF478  (2026-09-24; the TU is mounted from then on --
//     CollisionStateManager's MapPositionToOrientation needs GetRaceCar at link time)
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

// ---------------------------------------------------------------------------
// RaceCarCache::Update(const VehicleInterface&, const DeformationState&)  @ 0x826BF478
//   DWARF (BrnCollisionStateManager.h:151, body cpp:3920; locals `uint32_t i`,
//   `const RaceCarState* lpState`, `const CarState* lpCarState`).
//
//   for i in 0..7 (r26; the inlined accessor's h:854/855 index tripwires 0x826BF4D4..0x826BF510):
//     mbActive = IsRaceCarActive(i)             ; lhz maxRaceCarFlags[i] (+0x2780) ; clrlwi 31
//                                               ; DataPoint<bool>::operator=: +0x11 <- +0x10,
//                                               ;   +0x10 <- new (0x826BF518..0x826BF524)
//     if (current active):
//       lpState = GetRaceCarState(i)            ; bl 0x8227D690
//       mTransform = lpState->mTransform        ; +0x1F0, 4 rows: previous (+0x60..) <- current
//                                               ;   (+0x20..), current <- new (0x826BF544..598)
//       lpCarState = GetCarStateFromEntityId(lpState->mEntityId)   ; lwz 0x3C8 ; bl 0x822CC340
//       if (lpCarState):
//         mComOffset = lpState->mComOffset      ; +0x360 -> node+0x00
//         mMin / mMax = lpCarState->GetDeformedBBox()   ; +0x640 / +0x650 -> node+0xA0 / +0xB0
//
// A car that goes inactive keeps its last transform and box (only mbActive moves); the COM
// offset and the box are refreshed only while the deformation manager has a state for the car.
// The DWARF getter name is GetCarStateFromEntityId; this tree spells it GetCarStateF (the
// IDA-truncated export name) -- the same body @0x822CC340.
// ---------------------------------------------------------------------------
void RaceCarCache::Update( const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface& lInterface,
                           const BrnPhysics::Deformation::DeformationState& lDeformationState )
{
    for ( u32 i = 0; i < KU_MAX_NUM_RACE_CARS; ++i )
    {
        RaceCarCacheNode& lrCar = maRaceCars[i];
        const EActiveRaceCarIndex leIndex = static_cast<EActiveRaceCarIndex>( i );
        lrCar.mbActive = lInterface.IsRaceCarActive( leIndex );
        if ( lrCar.mbActive.GetCurrent() )
        {
            const BrnPhysics::Vehicle::RaceCarState* lpState = lInterface.GetRaceCarState( leIndex );
            lrCar.mTransform = lpState->mTransform;
            const BrnPhysics::Deformation::CarState* lpCarState =
                lDeformationState.GetCarStateF( lpState->mEntityId.muValue );
            if ( lpCarState )
            {
                lrCar.mComOffset = lpState->mComOffset;
                lrCar.mMin = lpCarState->mDeformedBBoxMin;
                lrCar.mMax = lpCarState->mDeformedBBoxMax;
            }
        }
    }
}

} // namespace Collision
} // namespace Logic
} // namespace BrnSound
