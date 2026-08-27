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
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h" // VehicleInputInterface::CreateRaceCar / RemoveRaceCar
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h"   // InSceneUpdateInterface (the Detach chain's four Remove* posts)
#include "GameSource/World/BrnEntityTypes.h"              // BrnWorld::E_ENTITYTYPE_RACECAR (the Attach seed)
#include "SharedClasses/World/BrnCollisionTag.h"          // BrnWorld::KU_COLLISION_FLAG_FATAL (IsWrecked)
#include "GameShared/GameClasses/Development/Log/CgsLog.h" // gpDebugPrint / gxMessageFilterFlags
#include <stdlib.h>   // getenv (the [wheel-diag] gate)
#include "GameShared/GameClasses/System/Timer/CgsFrameInterpolation.h" // ⚠️ FLAG PC QoL: BlendTransform (the render-pose interpolator)

#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationState.h" // DeformationState / CarState (UpdateDeformationState, 2026-08-24)

#include <cstring>   // memset (the console's own inlined clears)
#include <cmath>     // std::fabs (UpdateDeformationState's vandc sign-mask ABS)

// [deform-trace] host-side present counter, for EXACT frame correlation. Same extern the
// other correlated instruments use (CgsIm2d.cpp:24, CgsImRenderBufferTemplate.cpp:34,
// BrnRendererModule.cpp:288). BRN_FRAME_DUMP names its BMPs bb_<guPresentCount>.bmp, so
// printing this number turns "the frame around the impact" from an ESTIMATE into a filename.
// ⛔ A trace correlated against a dump of a DIFFERENT frame has already produced a false lead
// in this tree (device.cpp:255) -- do not go back to inferring the frame from a call index.
namespace renderengine { extern u32 guPresentCount; }

