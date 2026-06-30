// ============================================================================
// BrnWorld::RaceCar -- global race-car slot lifecycle + positioning.
//
// Reconstructed from the X360 ARTIST/"Breaker" build (BURNOUT_X360_ARTIST.XEX):
//   Construct             @ 0x822A4B08
//   Prepare               @ 0x822A4BE0
//   Release               (mirrors Construct; not in this trace's func set -- the
//                          ledgered eight below are the asm-attested bodies)
//   AddToWorld            @ 0x822BE4F0
//   AssignActiveRaceCar   @ 0x822BE8C8
//   RemoveActiveRaceCar   @ 0x822BEA00
//   RemoveFromWorld       @ 0x822A4C98
//   RequestResetOnTrack   @ 0x822BEB28
//   UpdatePositioningData @ 0x822D3788
//
// Behaviour + member offsets are authoritative from the asm; declaration shapes from
// the DecFIGS DWARF (BrnRaceCar.h). The Feb-2007 partial source supplied idiom only.
//
// The X360 build inlines several trivial helpers that this reconstruction restores as
// real calls/members:
//   * "mTransform = RwMath::GetMatrix44Affine_Identity()" -> mTransform.SetIdentity()
//     (the asm loads the identity rows from rw::math::vpu::detail::gIVector + zero wAxis).
//   * RwMath::IsValid(lTransform) -> rw::math::vpu::IsValid(...) (the per-lane
//     vcmpeqfp self-equality NaN cascade over the four matrix rows).
//   * IsInWorld() -> muType != E_RACE_CAR_TYPE_INACTIVE (the asserts spell "IsInWorld()").
// ============================================================================

#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCar.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnActiveRaceCar.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT + Begin/Fire/End
#include "GameShared/GameClasses/Development/CgsStrStream.h"  // CgsDev::StrStream (streamed asserts)
#include "GameShared/GameClasses/World/CgsWorldMap2D.h"       // CgsWorld::WorldMap2D::GetValue
#include "rw/math/vpu/matrix44affine_operation.h"             // rw::math::vpu::IsValid(Matrix44Affine)
#include "rw/math/vpu/vector3_operation.h"                    // rw::math::vpu::IsValid(Vector3)

