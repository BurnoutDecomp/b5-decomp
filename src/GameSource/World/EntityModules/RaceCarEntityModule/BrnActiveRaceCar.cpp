// ============================================================================
// BrnWorld::ActiveRaceCar -- per-frame state accessors for the live (simulated,
// in-range) half of a race car.
//
// Reconstructed from the X360 ARTIST/"Breaker" build (BURNOUT_X360_ARTIST.XEX):
//   GetTransform           @ 0x822CCEB8
//   GetDirection           @ 0x822CD038
//   GetVelocity            @ 0x822CD0F8
//   IsPlayer               @ 0x822B8540
//   IsCrashing             @ 0x822A2150   (mbIsCrashing @+0x52A)
//   IsOnRaceStartState     @ 0x822A2060   (meRaceStartState @+0x77C)
//   IsInAnyRaceStartState  @ 0x822A20D8   (meRaceStartState @+0x77C)
//   SetBraking             @ 0x822B8610   (miBrakingCounter @+0x738 / mbBraking @+0x1BE7)
//   UpdateWheelPhysicsState@ 0x822B8738   (wheel transform blocks @+0x310/+0x1020, on-ground @+0x526/+0x1560)
//
// SCOPE: the four forwarders touch only the method-homed members (mpGlobalRaceCar via
// GetGlobalRaceCar(), mbIsAttached via IsAttached()). The five per-frame accessors touch
// per-frame state members that the committed header now homes BY NAME in its private
// layout section (offsets X360-asm-proven, sized with u8 padding). Every member access
// below is by name -- no raw-offset access into the instance.
//
// Behaviour + member offsets are authoritative from the asm; declaration shapes from the
// DecFIGS DWARF. The forwarders mirror the X360 assert order exactly (no added/inverted
// branches): the inlined GetGlobalRaceCar() contributes the trailing IsAttached() assert.
// ============================================================================

#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnActiveRaceCar.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCar.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

