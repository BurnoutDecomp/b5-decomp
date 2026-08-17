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
#include "GameSource/Math/BrnMathUtils.h"                // BrnMath::IsNormal
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h" // VehicleInputInterface::CreateRaceCar
#include "GameSource/World/BrnEntityTypes.h"              // BrnWorld::E_ENTITYTYPE_RACECAR (the Attach seed)
#include "SharedClasses/World/BrnCollisionTag.h"          // BrnWorld::KU_COLLISION_FLAG_FATAL (IsWrecked)
#include "GameShared/GameClasses/Development/Log/CgsLog.h" // gpDebugPrint / gxMessageFilterFlags ([engine-diag])
#include "GameShared/GameClasses/System/Timer/CgsFrameInterpolation.h" // ⚠️ FLAG PC QoL: BlendTransform (the render-pose interpolator)

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

    // ⚠️ FLAG PC quality-of-life: the render-pose interpolator starts with no history, so
    // the first frame after this slot is built draws the tick pose straight rather than
    // blending it against uninitialised storage. (This module's array is not zero-filled --
    // DebugMemoryInit stamps module memory with 0x7FFFFFFF.)
    mBodyPoseTrack.Reset();
    for (u32 luWheel = 0; luWheel < KU_INTERP_WHEELS; ++luWheel)
        maWheelPoseTracks[luWheel].Reset();

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
    // 0x7C8: the console stores -1 here. ⚠️ BrnPhysics::Deformation::DeformationResetType
    // (BrnDeformationEvents.h:17) currently carries only E_DEFORMATION_RESET_NONE = 0, so the
    // sentinel has no recovered enumerator name yet -- the cast records that gap rather than
    // hiding it behind an s32 member.
    meBaseDeformationType        = static_cast<BrnPhysics::Deformation::DeformationResetType>( -1 );

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
    // ⭐⭐ RESTORED 2026-08-11 (create-path wave). This was the "[FLAG PC bring-up]
    // mHandlingBodyVolumeId seed omitted" hole in the banner above, and it is NOT optional any
    // more: AddHandlingModel @0x822D3EC8 publishes this exact id into the create event, and
    // VehicleManager::ProcessCreateEvents @0x82616770 takes BOTH the owner test
    // (`srwi r10,..,24 ; cmplwi r10,1` -> assert "lEvent.mVolumeInstanceID.GetEntityIDOwner() ==
    // BrnWorld::E_ENTITYTYPE_RACECAR", BrnVehicleManager.cpp:1303) AND the race-car SLOT INDEX
    // (`extrwi r27,r9,14,8`) out of it. MEASURED before this line existed: the create event
    // reached the drain with owner=0 index=0, i.e. an unpopulated id.
    //
    // Verbatim from the X360 Attach @0x822BEEE0 -- the three steps, in order:
    //   0x822BEF04  std  r30, 0xD0(r31)                    mHandlingBodyVolumeId = 0
    //   0x822BEF08..0x822BEF28                             SetEntityIDOwner(1) inlined
    //               (ld ; srdi 32 ; clrlwi r11,r11,8 ; oris r11,r11,0x100 ; sldi 32 ; or ; std)
    //   0x822BEF2C  lwz  r4, 0x748(r31)                    meActiveRaceCarIndex
    //   0x822BEF30  bl   VolumeInstanceId::SetEntityIDEntityIndex
    // The inlined middle step clears the top byte of the entity word and ORs in 0x01000000 ==
    // E_ENTITYTYPE_RACECAR (BrnEntityTypes.h:34) at KU_OWNER_BASE; it is spelled here through the
    // container's own out-of-line setter (@0x822B0E00), which reproduces that expression exactly.
    mHandlingBodyVolumeId.muId = 0;
    mHandlingBodyVolumeId.SetEntityIDOwner(static_cast<u8>(BrnWorld::E_ENTITYTYPE_RACECAR));
    mHandlingBodyVolumeId.SetEntityIDEntityIndex(static_cast<u32>(meActiveRaceCarIndex));

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

// ============================================================================
// ⚠️ FLAG PC QUALITY-OF-LIFE -- NOT X360 FUNCTIONS. See the banner on the declarations
// in BrnActiveRaceCar.h for why these exist and what they deliberately do not touch.
// ============================================================================
// Each of the three is the same three-line shape over one PoseTrack per interpolated
// transform -- the body, and each of the six WORLD wheel transforms. The ordering rules
// (restore before the producers, latch after them, apply per rendered frame) and the
// reason the restore is mandatory live on PoseTrack itself; the module drives the pairing
// (RaceCarEntityModule::PostPhysicsUpdate brackets its producers with the first two).
void ActiveRaceCar::RestoreTickRenderPose()
{
    mBodyPoseTrack.Restore(mRenderParams.GetBodyTransformForWrite());
    for (u32 luWheel = 0; luWheel < KU_INTERP_WHEELS; ++luWheel)
        maWheelPoseTracks[luWheel].Restore(mRenderParams.GetWheelTransform(luWheel));
}

void ActiveRaceCar::LatchTickRenderPose()
{
    mBodyPoseTrack.Latch(mRenderParams.GetBodyTransform());
    for (u32 luWheel = 0; luWheel < KU_INTERP_WHEELS; ++luWheel)
        maWheelPoseTracks[luWheel].Latch(mRenderParams.GetWheelTransform(luWheel));
}

void ActiveRaceCar::ApplyRenderPoseInterpolation(f32 lfAlpha)
{
    mBodyPoseTrack.Apply(mRenderParams.GetBodyTransformForWrite(), lfAlpha);
    for (u32 luWheel = 0; luWheel < KU_INTERP_WHEELS; ++luWheel)
        maWheelPoseTracks[luWheel].Apply(mRenderParams.GetWheelTransform(luWheel), lfAlpha);
}

