#include "GameSource/Sound/Vehicles/BrnVehicleState.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// =============================================================================
// BrnSound::Vehicles::VehicleState::AttachInfo  @ 0x82681FC8
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnVehicleState.h for the layout
// and store map. Construct validates the active-race-car index (signed bounds
// check: liVehicleIndex >= 0 && < KI_MAX_ACTIVE_RACE_CARS) and the asset pointer,
// then writes the three fields in the X360 store order.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace VehicleState
{

// @ 0x82681FC8.
//   cmpwi r30,0 ; blt -> assert       (index < 0 fires the range assert)
//   cmpwi r30,8 ; blt skip-assert     (index >= 8 fires the range assert)
//   cmplwi r28,0 ; bne skip-assert    (asset == 0 fires the non-null assert)
//   stw r28,0 ; std r27,8 ; stw r30,4 ; return this(r31)
AttachInfo* AttachInfo::Construct( u64 aAttachToken, void* apVehicleAsset, u32 auVehicleIndex )
{
    // Signed bounds check — the X360 compares as a signed int (blt/cmpwi).
    CGS_ASSERT(
        static_cast<s32>(auVehicleIndex) >= 0
            && static_cast<s32>(auVehicleIndex) < static_cast<s32>(KI_MAX_ACTIVE_RACE_CARS),
        "liVehicleIndex >= 0 && liVehicleIndex < static_cast<int32_t>(BrnWorld::KI_MAX_ACTIVE_RACE_CARS)");

    CGS_ASSERT(apVehicleAsset != 0, "lpVehicleAsset");

    mpVehicleAsset = apVehicleAsset; // stw r28, 0x00
    mAttachToken   = aAttachToken;   // std r27, 0x08
    muVehicleIndex = auVehicleIndex; // stw r30, 0x04

    return this;
}

} // namespace VehicleState
} // namespace Vehicles
} // namespace BrnSound
