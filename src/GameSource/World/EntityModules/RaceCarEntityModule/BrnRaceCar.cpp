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
// ⭐⭐⭐ READ THIS FIRST -- 2026-08-26 (resetpump wave). EVERY "ABSENT" / "INERT boot gate" /
// "still pins" CLAIM IN THE FOUR PARAGRAPHS BELOW IS HISTORY. THE WHOLE CHAIN IS LANDED AND A
// REQUEST HAS TRAVERSED IT, MEASURED (runs rp_crash2 / rp_crash3, asserts=0, no AV):
//     [resetpump] request SENT: global car 0 type 1        <- RCEM::SendResetOnTrackRequests
//     [resetpump] request RECEIVED by the AI module        <- the two AI bridges
//     [rot] request resolved: ... -> FAILURE (consumer uses GetResetCoords)
//     [resetpump] RESULT applied: ... -> (3003.20, 2.51, -1653.75)
//     [teleport] ResetActiveRaceCar RE-RESET car 0 -> road (...)   <- the car IS put back
// and the car then drove about a kilometre. The FAILURE arm is the console's designed fallback,
// not a hole: it is what routes the reset through THIS car's own GetResetCoords.
// ⭐ AND THE ONE RUNG THAT STILL FAILED WHEN THAT WAS WRITTEN IS NOW CLOSED TOO (crashclear
// wave, 2026-08-26): the crash STATE is cleared. The dispatch it referred to -- the
// `!mbResetTransform` arm of VehicleManager::ProcessResetEvents @0x82617E00 -- is
// VehiclePhysics::ClearCrashing @0x825D5450 (RaceCarPhysics vtable slot 1, probed off the
// image), and it is landed and called by name. mbCrashing goes to 0 on the recovery and its
// falling edge becomes GUI 377 LEAVE_CRASHED. See (P3) in
// BrnVehicleManager_WriteOutVehicleStats.cpp for the working-out.
// The paragraphs below are kept because their SHAPE of the chain, their instruction counts and
// their working-out are still the best map of it; only their status verbs have expired.
//
// ⛔⛔ [HISTORY, 2026-08-25] NOTHING IN THIS TREE READS mbToBeResetOnTrack. MEASURED, and the
// number the crash waves have been carrying ("~25 functions, ~2500 instructions") IS TOO SMALL BY
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
// ⭐⭐⭐ THE PARAGRAPH BELOW IS THE 2026-08-25 MEASUREMENT AND IT IS NOW HISTORY -- READ THIS
//   FIRST. On 2026-08-26 (aimodule slice 1) the AI module lifecycle LANDED:
//   AIModule::{Construct, Prepare, LoadMapData} are real bodies in
//   GameSource/World/AI/BrnAIModule.cpp, AI.dat loads, "WorldMapData" resolves and
//   BrnAI::ResetOnTrackManager IS Constructed against a bound road network -- measured on the
//   boot log, with the control that could falsify it (AISectionsData::muVersion reads 12, the
//   value KU_AI_SECTIONS_DATA_VERSION names, over 7639 sections and 3273824 B, which no
//   garbage pointer produces). So "the AI module does not run at all" is FALSE from that date.
//   ⛔ mbToBeResetOnTrack IS STILL READ BY NOBODY, and a heavy crash still pins: what remains
//   is the REQUEST/RESULT PUMP above the lifecycle --
//     SendResetOnTrackRequests @0x822CE178 (57)         [absent]
//     the 35-entry AI-car array AIModule::Construct parks [⭐ LANDED 2026-08-26, aicar_reset]
//     AIModule::Update / UpdateResetOnTrackManager       [still boot gates]
//     ResetOnTrackManager::Update + 32 siblings          [⭐ Update / ProcessResetOnTrackRequest
//         / ComputeResetOnTrack / ComputeInitialCoordinatesStandard LANDED 2026-08-26; the
//         remaining 28 are the geometry, parked at their own sites]
//     ProcessResetOnTrackResultQueue @0x822F4580 (192)   [absent]
//   Everything the paragraph below says about the SHAPE of the chain still holds; only its
//   claim about WHERE the break is has moved one rung up.
//
// ⭐⭐⭐ UPDATE 2026-08-26 (aicar_reset wave) -- AND ONE OF THE THINGS THIS FILE HAS BEEN
//   REPEATING SINCE 2026-08-25 IS WRONG. See the SHORTCUT paragraph further down: it says
//   GetResetCoords "would place the car at the origin". IT WOULD NOT. The asm has a second arm
//   (0x822BF37C) for an EMPTY ring that hands out mPhysicsState.mTransform's {wAxis, zAxis} --
//   the car's LIVE pose. MEASURED on a booted drive run: the ring is empty (depth 0) and
//   GetResetCoords returns the player's own moving position, tracking it down the road.
//   ⇒ the reset-on-track chain does NOT need the AI road network to produce a USABLE pose; it
//   needs the pump to run so the FAILURE result reaches ProcessResetOnTrackResultQueue's
//   GetResetCoords arm.
//   ⛔ THE PUMP'S REMAINING BLOCKERS, MEASURED THIS WAVE (not inferred):
//     * VehicleManager::GenerateAboveGroundLineTests @0x82633990 is ABSENT, so
//       RaceCarState::mAboveGroundTestResult.mbValid is FALSE every frame
//       ([collision-tag] aboveGroundValid=0 on every sample) -- which is why the newly landed
//       UpdateRaceCarCollisionTagging never sets an AI section and the reset ring stays empty.
//     * RaceCarEntityModule::WriteUpdatedAIData @0x822D1FC8 is ABSENT, so
//       AIModuleIO::RaceCarAIInterface::mbPlayerDataSet is never set -- and AIModule::Update
//       @0x8279B478 skips its ENTIRE body on `if (GetRaceCarAIInterface()->mbPlayerDataSet)`.
//       Landing AIModule::Update without it would be a body that provably never runs.
//
// ⭐⭐ AND THE MANAGER IS NOT THE BLOCKER -- THE AI MODULE IS, BECAUSE IT DOES NOT RUN AT ALL.
//   [SUPERSEDED 2026-08-26 -- see the block immediately above.]
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
//   ⚠️⚠️ RETRACTED 2026-08-26 (aicar_reset wave) -- HALF OF THIS IS FALSE AND IT COST FOUR WAVES.
//   The first clause was true until this wave (mPrevTransforms is now WRITTEN, by
//   ActiveRaceCar::UpdateResetTransform at RCEM::UpdateActiveRaceCarTransforms' console slot).
//   The implied second clause -- that an unwritten ring makes GetResetCoords useless -- was
//   NEVER true: the function has an explicit empty-ring arm at 0x822BF37C that returns
//   mPhysicsState.mTransform's {wAxis, zAxis}. Using it is NOT inventing a reset position; it is
//   calling the console's own function and getting the console's own answer.
//   ⭐ THE LESSON, AND IT IS THE CAMPAIGN'S OWN: "X reads Y and Y is never written" is a claim
//   about ONE BRANCH of X. Read the other branch before writing it down as a refutation --
//   [[check your witness observes the right branch]].
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

