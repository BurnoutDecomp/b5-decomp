#include "GameSource/Sound/Vehicles/BrnVehicleState.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// =============================================================================
// BrnSound::Vehicles::VehicleState out-of-line bodies.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnVehicleState.h for the
// DWARF-reconciled layout (2026-08-25 wave 5: VehicleState is a STRUCT per the
// DWARF -- the old namespace modelling and the GameShared CgsState.h rival are
// both folded onto the single header definition; the ctor body relocated here
// from the CgsState.cpp rival home).
//
//   VehicleState::VehicleState        @ 0x826C9E70
//   VehicleState::AttachInfo::Construct @ 0x82681FC8
//   VehicleState::GetEngineComponentKey @ (inlined at its PhysicsControl
//                                          forwarder @0x82682D10)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{

// @ 0x826C9E70. Clear the physics blob via RaceCarState::Clear (@0x8229FFC8) and
// seed the tail fields the asm writes: the attach record zeroed (asset/index/token
// = the old rival's mi1216/mi1220/mu1224 zero-seeds), the two component-name
// strings' first chars zeroed (bytes 1268/1281), the two key elements zeroed
// (mu1296/mu1304), mfMaxRpm = 0.0, and the collision flag + the is-active
// DataPoint bytes zeroed (mu1317/mu1318). mVehicleBoostInfo is untouched by the
// X360 ctor; value-init keeps it defined without fabricating stores.
VehicleState::VehicleState()
    : BrnSound::Logic::BrnState()
    , mAttachInfo()
    , mfMaxRpm(0.0f)
    , mbCollisionOccuredFlag(false)
    , bIsRaceCarActive()
{
    mVehiclePhysicsData.Clear();            // bl RaceCarState::Clear @0x8229FFC8

    mAttachInfo.mpVehicleAsset = 0;         // the old mi1216 zero-seed
    mAttachInfo.muVehicleIndex = 0;         // the old mi1220 zero-seed
    mAttachInfo.mAttachToken   = 0;         // the old mu1224 zero-seed

    for (u32 lu = 0; lu < sizeof(mauVehicleBoostInfo); ++lu)
        mauVehicleBoostInfo[lu] = 0;        // value-defined (untouched by the X360 ctor)

    mcaEngineComponentName[0][0] = '\0';    // stb 0, +1268
    mcaEngineComponentName[1][0] = '\0';    // stb 0, +1281

    mEngineComponentKey[0].mKey  = 0;       // the old mu1296 zero-seed
    mEngineComponentKey[0].muPad = 0;
    mEngineComponentKey[1].mKey  = 0;       // the old mu1304 zero-seed
    mEngineComponentKey[1].muPad = 0;
}

// The component attribute key, by name (the console (type+0xA2)*8 walk == this
// member array: 0xA2*8 == 0x510 == +1296) with the console's non-zero guard
// (see the PhysicsControl forwarder @0x82682D10, which inlines this read).
Attribute::Key VehicleState::GetEngineComponentKey( EEngineComponentType aeComponentType ) const
{
    const Attribute::Key lKey = mEngineComponentKey[aeComponentType].mKey;
    CGS_ASSERT(lKey != 0, "mEngineComponentKey != 0");
    return lKey;
}

// =============================================================================
// VehicleState::AttachInfo::Construct  @ 0x82681FC8
//
// Validates the active-race-car index (signed bounds check: liVehicleIndex >= 0
// && < KI_MAX_ACTIVE_RACE_CARS) and the asset pointer, then writes the three
// fields in the X360 store order.
//   cmpwi r30,0 ; blt -> assert       (index < 0 fires the range assert)
//   cmpwi r30,8 ; blt skip-assert     (index >= 8 fires the range assert)
//   cmplwi r28,0 ; bne skip-assert    (asset == 0 fires the non-null assert)
//   stw r28,0 ; std r27,8 ; stw r30,4 ; return this(r31)
// =============================================================================
VehicleState::AttachInfo* VehicleState::AttachInfo::Construct(
    u64 aAttachToken, void* apVehicleAsset, u32 auVehicleIndex )
{
    // Signed bounds check -- the X360 compares as a signed int (blt/cmpwi).
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

} // namespace Vehicles
} // namespace BrnSound