namespace BrnWorld
{

// The out-of-class home for the header's SetOnStartLine sentinel. flt_820037C8 in the
// ARTIST image is 0xBF800000 == -1.0f (dumped, not inferred); RaceCarEntityModule::
// SetAllCarsOnStartLine @0x822A4850 loads it once into f31 and stores it per car.
const f32 ActiveRaceCar::KF_NO_START_LINE_BOOST_CHANGE = -1.0f;

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

// ============================================================================
// Detach @ 0x822EB578   (ghost-car wave 2026-08-17)  -- THE INVERSE OF Attach.
//
// The only caller is RaceCarEntityModule::DetachActiveRaceCar @0x822FEDF8. Everything
// below is the asm read store-for-store (0x822EB578..0x822EB6AC); the two arguments are
// DWARF-declared (BrnActiveRaceCar.h:670 `void Detach(VehicleInputInterface *,
// OutputBuffer_PreScene::SceneInputInterface *)`) and match the asm's r4 / r5.
//
//   0x822EB590  IsAttached()                       assert BrnActiveRaceCar.cpp:0x3AF == :943
//   0x822EB5C8  lwz r11,0x6F0 -> mpRaceCar != NULL  assert :0x3B0 == :944
//   0x822EB5F4  IsActive() -> the teardown pair    RemoveHandlingModel(r4=vehicle),
//                                                  RemoveFromScene(r4=scene, r5=vehicle)
//                                                  (note the SWAP: Detach's own r5 becomes
//                                                   RemoveFromScene's r4)
//   0x822EB624  IsAttached()                       assert BrnActiveRaceCar.h:0x441 == :1089
//   0x822EB654  RaceCar::RemoveActiveRaceCar(mpRaceCar)  -- clears the BACK pointer first,
//                                                  which is what lets RemoveRaceCar's
//                                                  RaceCar::RemoveFromWorld assert
//                                                  "mpActiveRaceCar == NULL" pass.
//   0x822EB670..0x822EB694  the ten field stores, in asm order:
//        stw 1  0x744  meOnlineState = E_ONLINE_STATE_NORMAL   (Attach seeds the same value)
//        stb 1  0x79D  mbRenderThisFrame = true
//        stw 0  0x6F0  mpRaceCar = 0
//        stb 0  0x740  muState   = E_STATE_INACTIVE   <-- the ghost-car fix
//        stb 0  0x799  mbIsDisconnectedFromNetwork
//        stb 0  0x79A  mbIsInCarSelectOnline
//        stb 0  0x79B  mbCarSelectOnlineStateChanged
//        stb 0  0x79C  mbReceivedNetworkDriverControls
//        stw 0  0x794  miFlashFrequency
//        stb 0  0x781  mbAIToBeActivated
//   0x822EB698 / 0x822EB6A4  BaseResourcePtr::CreateFromHandle(this+0x1C90, &dword_82FAD960)
//                            and (this+0x1CB0, ...) -- `slot = NULLResourcePtr`
//                            (dword_82FAD960 IS NULLResourcePtr+0x14, the sentinel's
//                            {mpThis,muThreadId} pair; see CgsResourcePtr.h:169). Reproduced
//                            as the HANDLE half only, which is the half this class models
//                            (maPad1C90a + the two named ResourceHandles).
//                            [FLAG] the alias-list half of CreateFromHandle is not reproduced
//                            ANYWHERE in this tree -- OnResourcesLoaded's banner records the
//                            same gap for the write side.
//
// NOT cleared by the console: meActiveRaceCarIndex (+0x748). The slot keeps its own index for
// life (Construct is its only writer) -- DetachActiveRaceCar reads it BEFORE calling this, and
// RaceCar::RemoveActiveRaceCar decides separately whether the GLOBAL car forgets it.
// ============================================================================
void ActiveRaceCar::Detach( BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInterface,
                            CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface )
{
    CGS_ASSERT(IsAttached(), "IsAttached()");            // X360 BrnActiveRaceCar.cpp:943
    CGS_ASSERT(mpRaceCar != 0, "mpRaceCar != NULL");     // :944
    if (mpRaceCar == 0)
    {
        return;
    }

    if (IsActive())
    {
        RemoveHandlingModel(lpVehicleInterface);
        RemoveFromScene(lpSceneInterface, lpVehicleInterface);
    }

    CGS_ASSERT(IsAttached(), "IsAttached()");            // BrnActiveRaceCar.h:1089

    mpRaceCar->RemoveActiveRaceCar();

    meOnlineState                   = E_ONLINE_STATE_NORMAL;   // 0x744 = 1
    mbRenderThisFrame               = true;                    // 0x79D = 1
    mpRaceCar                       = 0;                       // 0x6F0 = 0
    muState                         = E_STATE_INACTIVE;        // 0x740 = 0
    mbIsDisconnectedFromNetwork     = false;                   // 0x799
    mbIsInCarSelectOnline           = false;                   // 0x79A
    mbCarSelectOnlineStateChanged   = false;                   // 0x79B
    mbReceivedNetworkDriverControls = false;                   // 0x79C
    miFlashFrequency                = 0;                       // 0x794
    mbAIToBeActivated               = false;                   // 0x781

    // `slot = CgsResource::NULLResourcePtr` x2 (see the banner). The wrappers themselves are
    // this class's two opaque ResourcePtr spans; what it models -- and what AddHandlingModel
    // reads back -- is the handle each stores at its own +0x14, so the pair is dropped to the
    // null handle by name.
    mDeformationModelHandle = CgsResource::NULLResourceHandle;   // this+0x1C90 (+0x14)
    mGraphicsModelHandle    = CgsResource::NULLResourceHandle;   // this+0x1CB0 (+0x14)

    // ⚠️ FLAG PC quality-of-life (verify F2, ghostcar): NOT a console store. The render-pose
    // interpolator (mBodyPoseTrack / maWheelPoseTracks, PC-only, reset in Construct) keeps the
    // OLD car's pose across a slot re-use; a re-spawn into this slot at another location would
    // blend old->new for one frame (a one-frame streak). Reset the history with the slot.
    mBodyPoseTrack.Reset();
    for (u32 luWheel = 0; luWheel < KU_INTERP_WHEELS; ++luWheel)
        maWheelPoseTracks[luWheel].Reset();
}

// ----------------------------------------------------------------------------
// RemoveHandlingModel @ 0x822D4070   (ghost-car wave 2026-08-17) -- the exact inverse of
// AddHandlingModel below: tell the physics vehicle manager to destroy this car's body.
// Only caller: Detach's IsActive() arm. DWARF BrnActiveRaceCar.h:1013
// (`void RemoveHandlingModel(VehicleInputInterface *)`), which matches the asm's r4.
//
//   0x822D4084  IsActive()                    assert BrnActiveRaceCar.cpp:0x4A2 == :1186
//   0x822D40B8  lpVehicleInterface != NULL    assert :0x4A3 == :1187
//   0x822D40DC  ld r11, 0xD0(this)            mHandlingBodyVolumeId (the 64-bit handle)
//   0x822D40E0  addis r3,r29,2 ; addi r3,r3,-0x6D0   == lpVehicleInterface + 129328
//                                             == &mRemoveRaceCarEventQueue (the seat
//                                             BrnVehicleInputInterface.h:151 already pins)
//   0x822D40F0  bl BaseEventQueue<RemoveRaceCarEvent>::AddEvent
//
// The console INLINED VehicleInputInterface::RemoveRaceCar here -- it has no standalone symbol
// in the XEX and is therefore absent from the ledger -- but the DWARF declares it
// (BrnVehicleInputInterface.h:126 `int32_t RemoveRaceCar(VolumeInstanceId)`) and those five
// instructions ARE its body. It is homed as a header inline on the interface (same shape as
// AddLineTestResult) so this class posts through the owner instead of reaching a private queue
// by offset. Its return value is discarded here, exactly as the console discards it (the
// caller tail-returns r3 and nothing reads it).
//
// RUNTIME SCOPE ON THIS BUILD: VehicleManager::ProcessRemoveEvents @0x826160C8 is not
// reconstructed, so the posted event is dropped with the frame's IO buffer and the physics body
// actually survives the detach. That is a MOUNT gap, not a reconstruction gap: the producer
// side is now correct and complete, and the day the drain lands the hull goes with the car.
// ----------------------------------------------------------------------------
void ActiveRaceCar::RemoveHandlingModel( BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInterface )
{
    CGS_ASSERT(IsActive(), "IsActive()");                              // X360 :1186
    CGS_ASSERT(lpVehicleInterface != 0, "lpVehicleInterface != NULL"); // :1187
    if (lpVehicleInterface == 0)
    {
        return;
    }

    lpVehicleInterface->RemoveRaceCar(mHandlingBodyVolumeId);
}

// ----------------------------------------------------------------------------
// RemoveFromScene @ 0x822D4100   (ghost-car wave 2026-08-17) -- drop the car's scene presence:
// its volume instance, its volume, its collision registration (conditionally) and its entity.
// Only caller: Detach's IsActive() arm. DWARF BrnActiveRaceCar.h:1031
// `void RemoveFromScene(SceneInputInterface *, VehicleInputInterface *)` -- and note the
// argument ORDER against Detach's own: Detach passes (r4 = its SCENE pointer, r5 = its VEHICLE
// pointer), i.e. the pair is swapped (asm 0x822EB610..0x822EB618).
//
//   0x822D4118  IsActive()                     assert BrnActiveRaceCar.cpp:0x51A == :1306
//   0x822D414C  lpSceneInterface != NULL       assert :0x51B == :1307
//   0x822D4174  ld r4,0xD0(this) -> InSceneUpdateInterface::RemoveVolumeInstance(id64)
//   0x822D4190  the same id via a stack qword  -> InSceneUpdateInterface::RemoveVolume(id64)
//   0x822D4194  lwz r11,0x744 == meOnlineState
//               `if ((meOnlineState != 0 && meOnlineState != 3) || mbAddedForCollision)`
//                  -> RemoveFromCollision(scene, vehicle)
//               (0 == E_ONLINE_STATE_CONNECTING, 3 == E_ONLINE_STATE_DISCONNECTED: a car that
//                never finished connecting, or has already dropped, was never put in the
//                collision set unless mbAddedForCollision says otherwise.)
//   0x822D41C4  ld ; srdi 32 -> the EMBEDDED 32-BIT ENTITY WORD -> RemoveEntity(entity, 1)
//   0x822D41E0  stb 0, 0x78A                   mbAddedToScene = false
//
// ID WIDTH -- READ THIS BEFORE "SIMPLIFYING" THE THREE CALLS. The console passes the FULL
// 64-bit mHandlingBodyVolumeId to RemoveVolumeInstance / RemoveVolume / RemoveForCollision (a
// bare `ld` into r4) and only the HIGH dword to RemoveEntity. The committed
// InSceneUpdateInterface declared the first three as taking a 32-bit `EntityId`, which was
// fitted to their one previous caller (PhysicalBodyPart::RemoveFromScene, whose ids really are
// 32-bit words) and which the DecFIGS DWARF contradicts:
//     CgsSceneManagerIO_SceneUpdate.h:393  void RemoveVolumeInstance(VolumeInstanceId)
//     CgsSceneManagerIO_SceneUpdate.h:378  void RemoveVolume(VolumeId)
//     CgsSceneManagerIO_SceneUpdate.h:423  void RemoveForCollision(VolumeInstanceId)
//     CgsSceneManagerIO_SceneUpdate.h:340  void RemoveEntity(EntityId, bool)  <- 32-bit, as committed
// Narrowing a race-car id to 32 bits would move the entity word from the HIGH dword to the LOW
// one and post a DIFFERENT handle. The DWARF-declared 64-bit overloads were therefore added
// alongside the committed 32-bit ones (additive, header-only inlines) and are what this calls.
//
// RUNTIME SCOPE ON THIS BUILD -- ⚠️ CORRECTED 2026-08-18 (wave Q5, scene-add cluster). This
// paragraph used to say "race cars are never REGISTERED with the scene manager in the first
// place (ActiveRaceCar::AddToScene @0x822EB768 is not reconstructed)". THAT IS NO LONGER TRUE:
// AddToScene / AddToCollision / OnHandlingModelAdded / UpdateCullingGroup /
// SendSceneUpdatesPostPhysics / DetermineCullingGroup are all bodied in
// BrnActiveRaceCar_wQ5_01.cpp. What IS still true, and is what makes these four posts inert:
//   * AddToScene's own caller chain does not run yet -- RaceCarEntityModule::
//     ProcessCreateVehicleEvents @0x822FF620 is still the PublishNewVehicleToDirector
//     WithoutPhysicsBringUp stand-in, so OnHandlingModelAdded is never reached;
//   * of the four queues this posts to only mRemoveEntityQueue is drained
//     (SceneManagerModule::BridgeInputSceneUpdateInterfaceToSubModules, which skips ids the
//     EntityManager does not know: `if (liIndex < 0) continue;`); the volume /
//     volume-instance / for-collision legs of that bridge are still absent (its own banner at
//     CgsSceneManagerModule.cpp:685 says so);
//   * AddToScene's AddDynamicVolume post is itself parked on the missing 64-bit
//     `AddDynamicVolume(VolumeId, const void*, u8)` overload -- see the wQ5 partfile.
// They are reproduced rather than parked because the producer side must be correct the day the
// drain lands, and because posting nothing is indistinguishable from a leak once it does.
// (CalculateVehicleLODs still needs the world module's stand-in for the same caller-chain
// reason.)
// ----------------------------------------------------------------------------
void ActiveRaceCar::RemoveFromScene( CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface,
                                     BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInterface )
{
    CGS_ASSERT(IsActive(), "IsActive()");                           // X360 :1306
    CGS_ASSERT(lpSceneInterface != 0, "lpSceneInterface != NULL");   // :1307
    if (lpSceneInterface == 0)
    {
        return;
    }

    lpSceneInterface->RemoveVolumeInstance(mHandlingBodyVolumeId);
    lpSceneInterface->RemoveVolume(CgsSceneManager::VolumeId(mHandlingBodyVolumeId.muId));

    if ((meOnlineState != E_ONLINE_STATE_CONNECTING && meOnlineState != E_ONLINE_STATE_DISCONNECTED)
        || mbAddedForCollision)
    {
        RemoveFromCollision(lpSceneInterface, lpVehicleInterface);
    }

    // The console hands RemoveEntity only the embedded 32-bit entity word (`srdi r11,r11,32`)
    // and the option word 1.
    lpSceneInterface->RemoveEntity(
        CgsSceneManager::EntityId(static_cast<u32>(mHandlingBodyVolumeId.muId >> 32)), 1u);

    mbAddedToScene = false;                                         // 0x78A
}

// ----------------------------------------------------------------------------
// RemoveFromCollision @ 0x822BF668   (ghost-car wave 2026-08-17) -- take the car's body out of
// the scene collision set and arm the "collision state changed" flag the next physics publish
// reads. Only caller: RemoveFromScene's online-state gate. DWARF BrnActiveRaceCar.h:1044
// `void RemoveFromCollision(SceneInputInterface *, VehicleInputInterface *)` -- TWO parameters,
// and the asm agrees that only the FIRST is read (`mr r31,r3 ; mr r28,r4`; r5 is passed by
// RemoveFromScene and never touched). The second is kept because the console's call site passes
// it and the DWARF declares it.
//
//   0x822BF67C  IsActive()                     assert BrnActiveRaceCar.cpp:0x5DC == :1500
//   0x822BF6B8  stb 1, 0x78D                   mbChangeCollisionState     = true
//   0x822BF6C0  stb 0, 0x78E                   mbCollisionStateToChangeTo = false
//   0x822BF6C4  stb 0, 0x78B                   mbAddedForCollision        = false
//   0x822BF6D4  the gxMessageFilterFlags & 1 debug line ("Removing for collision: " id "\n")
//   0x822BF738  InSceneUpdateInterface::RemoveForCollision(id64)   <- the FULL 8-byte handle
//   0x822BF748  stw 0xFFFF, 0x7D0              mCurrentCullingGroup = 0xFFFF
//   0x822BF74C  IsAttached()                   assert BrnActiveRaceCar.h:0x441 == :1089
//   0x822BF77C  lbz 0xA4(mpRaceCar) == 2       the NETWORK-car re-add arm -- see BLOCKED
//
// [FLAG BLOCKED] the network-car arm (`mpRaceCar->GetType() == E_RACE_CAR_TYPE_NETWORK`). It
// fires the console's "Trying to add/remove race car more than <max> times in a frame" tripwire
// and then posts a {mHandlingBodyVolumeId, mbAdded = false}
// BrnPhysics::Vehicle::VehicleAddedForCollisionEvent onto THIS OBJECT'S OWN queue --
// `bl VehicleAddedForCollisionEvent_::AddEvent` with r3 still == this, i.e. the queue at
// ActiveRaceCar +0x000, which the DWARF names mAddRemoveNetworkCarForCollisionQueue
// (EventQueue<VehicleAddedForCollisionEvent, 8>; the tripwire's `lwz 8(this)` / `lwz 4(this)`
// are that queue's miLength / miMaxLength).
// EXACT MISSING ITEM: that member is still `u8 maPad0000[144]` in this header (144 == the
// 16-byte queue header + 8 * sizeof(VehicleAddedForCollisionEvent), so it does fit exactly).
// It is deliberately NOT modelled in this wave: naming it would put a live mpEvents pointer at
// offset 0 of a class whose only construction path (Construct/Prepare/Attach) never calls
// EventQueue::Construct on it, which is precisely the "mpEvents != NULL / Reached Max length"
// pair that cost the drivable wave a day on VehicleInputInterface. The arm is UNREACHABLE on
// this build (E_RACE_CAR_TYPE_NETWORK requires an online session; the only spawn paths that run
// are the player and the AI rivals).
// DELETE-WHEN mAddRemoveNetworkCarForCollisionQueue is named AND ActiveRaceCar::Construct
// constructs it.
// ----------------------------------------------------------------------------
void ActiveRaceCar::RemoveFromCollision( CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface,
                                         BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInterface )
{
    CGS_ASSERT(IsActive(), "IsActive()");                           // X360 :1500
    (void)lpVehicleInterface;   // passed by the console's call site; never read by the callee

    mbChangeCollisionState     = true;    // 0x78D
    mbCollisionStateToChangeTo = false;   // 0x78E
    mbAddedForCollision        = false;   // 0x78B

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint << "Removing for collision: "
                                   << mHandlingBodyVolumeId.muId << "\n";
    }

