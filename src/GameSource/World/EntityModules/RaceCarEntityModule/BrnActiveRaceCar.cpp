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
#include "rw/math/vpu/matrix44affine_operation.h"    // rw::math::vpu::Mult

#include <cstring>   // memset (the console's own inlined clears)

namespace BrnWorld
{

// ============================================================================
// Lifecycle (pose wave 2026-07-31): Construct / Prepare / Attach / CalcBodyTransform.
//
// These four are what makes a race car EXIST. The console reaches them as
//   RaceCarEntityModule::Construct       -> ActiveRaceCar::Construct(i)     x8
//   RaceCarEntityModule::Prepare stage 3 -> ActiveRaceCar::Prepare()        x8
//   RaceCarEntityModule::AttachActiveRaceCar @0x822F4DB0
//                                        -> Prepare() then Attach(raceCar, ...)
//   RaceCarEntityModule::ReadUpdatedActiveRaceCarDataFromPhysics
//                                        -> UpdatePhysicsState -> CalcBodyTransform
//
// ⚠️ SCOPE. Construct/Prepare/Attach each write a handful of fields whose TYPES this
// header still keeps opaque (mAddRemoveNetworkCarForCollisionQueue, the two
// VolumeInstanceIds, mCrashData, mPrevTransforms, mDeformedBBox). Those writes are
// reproduced where the storage is a plain byte clear the console itself does with
// stores (mCrashData, mPrevTransforms' three ring-buffer counters) and FLAGGED where
// they need an absent type (the VolumeInstanceId pair). Nothing is paraphrased.
// ============================================================================


namespace
{
    // The identity Matrix44Affine the console builds on the stack from
    // flt_82001C98 (1.0f) / flt_82001CC0 (0.0f) in Construct, Prepare and Attach.
    inline Matrix44Affine MakeIdentityTransform()
    {
        Matrix44Affine lIdentity;
        lIdentity.xAxis = Vector3{ 1.0f, 0.0f, 0.0f, 0.0f };
        lIdentity.yAxis = Vector3{ 0.0f, 1.0f, 0.0f, 0.0f };
        lIdentity.zAxis = Vector3{ 0.0f, 0.0f, 1.0f, 0.0f };
        lIdentity.wAxis = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
        return lIdentity;
    }
}

// ----------------------------------------------------------------------------
// Construct @ 0x822EA9C0. Every store in the console body, in asm order, by name.
//
// The index store is the FIRST instruction after the prologue (`stw r4, 0x748(r31)` at
// 0x822EA9F0, BEFORE either range assert), so it happens even for an out-of-range index.
//
// Construct does NOT call Prepare (Prepare's only two xrefs are AttachActiveRaceCar and
// RaceCarEntityModule::Prepare); the module's Prepare stage 3 sweeps all eight slots
// right after Construct, which is where the rest of the reset comes from.
//
// [FLAG PC bring-up] one console call is not reproduced and not paraphrased: the
// mHandlingBodyVolumeId / mBaseDeformationID pair and
// mAddRemoveNetworkCarForCollisionQueue::Construct, all of which need types this header
// still keeps opaque (CgsSceneManager::VolumeInstanceId and
// CgsModule::EventQueue<VehicleAddedForCollisionEvent,8>).
// ----------------------------------------------------------------------------
void ActiveRaceCar::Construct(EActiveRaceCarIndex leActiveRaceCarIndex)
{
    meActiveRaceCarIndex = leActiveRaceCarIndex;                  // 0x748, before the asserts

    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
               "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    meRaceStartState             = E_RACE_START_STATE_RACING;     // 0x77C = 2
    mfTimeSinceCreation          = 0.0f;                          // 0x728
    mpRaceCar                    = nullptr;                       // 0x6F0
    mfDeferredResetTimer         = 0.0f;                          // 0x720
    muState                      = E_STATE_INACTIVE;              // 0x740
    meOnlineState                = E_ONLINE_STATE_NORMAL;         // 0x744 = 1
    mfTimeToStartLineBoostChange = -1.0f;                         // 0x734
    mbInsideAISectionSystem      = false;                         // 0x771
    mbIsTouchingAnotherRaceCar   = false;                         // 0x772
    mbIsTouchingPlayer           = false;                         // 0x773
    mbIsTouchingWorld            = false;                         // 0x774
    mDeferredResetPosition       = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };  // 0x700
    mDeferredResetDirection      = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };  // 0x710
    muPrevAISection              = 0x7FFF;                        // 0x73C
    muCurrAISection              = 0x7FFF;                        // 0x73E
    mbNotSendingNetworkUpdates      = false;                      // 0x798
    mbIsDisconnectedFromNetwork     = false;                      // 0x799
    mbIsInCarSelectOnline           = false;                      // 0x79A
    mbCarSelectOnlineStateChanged   = false;                      // 0x79B
    mbReceivedNetworkDriverControls = false;                      // 0x79C
    mCurrentInAirRotations       = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };  // 0x750
    mbRenderThisFrame            = true;                          // 0x79D
    mbIsInGameMode               = false;                         // 0x777
    miFlashFrequency             = 0;                             // 0x794
    miBrakeChangeCounter         = 0;                             // 0x738
    mbChangeCollisionState       = false;                         // 0x78D
    mbChangeCullingGroup         = false;                         // 0x78F
    mbWonLastEvent               = false;                         // 0x78C
    mbAddedToScene               = false;                         // 0x78A
    mbIsDoingStartLineBoost      = false;                         // 0x780
    mbIsWaitingForDeferredReset  = false;                         // 0x778
    mbDriveAwayCheckRequired     = true;                          // 0x730

    mPhysicsState.Clear();                                        // 0xE0

    mfInvulnerablityTime         = -1.0f;                         // 0x724
    mCentreOfMassTransform       = MakeIdentityTransform();       // 0x90

    // 0x590: the console stores mpData = this+0x5B0, miMaxLength = 4 and zeroes the three
    // positions -- FixedRingBuffer<Matrix44Affine,4>::Construct(). (The 0x20 gap between
    // the base and the inline array is what pins the base's 16-byte alignment padding.)
    mPrevTransforms.Construct();

    mfIndicatorTime              = 0.0f;                          // 0x1C88
    mfTimeInWater                = 0.0f;                          // 0x784
    mbUncrashedThisFrame         = false;                         // 0x77A
    mfBaseDeformAmount           = 0.0f;                          // 0x7CC
    mbAddedForCollision          = false;                         // 0x78B
    mbAIToBeActivated            = false;                         // 0x781
    mbRightIndicatorActive       = false;                         // 0x1C8C
    mbLeftIndicatorActive        = false;                         // 0x1C8D
    mbIsWrecked                  = false;                         // 0x782
    mbTakenDown                  = false;                         // 0x789
    mbCrashedIntoWater           = false;                         // 0x783
    mbCanDriveAwayFromCrash      = false;                         // 0x779
    mbEnableEngineSwitchOff      = true;                          // 0x770
    meBaseDeformationType        = -1;                            // 0x7C8

    // [FLAG PC bring-up] mAddRemoveNetworkCarForCollisionQueue.Construct() -- see banner.

    mCurrentCullingGroup         = 0xFFFF;                        // 0x7D0
    mRenderParams.Reset();                                        // 0x7E0
    mfTimeSinceLastStable        = 0.0f;                          // 0x760
    mbCurrentlyRotating          = false;                         // 0x764
    mCurrentInAirRotations       = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };  // 0x750 (again, asm order)
}