// ----------------------------------------------------------------------------
// ⭐⭐ UpdatePhysicsState @ 0x822D4418 -- THE PHYSICS -> RENDER SEAM (physics wave 1).
//
// One frame's published BrnPhysics::Vehicle::RaceCarState becomes this car's pose. Decoded
// instruction by instruction from the X360 asm (0x822D4418..0x822D48F4); the pseudocode was
// not used for anything (it renders the third argument as a bare `int a3` and an earlier
// scoping pass mistook that for a timestep -- it is the world map, see the header).
//
//   0x822D4430  IsActive()                                       assert :0x733 == :1843
//   0x822D4464  lpState != NULL                                  assert :0x734 == :1844
//   0x822D4488  lwz r11,0x6F0(r29) -> mpRaceCar != NULL          assert :0x735 == :1845
//   0x822D44B0  addi r27, r23, 0x1F0                             r27 = &lpState->mTransform
//   0x822D44B8..0x822D4700  four vcmpeqfp. NaN sweeps over the four rows of that matrix;
//               on failure the console composes "Bad racecar matrix coming from physics."
//               + the car's name + ", transform: " + the matrix into the assert buffer and
//               fires :0x736 == :1846. Reproduced as ONE CGS_ASSERT over the whole affine
//               (rw::math::vpu::IsValid is exactly that four-row finite sweep); the string
//               composition is diagnostic-only and is not rebuilt by hand.
//   0x822D4704  li r5,0x460 ; mr r4,r23 ; addi r3,r29,0xE0 ; bl XMemCpy
//                                                            ⭐ mPhysicsState = *lpState
//   0x822D4718  IsAttached()                                     assert (BrnActiveRaceCar.h:0x441)
//   0x822D475C  RaceCar::UpdatePositioningData(mpRaceCar, &lpState->mTransform, lpWorldMap)
//   0x822D4764  IsAttached()                                     assert (same site)
//   0x822D4798  RaceCar::UpdateVelocity(mpRaceCar, lpState->mLinearVelocity)  (lvx128 @+0x330)
//   0x822D47A4  CalcBodyTransform(local) ; 4 x stvx128 into this+0x7E0
//                                                            ⭐ mRenderParams.SetBodyTransform
//   0x822D47E8..0x822D485C  the four-wheel loop: copy lpState->maWheelTransforms[i]
//               (state+0x230+64i) into mRenderParams.mWheelTransforms[i] (this+0x1020+64i)
//               and lpState->mabWheelExists[i] (state+0x446+i) into
//               mRenderParams.mabWheelExists[i] (this+0x1560+i). The bound assert the loop
//               carries is GetWheelTransform's own (< 6, the RENDER array's width) and it is
//               reproduced by calling that accessor; the loop itself runs 0..3, the width of
//               the STATE array.
//   0x822D4860  mRenderParams.mbCrashing (this+0x1BE5) = lpState->mbCrashing (state+0x44A)
//   0x822D4870  mRenderParams.mbIsHidden (this+0x1BEB) = lpState->mbIsHidden (state+0x452)
//   0x822D4864..0x822D48F4  the three-way brake/reverse/engine-off tail on meEngineState
//               (this+0x768) and lpState->mi8Gear (state+0x444) -- see the code below.
//
// ⭐ FOURTH INDEPENDENT CONFIRMATION OF THE RaceCarState "+4" FIX. Every state offset this
// body touches -- 0x444 mi8Gear, 0x446 mabWheelExists[0], 0x44A mbCrashing, 0x452 mbIsHidden,
// 0x40C mfBrake -- lands on those members ONLY with the 8-byte mCarAssetAttribKey committed
// this wave (BrnVehicleEvents.h's banner). Under the old 4-byte model every one of them was
// off by one member.
//
// ⛔ NO CALLER YET, and that is the honest state of this slice: the console's only caller is
// RaceCarEntityModule::ReadUpdatedActiveRaceCarDataFromPhysics @0x822E87B8, which needs
// VehicleOutputInterface::maRaceCarStates to be populated -- i.e. the vehicle manager's
// ProcessCreateEvents/WriteOutVehicleStats legs, which are not landed. Until then
// PublishRenderPoseWithoutPhysicsBringUp still owns the render pose. This function is the
// consumer that replaces it, written against the real payload, and it is deliberately NOT
// wired to a fabricated producer.
// ----------------------------------------------------------------------------
void ActiveRaceCar::UpdatePhysicsState(const BrnPhysics::Vehicle::RaceCarState* lpState,
                                       CgsWorld::WorldMap2D* lpWorldMap)
{
    CGS_ASSERT(IsActive(), "IsActive()");                        // :1843
    CGS_ASSERT(lpState != 0, "lpState != NULL");                 // :1844
    CGS_ASSERT(mpRaceCar != 0, "mpRaceCar != NULL");             // :1845

    // The console's four-row NaN sweep over lpState->mTransform, then
    // "Bad racecar matrix coming from physics." + the car name + ", transform: " + the matrix.
    CGS_ASSERT(rw::math::vpu::IsValid(lpState->mTransform),
               "Bad racecar matrix coming from physics.");       // :1846

    // ⭐ 0xE0 == mPhysicsState. XMemCpy of the whole 1120-byte snapshot.
    mPhysicsState = *lpState;

    CGS_ASSERT(IsAttached(), "IsAttached()");
    mpRaceCar->UpdatePositioningData(lpState->mTransform, lpWorldMap);

    CGS_ASSERT(IsAttached(), "IsAttached()");
    mpRaceCar->UpdateVelocity(lpState->mLinearVelocity);

    // ⭐ 0x7E0 == mRenderParams.mBodyTransform.
    Matrix44Affine lBodyTransform;
    CalcBodyTransform(lBodyTransform);
    mRenderParams.SetBodyTransform(lBodyTransform);

    // The four road wheels: render transform + existence flag.
    for (u32 luWheel = 0; luWheel < 4u; ++luWheel)
    {
        mRenderParams.GetWheelTransform(luWheel) = lpState->maWheelTransforms[luWheel];
        mRenderParams.SetWheelExists(luWheel, lpState->mabWheelExists[luWheel]);
    }

    mRenderParams.SetCrashing(lpState->mbCrashing);              // this+0x1BE5
    mRenderParams.SetRaceCarHidden(lpState->mbIsHidden);         // this+0x1BEB

    // The brake / reverse / engine-off tail. `flt_82001CC0` is this file's own 0.0f
    // (the same constant MakeIdentityTransform's off-diagonal uses).
    if (meEngineState == RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_RUNNING
        || meEngineState == RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_STARTING)
    {
        if (lpState->mi8Gear != 0)
        {
            mRenderParams.SetReversing(false);                   // this+0x1BE8 = 0
            SetBraking(lpState->mfBrake > 0.0f);                 // this+0x1BE7 via the AI hysteresis
            mRenderParams.SetEngineOff(false);                   // this+0x1BE6 = 0
        }
        else
        {
            // Gear ordinal 0 is REVERSE (it is also the index the console uses into the
            // six-entry gear-ratio table, and UpdateRaceCarState reads mafGearRatios[0] first).
            mRenderParams.SetReversing(true);                    // this+0x1BE8 = 1
            mRenderParams.SetBraking(false);                     // this+0x1BE7 = 0 (direct, no hysteresis)
            mRenderParams.SetEngineOff(false);                   // this+0x1BE6 = 0
        }
    }
    else
    {
        SetBraking(false);
        mRenderParams.SetReversing(false);                       // this+0x1BE8 = 0
        mRenderParams.SetBraking(false);                         // this+0x1BE7 = 0 (the asm re-stores it)
        mRenderParams.SetEngineOff(true);                        // this+0x1BE6 = 1
    }
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

// ============================================================================
// IsWrecked @ 0x822BFDA0   (player-input wave 2026-08-11)
//
// LANDED as an absent callee of RaceCarEntityModule::ProcessPlayerVehicleInput @0x822FFE30,
// which uses `IsCrashing() && IsWrecked()` to decide whether to zero the player's driver
// controls (and, in Showtime, slam brake + handbrake to 1.0f) for the frame.
//
// The console body is a five-stage early-out ladder; every offset it touches maps to a NAMED
// member of this class or of mPhysicsState (RaceCarState @+224):
//   0x822BFDB4  lbz  0x782(this)          -> mbIsWrecked                       -> return true
//   0x822BFDDC  IsAttached() assert       -> BrnActiveRaceCar.h:1089
//   0x822BFE10  RaceCar::IsPlayerDriven(mpRaceCar)                             -> !player => true
//   0x822BFE24  IsAttached()              -> not attached => false  (NOT an assert; a test)
//   0x822BFE3C  lbz  0x44C(state)         -> RaceCarState +0x44C               -> mbIsDriveable
//                                            == false => true
//   0x822BFE50  lbz  0x1E8(state)         -> 488-448 == AboveGroundTestResult +40 -> mbValid
//                                            == false => false
//   0x822BFE60  lfs  0x4E4(this)          -> 1252-224 == RaceCarState +1028    -> mfTimeInAir
//                                            > 0.0f => false
//   0x822BFE88  IsCrashing()              -> false => false
//   0x822BFEA0  lwz  0x1E4(state) ; the low halfword ; >>14 &1
//                                         -> 484-448 == AboveGroundTestResult +36 ->
//                                            mCollisionTag, bit 14 == KU_COLLISION_FLAG_FATAL
//                                            (16384). Set => true, clear => false.
// i.e. "a player car counts as wrecked once it is undriveable, or once it is crashing on the
// ground against a FATAL surface".
//
// ⚠️ TWO NOTES FOR THE VERIFIER.
//  1. The `IsAttached()` at 0x822BFDDC is the ASSERT (BrnActiveRaceCar.h:1089, non-fatal, falls
//     through) and the one at 0x822BFE24 is a REAL test whose false arm returns false. They are
//     two different call sites and the pseudocode renders them identically; both are reproduced.
//  3. ⚠️ THREE CITATIONS CORRECTED 2026-08-11 (consolidation wave): the assert-arm IsAttached was
//     cited @0x822BFDC8 and IsPlayerDriven @0x822BFE00 -- both off by one call-setup block; the
//     real sites are 0x822BFDDC and 0x822BFE10. And the mbIsDriveable line read
//     "`lbz 0x44C(state)` -> 1100-224 == RaceCarState +0x44C", which does not compute
//     (1100 - 224 == 876, not 1100): 0x44C is ALREADY relative to GetPhysicsState()'s return, so
//     there is no +224 class offset to take back off it and the "-224" was spurious. (The two
//     neighbouring lines that DO subtract are correct and untouched: +0x4E4 is `this`-relative,
//     hence 1252-224 == 1028, and +0x1E8/+0x1E4 subtract the 448-byte AboveGroundTestResult seat
//     inside RaceCarState.)
//  2. mAboveGroundTestResult.mCollisionTag is the tree's `::CollisionTag { u32 muValue; }`
//     storage word, not BrnWorld::CollisionTag, so the fatal bit is tested against the named
//     BrnWorld::KU_COLLISION_FLAG_FATAL constant (== 16384 == bit 14 of the packed word's
//     material half, which is what the console's `lhz +2 ; srwi 14` extracts on big-endian).
// ============================================================================
bool ActiveRaceCar::IsWrecked() const
{
    if( mbIsWrecked )                                                // +0x782
    {
        return true;
    }

    CGS_ASSERT(IsAttached(), "IsAttached()");                        // BrnActiveRaceCar.h:1089

    if( !GetGlobalRaceCar()->IsPlayerDriven() )
    {
        return true;
    }

    if( !IsAttached() )
    {
        return false;
    }

    if( !GetPhysicsState()->mbIsDriveable )
    {
        return true;
    }

    if( !GetPhysicsState()->mAboveGroundTestResult.mbValid )
    {
        return false;
    }

    if( GetPhysicsState()->mfTimeInAir > 0.0f )
    {
        return false;
    }

    if( !IsCrashing() )
    {
        return false;
    }

    return ( GetPhysicsState()->mAboveGroundTestResult.mCollisionTag.muValue
             & BrnWorld::KU_COLLISION_FLAG_FATAL ) != 0;
}

// ============================================================================
// RequestPlaceOnTrack @ 0x822BFB58   (drivable wave 2026-08-01)
//
// The only way a car asks to be dropped onto the road surface. Two producers on this
// build: RaceCarEntityModule::PlaceRaceCarOnLoad @0x822CE588 (the moment a car's
// resources finish streaming, which is the start-of-game path) and
// HandleResetPlayerCarAction's TELEPORT arm.
//
// ⭐ ARGUMENTS RECOVERED FROM THE ASM, not the pseudocode -- incident TEN of the
// dropped-argument rule and the SECOND time it is the vector registers:
//   v1  = lPosition      (`vmr128 v126, v1`, stored to +0x7A0)
//   v2  = lDirection     (`vmr128 v127, v2`, stored to +0x7B0)
//   fp1 = lfSpeed        (stored to +0x7C0)
// Hex-Rays renders the whole thing as `RequestPlaceOnTrack(int a1, double a2)`.
//
// Note the LATCH: every store sits inside `if (!mbToBePlacedOnTrack)`, so a second
// request arriving while one is still pending is IGNORED -- that is console behaviour,
// and it is why PlaceOnTrackManager::PrePhysicsUpdate must Clear before it resets.
// ============================================================================
void ActiveRaceCar::RequestPlaceOnTrack( const Vector3& lPosition, const Vector3& lDirection,
                                         f32 lfSpeed )
{
    CGS_ASSERT(IsAttached(), "IsAttached()");                        // BrnActiveRaceCar.cpp:1801
    CGS_ASSERT(rw::math::vpu::IsValid(lDirection),
               "RwMath::IsValid( lDirection )");                     // :1802
    CGS_ASSERT(BrnMath::IsNormal(lDirection),
               "BrnMath::IsNormal( lDirection )");                   // :1803

    if( !mbToBePlacedOnTrack )
    {
        mfPlaceOnTrackSpeed    = lfSpeed;                            // +0x7C0
        mPlaceOnTrackPosition  = lPosition;                          // +0x7A0
        mPlaceOnTrackDirection = lDirection;                         // +0x7B0
        mbToBePlacedOnTrack    = true;                               // +0x7C4

        CGS_ASSERT(mfPlaceOnTrackSpeed >= 0.0f, "mfPlaceOnTrackSpeed >= 0.0f");   // :1813
    }
}

// ============================================================================
// OnResourcesLoaded @ 0x822EB168   (drivable wave 2026-08-01)  -- ATTACHED -> WAITING.
//
// This is the step the old PromoteAttachedCarToActiveBringUp stood in for, half of it.
// Its FIRST store is the state transition, and that is the load-bearing part: nothing
// else in the XEX writes E_STATE_WAITING, and ResetActiveRaceCar's promote arm is
// gated on exactly that value.
//
// WHAT IS REPRODUCED (asm order):
//   * the two asserts, the state store,
//   * both resource HANDLES (the console's two BaseResourcePtr::CreateFromHandle calls
//     store the handle at wrapper+0x14; AddHandlingModel reads exactly those two words),
//   * the detached-part render queue Construct,
//   * mbCanDriveAwayFromCrash / mbUncrashedThisFrame clears the module's caller makes
//     right after (they are OnRaceCarResourcesLoaded's own two trailing stores).
//
// [FLAG PC bring-up] WHAT IS NOT, and why -- three legs, none paraphrased:
//   1. mCentreOfMassTransform <- BrnPhysics::Def(mDeformationModelResourcePtr) + 1552.
//      Needs the alias-list half of CreateFromHandle (to get the resource MEMORY, not
//      just the handle) HERE. ⭐ LANDED ELSEWHERE (seat wave 2026-08-05): the promote site
//      (RaceCarEntityModule::ResetActiveRaceCar) forwards the resident spec's +1552 matrix
//      through SetCentreOfMassTransformBringUp, so CalcBodyTransform now multiplies the
//      SHIPPED model-space->handling-space matrix, not the identity. This slot still
//      belongs here once the alias leg lands. DELETE-WHEN the spec is homed.
//   2. the four RenderParams::SetWheelScale(i, Def + 96 + 48*i) calls -- same dependency,
//      and this build cannot draw wheels at all (Model::SetupShaderConstantsForInstancing
//      is absent).
//   3. Attrib::FindCollection(-206702987) -> burnoutcargraphicsasset -> the two dwords
//      stored at +0x1C80/+0x1C84 (miDefaultColourIndex / miDefaultColourPalette). Reads
//      an attribute collection the vehicle's own attrib vault owns; SetupCarColour is the
//      only consumer and it is not reconstructed either.
//   4. ResetVerletOffsets @0x822A4E90 -- the ledger calls it `reviewed`, the tree has no
//      body for it (same drift as BrnMath::BuildTransform last wave).
// ============================================================================
void ActiveRaceCar::OnResourcesLoaded( const CgsResource::ResourceHandle& lrDeformationModelHandle,
                                       const CgsResource::ResourceHandle& lrGraphicsModelHandle,
                                       const Vector3&                     lrInitialVelocity,
                                       u64                                luCarAssetAttribKey )
{
    CGS_ASSERT(IsAttached(), "IsAttached()");     // BrnActiveRaceCar.cpp:821
    CGS_ASSERT(!IsActive(), "!IsActive()");       // :822

    (void)lrInitialVelocity;     // consumed by AddHandlingModel, carried for signature parity
    (void)luCarAssetAttribKey;   // ditto

    muState = E_STATE_WAITING;                                       // +0x740 = 2

    mDeformationModelHandle = lrDeformationModelHandle;              // +0x1CA4
    mGraphicsModelHandle    = lrGraphicsModelHandle;                 // +0x1CC4

    // BrnWorld::DetachedPartRenderEvent<20>::Construct(this + 5520) -- the queue lives
    // inside mRenderParams and is the one member of the block this header names.
    mRenderParams.GetDetachedPartQueue().Construct();
}

// ============================================================================
// AddHandlingModel @ 0x822D3EC8   (drivable wave 2026-08-01)
//
// Hands a just-promoted car to the physics vehicle manager. Every argument below is
// read off the ASM at 0x822D3EE4..0x822D4054, because the pseudocode is wrong twice
// over (it drops the vector velocity and mis-pairs the 64-bit VolumeInstanceId load):
//   r3 = lpVehicleInterface   r4 = mHandlingBodyVolumeId (ld 0xD0)
//   r5 = lrInitialTransform   r6 = luCarAssetAttribKey
//   r7 = mDeformationModelHandle (ld 0x1CA4)   r8 = mGraphicsModelHandle (ld 0x1CC4)
//   r9 = mpRaceCar->GetType() (lbz 0xA4)       f1 = mfBaseDeformAmount (lfs 0x7CC)
//   stack: meBaseDeformationType (lwz 0x7C8), the computed bool, lu8CarStrengthStat
//   v1 = lrInitialVelocity     v2 = vspltisw 0  (angular velocity is ALWAYS zero here)
//
// The one piece of logic is the bool: it is TRUE only when the caller asked for it AND
// the paired global car is the PLAYER (`lbz r11, 0xA4(mpRaceCar)` == E_RACE_CAR_TYPE_PLAYER
// == 0). CreateRaceCar's own parameter name for it is lbDisablePhysicsStateReset.
// ============================================================================
void ActiveRaceCar::AddHandlingModel( BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInterface,
                                      u64                   luCarAssetAttribKey,
                                      const Matrix44Affine& lrInitialTransform,
                                      const Vector3&        lrInitialVelocity,
                                      bool                  lbResettingPhysicsState,
                                      u8                    lu8CarStrengthStat )
{
    CGS_ASSERT(IsActive(), "IsActive()");                             // BrnActiveRaceCar.cpp:1153
    CGS_ASSERT(lpVehicleInterface != 0, "lpVehicleInterface != NULL"); // :1154
    CGS_ASSERT(mpRaceCar != 0, "mpRaceCar != NULL");                   // :1155

    bool lbDisablePhysicsStateReset = false;
    if( lbResettingPhysicsState )
    {
        CGS_ASSERT(IsAttached(), "IsAttached()");                      // BrnActiveRaceCar.h:1089
        lbDisablePhysicsStateReset = ( mpRaceCar->GetType() == E_RACE_CAR_TYPE_PLAYER );
    }

    CGS_ASSERT(IsAttached(), "IsAttached()");                          // BrnActiveRaceCar.h:1089

    lpVehicleInterface->CreateRaceCar(
        mHandlingBodyVolumeId,
        lrInitialTransform,
        lrInitialVelocity,
        Vector3{ 0.0f, 0.0f, 0.0f, 0.0f },      // asm `vspltisw128 v127, 0` -> v2
        luCarAssetAttribKey,
        mDeformationModelHandle,
        mGraphicsModelHandle,
        mpRaceCar->GetType(),
        mfBaseDeformAmount,                                            // +0x7CC
        meBaseDeformationType,                                         // +0x7C8
        lbDisablePhysicsStateReset,
        static_cast<s32>( lu8CarStrengthStat ) );
}

// ----------------------------------------------------------------------------
// [FLAG PC bring-up] SeedPhysicsStateFromCreateEventBringUp -- NOT an X360 function.
// See the banner in BrnActiveRaceCar.h. The console's UpdatePhysicsState memcpy's the
// whole 1120-byte RaceCarState; only mTransform is knowable without a physics tick, and
// only mTransform is written here.
// ----------------------------------------------------------------------------
void ActiveRaceCar::SeedPhysicsStateFromCreateEventBringUp(const Matrix44Affine& lrTransform)
{
    CGS_ASSERT(IsAttached(), "IsAttached()");
    mPhysicsState.mTransform = lrTransform;
}

// ----------------------------------------------------------------------------
// [FLAG PC bring-up] SetCentreOfMassTransformBringUp -- NOT an X360 function. See the header
// banner: this is the console OnResourcesLoaded's `Def(...) + 1552` read, fed from the resident
// spec by the promote site instead of through the unreconstructed resource alias leg.
// ----------------------------------------------------------------------------
void ActiveRaceCar::SetCentreOfMassTransformBringUp(const Matrix44Affine& lrCarModelSpaceToHandlingBodySpace)
{
    CGS_ASSERT(IsAttached(), "IsAttached()");
    mCentreOfMassTransform = lrCarModelSpaceToHandlingBodySpace;
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

// ============================================================================
// ⭐⭐ UpdateEngineState @ 0x822A4F50   (163 instructions)   -- COMPLETE
//   (engine wave 2026-08-12)
//
// THE IGNITION. Pressing the gas (or the brake) starts the engine:
//   OFF --(demand)--> STARTING --(1.2 s)--> RUNNING --(15 s idle)--> STOPPING --(0.5 s)--> OFF
// and RUNNING is the state ProcessPlayerVehicleInput @0x822FFE30 requires before it fills the
// driver-controls record with anything but zeros. Nothing else in the XEX writes meEngineState
// away from OFF, and ActiveRaceCar::Attach parks the junkyard player car at OFF by design.
//
// ---- SIGNATURE (see the header banner; every argument traced to a named module member) -----
// Hex-Rays' a5/a6/a7 are the phantom GPR shadows the PPC ABI reserves for f1/f2/f3; the body
// never touches r4/r5/r6.
//
// ---- CONSTANTS (rodata, read out of the asm) -----------------------------------------------
//   flt_8201497C = 0.05f   the throttle/brake dead-band       (0x822A4F78/7C/84)
//   flt_82014980 = 1.2f    STARTING -> RUNNING crank time     (0x822A50B8)
//   flt_82014984 = 2.0f    |mfSpeedMPH| that keeps it RUNNING (0x822A510C)
//   flt_82014988 = 15.0f   RUNNING -> STOPPING idle timeout   (0x822A5120)
//   flt_820147FC = 0.5f    STOPPING -> OFF                    (0x822A5164)
//   flt_82001CC0 = 0.0f    this file's own zero
//
// ---- THE TWO NON-OBVIOUS BRANCHES ----------------------------------------------------------
// * 0x822A51BC (the `mbEnableEngineSwitchOff == false` / "not my car" arm) is
//     cntlzw r11, state ; extrwi r11,r11,1,26 ; xori r11,r11,1 ; addi r11,r11,1
//   cntlzw is 32 only when state == 0, and bit 26 (big-endian numbering) is the 0x20 bit of
//   that count, so the whole sequence is exactly
//     meEngineState = (meEngineState == OFF) ? STARTING : RUNNING;
//   i.e. a car that may not switch its engine off is dragged toward RUNNING every frame.
// * case STOPPING's `bne cr6, loc_822A50C8` (0x822A5158) jumps into case STARTING's tail --
//   the shared "time = 0; state = RUNNING" epilogue (Hex-Rays' LABEL_21).
//
// ⚠️ NOT A DIVERGENCE: the console's `if (v13 && !a9)` in case OFF is the reason the car does
// not crank on the car-select screen. It is reproduced verbatim.
// ----------------------------------------------------------------------------
void ActiveRaceCar::UpdateEngineState(f32 lfTimeStep,
                                      f32 lfAcceleration,
                                      f32 lfBraking,
                                      bool lbIsInOnlineGameMode,
                                      bool lbInCarSelectScreen)
{
    const f32 KF_CONTROL_DEAD_BAND  = 0.05f;         // flt_8201497C
    const f32 KF_CRANK_TIME         = 1.2f;          // flt_82014980
    const f32 KF_ROLLING_SPEED_MPH  = 2.0f;          // flt_82014984
    const f32 KF_IDLE_SHUTDOWN_TIME = 15.0f;         // flt_82014988
    const f32 KF_STOPPING_TIME      = 0.5f;          // flt_820147FC

    // 0x822A4F78..0x822A4F9C. An online car is always treated as "the driver is asking for
    // throttle" -- a remote car's engine must not idle itself off on our machine.
    const bool lbEngineDemanded = ( lfAcceleration > KF_CONTROL_DEAD_BAND )
                               || ( lfBraking      > KF_CONTROL_DEAD_BAND )
                               || lbIsInOnlineGameMode;

    if( !IsActive() )
    {
        mfEngineStateTime = 0.0f;                                        // 0x822A4FC0
        return;
    }

    // 0x822A4FD0..0x822A4FF0. mbEnableEngineSwitchOff (+0x770) / mbIsInGameMode (+0x777) are the
    // same pair ActiveRaceCar::Attach uses to decide the seed state.
    if( !mbEnableEngineSwitchOff || ( mbIsInGameMode && !lbInCarSelectScreen ) )
    {
        // 0x822A51BC -- see the banner.
        mfEngineStateTime = 0.0f;
        meEngineState =
            ( meEngineState == RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_OFF )
                ? RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_STARTING
                : RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_RUNNING;
        return;
    }

    // 0x822A4FF4..0x822A5038. A crashing car's engine is forced RUNNING out of STARTING or
    // STOPPING (so the wreck keeps its engine note) and its timer is cleared either way.
    if( IsCrashing() )
    {
        mfEngineStateTime = 0.0f;
        if( meEngineState == RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_STARTING
         || meEngineState == RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_STOPPING )
        {
            meEngineState = RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_RUNNING;
        }
        return;
    }

    switch( meEngineState )                                              // 0x822A503C jump table
    {
    case RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_OFF:      // 0x822A506C
        // ⭐ THE GAS PEDAL. `&& !lbInCarSelectScreen` is the console's own: the car on the
        // car-select podium never cranks, however hard the pad is pushed.
        if( lbEngineDemanded && !lbInCarSelectScreen )
        {
            mfEngineStateTime = 0.0f;
            meEngineState = RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_STARTING;
        }
        break;

    case RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_STARTING: // 0x822A50A4
        if( lbEngineDemanded )
        {
            mfEngineStateTime = mfEngineStateTime + lfTimeStep;
            if( mfEngineStateTime > KF_CRANK_TIME )
            {
                mfEngineStateTime = 0.0f;
                meEngineState = RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_RUNNING;
            }
        }
        else
        {
            // Let go mid-crank and it drops straight back to OFF -- note the console does NOT
            // clear the timer here (0x822A50E8 stores only the state).
            meEngineState = RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_OFF;
        }
        break;

    case RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_RUNNING:  // 0x822A50F8
        // Demand, or still rolling faster than 2 mph, keeps it running -- and the console
        // reaches the shared `mfEngineStateTime = 0` epilogue at 0x822A4FB8 to do it.
        if( lbEngineDemanded
         || ( ( mPhysicsState.mfSpeedMPH < 0.0f ? -mPhysicsState.mfSpeedMPH
                                                :  mPhysicsState.mfSpeedMPH )
              > KF_ROLLING_SPEED_MPH ) )
        {
            mfEngineStateTime = 0.0f;
        }
        else
        {
            mfEngineStateTime = mfEngineStateTime + lfTimeStep;
            if( mfEngineStateTime > KF_IDLE_SHUTDOWN_TIME )
            {
                mfEngineStateTime = 0.0f;
                meEngineState = RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_STOPPING;
            }
        }
        break;

    case RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_STOPPING: // 0x822A5150
        if( lbEngineDemanded )
        {
            mfEngineStateTime = 0.0f;
            meEngineState = RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_RUNNING;
        }
        else
        {
            mfEngineStateTime = mfEngineStateTime + lfTimeStep;
            if( mfEngineStateTime > KF_STOPPING_TIME )
            {
                meEngineState = RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_OFF;
                mfEngineStateTime = 0.0f;
            }
        }
        break;

    default:                                                            // 0x822A5190
        CGS_ASSERT( false, "How did it get here?" );                     // X360 :1743
        break;
    }
}

// ============================================================================
// Update @ 0x822F78B0   (400 instructions)   -- PARTIAL SLICE   (engine wave 2026-08-12)
//
// The per-frame tick of one active race car. Its ONLY caller is
// RaceCarEntityModule::UpdateActiveCars @0x822FF250, and it is the ONLY caller of
// UpdateEngineState -- which is why it has to exist at all for the gas pedal to work.
//
// ---- THE CONSOLE'S FULL ARGUMENT LIST (derived slot by slot from the two asms) -------------
// Param save area starts at r1+0x10 (calibrated: UpdateActiveCars' `stb r8, 0x57(r1)` is read
// back by Update as `lbz r8, arg_57(r1)`, so caller displacement == callee arg name).
//   r3   this
//   r4   an int  (Update: `mr r27, r4`)                    <- UpdateActiveCars' own r4,
//                                                             = sub_822B5EA0(lpOutput)
//   r10  bool    (Update: `mr r24, r10`)                   -> lbIsInOnlineGameMode  ✔ USED
//   0x50 bool    (`lbz r8, arg_57`)                        -> lbInCarSelectScreen   ✔ USED
//   0x58 bool                                              <- InputBuffer_PrePhysics::
//                                                             GetInHardStopCamera()
//   0x60 ptr                                               <- module + 0x18490
//   0x68 ptr     (`lwz r23, arg_6C`, asserted non-NULL)    <- lpVehicleOutput
//   0x70 int                                               <- module + 0x18368 (meGameModeType)
//   f1   f32     -> lfTimeStep      ✔ USED   <- module mfTimeStep      (+0x18398)
//   f2   f32                                 <- module +0x183A0
//   f3   f32                                 <- module +0x183A4  (-> CalculateWheelAngular…)
//   f4   f32     -> lfAcceleration  ✔ USED   <- mPlayerVehicleControls.mfAcceleration (+0x183C8)
//   f5   f32     -> lfBraking       ✔ USED   <- mPlayerVehicleControls.mfBraking      (+0x183CC)
//   v1/v2 two Vector3s                       <- module +0x18720 / +0x18730
//
// ---- WHAT THIS SLICE REPRODUCES ------------------------------------------------------------
//   the IsAttached assert; the per-frame dt work on members this tree has NAMED
//   (mfInvulnerablityTime, mfTimeSinceCreation, the two mbIsTouching* clears,
//   mbDriveAwayCheckRequired/mbCanDriveAwayFromCrash); the muType == E_RACE_CAR_TYPE_PLAYER
//   gate at 0x822F7E24 and the UpdateEngineState call behind it; the mbAIToBeActivated clear;
//   and the mbCrashedIntoWater timer.
//
// ---- [FLAG PC bring-up] WHAT THIS SLICE DROPS -- named, not paraphrased --------------------
//  1. `lpVehicleOutput != NULL` (X360 :260) -- the argument itself is not plumbed here.
//  2. mbIsTouchingWorld's value (0x822F7A0C): `mbCrashing ? false : (*(this+0x4E4) <= 0.0f)`.
//     +0x4E4 is RaceCarState+0x404 and this tree has not named that field, so the flag is
//     LEFT ALONE rather than written from a guess. Nothing in the PC build reads it today.
//  3. the whole route/direction block (X360 0x822F7A5C..0x822F7C5C): GetDirection, BrnMath::
//     Flatten, the RwMathVPU::IsValid assert (:315), the mfTimeDriveableInCrash accumulator
//     and the `> 1.5s` VariableEventQueue<1536,16>::AddEvent(type 38). It is gated on
//     +0x536 (RaceCarState+0x456, also unnamed here) and on the two VMX route vectors, which
//     this slice does not receive.
//  4. the IsOnRaceStartState(0) start-line rev RNG (0x822F7C64..0x822F7CE4) -- it needs the
//     module's RNG at +0x18490.
//  5. RaceCar::GetTransform / GetPreviousPosition / GetPosition (0x822F7D44..0x822F7DC8):
//     the console calls them and DISCARDS all three results (v102/v103/v104 are dead in the
//     decompilation) -- almost certainly an inlined body Hex-Rays lost. Dropped deliberately.
//  6. CalculateWheelAngularVelocities @0x822BFCF8, UpdateInAirRotations @0x822BFFA8,
//     SendAddedRemovedNetworkCarForCollisionEvents @0x822BF840, UpdateIndicators @0x822A5340 --
//     NONE of the four exists anywhere in this tree yet.
//     ⚠️ #6 is why the wheels still do not spin: CalculateWheelAngularVelocities is the
//     producer for them, and GetWheelsWorldTransfrom @0x825D8878 is bodyless besides.
//  7. the mbIsWaitingForDeferredReset -> RequestPlaceOnTrack countdown (0x822F7E80..0x822F7EB8).
//     RequestPlaceOnTrack exists, but the latch is only ever armed by code this build has not
//     landed, so running the countdown would be dead work with a live teleport at the end.
// ----------------------------------------------------------------------------
void ActiveRaceCar::Update(f32 lfTimeStep,
                           f32 lfAcceleration,
                           f32 lfBraking,
                           bool lbIsInOnlineGameMode,
                           bool lbInCarSelectScreen)
{
    CGS_ASSERT( IsAttached(), "IsAttached()" );          // BrnActiveRaceCar.h:1418

    // 0x822F7964..0x822F797C. Not crashing => the "can I drive away?" check is re-armed and the
    // answer is cleared. mPhysicsState.mbCrashing is the console's `lbz r10, 0x52A(r31)`.
    const bool lbCrashing = mPhysicsState.mbCrashing;
    if( !lbCrashing )
    {
        mbDriveAwayCheckRequired = true;                 // +0x730
        mbCanDriveAwayFromCrash  = false;                // +0x779
    }

    // 0x822F7984..0x822F79B8.
    if( mfInvulnerablityTime > 0.0f )                    // +0x724 (Attach seeds -1.0f)
    {
        mfInvulnerablityTime = mfInvulnerablityTime - lfTimeStep;
    }
    mfTimeSinceCreation = mfTimeSinceCreation + lfTimeStep;   // +0x728

    mbIsTouchingAnotherRaceCar = false;                  // +0x772
    mbIsTouchingPlayer         = false;                  // +0x773
    // [FLAG PC bring-up] mbIsTouchingWorld (+0x774) -- drop #2 in the banner.

    // 0x822F7DF4..0x822F7E48. ⭐ THE GATE AND THE CALL. Only a PLAYER-typed global slot gets an
    // engine state; AI / traffic / remote slots skip it entirely.
    CGS_ASSERT( mpRaceCar->GetType() < E_RACE_CAR_TYPE_COUNT,
                "muType < E_RACE_CAR_TYPE_COUNT" );      // BrnRaceCar.h:577
    if( mpRaceCar->GetType() == E_RACE_CAR_TYPE_PLAYER )
    {
        // ---- [engine-diag] PC bring-up instrument -- DELETE WHEN the car drives -----------
        // Placed in the CALLER so the reconstructed UpdateEngineState body stays byte-for-byte
        // the console's shape (it has six early returns; wrapping it would have restructured
        // it). Logs only TRANSITIONS, so the whole ignition chain is four lines in BrnGame.log.
        const RaceCarEntityModuleIO::EActiveRaceCarEngineState leEntryState = meEngineState;

        UpdateEngineState( lfTimeStep, lfAcceleration, lfBraking,
                           lbIsInOnlineGameMode, lbInCarSelectScreen );

        if( meEngineState != leEntryState
            && ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0
            && CgsDev::Log::gpDebugPrint != 0 )
        {
            *CgsDev::Log::gpDebugPrint
                << "[engine-diag] meEngineState " << static_cast<s32>( leEntryState )
                << " -> " << static_cast<s32>( meEngineState )
                << "  (0=OFF 1=STARTING 2=RUNNING 3=STOPPING)"
                << "  accel " << lfAcceleration
                << " brake " << lfBraking
                << " mph "   << mPhysicsState.mfSpeedMPH
                << " carsel " << ( lbInCarSelectScreen ? 1 : 0 )
                << " swoff "  << ( mbEnableEngineSwitchOff ? 1 : 0 )
                << " ingame " << ( mbIsInGameMode ? 1 : 0 ) << "\n";
        }
        // ---- end [engine-diag] ------------------------------------------------------------
    }

    mbAIToBeActivated = false;                           // +0x781 (0x822F7E54)

    // 0x822F7EBC..0x822F7ED0.
    if( mbCrashedIntoWater )                             // +0x783
    {
        mfTimeInWater = mfTimeInWater + lfTimeStep;      // +0x784
    }
}

}