// ============================================================================
// THE ROAD-RAGE RANGE PREDICATES (rival range-loop wave, lane W1, 2026-09-05)
//
//   RaceCar::AreCarsHeadOn      X360 0x822BEBE8   (BrnRaceCar.h:176, private)
//   RaceCar::ShouldBeInRange    X360 0x822D3B00   (BrnRaceCar.h:162)
//   RaceCar::ShouldBeOutOfRange X360 0x822D3C48   (BrnRaceCar.h:163)
//
// The two Should* predicates are the whole in/out hysteresis of
// RaceCarEntityModule::UpdateInAndOutOfRangeCars @0x822FF8F8 (its only caller): a rival goes
// OUT at the ShouldBeOutOfRange radius and can only come back at the strictly smaller
// ShouldBeInRange radius, so a car sitting on the boundary does not oscillate.
//
// AreCarsHeadOn was DECLARATION-ONLY in this tree (no body anywhere), so both predicates
// would have been an LNK2019 without it; it is bodied here, in its declaring TU.
//
// SIGNATURE FROM THE ASM, NOT THE PSEUDOCODE. Hex-Rays renders all three as `int (int, int)`
// and hides the sret convention: `RaceCar::GetPosition()` returns a Vector3 BY VALUE, so the
// console passes the destination buffer in r3 and the RECEIVER in r4
// (`addi r3, r1, var_50 ; bl GetPosition` with r4 still holding the *other* car). That is why
// the first GetPosition in each body is the OTHER car's, not this one's.
//
// ONE CONSOLE COMPUTATION IN ShouldBeInRange IS DEAD, AND IT IS REPRODUCED AS SUCH.
// 0x822D3B90..0x822D3BB0 calls GetDirection on the player car, dot-products it against the
// separation and extracts the vcmpgefp. bit into r11 -- and then 0x822D3BB8 `cmplwi cr6, r11, 0`
// is immediately overwritten by 0x822D3BC4 `fcmpu cr6, f13, f0`, so the bit never reaches a
// branch. The sibling ShouldBeOutOfRange @0x822D3D04 DOES branch on the same bit, to two
// DIFFERENT thresholds (40000 in front / 52900 behind). The compiler merged
// ShouldBeInRange's two arms because BOTH of its thresholds are the SAME pooled literal
// flt_8201C214 == 32400.0f -- i.e. the source has the same `in front ? A : B` shape with
// A == B. Reproduced here as that branch with the same constant on both sides rather than
// dropped, because GetDirection is a real call carrying real asserts.
//
// EVERY CONSTANT READ OUT OF THE DECRYPTED ARTIST IMAGE AT ITS `lfs` SYMBOL:
//   flt_82001CC0 = 0.0        flt_8201495C = 700.0     flt_8201C210 = 350.0
//   flt_8201C214 = 32400.0    flt_82014968 = 750.0     flt_8201C218 = 375.0
//   flt_8201C21C = 52900.0    flt_8201C220 = 40000.0
// The three squared values are exactly 180^2 / 200^2 / 230^2, and 700/750/350/375 are the
// UNsquared head-on radii the console squares at run time (`fmuls f0, f31, f31`).
// ============================================================================