// ----------------------------------------------------------------------------
// Prepare @ 0x822EAC28. Every store in the console body, in asm order, by name.
// Returns true (`li r3, 1` at 0x822EADCC) -- the module's stage-3 sweep ignores it.
// ----------------------------------------------------------------------------
bool ActiveRaceCar::Prepare()
{
    muPrevAISection      = 0x7FFF;                                 // 0x73C
    muCurrAISection      = 0x7FFF;                                 // 0x73E
    mfTimeSinceCreation  = 0.0f;                                   // 0x728
    mpRaceCar            = nullptr;                                // 0x6F0
    muState              = E_STATE_INACTIVE;                       // 0x740
    meRaceStartState     = E_RACE_START_STATE_RACING;              // 0x77C = 2

    // 0x90: the four identity rows the console assembles on the stack and stores as
    // mCentreOfMassTransform. OnResourcesLoaded is what replaces it with the authored
    // body-to-chassis offset out of the vehicle's physics def.
    mCentreOfMassTransform = MakeIdentityTransform();

    mPrevTransforms.Clear();                                       // 0x598/0x59C/0x5A0
    mbUncrashedThisFrame     = false;                              // 0x77A
    mLastRecordedPosition    = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };  // 0x6B0
    mbAddedForCollision      = false;                              // 0x78B
    mbComingInRange          = false;                              // 0x775
    mbIsInGameMode           = false;                              // 0x777
    mbIsJoiningGameMode      = false;                              // 0x776
    mbAIToBeActivated        = false;                              // 0x781
    mbWonLastEvent           = false;                              // 0x78C

    mRenderParams.Reset();                                         // 0x7E0

    mCurrentInAirRotations   = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };  // 0x750
    mfPlaceOnTrackSpeed      = 0.0f;                               // 0x7C0
    mfTimeDriveableInCrash   = 0.0f;                               // 0x72C
    mbToBePlacedOnTrack      = false;                              // 0x7C4
    mfTimeInWater            = 0.0f;                               // 0x784
    mbIsWrecked              = false;                              // 0x782
    mfEngineStateTime        = 0.0f;                               // 0x76C
    mbCrashedIntoWater       = false;                              // 0x783
    mfTimeSinceLastStable    = 0.0f;                               // 0x760
    mbIsInShowtime           = false;                              // 0x788
    mbTakenDown              = false;                              // 0x789

    // 0x7A0 / 0x7B0: both seeded to (-1, -1, -1, 0) from the shared rodata -1.0f
    // (flt_820037C8), i.e. "no place-on-track request pending".
    mPlaceOnTrackPosition    = Vector3{ -1.0f, -1.0f, -1.0f, 0.0f };
    mPlaceOnTrackDirection   = Vector3{ -1.0f, -1.0f, -1.0f, 0.0f };

    meEngineState            = RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_RUNNING; // 0x768 = 2
    mbEnableEngineSwitchOff  = true;                               // 0x770
    miDefaultColourIndex     = -1;                                 // 0x1C80
    miDefaultColourPalette   = -1;                                 // 0x1C84
    mbCurrentlyRotating      = false;                              // 0x764

    return true;
}

