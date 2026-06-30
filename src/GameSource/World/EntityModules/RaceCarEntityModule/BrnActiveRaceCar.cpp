// ============================================================================
// BrnWorld::ActiveRaceCar -- per-frame state accessors for the live (simulated,
// in-range) half of a race car.
//
// Reconstructed from the X360 ARTIST/"Breaker" build (BURNOUT_X360_ARTIST.XEX):
//   GetTransform           @ 0x822CCEB8
//   GetDirection           @ 0x822CD038
//   GetVelocity            @ 0x822CD0F8
//   IsPlayer               @ 0x822B8540
//   IsCrashing             @ 0x822A2150   (declaration-only -- un-homed member @+0x52A)
//   IsOnRaceStartState     @ 0x822A2060   (declaration-only -- un-homed member @+0x77C)
//   IsInAnyRaceStartState  @ 0x822A20D8   (declaration-only -- un-homed member @+0x77C)
//   SetBraking             @ 0x822B8610   (declaration-only -- un-homed members @+0x738/+0x1BE7)
//   UpdateWheelPhysicsState@ 0x822B8738   (declaration-only -- VMX pipeline + un-homed wheel blocks)
//
// SCOPE: the committed BrnActiveRaceCar.h is a SPARSE declaration surface -- it homes
// only the four X360-attested members the global RaceCar lifecycle code touches
// (miActiveRaceCarIndex @+0x748, mpGlobalRaceCar @+0x6F0, mbIsAttached, mbToBePlacedOnTrack
// @+0x7C4) plus the nested RenderParams; the full 0x1CD0/7376-byte instance is NOT laid
// out. So only the four forwarders that touch ONLY homed members (mpGlobalRaceCar via
// GetGlobalRaceCar(), mbIsAttached via IsAttached()) are bodied here. The five methods
// that read/write per-frame state at deep un-homed offsets are declared (in the header)
// but left undefined: bodying them would require either laying out the whole instance
// (out of scope for this declaration-surface header) or raw-offset pointer hacks into a
// committed aggregate (forbidden). The per-TU `cl /c` gate compiles, not links, so the
// declared-but-undefined methods compile cleanly. The X360 behaviour of each deferred
// method is recorded in the header comment next to its declaration.
//
// Behaviour + member offsets are authoritative from the asm; declaration shapes from the
// DecFIGS DWARF. The forwarders mirror the X360 assert order exactly (no added/inverted
// branches): the inlined GetGlobalRaceCar() contributes the trailing IsAttached() assert.
// ============================================================================

#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnActiveRaceCar.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCar.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnWorld
{

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
// IsCrashing / IsOnRaceStartState / IsInAnyRaceStartState / SetBraking /
// UpdateWheelPhysicsState are DECLARATION-ONLY (see the header). Their bodies read/write
// per-frame ActiveRaceCar state at deep un-homed offsets (and, for UpdateWheelPhysicsState,
// a multi-stage VMX copy) that this sparse declaration-surface header does not lay out; they
// are intentionally left undefined and resolve at link against the not-yet-homed full layout.
// ----------------------------------------------------------------------------

}