namespace BrnWorld
{

// kCGSID_NULL -- the empty content id (base-40 compress of the empty string == 0).
static const CgsID KU_CGSID_NULL = 0;

// Highest valid rival index tracked in the world (KI_MAX_RIVALS_IN_WORLD). The X360
// AddToWorld asm range-checks the rival index against 0x22 (34) -- the rival-slot count.
static const s32 KI_MAX_RIVALS_IN_WORLD = 34;

// "no rival" sentinel for the rival index (non-AI cars store this).
static const s8 KI_INVALID_RIVAL = -1;

// ----------------------------------------------------------------------------
// Construct @ 0x822A4B08. One-time construction of a global slot: identity transform,
// cleared identity/region/flags, and the permanent global-slot index.
// ----------------------------------------------------------------------------
void RaceCar::Construct(EGlobalRaceCarIndex leGlobalRaceCarIndex)
{
    mTransform.SetIdentity();
    mPreviousPosition.SetZero();

    mRivalId = KU_CGSID_NULL;
    mModelId = KU_CGSID_NULL;
    mWheelId = KU_CGSID_NULL;

    muType                = E_RACE_CAR_TYPE_INACTIVE;
    miGlobalRaceCarIndex  = static_cast<s8>(leGlobalRaceCarIndex);
    mfPersistentDamage    = 0.0f;

    mbIsInGameMode               = false;
    mbCarSelectAllowedInGameMode = false;
    miRivalIndex                 = -1;
    miRivalDistrict              = -1;

    ClearActiveRaceCarIndex();
    mWorldRegion.Construct(E_DISTRICT_INVALID);
    ClearActiveRaceCar();
    SetCanPassThroughTraffic(false);

    mbIsDispersing = false;
}

// ----------------------------------------------------------------------------
// Prepare @ 0x822A4BE0. Resets a slot's per-use state before it is added to the world.
// Unlike Construct it does NOT (re)assign the global-slot index, and leaves mVelocity,
// mfResetOnTrackDistance, miStartingGridPosition and mWorldRegion untouched.
// ----------------------------------------------------------------------------
bool RaceCar::Prepare()
{
    mTransform.SetIdentity();
    mPreviousPosition.SetZero();

    mRivalId = KU_CGSID_NULL;
    mModelId = KU_CGSID_NULL;
    mWheelId = KU_CGSID_NULL;

    muType = E_RACE_CAR_TYPE_INACTIVE;

    mbIsInGameMode               = false;
    mbCarSelectAllowedInGameMode = false;
    miRivalIndex                 = -1;
    miRivalDistrict              = -1;

    miColourIndex      = -1;
    miColourPalette    = -1;
    miOpponentIndex    = -1;
    mfPersistentDamage = 0.0f;

    mfResetOnTrackSpeed = 0.0f;
    meResetOnTrackType  = BrnAI::E_RESET_TYPE_INVALID;
    mbToBeResetOnTrack  = false;

    ClearActiveRaceCar();
    ClearActiveRaceCarIndex();
    SetAllowedInRoadRage(false);

    mbIsDispersing = false;

    return true;
}

// ----------------------------------------------------------------------------
// AddToWorld @ 0x822BE4F0. Activates a prepared slot with a type, transform and
// identity. "Rival" cars (E_RACE_CAR_TYPE_AI) carry a rival index + opponent index;
// all other types must pass the invalid sentinels.
// ----------------------------------------------------------------------------
void RaceCar::AddToWorld(ERaceCarType leType,
                         const Matrix44Affine& lTransform,
                         CgsID lRivalId,
                         CgsID lModelId,
                         CgsID lWheelModelId,
                         s8 liRivalIndex,
                         s32 liOpponentIndex)
{
    CGS_ASSERT(miGlobalRaceCarIndex != E_GLOBAL_RACE_CAR_INDEX_INVALID, "Using unprepared RaceCar");
    CGS_ASSERT(mpActiveRaceCar == nullptr, "RaceCar in a weird state in AddToWorld");

    if (leType != E_RACE_CAR_TYPE_PLAYER
        && leType != E_RACE_CAR_TYPE_NETWORK
        && leType != E_RACE_CAR_TYPE_AI)
    {
        char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << "Invalid RaceCar type in AddToWorld: " << static_cast<s32>(leType);
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
        CgsDev::Assert::EndAssert();
    }

    CGS_ASSERT(rw::math::vpu::IsValid(lTransform), "RwMath::IsValid( lTransform )");
    CGS_ASSERT(lModelId      != KU_CGSID_NULL, "lModelId != kCGSID_NULL");
    CGS_ASSERT(lWheelModelId != KU_CGSID_NULL, "lWheelModelId != kCGSID_NULL");

    muType     = static_cast<u8>(leType);
    mTransform = lTransform;
    mRivalId   = lRivalId;
    mModelId   = lModelId;
    mWheelId   = lWheelModelId;

    mbIsInGameMode               = false;
    mbCarSelectAllowedInGameMode = false;

    if (leType == E_RACE_CAR_TYPE_AI)
    {
        // X360 @0x822BE4F0 raw asm (0x822BE82C): extsb r29 then
        //   if (r29 <  0)   goto check_sentinel;
        //   if (r29 <  34)  goto valid;        // [0,34) ok
        //   check_sentinel: if (r29 == -1) goto valid;   else fire
        // i.e. FIRE iff (idx < 0 || idx >= KI_MAX_RIVALS_IN_WORLD) && idx != -1. Valid only for
        // [0, KI_MAX_RIVALS_IN_WORLD) or the -1 sentinel. (Hex-Rays renders this as `SBYTE3 >= 0x22 &&
        // SBYTE3 != -1`, which DROPS the negative-range branch -- do not trust that simplification;
        // the real check rejects negatives other than -1 too.)
        if ((liRivalIndex < 0 || liRivalIndex >= KI_MAX_RIVALS_IN_WORLD) && liRivalIndex != KI_INVALID_RIVAL)
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Dodgy rival index  " << static_cast<s32>(liRivalIndex) << "\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }

        miRivalIndex    = liRivalIndex;
        miOpponentIndex = liOpponentIndex;
        mbIsDispersing  = false;
    }
    else
    {
        miOpponentIndex = liOpponentIndex;
        mbIsDispersing  = false;
        miRivalIndex    = KI_INVALID_RIVAL;
    }
}