// ----------------------------------------------------------------------------
// Attach @ 0x822BEEE0. Bind lpRaceCar to this slot.
//
// ⭐ THE POSE. The console seeds mPhysicsState.mTransform (this+0x2D0 == mPhysicsState
// +496) straight from RaceCar::GetTransform(). Together with the identity
// mCentreOfMassTransform Prepare left behind, CalcBodyTransform's product is exactly the
// spawn transform -- which is why a car that has never been through the physics module
// still has a correct, authored world pose the moment it is attached.
//
// ⭐ AND THE PART MASK. The two 64-bit -1 stores at this+0x1580/0x1588 are
// mRenderParams.mBodyPartVisibility's two u64 words: Attach makes EVERY body part
// visible. (The render wave's bring-up producer called MakeAllPartsVisible() on a hunch;
// this is the console code that justifies it.)
//
// [FLAG PC bring-up] TWO console steps are not reproduced, both for want of an absent
// type, and neither is paraphrased:
//   * the mHandlingBodyVolumeId seed (`std 0, 0xD0(this)`, set entity type byte 1, then
//     CgsSceneManager::VolumeInstanceId::SetEntityIDEntityIndex(meActiveRaceCarIndex));
//     the id is only consumed by AddToCollision / RemoveFromCollision, neither of which
//     exists here.
//   * mPhysicsState.mHalfExtent = (1.1, 0.75, 1.8) IS reproduced (named member).
// ----------------------------------------------------------------------------
void ActiveRaceCar::Attach(RaceCar* lpRaceCar, bool lbCarSelectDontStreamAudio)
{
    // [FLAG PC bring-up] mHandlingBodyVolumeId seed omitted -- see the banner.

    CGS_ASSERT(!IsAttached(), "!IsAttached()");
    CGS_ASSERT(lpRaceCar != nullptr, "lpRaceCar != NULL");
    CGS_ASSERT(mpRaceCar == nullptr, "mpRaceCar == NULL");

    mpRaceCar = lpRaceCar;                                         // 0x6F0
    muState   = E_STATE_ATTACHED;                                  // 0x740 = 1

    CGS_ASSERT(IsAttached(), "IsAttached()");
    mpRaceCar->AssignActiveRaceCar(this);
    CGS_ASSERT(IsAttached(), "IsAttached()");

    // 0x1BE4 == mRenderParams + 5124 == mbDamaged. The console inlines the three tests of
    // RaceCar::ToBeRenderedDamaged @0x822B3D70 here (mfPersistentDamage > 0 ||
    // IsPlayerDriven() || IsNetworkDriven()), in that order.
    mRenderParams.SetDamaged(mpRaceCar->ToBeRenderedDamaged());

    mPhysicsState.Clear();                                         // 0xE0

    // ⭐ 0x2D0 == mPhysicsState + 496 == RaceCarState::mTransform.
    mPhysicsState.mTransform = mpRaceCar->GetTransform();

    // 0x430 == mPhysicsState + 848 == RaceCarState::mHalfExtent. The console's default
    // car box until the physics module publishes a real one.
    mPhysicsState.mHalfExtent = Vector3{ 1.1f, 0.75f, 1.8f, 0.0f };

    // 0x540: ten 64-bit zero stores over the 80-byte mCrashData.
    memset(&mCrashData, 0, sizeof(mCrashData));

    mfInvulnerablityTime = -1.0f;                                  // 0x724
    muPrevAISection      = 0x7FFF;                                 // 0x73C
    muCurrAISection      = 0x7FFF;                                 // 0x73E

    CGS_ASSERT(mpRaceCar->GetType() < E_RACE_CAR_TYPE_COUNT, "muType < E_RACE_CAR_TYPE_COUNT");
    if (mpRaceCar->GetType() != E_RACE_CAR_TYPE_PLAYER)
    {
        mPrevTransforms.Clear();                                   // 0x598/0x59C/0x5A0
    }

    mCentreOfMassTransform = MakeIdentityTransform();              // 0x90
    meRaceStartState       = E_RACE_START_STATE_RACING;            // 0x77C = 2

    mbUncrashedThisFrame          = false;                         // 0x77A
    mbAddedForCollision           = false;                         // 0x78B
    meOnlineState                 = E_ONLINE_STATE_NORMAL;         // 0x744 = 1
    mbIsDisconnectedFromNetwork   = false;                         // 0x799
    mbIsInCarSelectOnline         = false;                         // 0x79A
    mbCarSelectOnlineStateChanged = false;                         // 0x79B
    mbReceivedNetworkDriverControls = false;                       // 0x79C
    mbNotSendingNetworkUpdates    = false;                         // 0x798
    miFlashFrequency              = 0;                             // 0x794
    mbRenderThisFrame             = true;                          // 0x79D

    // 0x1580 / 0x1588: both mBodyPartVisibility words to all-ones.
    mRenderParams.MakeAllPartsVisible();

    CGS_ASSERT(mpRaceCar->GetType() < E_RACE_CAR_TYPE_COUNT, "muType < E_RACE_CAR_TYPE_COUNT");
    if (mpRaceCar->GetType() != E_RACE_CAR_TYPE_PLAYER
        || !mbEnableEngineSwitchOff
        || (mbIsInGameMode && !lbCarSelectDontStreamAudio))
    {
        meEngineState = RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_RUNNING;   // 0x768 = 2
    }
    else
    {
        meEngineState = RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_OFF;       // 0x768 = 0
    }

    mfEngineStateTime  = 0.0f;                                     // 0x76C
    mbTakenDown        = false;                                    // 0x789
    mfTimeInWater      = 0.0f;                                     // 0x784
    mbIsWrecked        = false;                                    // 0x782
    mbCrashedIntoWater = false;                                    // 0x783
}

// ----------------------------------------------------------------------------
// CalcBodyTransform @ 0x822B8828.
//
// The whole 384-instruction console body is the four RwMath::IsValid dev-assert blocks
// over mPhysicsState.mTransform ("Invalid racecar physics transform for racecar <n>: ")
// plus the final vmaddfp chain, which is the plain affine product of the two matrices
// with mCentreOfMassTransform on the left.
// ----------------------------------------------------------------------------
void ActiveRaceCar::CalcBodyTransform(Matrix44Affine& lrBodyTransform) const
{
    CGS_ASSERT(IsActive(), "IsActive()");
    CGS_ASSERT(rw::math::vpu::IsValid(mPhysicsState.mTransform),
               "Invalid racecar physics transform");

    lrBodyTransform = rw::math::vpu::Mult(mCentreOfMassTransform, mPhysicsState.mTransform);
}

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