    if (lpSceneInterface != 0)
    {
        lpSceneInterface->RemoveForCollision(mHandlingBodyVolumeId);
    }

    mCurrentCullingGroup = 0xFFFF;        // 0x7D0

    CGS_ASSERT(IsAttached(), "IsAttached()");                       // BrnActiveRaceCar.h:1089

    // [FLAG BLOCKED] the E_RACE_CAR_TYPE_NETWORK re-add post -- see the banner.
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
// The RaceCarState "+4" layout pin: every state offset this body touches -- 0x444 mi8Gear,
// 0x446 mabWheelExists[0], 0x44A mbCrashing, 0x452 mbIsHidden, 0x40C mfBrake -- lands on those
// members ONLY with the 8-byte mCarAssetAttribKey (BrnVehicleEvents.h's banner). Under a
// 4-byte key every one of them is off by one member.
//
// LIVE: the console's only caller, RaceCarEntityModule::ReadUpdatedActiveRaceCarDataFromPhysics
// @0x822E87B8, runs every PostPhysics frame with its mUsedRaceCars gate passing (vehicle-manager
// ProcessCreateEvents + WriteOutVehicleStats are landed and mounted), so this body owns
// mPhysicsState and the render pose.
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

    // ⚠️ FLAG PC quality-of-life: THIS IS A TELEPORT, so drop the interpolation history.
    // The car is being placed, not moved -- blending from wherever it was before would smear
    // it across the world for one frame. This is the honest place to say so: the producer
    // KNOWS the pose is discontinuous, whereas the blend can only guess from the values (and
    // guessing is what mis-classified a spinning road wheel as a cut -- see BlendTransform).
    ResetRenderPoseInterpolation();
}

