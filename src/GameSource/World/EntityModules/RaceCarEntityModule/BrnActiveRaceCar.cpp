// ============================================================================
// BrnWorld::ActiveRaceCar -- identity + per-frame state accessors for the live
// (simulated, in-range) half of a race car.
//
// Reconstructed from the X360 ARTIST/"Breaker" build (BURNOUT_X360_ARTIST.XEX):
//   GetActiveRaceCarIndex  @ (inlined; reads meActiveRaceCarIndex @+0x748)
//   GetGlobalRaceCar       @ (inlined; reads mpRaceCar @+0x6F0)
//   IsAttached             @ 0x822A1F10   (mpRaceCar != NULL)
//   IsActive               @ 0x822A1FB8   (muState @+0x740 == E_STATE_ACTIVE)
//   GetTransform           @ 0x822CCEB8
//   GetDirection           @ 0x822CD038
//   GetVelocity            @ 0x822CD0F8
//   IsPlayer               @ 0x822B8540
//   IsCrashing             @ 0x822A2150   (mPhysicsState.mbCrashing)
//   IsOnRaceStartState     @ 0x822A2060   (meRaceStartState @+0x77C)
//   IsInAnyRaceStartState  @ 0x822A20D8   (meRaceStartState @+0x77C)
//   SetBraking             @ 0x822B8610   (miBrakeChangeCounter @+0x738 /
//                                          mRenderParams.mbIsBraking)
//   UpdateWheelPhysicsState@ 0x822B8738   (mPhysicsState.maWheelTransforms[4] +
//                                          mRenderParams.mWheelTransforms[])
//
// ---- 2026-07-31: THREE MIS-ATTRIBUTIONS CORRECTED --------------------------
// The previous revision homed the two wheel-transform blocks, the two on-ground byte
// arrays, mbIsCrashing and mbBraking directly on ActiveRaceCar at raw offsets. They are
// not ActiveRaceCar members: block A / the on-ground bytes / the crash flag live in
// mPhysicsState (RaceCarState @+224 -> +560/+1094/+1098) and block B / its on-ground
// bytes / the braking flag live in mRenderParams (@+2016 -> +2112/+3456/+5127). Subtract
// the sub-object base from each console offset and all six land exactly. The physics-side
// wheel arrays are [4] (RaceCarState), not [6]. See the header banner.
//
// Every member access below is BY NAME through the two sub-objects; the numeric offsets
// survive only as comments (the offsetof pins are retired -- see the header's x64 note).
// Behaviour is authoritative from the asm; declaration shapes from the DecFIGS DWARF.
// ============================================================================