// ---- ShouldBeInRange (0x822D3B00) -----------------------------------------------------
// flt_8201C214. Both arms of the in-game-mode test; 180 m squared.
static const f32 KF_IN_RANGE_DISTANCE_SQUARED = 32400.0f;
// flt_8201495C / flt_8201C210. The free-burn head-on / not-head-on radii, squared at run time.
static const f32 KF_IN_RANGE_HEAD_ON_DISTANCE = 700.0f;
static const f32 KF_IN_RANGE_DISTANCE         = 350.0f;

// ---- ShouldBeOutOfRange (0x822D3C48) --------------------------------------------------
// flt_8201C220 / flt_8201C21C. 200 m squared ahead of the player, 230 m squared behind.
static const f32 KF_OUT_OF_RANGE_IN_FRONT_DISTANCE_SQUARED = 40000.0f;
static const f32 KF_OUT_OF_RANGE_BEHIND_DISTANCE_SQUARED   = 52900.0f;
// flt_82014968 / flt_8201C218. The free-burn head-on / not-head-on radii.
static const f32 KF_OUT_OF_RANGE_HEAD_ON_DISTANCE = 750.0f;
static const f32 KF_OUT_OF_RANGE_DISTANCE         = 375.0f;

// ----------------------------------------------------------------------------
// AreCarsHeadOn @ 0x822BEBE8. True when car 1 is AHEAD of car 0 along car 0's own heading
// AND the two headings oppose. Both tests are strict (`vcmpgtfp.`), both against 0.0f
// (flt_82001CC0, splatted out of a zeroed stack Vector3), and the first failure returns
// false without ever fetching car 1's direction.
//   0x822BEC64  vsubfp128 v13, v126, v127     lPosition1 - lPosition0
//   0x822BEC78  vmsum3fp128 v13, v13, v12     dotted with GetDirection(car0)
//   0x822BEC7C  vcmpgtfp. v13 > 0
//   0x822BECCC  vmsum3fp128 v13, v13, v127    GetDirection(car1) . GetDirection(car0)
//   0x822BECDC  vcmpgtfp. 0 > v13
// ----------------------------------------------------------------------------
bool RaceCar::AreCarsHeadOn(const RaceCar* lpRaceCar0, const RaceCar* lpRaceCar1) const
{
    const Vector3 lPosition0  = lpRaceCar0->GetPosition();
    const Vector3 lPosition1  = lpRaceCar1->GetPosition();
    const Vector3 lDirection0 = lpRaceCar0->GetDirection();

    if (rw::math::vpu::Dot(lPosition1 - lPosition0, lDirection0) <= 0.0f)
    {
        return false;   // car 1 is not ahead of car 0
    }

    const Vector3 lDirection1 = lpRaceCar1->GetDirection();

    return rw::math::vpu::Dot(lDirection1, lDirection0) < 0.0f;
}