namespace BrnWorld
{

// PIN every X360-asm-proven byte offset the per-frame accessors depend on. This is the
// ONLY place these offsets appear numerically; all member access in the bodies is by name.
// ActiveRaceCar carries a single (private) data-member access control and no virtuals, so
// it is standard-layout and offsetof is well-defined.
#define PIN_ARC_OFFSETS()                                                                               \
    do {                                                                                                \
        static_assert(offsetof(ActiveRaceCar, maWheelPhysicsTransformA) == 784,  "wheel xform A @784");  \
        static_assert(offsetof(ActiveRaceCar, mau8WheelOnGroundA)       == 1318, "on-ground A @1318");   \
        static_assert(offsetof(ActiveRaceCar, mbIsCrashing)             == 1322, "mbIsCrashing @1322");  \
        static_assert(offsetof(ActiveRaceCar, miBrakingCounter)         == 1848, "brake counter @1848"); \
        static_assert(offsetof(ActiveRaceCar, meRaceStartState)         == 1916, "race start @1916");    \
        static_assert(offsetof(ActiveRaceCar, maWheelPhysicsTransformB) == 4128, "wheel xform B @4128"); \
        static_assert(offsetof(ActiveRaceCar, mau8WheelOnGroundB)       == 5472, "on-ground B @5472");   \
        static_assert(offsetof(ActiveRaceCar, mbBraking)                == 7143, "mbBraking @7143");     \
        static_assert(sizeof(ActiveRaceCar)                             == 7376, "instance == 0x1CD0");  \
    } while (0)

// ----------------------------------------------------------------------------
// GetTransform @ 0x822CCEB8. Forwards to the paired global slot's world transform.
// The third IsAttached() assert is the one inlined from GetGlobalRaceCar() itself.
// ----------------------------------------------------------------------------
Matrix44Affine ActiveRaceCar::GetTransform() const
{
    CGS_ASSERT(IsAttached(), "IsAttached()");
    CGS_ASSERT(GetGlobalRaceCar() != nullptr, "mpRaceCar != NULL");
    CGS_ASSERT(IsAttached(), "IsAttached()");

    return GetGlobalRaceCar()->GetTransform();
}

// ----------------------------------------------------------------------------
// GetDirection @ 0x822CD038. Forwards to the paired global slot's facing direction.
// ----------------------------------------------------------------------------
Vector3 ActiveRaceCar::GetDirection() const
{
    CGS_ASSERT(IsAttached(), "IsAttached()");
    CGS_ASSERT(GetGlobalRaceCar() != nullptr, "mpRaceCar != NULL");
    CGS_ASSERT(IsAttached(), "IsAttached()");

    return GetGlobalRaceCar()->GetDirection();
}

// ----------------------------------------------------------------------------
// GetVelocity @ 0x822CD0F8. Forwards to the paired global slot's velocity. The X360
// asm has a single IsAttached() assert here (the GetGlobalRaceCar() inline contributes
// the only one -- the mpRaceCar-NULL assert is absent in this lighter forwarder).
// ----------------------------------------------------------------------------
Vector3 ActiveRaceCar::GetVelocity() const
{
    CGS_ASSERT(IsAttached(), "IsAttached()");

    return GetGlobalRaceCar()->GetVelocity();
}

// ----------------------------------------------------------------------------
// IsPlayer @ 0x822B8540. The car is player-driven iff the paired global slot's type
// is E_RACE_CAR_TYPE_PLAYER. Asserts (in asm order): mpGlobalRaceCar != NULL, then
// IsAttached(), then -- inlined from RaceCar::GetType() -- muType < E_RACE_CAR_TYPE_COUNT.
// ----------------------------------------------------------------------------
bool ActiveRaceCar::IsPlayer() const
{
    CGS_ASSERT(GetGlobalRaceCar() != nullptr, "mpRaceCar != NULL");
    CGS_ASSERT(IsAttached(), "IsAttached()");

    RaceCar* lpGlobalRaceCar = GetGlobalRaceCar();
    CGS_ASSERT(lpGlobalRaceCar->GetType() < E_RACE_CAR_TYPE_COUNT, "muType < E_RACE_CAR_TYPE_COUNT");

    return lpGlobalRaceCar->GetType() == E_RACE_CAR_TYPE_PLAYER;
}

// ----------------------------------------------------------------------------
// IsCrashing @ 0x822A2150. Assert IsAttached(), then return the per-frame crash flag.
// ----------------------------------------------------------------------------
bool ActiveRaceCar::IsCrashing() const
{
    CGS_ASSERT(IsAttached(), "IsAttached()");

    return mbIsCrashing;
}

// ----------------------------------------------------------------------------
// IsOnRaceStartState @ 0x822A2060. Assert IsAttached(), then test the current race-start
// phase against the queried ordinal. (X360 computes the equality via subf/cntlzw/extrwi.)
// ----------------------------------------------------------------------------
bool ActiveRaceCar::IsOnRaceStartState(s32 liState) const
{
    CGS_ASSERT(IsAttached(), "IsAttached()");

    return liState == meRaceStartState;
}

// ----------------------------------------------------------------------------
// IsInAnyRaceStartState @ 0x822A20D8. Assert IsAttached(), then report whether the race
// is in either of its two start phases (ordinals 0 or 1).
// ----------------------------------------------------------------------------
bool ActiveRaceCar::IsInAnyRaceStartState() const
{
    CGS_ASSERT(IsAttached(), "IsAttached()");

    return meRaceStartState == E_RACE_START_STATE_STAGE_0
        || meRaceStartState == E_RACE_START_STATE_STAGE_1;
}

// ----------------------------------------------------------------------------
// SetBraking @ 0x822B8610. Asserts (in asm order): mpGlobalRaceCar != NULL, IsAttached(),
// then -- inlined from RaceCar::GetType() -- muType < E_RACE_CAR_TYPE_COUNT. For an AI car
// the braking input drives a hysteresis counter (ramps up +1 to a +10 ceiling while
// braking, decays -2 to a -10 floor while not) and mbBraking latches on once the counter
// is positive; every other car type takes the raw braking flag.
// ----------------------------------------------------------------------------
void ActiveRaceCar::SetBraking(bool lbBraking)
{
    CGS_ASSERT(GetGlobalRaceCar() != nullptr, "mpRaceCar != NULL");
    CGS_ASSERT(IsAttached(), "IsAttached()");

    RaceCar* lpGlobalRaceCar = GetGlobalRaceCar();
    CGS_ASSERT(lpGlobalRaceCar->GetType() < E_RACE_CAR_TYPE_COUNT, "muType < E_RACE_CAR_TYPE_COUNT");

    if (lpGlobalRaceCar->GetType() == E_RACE_CAR_TYPE_AI)
    {
        const s32 KI_BRAKING_COUNTER_MAX =  10;
        const s32 KI_BRAKING_COUNTER_MIN = -10;

        if (lbBraking)
        {
            miBrakingCounter = miBrakingCounter + 1;
            if (miBrakingCounter >= KI_BRAKING_COUNTER_MAX)
            {
                miBrakingCounter = KI_BRAKING_COUNTER_MAX;
            }
        }
        else
        {
            miBrakingCounter = miBrakingCounter - 2;
            if (miBrakingCounter <= KI_BRAKING_COUNTER_MIN)
            {
                miBrakingCounter = KI_BRAKING_COUNTER_MIN;
            }
        }

        mbBraking = (miBrakingCounter > 0);
    }
    else
    {
        mbBraking = lbBraking;
    }
}

// ----------------------------------------------------------------------------
// UpdateWheelPhysicsState @ 0x822B8738. For each of the four road wheels, copy the wheel's
// 64-byte physics transform out of the physics snapshot into BOTH transform blocks (A and B)
// and copy the wheel's on-ground byte into both on-ground arrays. The console does this with
// compiler-unrolled lvx128/stvx128 (whole-Matrix44 loads/stores); the faithful C++ is a
// Matrix44 copy-assign per wheel. The inlined accessor for block B asserts the wheel index
// against KU_DEFORMATION_MODEL_DATA_MAX_WHEELS (6); the loop only ever visits the four road
// wheels, which the on-ground arrays (sized [4]) pin.
// ----------------------------------------------------------------------------
void ActiveRaceCar::UpdateWheelPhysicsState(const void* lpPhysicsWheelData)
{
    PIN_ARC_OFFSETS();   // compile-time layout pin (no runtime cost)

    // Read-only view of the physics wheel-data snapshot the caller
    // (RaceCarEntityModule::ReadUpdatedActiveRaceCarDataFromPhysics) passes. Layout is
    // X360-asm-attested: per-wheel entries stride 96 bytes with the 64-byte transform at
    // the front, and the four on-ground bytes packed at +0x180 (= 4 * 96).
    struct PhysicsWheelSnapshot
    {
        struct WheelEntry
        {
            Matrix44 mTransform;   // +0x00 (64 bytes)
            u8       mPad40[32];   // +0x40 .. +0x60 (96-byte stride)
        };
        WheelEntry maWheels[4];    // +0x000 .. +0x180
        u8         mau8OnGround[4];// +0x180 .. +0x184
    };

    const PhysicsWheelSnapshot* lpSnapshot = static_cast<const PhysicsWheelSnapshot*>(lpPhysicsWheelData);

    const u32 KU_ROAD_WHEEL_COUNT = 4;
    for (u32 luWheel = 0; luWheel < KU_ROAD_WHEEL_COUNT; ++luWheel)
    {
        maWheelPhysicsTransformA[luWheel] = lpSnapshot->maWheels[luWheel].mTransform;
        mau8WheelOnGroundA[luWheel]       = lpSnapshot->mau8OnGround[luWheel];

        CGS_ASSERT(luWheel < 6, "luWheelIndex < BrnPhysics::Deformation::KU_DEFORMATION_MODEL_DATA_MAX_WHEELS");

        maWheelPhysicsTransformB[luWheel] = maWheelPhysicsTransformA[luWheel];
        mau8WheelOnGroundB[luWheel]       = lpSnapshot->mau8OnGround[luWheel];
    }
}

}