// ----------------------------------------------------------------------------
// AssignActiveRaceCar @ 0x822BE8C8. Binds the active (simulated) car to this slot and
// adopts its active-race-car index. The two must reference each other.
// ----------------------------------------------------------------------------
void RaceCar::AssignActiveRaceCar(ActiveRaceCar* lpActiveRaceCar)
{
    CGS_ASSERT(GetType() < E_RACE_CAR_TYPE_COUNT, "muType < E_RACE_CAR_TYPE_COUNT");
    CGS_ASSERT(IsInWorld(), "IsInWorld()");
    CGS_ASSERT(lpActiveRaceCar != nullptr, "lpActiveRaceCar != NULL");

    mpActiveRaceCar = lpActiveRaceCar;

    SetActiveRaceCarIndex(lpActiveRaceCar->GetActiveRaceCarIndex());

    CGS_ASSERT(miActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID,
               "miActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID");
    CGS_ASSERT(mpActiveRaceCar->IsAttached(), "IsAttached()");
    CGS_ASSERT(mpActiveRaceCar->GetGlobalRaceCar() == this, "mpActiveRaceCar->GetGlobalRaceCar() == this");
}

// ----------------------------------------------------------------------------
// RemoveActiveRaceCar @ 0x822BEA00. Detaches the active car; the active-race-car index
// is cleared unless the slot is kept reserved for an in-progress game mode that does
// not allow car-select (i.e. cleared when !mbIsInGameMode || mbCarSelectAllowedInGameMode).
// ----------------------------------------------------------------------------
void RaceCar::RemoveActiveRaceCar()
{
    CGS_ASSERT(GetType() < E_RACE_CAR_TYPE_COUNT, "muType < E_RACE_CAR_TYPE_COUNT");
    CGS_ASSERT(IsInWorld(), "IsInWorld()");
    CGS_ASSERT(mpActiveRaceCar != nullptr, "mpActiveRaceCar != NULL");
    CGS_ASSERT(mpActiveRaceCar->IsAttached(), "IsAttached()");
    CGS_ASSERT(mpActiveRaceCar->GetGlobalRaceCar() == this, "mpActiveRaceCar->GetGlobalRaceCar() == this");

    ClearActiveRaceCar();

    if (!mbIsInGameMode || mbCarSelectAllowedInGameMode)
    {
        ClearActiveRaceCarIndex();
    }
}

// ----------------------------------------------------------------------------
// RemoveFromWorld @ 0x822A4C98. Returns the slot to the inactive state.
// ----------------------------------------------------------------------------
void RaceCar::RemoveFromWorld()
{
    CGS_ASSERT(GetType() < E_RACE_CAR_TYPE_COUNT, "muType < E_RACE_CAR_TYPE_COUNT");
    CGS_ASSERT(IsInWorld(), "IsInWorld()");
    CGS_ASSERT(mpActiveRaceCar == nullptr,
               "RaceCar being removed from world while it still has an ActiveRaceCar attached");

    muType         = E_RACE_CAR_TYPE_INACTIVE;
    mbIsDispersing = false;
}