#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnActiveRaceCar.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCar.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnWorld
{

// ----------------------------------------------------------------------------
// The identity accessors the header declares. All four are inlined into every caller
// on the X360 (they are one load each); IsAttached is also emitted standalone
// @0x822A1F10 and IsActive @0x822A1FB8, so both keep an out-of-line home here.
// ----------------------------------------------------------------------------
EActiveRaceCarIndex ActiveRaceCar::GetActiveRaceCarIndex() const
{
    return meActiveRaceCarIndex;
}

RaceCar* ActiveRaceCar::GetGlobalRaceCar() const
{
    CGS_ASSERT(IsAttached(), "IsAttached()");

    return mpRaceCar;
}

bool ActiveRaceCar::IsAttached() const
{
    return mpRaceCar != nullptr;
}

// IsActive @ 0x822A1FB8. The two asserts are the X360's own, in asm order.
bool ActiveRaceCar::IsActive() const
{
    CGS_ASSERT(muState < E_STATE_COUNT, "muState < E_STATE_COUNT");
    CGS_ASSERT(muState == E_STATE_INACTIVE || mpRaceCar != nullptr,
               "Active ActiveRaceCar without a RaceCar");

    return muState == E_STATE_ACTIVE;
}

bool ActiveRaceCar::ToBePlacedOnTrack() const
{
    return mbToBePlacedOnTrack;
}

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
// IsCrashing @ 0x822A2150. Assert IsAttached(), then return the physics snapshot's crash
// flag (X360 this+0x52A == mPhysicsState @+224 + mbCrashing @+1098 -- the same byte
// GenerateDispatchLists reads through GetPhysicsState() when it gates the coronas).
// ----------------------------------------------------------------------------
bool ActiveRaceCar::IsCrashing() const
{
    CGS_ASSERT(IsAttached(), "IsAttached()");

    return mPhysicsState.mbCrashing;
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

    return meRaceStartState == E_RACE_START_STATE_ON_START_LINE
        || meRaceStartState == E_RACE_START_STATE_ROLLING_START;
}

// ----------------------------------------------------------------------------
// SetBraking @ 0x822B8610. Asserts (in asm order): mpGlobalRaceCar != NULL, IsAttached(),
// then -- inlined from RaceCar::GetType() -- muType < E_RACE_CAR_TYPE_COUNT. For an AI car
// the braking input drives a hysteresis counter (ramps up +1 to a +10 ceiling while
// braking, decays -2 to a -KI_MAX_BRAKE_COUNTER floor while not) and the render snapshot's
// mbIsBraking latches on once the counter is positive; every other car type publishes the
// raw braking flag. (The flag's home is mRenderParams.mbIsBraking -- X360 this+0x1BE7 ==
// mRenderParams @+2016 + mbIsBraking @+5127 -- not an ActiveRaceCar member.)
// ----------------------------------------------------------------------------
void ActiveRaceCar::SetBraking(bool lbBraking)
{
    CGS_ASSERT(GetGlobalRaceCar() != nullptr, "mpRaceCar != NULL");
    CGS_ASSERT(IsAttached(), "IsAttached()");

    RaceCar* lpGlobalRaceCar = GetGlobalRaceCar();
    CGS_ASSERT(lpGlobalRaceCar->GetType() < E_RACE_CAR_TYPE_COUNT, "muType < E_RACE_CAR_TYPE_COUNT");

    if (lpGlobalRaceCar->GetType() == E_RACE_CAR_TYPE_AI)
    {
        if (lbBraking)
        {
            miBrakeChangeCounter = miBrakeChangeCounter + 1;
            if (miBrakeChangeCounter >= KI_MAX_BRAKE_COUNTER)
            {
                miBrakeChangeCounter = KI_MAX_BRAKE_COUNTER;
            }
        }
        else
        {
            miBrakeChangeCounter = miBrakeChangeCounter - 2;
            if (miBrakeChangeCounter <= -KI_MAX_BRAKE_COUNTER)
            {
                miBrakeChangeCounter = -KI_MAX_BRAKE_COUNTER;
            }
        }

        mRenderParams.SetBraking(miBrakeChangeCounter > 0);
    }
    else
    {
        mRenderParams.SetBraking(lbBraking);
    }
}

// ----------------------------------------------------------------------------
// UpdateWheelPhysicsState @ 0x822B8738. For each of the four road wheels, copy the wheel's
// 64-byte physics transform out of the physics snapshot into BOTH the physics state
// (mPhysicsState.maWheelTransforms[4] -- X360 this+0x310) and the render snapshot
// (mRenderParams.mWheelTransforms[] -- X360 this+0x1020), and copy the wheel's on-ground
// byte into both mabWheelExists arrays (X360 this+0x526 / this+0x1560). The console does
// this with compiler-unrolled lvx128/stvx128 (whole-matrix loads/stores); the faithful C++
// is a matrix copy-assign per wheel. The inlined render-side accessor asserts the wheel
// index against KU_DEFORMATION_MODEL_DATA_MAX_WHEELS (6); the loop only ever visits the
// four road wheels, which the physics-side arrays (RaceCarState's [4]) pin.
// ----------------------------------------------------------------------------
void ActiveRaceCar::UpdateWheelPhysicsState(const void* lpPhysicsWheelData)
{
    // Read-only view of the physics wheel-data snapshot the caller
    // (RaceCarEntityModule::ReadUpdatedActiveRaceCarDataFromPhysics) passes. Layout is
    // X360-asm-attested: per-wheel entries stride 96 bytes with the 64-byte transform at
    // the front, and the four on-ground bytes packed at +0x180 (= 4 * 96).
    struct PhysicsWheelSnapshot
    {
        struct WheelEntry
        {
            Matrix44Affine mTransform;   // +0x00 (64 bytes)
            u8             mPad40[32];   // +0x40 .. +0x60 (96-byte stride)
        };
        WheelEntry maWheels[4];          // +0x000 .. +0x180
        u8         mau8OnGround[4];      // +0x180 .. +0x184
    };

    const PhysicsWheelSnapshot* lpSnapshot =
        static_cast<const PhysicsWheelSnapshot*>(lpPhysicsWheelData);

    const u32 KU_ROAD_WHEEL_COUNT = 4;
    for (u32 luWheel = 0; luWheel < KU_ROAD_WHEEL_COUNT; ++luWheel)
    {
        mPhysicsState.maWheelTransforms[luWheel] = lpSnapshot->maWheels[luWheel].mTransform;
        mPhysicsState.mabWheelExists[luWheel]    = (lpSnapshot->mau8OnGround[luWheel] != 0);

        CGS_ASSERT(luWheel < 6,
                   "luWheelIndex < BrnPhysics::Deformation::KU_DEFORMATION_MODEL_DATA_MAX_WHEELS");

        mRenderParams.GetWheelTransform(luWheel) = mPhysicsState.maWheelTransforms[luWheel];
        mRenderParams.SetWheelExists(luWheel, lpSnapshot->mau8OnGround[luWheel] != 0);
    }
}

}
