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
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // RCEntityGlobalRaceCarOutputInterface (FillInOutputInterface)
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
//
// ⛔⛔ NOTHING IN THIS TREE READS mbToBeResetOnTrack. MEASURED 2026-08-25, and the number
// the crash waves have been carrying ("~25 functions, ~2500 instructions") IS TOO SMALL BY
// MORE THAN HALF, and names the wrong blocker. The real chain and its real cost:
//
//   RCEM::SendResetOnTrackRequests @0x822CE178 (57)   -- the only reader of this flag. Walks
//       all 35 global race cars and, for each with muType != 3 && mbToBeResetOnTrack, pushes
//       an AIModuleIO::ResetOnTrackRequest onto RaceCarEntityModuleIO::OutputBuffer_PostScene.
//       Called ONLY from RCEM::PostSceneUpdate @0x822FE3F0. ABSENT.
//   -> WorldModule::BridgeRaceCarModuleToAIModule_PostScene            -- INERT boot gate
//   -> BrnAI::AIModule::Update @0x8279B478 (319)                       -- INERT boot gate
//      -> AIModule::UpdateResetOnTrackManager @0x8279ABB0 (192)        -- ABSENT
//         -> ResetOnTrackManager::Update @0x8279A890 (199) and 32 siblings
//            (ProcessResetOnTrackRequest 279, ComputeResetOnTrack 134, ScanBackwards/Forwards
//             AlongExtrapolatedRoute 284/230, UpdateResetOnTrackSectionUsingCurrentSection 299,
//             AvoidObstacles 263, ComputeAISectionWidth 209, ConvertNodesToPositionAndDirection
//             201, ComputeInitialCoordinatesStandard 219, ResetNearRoutelessPlayer 189,
//             InterpolatePositionFromAngle 186, ...) -- 4,750 insns, ONE bodied (GetAICar).
//   -> RCEM::ProcessResetOnTrackResultQueue @0x822F4580 (192), from PrePhysicsUpdate -- ABSENT
//   -> ActiveRaceCar::RequestPlaceOnTrack -> PlaceOnTrackManager -> RCEM::ResetActiveRaceCar
//      -> VehicleInputInterface::ResetRaceCar -> the ResetVehicleEvent drain. ALL REAL, ALL LIVE.
//   Direct closure, counted from the ARTIST export set: 37 functions / 5,307 instructions.
//
// ⭐⭐ AND THE MANAGER IS NOT THE BLOCKER -- THE AI MODULE IS, BECAUSE IT DOES NOT RUN AT ALL.
//   ResetOnTrackManager is an EMBEDDED MEMBER of AIModule at +286128, and its only constructor
//   call site is AIModule::Prepare @0x82798070 stage 3:
//       ResetOnTrackManager::Construct(module+286128, GetAISectionsData(), module+560)
//   In this build AIModule::{Construct,Prepare,Update,PostPhysicsUpdate,Release,Destruct} are
//   ALL quiet boot-gate stubs in WorldLinkStubs.cpp, and the live log says so every run
//   ("AIModule::Prepare: inert", "AIModule::Update: inert"). So today the manager is never
//   constructed, mpAISectionData is null, mpaAICars is garbage, and AIModule::Prepare's stage 2
//   -- AIModule::LoadMapData @0x82795340 (167), which LoadBundle()s "AI.dat" and requests
//   CgsResource::ID::HashString("WorldMapData") type 5 -- never runs, so the AI ROAD NETWORK
//   THE WHOLE SUBSYSTEM QUERIES IS NEVER LOADED. (The DATA is fine: build/game/AI.DAT is present
//   and already ported -- bnd2 platform byte @+8 == 4, 3.27 MB. The hole is entirely code.)
//   ⚠️ [[hollow-shell-classes]] one level up, exactly like the CrashModule lifecycle defect of
//   this same day: BrnAIModule.h models the module as 250 KB of opaque padding with NO named
//   member for the stage machine (+294764), the route-map ready flag (+295896), the manager
//   (+286128), the AI-car array (+560), the player index (+322044) or the resource receiver
//   queue (+73708). Bodying the manager on top of that would be ~4,750 instructions that run
//   against an unconstructed object -- [[valid-pointer-invalid-object]], and no assert can see
//   it. THE MODULE LIFECYCLE + THE AIModuleIO BUFFER LAYOUTS COME FIRST.
//   ⚠️ AIModuleIO::OutputBuffer is a 1-byte PLACEHOLDER on the host today; that is already why
//   BridgeAIToEntityModules_PrePhysics is PARKED, and it is where the ResetOnTrackResult ring
//   has to live. [[silent-drop-stubs]] + the un-gate-a-producer AV class apply in full.
//
// ⛔ THE SHORTCUT IS STILL REFUTED, RE-VERIFIED: ActiveRaceCar::GetResetCoords @0x822BF2D0
//   reads mPrevTransforms, which this tree Constructs, Clear()s and NEVER WRITES
//   (BrnPlaceOnTrackManager.cpp:325 flags it). Do not invent a reset position.
//
// DELETE-WHEN the AI module runs its own lifecycle, AI.dat/WorldMapData loads, and a heavy
// crash recovers. Until then crash ENTRY is disabled on the public path -- see the bring-up
// flag banner in BrnVehicleManager.cpp::SetRaceCarCrashing.
// Reference: scratchpad resetontrack_log.md.
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
// GetPosition @0x822B3588 and GetDirection @0x822B3610 moved to BrnRaceCar.h as inline
// members (render wave 2026-07-31): the X360 inlines both at every call site, and homing
// them in the header lets a consumer link against RaceCar without this whole lifecycle TU.

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