// ----------------------------------------------------------------------------
// RequestResetOnTrack @ 0x822BEB28. Queues a reset-on-track request, unless one is
// already pending or the attached active car is already due to be placed on track.
// ----------------------------------------------------------------------------
void RaceCar::RequestResetOnTrack(f32 lfSpeed, BrnAI::EResetType leType, f32 lfDistance)
{
    if (mbToBeResetOnTrack)
    {
        return;
    }

    if (HasActiveRaceCar() && GetActiveRaceCar()->ToBePlacedOnTrack())
    {
        return;
    }

    mfResetOnTrackSpeed    = lfSpeed;
    mfResetOnTrackDistance = lfDistance;
    meResetOnTrackType     = leType;
    mbToBeResetOnTrack     = true;

    CGS_ASSERT(mfResetOnTrackSpeed >= 0.0f, "mfResetOnTrackSpeed >= 0.0f");
}

// ----------------------------------------------------------------------------
// UpdatePositioningData @ 0x822D3788. Snapshots the previous position, stores the new
// transform, then resamples the world region (district -> county) at the new position.
// ----------------------------------------------------------------------------
void RaceCar::UpdatePositioningData(const Matrix44Affine& lTransform, CgsWorld::WorldMap2D* lpWorldMap)
{
    CGS_ASSERT(GetType() < E_RACE_CAR_TYPE_COUNT, "muType < E_RACE_CAR_TYPE_COUNT");
    CGS_ASSERT(IsInWorld(), "IsInWorld()");

    if (!rw::math::vpu::IsValid(lTransform))
    {
        char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << "Invalid transform for out-of-range racecar "
                   << static_cast<s32>(miGlobalRaceCarIndex) << ": ";
        // (the X360 then streams the transform itself)
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
        CgsDev::Assert::EndAssert();
    }

    mPreviousPosition = mTransform.Pos();
    mTransform        = lTransform;

    EDistrict leDistrict = static_cast<EDistrict>(lpWorldMap->GetValue(mTransform.Pos()));
    if (leDistrict == static_cast<EDistrict>(CgsWorld::KU_INVALID_WORLD_MAP_VALUE))
    {
        leDistrict = E_DISTRICT_INVALID;
    }

    // The `leDistrict < E_DISTRICT_COUNT` assert the X360 shows here is inlined from
    // WorldRegion::Construct (BrnWorldRegion.h:155) -- it lives once in WorldRegion::Construct
    // (which we call below), so it is NOT duplicated at this call site.
    mWorldRegion.Construct(leDistrict);
}

// ----------------------------------------------------------------------------
// UpdateVelocity @ 0x822B3DE0. Validates the supplied velocity (the X360 RwMath::IsValid
// per-lane NaN check over x/y/z) and stores it into mVelocity (+0x50).
// ----------------------------------------------------------------------------
void RaceCar::UpdateVelocity(Vector3 lVelocity)
{
    CGS_ASSERT(GetType() < E_RACE_CAR_TYPE_COUNT, "muType < E_RACE_CAR_TYPE_COUNT");
    CGS_ASSERT(IsInWorld(), "IsInWorld()");
    CGS_ASSERT(rw::math::vpu::IsValid(lVelocity), "RwMath::IsValid( lVelocity )");

    mVelocity = lVelocity;
}

// ----------------------------------------------------------------------------
// SetInCurrentGameMode @ 0x822B3F08. Marks whether this slot participates in the current
// game mode (and whether car-select is allowed). Entering game mode requires the slot to
// already be in the world. When leaving game mode and there is no attached active car the
// active-race-car index is released back to invalid.
// ----------------------------------------------------------------------------
void RaceCar::SetInCurrentGameMode(bool lbInGameMode, bool lbCarSelectAllowed)
{
    if (lbInGameMode)
    {
        CGS_ASSERT(IsInWorld(), "!(lbIsInGameMode && !IsInWorld())");
    }

    mbIsInGameMode               = lbInGameMode;
    mbCarSelectAllowedInGameMode = lbCarSelectAllowed;

    if (!lbInGameMode && mpActiveRaceCar == nullptr)
    {
        miActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
    }
}