// ⚠️ FLAG PC quality-of-life -- NOT an X360 function. Forget the last two ticks, so the next
// frame draws the tick pose straight instead of blending across a discontinuity.
void ActiveRaceCar::ResetRenderPoseInterpolation()
{
    mBodyPoseTrack.Reset();
    for (u32 luWheel = 0; luWheel < KU_INTERP_WHEELS; ++luWheel)
        maWheelPoseTracks[luWheel].Reset();
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
// GetPosition @ 0x822CCF78. The three asserts and their order are visible in
// the Breaker body; the final accessor is the inlined RaceCar::GetPosition.
// ----------------------------------------------------------------------------
Vector3 ActiveRaceCar::GetPosition() const
{
    CGS_ASSERT(IsAttached(), "IsAttached()");
    CGS_ASSERT(GetGlobalRaceCar() != nullptr, "mpRaceCar != NULL");
    CGS_ASSERT(IsAttached(), "IsAttached()");

    return GetGlobalRaceCar()->GetPosition();
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
// UpdateDeformationState @ 0x822D4A58 (107 insns; landed 2026-08-24, deform-land wave).
// The per-car deformation readback -- see the header declaration for the field map. Body
// decoded from the headless asm dump (scratchpad land_asm.txt):
//   - assert IsAttached() (BrnActiveRaceCar.h:1096);
//   - carState = lpDeformationState->GetCarStateF(mPhysicsState.mEntityId)  [`lwz r4,0x4A8`];
//   - mRenderParams.mfDeformationSquared = carState+0x6A0 (summed displacement²);
//   - mvfLowestPointWorldSpace = splat( pos.y - |xAxis.y*ext.x| - |yAxis.y*ext.y|
//                                             - |zAxis.y*ext.z| )
//     (rows/pos from mPhysicsState.mTransform @+0x2D0..0x300, extents from
//      mPhysicsState.mHalfExtent @+0x430; the vandc-sign-mask ABS + three vsubfp);
//   - mDeformedBBox <- the 32-byte pair at carState+0x640 (four `ld/std`);
//   - mRenderParams.maAxlePositions[w] <- carState wheel tag point w (+0x660+16w), with the
//     two baked index tripwires (BrnDeformationState.h:75 / BrnActiveRaceCar.h:2081).
// ⚠️ DIVERGENCE (named, PC bring-up): the console dereferences the GetCarStateF result
// unguarded -- its flow guarantees an attached car owns a live deformation slot. On this
// build the deformation model add path is younger than the car create path, so a null here
// is asserted + skipped LOUDLY rather than dereferenced.
// ----------------------------------------------------------------------------
void ActiveRaceCar::UpdateDeformationState(
        const BrnPhysics::Deformation::DeformationState* lpDeformationState)
{
    CGS_ASSERT(IsAttached(), "IsAttached()");

    const BrnPhysics::Deformation::CarState* lpCarState =
        lpDeformationState->GetCarStateF(mPhysicsState.mEntityId.muValue);

    // [FLAG PC bring-up] see the DIVERGENCE note above -- not a console branch.
    CGS_ASSERT(lpCarState != 0, "lpCarState != NULL [PC bring-up guard]");
    if (lpCarState == 0)
    {
        static bool sbReportedNoCarState = false;
        if (!sbReportedNoCarState)
        {
            sbReportedNoCarState = true;
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[deform-readback] UpdateDeformationState: no live CarState for entity "
                    << mPhysicsState.mEntityId.muValue
                    << " -- deformation readback skipped for this car\n";
            }
        }
        return;
    }

    // Summed squared sensor displacement -> the render damage scalar.
    mRenderParams.SetDeformationSquared(lpCarState->GetSummedDisplacementSquared());

    // [deform-readback] one-shot measurement: the first NON-ZERO summed displacement seen
    // (the deform-land wave's acceptance metric -- prove the dents are real numbers, not a
    // texture trick). Prints once per boot.
    //
    // ⛔⛔ READ THIS BEFORE QUOTING THE LINE IT PRINTS. It is a ONE-SHOT, it says "first",
    // and it fires at the junkyard 0.85 deform preset. It therefore proves EXACTLY one
    // thing: the sensor pipeline came alive once. It is NOT a series, it says NOTHING about
    // whether a value ever changes, and quoting it as "the deformation number in a crash run"
    // is a category error (2026-08-27: nearly concluded "crashes don't deform" from it).
    // ⭐ THE SERIES IS THE [deform-trace] BLOCK BELOW -- use that.
    {
        static bool sbReportedDisplacement = false;
        if (!sbReportedDisplacement && lpCarState->GetSummedDisplacementSquared() > 0.0f
            && (CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            sbReportedDisplacement = true;
            *CgsDev::Log::gpDebugPrint
                << "[deform-readback] first non-zero CarState: summedDispSq="
                << lpCarState->GetSummedDisplacementSquared()
                << " sensor0Disp=(" << lpCarState->maSensors[0].mDisplacement.x
                << ", " << lpCarState->maSensors[0].mDisplacement.y
                << ", " << lpCarState->maSensors[0].mDisplacement.z
                << ") numSensors=" << static_cast<u32>(lpCarState->mu8NumSensors) << "\n";
        }
    }

    // =========================================================================================
    // [deform-trace] PER-FRAME deformation witness. NOT X360 -- a host-side instrument, opt-in
    // via BRN_DEFORM_TRACE (a sampling PERIOD in calls; 0/unset = inert, 1 = every call).
    //
    // WHY IT EXISTS: the one-shot above cannot answer "does the car deform IN A CRASH", because
    // a single sample cannot show growth. This block emits a SERIES, and pins every axis the
    // question turns on so no line of it can be read as being about something else:
    //   WHICH CAR   -- entity id, plus IsPlayer()/IsCrashing() read AT THIS CALL (not inferred
    //                  from a branch elsewhere in the frame).
    //   WHICH VALUE -- BOTH ends of the chain on one line:
    //                    dispSq  = CarState::mfSummedDisplacementSquared, the SIM's summed
    //                              squared sensor displacement (-> mfDeformationSquared, the
    //                              damage SCALAR: shading, lights-out, audio).
    //                    maxVer  = max |xyz| over RenderParams::maVerletOffsets[128], the array
    //                              that becomes shader constant 22 and is the ONLY thing that
    //                              actually MOVES A VERTEX (BrnRaceCarEntityModule_Render.cpp:503).
    //                  ⭐ These are DIFFERENT QUANTITIES. A run where dispSq climbs and maxVer
    //                  stays 0 is a car that is "damaged" in every scalar sense and visibly
    //                  undented. Reporting one as the other is the trap this block exists to
    //                  make impossible.
    //   WHEN        -- a monotone call counter, so growth is orderable without trusting a clock.
    //
    // ⚠️ ONE-FRAME SKEW, NAMED: maVerletOffsets is filled by the L4 skinned-model copy in
    // RaceCarEntityModule::ReadUpdatedActiveRaceCarDataFromPhysics (BrnRaceCarEntityModule.cpp
    // L4, ~:3652). Whether that leg runs before or after this one within a frame decides whether
    // maxVer here is this frame's or last frame's. Either way it is the array constant 22 gets;
    // the skew is at most one frame and cannot manufacture or hide a sustained change.
    // ⭐ THE CONTROL for "you measured the wrong array" is the low-rate [deform-upload] line at
    // the constant-22 upload site itself; the two maxVer values must agree.
    //
    // Emission rule: print when (calls % period == 0) OR when either measured quantity has
    // MOVED since this car's last printed line -- so flat stretches stay cheap and every change
    // is captured at full resolution.
    // DELETE-WHEN the crash-deformation question is closed and banked.
    // =========================================================================================
    {
        static s32 siTracePeriod = -1;
        if (siTracePeriod < 0)
        {
            const char* lpcEnv = getenv("BRN_DEFORM_TRACE");
            siTracePeriod = (lpcEnv != 0) ? atoi(lpcEnv) : 0;
            if (siTracePeriod < 0) { siTracePeriod = 0; }
        }

        if (siTracePeriod > 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            const f32 lfDispSq = lpCarState->GetSummedDisplacementSquared();

            // max |xyz| over the 128 verlet rows -- the uploaded vertex offset magnitude.
            // (w is the scratch AMOUNT lane, not a position: DEBUG_OverrideScratchAmount owns
            // it, and constant 22's consumer offsets a vertex by xyz. Including w here would
            // report the debug lane as if it were a dent.)
            // ⛔⛔ A MAX ALONE IS THE WRONG STATISTIC HERE, and reading it as "the mesh did not
            // move" is a false negative waiting to happen: the junkyard preset pins ONE row
            // (row 41) at a value the running dents never exceed, so max stays flat while any
            // number of other rows change underneath it. The SUM and the non-zero ROW COUNT are
            // what actually move when the mesh moves; max is kept only to name the worst row.
            // (2026-08-27: caught mid-campaign, before the flat max was reported as an answer.)
            const Vector3Plus* lpVerlet = mRenderParams.GetVerletOffsets();
            f32 lfMaxVerlet = 0.0f;
            f32 lfSumVerlet = 0.0f;
            s32 liMaxVerletRow = -1;
            s32 liNonZeroRows  = 0;
            for (u32 luRow = 0; luRow < 128u; ++luRow)
            {
                const f32 lfRow = std::fabs(lpVerlet[luRow].x)
                                + std::fabs(lpVerlet[luRow].y)
                                + std::fabs(lpVerlet[luRow].z);
                lfSumVerlet += lfRow;
                if (lfRow > 1.0e-6f) { ++liNonZeroRows; }
                if (lfRow > lfMaxVerlet) { lfMaxVerlet = lfRow; liMaxVerletRow = static_cast<s32>(luRow); }
            }

            // max |displacement| over the live sensors, and which sensor carries it.
            f32 lfMaxSensor = 0.0f;
            s32 liMaxSensor = -1;
            const u32 luNumSensors = static_cast<u32>(lpCarState->mu8NumSensors);
            const u32 luScan = (luNumSensors < BrnPhysics::Deformation::CarState::KU_MAX_SENSORS)
                             ? luNumSensors : BrnPhysics::Deformation::CarState::KU_MAX_SENSORS;
            for (u32 luSensor = 0; luSensor < luScan; ++luSensor)
            {
                const f32 lfMag = std::fabs(lpCarState->maSensors[luSensor].mDisplacement.x)
                                + std::fabs(lpCarState->maSensors[luSensor].mDisplacement.y)
                                + std::fabs(lpCarState->maSensors[luSensor].mDisplacement.z);
                if (lfMag > lfMaxSensor) { lfMaxSensor = lfMag; liMaxSensor = static_cast<s32>(luSensor); }
            }

            // Per-car "did it move" memory. Eight slots, keyed by entity id; a car whose id is
            // not resident evicts the least-recently-seen slot. Purely diagnostic storage --
            // NOT a member, so no attested ActiveRaceCar/RenderParams offset is disturbed.
            static u32 sauTraceIds[8]   = { 0, 0, 0, 0, 0, 0, 0, 0 };
            static f32 safTraceDisp[8]  = { 0, 0, 0, 0, 0, 0, 0, 0 };
            static f32 safTraceVer[8]   = { 0, 0, 0, 0, 0, 0, 0, 0 };
            static bool sabTraceUsed[8] = { false, false, false, false, false, false, false, false };
            static u32 sluTraceNext     = 0;
            static u32 sluTraceCalls    = 0;

            ++sluTraceCalls;

            const u32 luId = mPhysicsState.mEntityId.muValue;
            s32 liSlot = -1;
            for (u32 luS = 0; luS < 8u; ++luS)
            {
                if (sabTraceUsed[luS] && sauTraceIds[luS] == luId) { liSlot = static_cast<s32>(luS); break; }
            }
            if (liSlot < 0)
            {
                liSlot = static_cast<s32>(sluTraceNext % 8u);
                ++sluTraceNext;
                sauTraceIds[liSlot]  = luId;
                sabTraceUsed[liSlot] = true;
                safTraceDisp[liSlot] = -1.0f;   // force a first print for a newly seen car
                safTraceVer[liSlot]  = -1.0f;
            }

            // ⚠️ CHANGE-DETECTION IS PLAYER-ONLY, AND THAT IS A MEASUREMENT DECISION, not tidiness.
            // maVerletOffsets moves EVERY frame on every car even when nothing is dented -- the
            // suspension leg (DeformableObject::UpdateIKSuspensionOffsets) writes the four wheel
            // tag rows from live suspension compression. So "changed" is true ~always, and with
            // eight cars that is ~480 log lines a second. This build is FRAME-COUPLED (above 60 fps
            // the game speeds up), so an instrument heavy enough to move the frame rate changes the
            // very sim it is measuring. Player car gets full resolution; the rest get the period.
            const bool lbIsPlayer = IsPlayer();
            const bool lbMoved = lbIsPlayer
                              && ( (std::fabs(lfDispSq    - safTraceDisp[liSlot]) > 1.0e-6f)
                                || (std::fabs(lfSumVerlet - safTraceVer[liSlot])  > 1.0e-6f) );
            const bool lbPeriodic = ((sluTraceCalls % static_cast<u32>(siTracePeriod)) == 0u);

            if (lbMoved || lbPeriodic)
            {
                safTraceDisp[liSlot] = lfDispSq;
                safTraceVer[liSlot]  = lfSumVerlet;

                *CgsDev::Log::gpDebugPrint
                    << "[deform-trace] call " << static_cast<s32>(sluTraceCalls)
                    << " present " << static_cast<s32>(renderengine::guPresentCount)
                    << " ent " << luId
                    << " player " << (lbIsPlayer ? 1 : 0)
                    << " crashing " << (IsCrashing() ? 1 : 0)
                    << " wrecked " << (IsWrecked() ? 1 : 0)
                    << " dispSq " << lfDispSq
                    << " maxSensor " << lfMaxSensor << " @" << liMaxSensor
                    << " nSensors " << static_cast<s32>(luNumSensors)
                    << " sumVerlet " << lfSumVerlet
                    << " nnzVerlet " << liNonZeroRows
                    << " maxVerlet " << lfMaxVerlet << " @" << liMaxVerletRow
                    << "\n";
            }
        }
    }

    // Oriented-box lowest world-space Y (the vandc-ABS + three vsubfp splat chain).
    {
        const Matrix44Affine& lrT = mPhysicsState.mTransform;
        const Vector3&        lrE = mPhysicsState.mHalfExtent;
        const f32 lfLowestY = lrT.wAxis.y
                            - std::fabs(lrT.xAxis.y * lrE.x)
                            - std::fabs(lrT.yAxis.y * lrE.y)
                            - std::fabs(lrT.zAxis.y * lrE.z);
        mvfLowestPointWorldSpace = VecFloat{ lfLowestY, lfLowestY, lfLowestY, lfLowestY };
    }

    // The deformed-bbox pair (carState+0x640, 32 bytes -- four ld/std on console).
    mDeformedBBox.mMin = Vector4{ lpCarState->mDeformedBBoxMin.x, lpCarState->mDeformedBBoxMin.y,
                                  lpCarState->mDeformedBBoxMin.z, lpCarState->mDeformedBBoxMin.w };
    mDeformedBBox.mMax = Vector4{ lpCarState->mDeformedBBoxMax.x, lpCarState->mDeformedBBoxMax.y,
                                  lpCarState->mDeformedBBoxMax.z, lpCarState->mDeformedBBoxMax.w };

    // The four wheel tag points -> the render-side axle rows.
    for (u32 luWheel = 0; luWheel < 4u; ++luWheel)
    {
        CGS_ASSERT(luWheel < 4u,
                   "luWheelIndex < (int32_t)BrnPhysics::Vehicle::eNumDrivenWheels");
        mRenderParams.SetAxlePosition(luWheel, lpCarState->GetWheelTagPoint(luWheel));
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

    // [DIAG wheel-blank regression, 2026-08-25] NOT IN THE X360 BINARY. Env-gated
    // (BRN_WHEEL_DIAG=1) witness of what the deformation L3 slot DELIVERS before it
    // overwrites the vehicle-stats wheel pose: one line per call every 60th call.
    // DELETE-WHEN the wheel regression is pinned and fixed.
    {
        static const bool sbWheelDiag = (getenv("BRN_WHEEL_DIAG") != 0);
        static s32 siL3Frame = 0;
        if (sbWheelDiag && ((siL3Frame++ % 60) == 0) && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[wheel-diag] L3 f" << siL3Frame - 1
                << " w0 T (" << lpSnapshot->maWheels[0].mTransform.wAxis.x
                << ", " << lpSnapshot->maWheels[0].mTransform.wAxis.y
                << ", " << lpSnapshot->maWheels[0].mTransform.wAxis.z
                << ") row0x " << lpSnapshot->maWheels[0].mTransform.xAxis.x
                << " onGround (" << static_cast<s32>(lpSnapshot->mau8OnGround[0])
                << "," << static_cast<s32>(lpSnapshot->mau8OnGround[1])
                << "," << static_cast<s32>(lpSnapshot->mau8OnGround[2])
                << "," << static_cast<s32>(lpSnapshot->mau8OnGround[3]) << ")\n";
        }
    }

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

    // [DIAG] NOT IN THE X360 BINARY. The IGNITION-TICK rung, paired with the [ignition] attach
    // line in BrnRaceCarEntityModule.cpp. It reports the state machine's own inputs on the FIRST
    // tick and on every state change thereafter, so a car whose engine never leaves OFF names the
    // arm that held it there instead of leaving it to be inferred from a missing event
    // [[diagnostics-that-lie]]. Delete with the rest of the bring-up diagnostics.
    {
        static s32 siLastReported = -1;
        static s32 siTicks        = 0;
        const s32  liState        = static_cast<s32>( meEngineState );
        if( ( liState != siLastReported || siTicks < 3 ) && CgsDev::Log::gpDebugPrint != 0 )
        {
            *CgsDev::Log::gpDebugPrint
                << "[ignition] tick state=" << liState
                << " active=" << ( IsActive() ? 1 : 0 )
                << " enableSwitchOff=" << ( mbEnableEngineSwitchOff ? 1 : 0 )
                << " carInGameMode=" << ( mbIsInGameMode ? 1 : 0 )
                << " inCarSelect=" << ( lbInCarSelectScreen ? 1 : 0 )
                << " crashing=" << ( IsCrashing() ? 1 : 0 )
                << " accel=" << lfAcceleration
                << " brake=" << lfBraking << "\n";
        }
        siLastReported = liState;
        ++siTicks;
    }

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
        UpdateEngineState( lfTimeStep, lfAcceleration, lfBraking,
                           lbIsInOnlineGameMode, lbInCarSelectScreen );
    }

    mbAIToBeActivated = false;                           // +0x781 (0x822F7E54)

    // 0x822F7EBC..0x822F7ED0.
    if( mbCrashedIntoWater )                             // +0x783
    {
        mfTimeInWater = mfTimeInWater + lfTimeStep;      // +0x784
    }
}


// =================================================================================================
// THE CRASH-EXIT SET   (crash exit wave, 2026-08-25)
//
// These four are the consumer end of the crash module's RaceCarCrashCompleteEvent: once the crash
// module says a wreck is finished, RaceCarEntityModule::ProcessRaceCarCrashCompleteEvents lands
// here (or on RaceCar::RequestResetOnTrack) and the car becomes drivable again.
// =================================================================================================

// -------------------------------------------------------------------------------------------------
// IsDriveableAfterCrash @ 0x822D48F8   (88 insns)
//
//   0x822D4910  CGS_ASSERT(IsAttached())                       BrnActiveRaceCar.h:1089
//   0x822D4930  lwz r, 0x6F0(this) ; lbz 0xA4(raceCar)         mpRaceCar->GetType(), asserted < 4
//   0x822D4954  if (type != 0 /*E_RACE_CAR_TYPE_PLAYER*/) return false
//   0x822D4960  if (IsWrecked()) return false
//   0x822D4978  the VMX block: build the unit Y axis (0,1,0,0) on the stack, take
//               GetTransform()'s row at +0x10 (the car's own UP axis), vmsum3fp128 the two -- a
//               3-component DOT -- and compare it against a splat of the scalar at v14[0], which
//               the code has just set to 0.0f.  ⇒ `if (0.0f > dot(carUp, worldUp)) return false`
//               i.e. the car is upside down (or past 90 degrees).
//   0x822D49E0  if (!mPhysicsState.mbIsFrontRayOccluded) return true
//   0x822D49F0  if (!mbIsInGameMode) return true
//               return false
//
// ⭐ THE LAST TWO LINES ARE THE WHOLE POINT OF THE PREDICATE and they are easy to misread as one
// test. They are two separate early-outs: a car whose front ray is CLEAR is always driveable, and
// even a blocked car is driveable when we are NOT in a game mode -- which is free burn, i.e. this
// build. So on the free-burn path this reduces to "an upright, unwrecked PLAYER car can always
// drive away from its own crash", which is exactly the behaviour the game is famous for.
// -------------------------------------------------------------------------------------------------
bool ActiveRaceCar::IsDriveableAfterCrash() const
{
    CGS_ASSERT( IsAttached(), "IsAttached()" );   // BrnActiveRaceCar.h:1089

    CGS_ASSERT( mpRaceCar->GetType() < E_RACE_CAR_TYPE_COUNT, "muType < E_RACE_CAR_TYPE_COUNT" );
    if( mpRaceCar->GetType() != E_RACE_CAR_TYPE_PLAYER )
    {
        return false;
    }

    if( IsWrecked() )
    {
        return false;
    }

    // 0x822D4978..0x822D49DC -- dot(the car's up axis, world up) must not be below zero.
    // The console builds the world-up constant on the stack as (0,1,0,0) and splats the 0.0f
    // comparand from the adjacent stack slot; both are read from the image, not assumed.
    const Matrix44Affine lTransform = GetTransform();
    const Vector3 lWorldUp = { 0.0f, 1.0f, 0.0f, 0.0f };
    if( 0.0f > rw::math::vpu::Dot( lTransform.Up(), lWorldUp ) )
    {
        return false;
    }

    // 0x822D49E0 / 0x822D49F0 -- two independent early-outs. See the banner.
    if( !mPhysicsState.mbIsFrontRayOccluded )
    {
        return true;
    }
    if( !mbIsInGameMode )
    {
        return true;
    }
    return false;
}

// -------------------------------------------------------------------------------------------------
// IsDeformationFixedAfterCrash @ 0x822BFED8   (52 insns)
//
// The same family as IsDriveableAfterCrash MINUS the orientation test, and with every arm's
// polarity flipped: a non-player type, a wrecked car, a clear front ray and "not in a game mode"
// all return TRUE, and only "front ray occluded AND in a game mode" returns false.
// -------------------------------------------------------------------------------------------------
bool ActiveRaceCar::IsDeformationFixedAfterCrash() const
{
    CGS_ASSERT( IsAttached(), "IsAttached()" );   // BrnActiveRaceCar.h:1089

    CGS_ASSERT( mpRaceCar->GetType() < E_RACE_CAR_TYPE_COUNT, "muType < E_RACE_CAR_TYPE_COUNT" );
    if( mpRaceCar->GetType() != E_RACE_CAR_TYPE_PLAYER )
    {
        return true;
    }

    if( IsWrecked() )
    {
        return true;
    }

    if( !mPhysicsState.mbIsFrontRayOccluded )
    {
        return true;
    }
    if( !mbIsInGameMode )
    {
        return true;
    }
    return false;
}

// -------------------------------------------------------------------------------------------------
// ResetVerletOffsets @ 0x822A4E90   (48 insns)
//
//   0x822A4EB4  CGS_ASSERT(!IsInactive())   BrnActiveRaceCar.cpp:1128
//   the loop stores a zero VMX register into 128 consecutive 16-byte slots at this+0x820, which is
//   mRenderParams (+0x7E0) + 0x40 == RenderParams::maVerletOffsets, carrying the
//   "luPartIndex < KI_MAX_RACE_CAR_VERLET_POINTS" tripwire (BrnActiveRaceCar.h:1866) from the
//   inlined element accessor.
// -------------------------------------------------------------------------------------------------
void ActiveRaceCar::ResetVerletOffsets()
{
    CGS_ASSERT( !IsInactive(), "!IsInactive()" );   // BrnActiveRaceCar.cpp:1128

    Vector3Plus* lpVerletOffsets = mRenderParams.GetVerletOffsets();
    const Vector3Plus lZero = { 0.0f, 0.0f, 0.0f, 0.0f };
    for( u32 luPartIndex = 0; luPartIndex < KU_MAX_RACE_CAR_VERLET_POINTS; ++luPartIndex )
    {
        CGS_ASSERT( luPartIndex < KU_MAX_RACE_CAR_VERLET_POINTS,
                    "luPartIndex < (uint32_t)KI_MAX_RACE_CAR_VERLET_POINTS" );   // h:1866
        lpVerletOffsets[luPartIndex] = lZero;
    }
}

// -------------------------------------------------------------------------------------------------
// ResetAfterCrash @ 0x822BF3A0   (77 insns)
//
//   0x822BF3B8  CGS_ASSERT(IsAttached())   BrnActiveRaceCar.cpp:1048
//   0x822BF3DC  if (IsActive()) {
//     0x822BF3E8    if (!lbKeepVerletOffsets) ResetVerletOffsets();
//     0x822BF444    mRenderParams.SetDamaged(mpRaceCar->ToBeRenderedDamaged());   (stb 0x1BE4)
//     0x822BF480    if (mPhysicsState.mbCrashing) mbUncrashedThisFrame = true;    (0x52A -> 0x77A)
//     0x822BF494    mfInvulnerablityTime = 2.0f      (flt_82014984, READ FROM THE IMAGE)
//     0x822BF4A4    std -1, 0x1580 ; std -1, 0x1588  -- BOTH u64 fields of
//                   mRenderParams.mBodyPartVisibility  ⇒ EVERY BODY PART VISIBLE AGAIN
//   }
//   ...and unconditionally (outside the IsActive arm):
//     0x822BF4B8    mfTimeInWater          = 0.0f    (flt_82001CC0)
//     0x822BF4BC    mbIsWrecked            = false
//     0x822BF4C0    mfTimeDriveableInCrash = 0.0f
//                   mbCrashedIntoWater     = false
//                   mbIsInShowtime         = false
//
// ⭐ THE BODY-PART RESTORE IS THE VISIBLE HALF OF "THE CRASH IS OVER": mBodyPartVisibility is the
// mask the deformation renderer uses to hide parts that flew off. RenderParams::Reset seeds it
// with the literal 0xB80FFFFFFFF per field; this path stores ~0 into both, which is BitArray's
// own SetAll(). Written by name -- the console's `std -1` pair is a whole-word idiom, not a
// per-bit one, and BitArray already spells that exact shape.
//
// ⚠️ mbUncrashedThisFrame is set only when the car was STILL FLAGGED CRASHING at the moment the
// reset landed. That is the one-frame edge the rest of the entity module keys "the player just
// stopped crashing" off, so it must not be hoisted out of the IsActive() arm.
// -------------------------------------------------------------------------------------------------
void ActiveRaceCar::ResetAfterCrash( bool lbKeepVerletOffsets )
{
    CGS_ASSERT( IsAttached(), "IsAttached()" );   // BrnActiveRaceCar.cpp:1048

    if( IsActive() )
    {
        if( !lbKeepVerletOffsets )
        {
            ResetVerletOffsets();
        }

        CGS_ASSERT( IsAttached(), "IsAttached()" );   // h:1089
        mRenderParams.SetDamaged( mpRaceCar->ToBeRenderedDamaged() );

        CGS_ASSERT( IsAttached(), "IsAttached()" );   // h:1418
        if( mPhysicsState.mbCrashing )
        {
            mbUncrashedThisFrame = true;
        }

        mfInvulnerablityTime = 2.0f;                    // flt_82014984
        // std -1 x2 @0x1580/0x1588 -- both mBodyPartVisibility words to all-ones. Spelled with
        // the same accessor ActiveRaceCar::Attach already uses for the identical console idiom.
        mRenderParams.MakeAllPartsVisible();
    }

    mfTimeInWater          = 0.0f;   // flt_82001CC0
    mbIsWrecked            = false;
    mfTimeDriveableInCrash = 0.0f;
    mbCrashedIntoWater     = false;
    mbIsInShowtime         = false;
}

// =================================================================================================
// THE RESET-ON-TRACK RING  (aicar_reset wave 2026-08-26)
// =================================================================================================
// Three console functions -- SetAISection, UpdateResetTransform, GetResetCoords -- that between
// them answer "where does a crashed car go back". See the block banner on their declarations in
// BrnActiveRaceCar.h for the four-banner claim they retire.
//
// ⭐ THE SURFACE COLLISION TAG IS THE AI SECTION INDEX. Nothing here queries the AI road network:
// BrnWorld::CollisionTag is {u16 mu16GroupTag; u16 mu16MaterialTag} and GetAISectionIndex() is
// `mu16GroupTag & KU_MAX_AI_SECTION_INDEX`. The section index is baked into the world's own
// collision surfaces and arrives with every above-ground ray hit.
//
// ⚠️ TYPE FORK, PRE-EXISTING AND FLAGGED WHERE IT LIVES (BrnVehicleManager_TractionLineTests.cpp
// :317): Wheel::RoadContact::mCollisionTag and AboveGroundTestResult::mCollisionTag are the
// ONE-FIELD PLACEHOLDER `::CollisionTag { u32 muValue; }` (BrnCommonTypes.h:29), not
// BrnWorld::CollisionTag, so the two field reads below are spelled as shifts on muValue -- the
// same idiom VehiclePhysics::GetSurfaceLinearDrag and the traction-line harvest already use, and
// the same halves: Wheel::AddTractionPoint packs the console's +0x26 halfword (the MATERIAL tag,
// which carries the DRIVEABLE flag) into the LOW 16 bits and the +0x24 halfword (the GROUP tag,
// which carries the section index) into the HIGH 16.
// =================================================================================================

namespace
{
    // BrnWorld::CollisionTag::IsDrivable() on the placeholder tag type. The console tests
    // `lhz rN, 0x26(contact) ; srwi rN, rN, 13 ; clrlwi rN, rN, 31` -- bit 13 of the material
    // halfword, which is BrnWorld::KU_COLLISION_FLAG_DRIVEABLE (8192).
    // ⚠️ `::CollisionTag`, EXPLICITLY GLOBAL. Unqualified `CollisionTag` inside namespace
    // BrnWorld binds to BrnWorld::CollisionTag (the REAL two-halfword type), which is NOT what
    // RoadContact carries -- and the mistake would compile if the two ever gained a converting
    // constructor. [[shadowing redeclarations]] in miniature.
    inline bool TagIsDrivable( const ::CollisionTag& lrTag )
    {
        return ( lrTag.muValue & BrnWorld::KU_COLLISION_FLAG_DRIVEABLE ) != 0;
    }

    // The console's own 15 m gate, as the SQUARED literal it bakes (flt_82018E3C == 225.0f).
    const f32 KF_RESET_TRANSFORM_MIN_DISTANCE_SQUARED = 225.0f;
}

// -------------------------------------------------------------------------------------------------
// SetAISection @0x822A51F0
//
//   0x822A51F0  bl IsActive ; assert "IsActive()"                    (BrnActiveRaceCar.cpp:1911)
//   if (index == 0x7FFF) { muPrevAISection = muCurrAISection;        (sth 0x73E -> 0x73C)
//                          mbInsideAISectionSystem = false;          (stb 0, 0x771)
//                          muCurrAISection = 0x7FFF; return; }
//   if (!mbInsideAISectionSystem)      { muCurrAISection = index; muPrevAISection = 0x7FFF; }
//   else if (index != muCurrAISection) { muPrevAISection = muCurrAISection; muCurrAISection = index; }
//   mbInsideAISectionSystem = true;
//
// ⭐ THE ENTER/LEAVE ASYMMETRY IS THE CONSOLE'S, NOT A SIMPLIFICATION. Entering the system from
// outside stamps muPrevAISection with the INVALID sentinel rather than with whatever stale section
// the car left the system at -- so "which section did I come from" is only ever answered from a
// contiguous run inside the system. Reproduced verbatim.
// -------------------------------------------------------------------------------------------------
void ActiveRaceCar::SetAISection( u16 lu16AISectionIndex )
{
    CGS_ASSERT( IsActive(), "IsActive()" );   // BrnActiveRaceCar.cpp:1911

    if( lu16AISectionIndex == BrnWorld::KI_INVALID_SECTION_INDEX )
    {
        muPrevAISection         = muCurrAISection;
        mbInsideAISectionSystem = false;
        muCurrAISection         = BrnWorld::KI_INVALID_SECTION_INDEX;
        return;
    }

    if( !mbInsideAISectionSystem )
    {
        muCurrAISection = lu16AISectionIndex;
        muPrevAISection = BrnWorld::KI_INVALID_SECTION_INDEX;
    }
    else if( lu16AISectionIndex != muCurrAISection )
    {
        muPrevAISection = muCurrAISection;
        muCurrAISection = lu16AISectionIndex;
    }

    mbInsideAISectionSystem = true;
}

// -------------------------------------------------------------------------------------------------
// UpdateResetTransform @0x822BF8D0   (the ONLY writer of mPrevTransforms in the image)
//
//   0x822BF8E4  bl IsActive ; if (!active) return                    (no assert on this one)
//   0x822BF900  r11 = this + 0x106 == &mPhysicsState.maWheels[0].mRoadContact.mCollisionTag + 2
//   0x822BF904  loop x4, stride 0x70 == sizeof(WheelLite):
//                 lbz  2(r11)   -> mRoadContact.mbIsOnGround      ; 0 -> break, flag = false
//                 lhz  0(r11)   -> mRoadContact.mCollisionTag lo  ; bit 13 clear -> break, false
//   0x822BF948  if (flag && !mPhysicsState.mbCrashing(0x52A))  -> record
//   0x822BF954  else if (mbIsInShowtime(0x788)
//                        && mPhysicsState.mAboveGroundTestResult.mbValid(0x2C8)
//                        && that result's tag(0x2C6) is drivable) -> record
//               else return
//   0x822BF984  record: if (mPhysicsState.mfTimeInAir(0x4E4) != 0.0f) return
//   0x822BF994          if (muCurrAISection(0x73E) == 0x7FFF)     return
//   0x822BF9A0          if (mPrevTransforms.miLength(0x5A0) != 0
//                           && MagnitudeSquared(mLastRecordedPosition(0x6B0)
//                                               - mPhysicsState.mTransform.wAxis(0x300)) <= 225.0f)
//                        return
//   0x822BFA04          mLastRecordedPosition = mPhysicsState.mTransform.wAxis
//   0x822BFA14          mPrevTransforms.Push(mPhysicsState.mTransform)     (this+0x2D0)
//
// ⭐ THE 15 m GATE IS WHAT MAKES A FOUR-DEEP RING USEFUL. GetResetCoords reads the OLDEST live
// entry, so the ring is a "roughly 45-60 m back along the road I actually drove" memory, not the
// last four frames. Recording every frame would make the reset a no-op teleport.
// ⭐ `mfTimeInAir == 0.0f` is an exact float compare in the asm (`fcmpu` against flt_82001CC0 ==
// 0.0f), not a tolerance. Kept exact.
// ⚠️ The second arm (SHOWTIME) uses the single above-ground ray instead of the four wheels,
// because a car in showtime is tumbling and its wheels are not on anything.
//
// ⛔⛔ MEASURED THE DAY IT LANDED: THIS FUNCTION RECORDS NOTHING YET, AND THE REASON IS ONE RUNG
// UP, NOT HERE. `muCurrAISection` is written by exactly one thing -- RaceCarEntityModule::
// UpdateRaceCarCollisionTagging, landed alongside this function -- and that reads
// mPhysicsState.mAboveGroundTestResult, whose PRODUCER (VehicleManager::
// GenerateAboveGroundLineTests @0x82633990, the only thing in the image that posts an
// InEventLineTestNearest for a race car) is ABSENT from this tree. A booted drive run says so
// outright:
//     [collision-tag] car 0 aboveGroundValid=0 tag=0x-32768 ...   (0xFFFF8000, the CLEAR value)
//     [rot-ring] player depth=0 aiSection=32767 inSystem=0
// so this function's `muCurrAISection == KI_INVALID_SECTION_INDEX` gate refuses every frame.
// ⭐ THAT IS NOT A REASON TO WITHHOLD IT. The wheel contacts ARE real on the same run
// (wheel0OnGround=1, wheel0Tag drivable), the function sits at the console's own call site, and
// its consumer GetResetCoords has a SECOND arm that works today. The moment the above-ground
// round trip lands, the ring fills with no further change here.
// DELETE-WHEN [rot-ring] reports a non-zero depth on a drive run.
// -------------------------------------------------------------------------------------------------
void ActiveRaceCar::UpdateResetTransform()
{
    if( !IsActive() )
    {
        return;
    }

    bool lbAllWheelsOnDrivableRoad = true;
    for( s32 liWheel = 0; liWheel < 4; ++liWheel )
    {
        const BrnPhysics::Vehicle::Wheel::RoadContact& lrContact =
            mPhysicsState.maWheels[liWheel].mRoadContact;

        if( !lrContact.mbIsOnGround || !TagIsDrivable( lrContact.mCollisionTag ) )
        {
            lbAllWheelsOnDrivableRoad = false;
            break;
        }
    }

    const bool lbOnRoad = ( lbAllWheelsOnDrivableRoad && !mPhysicsState.mbCrashing );
    const bool lbShowtimeOnRoad =
        ( mbIsInShowtime
          && mPhysicsState.mAboveGroundTestResult.mbValid
          && TagIsDrivable( mPhysicsState.mAboveGroundTestResult.mCollisionTag ) );

    if( !lbOnRoad && !lbShowtimeOnRoad )
    {
        return;
    }

    if( mPhysicsState.mfTimeInAir != 0.0f )
    {
        return;
    }

    if( muCurrAISection == BrnWorld::KI_INVALID_SECTION_INDEX )
    {
        return;
    }

    const Vector3 lPosition = mPhysicsState.mTransform.wAxis;

    if( mPrevTransforms.GetLength() != 0 )
    {
        const Vector3 lDelta = { mLastRecordedPosition.x - lPosition.x,
                                 mLastRecordedPosition.y - lPosition.y,
                                 mLastRecordedPosition.z - lPosition.z,
                                 0.0f };
        // `vmsum3fp128 v0, v0, v0` -- the THREE-lane squared magnitude (not the XZ one).
        const f32 lfDistanceSquared =
            lDelta.x * lDelta.x + lDelta.y * lDelta.y + lDelta.z * lDelta.z;

        if( lfDistanceSquared <= KF_RESET_TRANSFORM_MIN_DISTANCE_SQUARED )
        {
            return;
        }
    }

    mLastRecordedPosition = lPosition;
    mPrevTransforms.Push( &mPhysicsState.mTransform );
}

// -------------------------------------------------------------------------------------------------
// GetResetCoords @0x822BF2D0
//
//   0x822BF2E8  bl IsAttached ; assert "IsAttached()"                 (BrnActiveRaceCar.cpp:1022)
//   0x822BF318  if (mPrevTransforms.miLength(0x5A0) > 0)
//   0x822BF33C     r11 = mpData(0x590) + miReadPos(0x598) * 64        == mPrevTransforms[0]
//   0x822BF358     position  = r11 + 0x30   (wAxis)   direction = r11 + 0x20  (zAxis)
//   0x822BF37C  else
//   0x822BF384     position  = this + 0x300 (mPhysicsState.mTransform.wAxis)
//   0x822BF38C     direction = this + 0x2F0 (mPhysicsState.mTransform.zAxis)
//
// ⚠️ THE TWO ARMS SWAP THE OUT REGISTERS AND IT IS NOT A BUG. In the ring arm the console loads
// wAxis into v13 and zAxis into v0, then stores `v13 -> r30` and `v0 -> r29`; in the fallback arm
// it loads +0x300 and stores it straight to r30, +0x2F0 to r29. Both arms therefore put the
// POSITION in the first out-parameter and the DIRECTION in the second. Read the register dance,
// not the register names.
// ⭐ `mPrevTransforms[0]` is mpData[miReadPos] -- the OLDEST live entry, i.e. the furthest-back
// recorded transform, not the most recent one.
// -------------------------------------------------------------------------------------------------
void ActiveRaceCar::GetResetCoords( Vector3* lpOutPosition, Vector3* lpOutDirection ) const
{
    CGS_ASSERT( IsAttached(), "IsAttached()" );   // BrnActiveRaceCar.cpp:1022

    if( mPrevTransforms.GetLength() > 0 )
    {
        const Matrix44Affine& lrTransform = mPrevTransforms[0u];
        *lpOutPosition  = lrTransform.wAxis;
        *lpOutDirection = lrTransform.zAxis;
        return;
    }

    *lpOutPosition  = mPhysicsState.mTransform.wAxis;
    *lpOutDirection = mPhysicsState.mTransform.zAxis;
}

}