// ============================================================================
// X360 0x822BED20 -- FillInOutputInterface. The per-GLOBAL-car publish: gather this
// car's snapshot and hand it to RCEntityGlobalRaceCarOutputInterface::SetRaceCarData.
// Called from RaceCarEntityModule::UpdateOutputInterfaces step 5 (the 0..34 loop),
// which supplies the speed (active car mph * KF_MPH_TO_MPS when attached, else 0)
// and the AI section (active muCurrAISection when attached, else 0x7FFF).
//
// The console's four muType range asserts (:577/:590/:603 x2) come from its inlined
// type-flag reads; reproduced once up front (the PC getters carry no asserts of
// their own for the flag tests). GetPosition/GetDirection carry their own :  pair.
// [hud H3b tracking slice 2026-08-25 -- this body retires the UpdateOutputInterfaces
// step-5 FLAG; its consumer is the satnav 199 producer in GameBridgeWorldToGui.cpp.]
// ============================================================================
void RaceCar::FillInOutputInterface(
        RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpGlobalCarInterface,
        f32 lfSpeed,
        u16 lu16AISection)
{
    CGS_ASSERT(GetType() < E_RACE_CAR_TYPE_COUNT, "muType < E_RACE_CAR_TYPE_COUNT"); // :603
    CGS_ASSERT(GetType() < E_RACE_CAR_TYPE_COUNT, "muType < E_RACE_CAR_TYPE_COUNT"); // :590
    CGS_ASSERT(GetType() < E_RACE_CAR_TYPE_COUNT, "muType < E_RACE_CAR_TYPE_COUNT"); // :603
    CGS_ASSERT(GetType() < E_RACE_CAR_TYPE_COUNT, "muType < E_RACE_CAR_TYPE_COUNT"); // :577

    // r19: the rival index rides only on AI cars (the console `if (muType == 1)` arm).
    s8 li8RivalIndex = -1;
    if (GetType() == E_RACE_CAR_TYPE_AI)
    {
        li8RivalIndex = GetRivalIndex();
    }

    const Vector3 lDirection = GetDirection();   // v2 (GetDirection @0x822B3610)
    const Vector3 lPosition  = GetPosition();    // v1 (GetPosition @0x822B3588)

    lpGlobalCarInterface->SetRaceCarData(
        lPosition,
        lDirection,
        GetWorldRegion(),                                   // QWORD @+0x7C
        GetRivalId(),
        GetModelId(),
        lfSpeed,                                            // f1 (caller-computed)
        lu16AISection,                                      // r6 (caller-computed)
        GetGlobalRaceCarIndex(),
        li8RivalIndex,
        GetActiveRaceCarIndex(),                            // s8 @+0xAC
        GetType() == E_RACE_CAR_TYPE_PLAYER,                // r28: muType == 0
        GetType() == E_RACE_CAR_TYPE_AI,                    // r21: muType == 1
        GetType() == E_RACE_CAR_TYPE_NETWORK,               // r20: muType == 2
        IsInCurrentGameMode(),                              // r24
        IsDispersing(),                                     // r18: byte @+0xAB
        HasActiveRaceCar());                                // r25 (in-range flag)
}

}