// ----------------------------------------------------------------------------
// SetActiveRaceCarIndex @ 0x822A1030. Records the active-race-car slot index. Re-assigning
// to a different index while one is already set (and not the invalid sentinel) is an error.
// ----------------------------------------------------------------------------
void RaceCar::SetActiveRaceCarIndex(EActiveRaceCarIndex leIndex)
{
    if (miActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID
        && miActiveRaceCarIndex != static_cast<s8>(leIndex))
    {
        char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << "Active car index modified when not unused";
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
        CgsDev::Assert::EndAssert();
    }

    miActiveRaceCarIndex = static_cast<s8>(leIndex);
}

// ----------------------------------------------------------------------------
// GetActiveRaceCar @ 0x822B3988. Returns the attached active (simulated) car, or null.
// ----------------------------------------------------------------------------
ActiveRaceCar* RaceCar::GetActiveRaceCar()
{
    CGS_ASSERT(GetType() < E_RACE_CAR_TYPE_COUNT, "muType < E_RACE_CAR_TYPE_COUNT");
    CGS_ASSERT(IsInWorld(), "IsInWorld()");

    return mpActiveRaceCar;
}

// ----------------------------------------------------------------------------
// GetPosition @ 0x822B3500. The car's world position (mTransform translation row, +0x30).
// ----------------------------------------------------------------------------
Vector3 RaceCar::GetPosition() const
{
    CGS_ASSERT(GetType() < E_RACE_CAR_TYPE_COUNT, "muType < E_RACE_CAR_TYPE_COUNT");
    CGS_ASSERT(IsInWorld(), "IsInWorld()");

    return mTransform.Pos();
}

// ----------------------------------------------------------------------------
// GetDirection @ 0x822B3610. The car's forward direction (mTransform At/zAxis row, +0x20).
// ----------------------------------------------------------------------------
Vector3 RaceCar::GetDirection() const
{
    CGS_ASSERT(GetType() < E_RACE_CAR_TYPE_COUNT, "muType < E_RACE_CAR_TYPE_COUNT");
    CGS_ASSERT(IsInWorld(), "IsInWorld()");

    return mTransform.At();
}

// ----------------------------------------------------------------------------
// HasActiveRaceCar @ 0x822A0CD8. True iff this slot is in the world and has an attached
// active car. An inactive slot short-circuits to false (no IsInWorld assert is taken).
// ----------------------------------------------------------------------------
bool RaceCar::HasActiveRaceCar() const
{
    CGS_ASSERT(GetType() < E_RACE_CAR_TYPE_COUNT, "muType < E_RACE_CAR_TYPE_COUNT");

    return muType != E_RACE_CAR_TYPE_INACTIVE && mpActiveRaceCar != nullptr;
}

// ----------------------------------------------------------------------------
// IsInRangeRival @ 0x822A0F20. An AI car that currently has an active (in-range) car.
// ----------------------------------------------------------------------------
bool RaceCar::IsInRangeRival() const
{
    CGS_ASSERT(GetType() < E_RACE_CAR_TYPE_COUNT, "muType < E_RACE_CAR_TYPE_COUNT");

    if (muType == E_RACE_CAR_TYPE_AI)
    {
        return HasActiveRaceCar();
    }

    return false;
}

// ----------------------------------------------------------------------------
// IsOutOfRangeRival @ 0x822A0E90. An AI car that is currently out of range (no active car).
// ----------------------------------------------------------------------------
bool RaceCar::IsOutOfRangeRival() const
{
    CGS_ASSERT(GetType() < E_RACE_CAR_TYPE_COUNT, "muType < E_RACE_CAR_TYPE_COUNT");

    return muType == E_RACE_CAR_TYPE_AI && !HasActiveRaceCar();
}

// ----------------------------------------------------------------------------
// ToBeRenderedDamaged @ 0x822B3D70. Whether this car should render with accumulated damage:
// it has persistent damage, or it is the player car, or it is a network car.
// ----------------------------------------------------------------------------
bool RaceCar::ToBeRenderedDamaged() const
{
    if (mfPersistentDamage > 0.0f)
    {
        return true;
    }

    if (IsPlayerDriven())
    {
        return true;
    }

    if (IsNetworkDriven())
    {
        return true;
    }

    return false;
}

}