// ----------------------------------------------------------------------------
// ShouldBeInRange @ 0x822D3B00. Whether this out-of-range rival is close enough to the player
// car to be brought back into simulation.
//   0x822D3B48  lbz r11, 0xA6(r31)   -- mbIsInGameMode picks the arm
//   in a game mode : distance^2 < 32400 (180 m), whichever side of the player the car is on
//   free burn      : distance^2 < (head-on ? 700 : 350)^2
// ----------------------------------------------------------------------------
bool RaceCar::ShouldBeInRange(const RaceCar* lpPlayerRaceCar) const
{
    // v127 = this position - player position (0x822D3B4C `vsubfp128 v127, v0, v127`).
    const Vector3 lSeparation      = GetPosition() - lpPlayerRaceCar->GetPosition();
    const f32     lfDistanceSquared = rw::math::vpu::Dot(lSeparation, lSeparation);

    if (mbIsInGameMode)
    {
        // 0x822D3B90..0x822D3BB0. The console computes this and DISCARDS it -- see the
        // banner. Kept because the call is the console's; the branch it feeds was merged
        // away by the compiler because both arms compare against the same pooled literal.
        const bool lbInFrontOfPlayer =
            rw::math::vpu::Dot(lpPlayerRaceCar->GetDirection(), lSeparation) >= 0.0f;

        return lbInFrontOfPlayer ? (lfDistanceSquared < KF_IN_RANGE_DISTANCE_SQUARED)
                                 : (lfDistanceSquared < KF_IN_RANGE_DISTANCE_SQUARED);
    }

    const f32 lfRange = AreCarsHeadOn(this, lpPlayerRaceCar) ? KF_IN_RANGE_HEAD_ON_DISTANCE
                                                            : KF_IN_RANGE_DISTANCE;

    return lfDistanceSquared < (lfRange * lfRange);
}

// ----------------------------------------------------------------------------
// ShouldBeOutOfRange @ 0x822D3C48. Whether this in-range rival has drifted far enough from the
// player car to be dropped out of simulation. Same two arms, but the in-game-mode arm DOES
// branch on which side of the player the car is: a car AHEAD survives to 200 m, a car BEHIND
// to 230 m (the player is driving forwards, so the car behind falls away faster).
//   0x822D3CF0  vcmpgefp. dot(playerDir, separation) >= 0
//   0x822D3D08  in front -> 40000    0x822D3D24  behind -> 52900
// ----------------------------------------------------------------------------
bool RaceCar::ShouldBeOutOfRange(const RaceCar* lpPlayerRaceCar) const
{
    const Vector3 lSeparation      = GetPosition() - lpPlayerRaceCar->GetPosition();
    const f32     lfDistanceSquared = rw::math::vpu::Dot(lSeparation, lSeparation);

    if (mbIsInGameMode)
    {
        const bool lbInFrontOfPlayer =
            rw::math::vpu::Dot(lpPlayerRaceCar->GetDirection(), lSeparation) >= 0.0f;

        return lbInFrontOfPlayer
            ? (lfDistanceSquared > KF_OUT_OF_RANGE_IN_FRONT_DISTANCE_SQUARED)
            : (lfDistanceSquared > KF_OUT_OF_RANGE_BEHIND_DISTANCE_SQUARED);
    }

    const f32 lfRange = AreCarsHeadOn(this, lpPlayerRaceCar) ? KF_OUT_OF_RANGE_HEAD_ON_DISTANCE
                                                             : KF_OUT_OF_RANGE_DISTANCE;

    return lfDistanceSquared > (lfRange * lfRange);
}

}
