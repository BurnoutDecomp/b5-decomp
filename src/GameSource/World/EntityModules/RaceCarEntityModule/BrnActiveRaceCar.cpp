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
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleIOQueues.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "rw/math/vpu/matrix44affine_operation.h"    // rw::math::vpu::Mult
#include "rw/math/vpu/vector3_operation.h"        // rw::math::vpu::TransformVector's operands (operator+ / operator*)
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

// includes folded in from the BrnActiveRaceCar_w*.cpp partfiles (2026-09-15)
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h" // StreamedDeformationSpec::mHandlingBodyDimensions
#include "vendor/renderware/collision/CollisionVolume.hpp"                      // rw::collision::BoxVolume
#include "rw/rwcore_structs.h"                                                 // rw::Resource (BoxVolume::Initialize's first argument)
#include "rw/physics/rigidbody.h"                                              // rw::physics::ACTIVE_BODY
#include "GameSource/AttribSys/Generated/classes/burnoutcarasset.h"            // OnResourcesLoaded's colour leg (0x822EB474)
#include "GameSource/AttribSys/Generated/classes/burnoutcargraphicsasset.h"    // PlayerColourIndex / PlayerColourPaletteIndex

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

    // This TU's spelling of BrnPhysics::Def @0x822C7708 (ResourcePtr<StreamedDeformationSpec>::
    // operator-> over the car's +0x1C90 wrapper), resolved through the handle the wrapper keeps
    // at +0x14. Defined with AddToScene's helpers below (see its banner); declared here for
    // OnResourcesLoaded's Def() read (0x822EB210).
    const BrnPhysics::Deformation::StreamedDeformationSpec*
    ResolveDeformationSpec(const CgsResource::ResourceHandle& lrHandle);
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
    ResetRenderPoseInterpolation();

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
    ResetRenderPoseInterpolation();
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
// Update_PreScene @ 0x822EAE08   (215 insns)   -- crash parity 2026-09-23 (G61-D2 / G61-D3)
//
// Its only caller is RaceCarEntityModule::UpdateRaceCars_PreScene @0x822F5578 (every slot, every
// un-paused PreScene). Until this landed NEITHER function had a body in the tree, so on PC:
//   * mbCrashedIntoWater (+0x783) was never cleared per frame. CheckForResetOnTrackConditions sets
//     it while the car stands on water and ActiveRaceCar::Update adds dt to mfTimeInWater while it
//     is set -- so after the first water contact that did not reset the car, mfTimeInWater kept
//     growing on dry land, and the next water contact was instantly over KF_MAX_TIME_IN_WATER
//     (1.0 s) and reset the car on the spot. The console clears it here, on every path.
//   * the meOnlineState machine (online connect / lost contact / disconnect / car-select collision
//     toggles) never ran -- inert offline, where every car is E_ONLINE_STATE_NORMAL with all four
//     network flags false, so the NORMAL arm falls straight through to the tail.
//
// The switch, arm for arm (r29 = lpSceneInterface, r28 = lpVehicleInterface, r27 = 0):
//   0 CONNECTING  0x822EAE58  lbz 0x79C ; stb 0, 0x79D ; (0x79C && IsActive()) ->
//                 stw 1, 0x744 ; stb 1, 0x79D ; lbz 0x79A ? <log> : AddToCollision
//   1 NORMAL      0x822EAEF0  IsActive() ; lbz 0x799 ? (!IsCrashing() -> RemoveFromCollision,
//                 state 3) : (lbz 0x798 && !IsCrashing() -> RemoveFromCollision, state 2) ;
//                 lbz 0x79B -> assert muType == 2 (:490), then the car-select toggle, stb 0, 0x79B
//   2 LOST        0x822EB034  lbz 0x798 == 0 -> (lbz 0x79A == 0 -> AddToCollision) ; stb 1, 0x79D ;
//                 stw 1, 0x744 | lbz 0x799 -> (IsActive() && !IsCrashing() -> RemoveFromCollision,
//                 state 3) | else the 30/60-frame flash counter on +0x794 / +0x79D
//   3 DISCONNECTED 0x822EB0F0 stb 0, 0x79D
// ----------------------------------------------------------------------------
void ActiveRaceCar::Update_PreScene( CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface,
                                     BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInterface,
                                     BrnAI::AIModuleIO::RaceCarAIInterface* lpRaceCarAIInterface )
{
    (void)lpRaceCarAIInterface;   // r6: passed by UpdateRaceCars_PreScene, never read

    switch( meOnlineState )                                                 // lwz 0x744
    {
    case E_ONLINE_STATE_CONNECTING:
    {
        const bool lbReceivedNetworkDriverControls = mbReceivedNetworkDriverControls;   // lbz 0x79C
        mbRenderThisFrame = false;                                          // stb r27, 0x79D
        if( lbReceivedNetworkDriverControls && IsActive() )
        {
            meOnlineState     = E_ONLINE_STATE_NORMAL;                      // stw 1, 0x744
            mbRenderThisFrame = true;                                       // stb 1, 0x79D
            if( !mbIsInCarSelectOnline )                                    // lbz 0x79A
            {
                AddToCollision( lpSceneInterface, lpVehicleInterface );
            }
            else if( ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0 && CgsDev::Log::gpDebugPrint != 0 )
            {
                *CgsDev::Log::gpDebugPrint
                    << "\n[ACTIVE RACE CAR]: Not adding active race car "
                    << static_cast<s32>( meActiveRaceCarIndex )             // lwz 0x748
                    << " for collision on online connect as it is in a junkyard\n";
            }
        }
        break;
    }

    case E_ONLINE_STATE_NORMAL:
        if( !IsActive() )
        {
            break;
        }
        if( mbIsDisconnectedFromNetwork )                                   // lbz 0x799
        {
            if( !IsCrashing() )
            {
                RemoveFromCollision( lpSceneInterface, lpVehicleInterface );
                meOnlineState = E_ONLINE_STATE_DISCONNECTED;                // li 3
            }
        }
        else if( mbNotSendingNetworkUpdates && !IsCrashing() )             // lbz 0x798
        {
            RemoveFromCollision( lpSceneInterface, lpVehicleInterface );
            meOnlineState = E_ONLINE_STATE_LOST_CONTACT;                    // li 2
        }

        if( mbCarSelectOnlineStateChanged )                                 // lbz 0x79B
        {
            // `lbz r11, 0xA4(GetGlobalRaceCar()) ; cmplwi 2` -- the raw type byte, no range assert.
            CGS_ASSERT( GetGlobalRaceCar()->GetType() == E_RACE_CAR_TYPE_NETWORK,
                        "Trying to update a non-network car car select state" );   // :490
            if( !mbIsInCarSelectOnline )                                    // lbz 0x79A
            {
                if( !mbAddedForCollision )                                  // lbz 0x78B
                {
                    AddToCollision( lpSceneInterface, lpVehicleInterface );
                }
            }
            else if( mbAddedForCollision )
            {
                RemoveFromCollision( lpSceneInterface, lpVehicleInterface );
            }
            mbCarSelectOnlineStateChanged = false;                          // stb r27, 0x79B
        }
        break;

    case E_ONLINE_STATE_LOST_CONTACT:
        if( !mbNotSendingNetworkUpdates )                                   // lbz 0x798
        {
            if( !mbIsInCarSelectOnline )                                    // lbz 0x79A
            {
                AddToCollision( lpSceneInterface, lpVehicleInterface );
            }
            mbRenderThisFrame = true;                                       // stb 1, 0x79D
            meOnlineState     = E_ONLINE_STATE_NORMAL;                      // stw 1, 0x744
        }
        else if( mbIsDisconnectedFromNetwork )                              // lbz 0x799
        {
            if( IsActive() && !IsCrashing() )
            {
                RemoveFromCollision( lpSceneInterface, lpVehicleInterface );
                meOnlineState = E_ONLINE_STATE_DISCONNECTED;                // li 3
            }
        }
        else
        {
            // The lost-contact flash: drawn for 30 frames, hidden for the next 30.
            ++miFlashFrequency;                                             // lwz/addi/stw 0x794
            if( miFlashFrequency <= 30 )                                    // cmpwi 0x1E ; ble
            {
                mbRenderThisFrame = true;
            }
            else
            {
                mbRenderThisFrame = false;                                  // stb r27, 0x79D
                if( miFlashFrequency > 60 )                                 // cmpwi 0x3C ; ble
                {
                    miFlashFrequency = 0;
                }
            }
        }
        break;

    case E_ONLINE_STATE_DISCONNECTED:
        mbRenderThisFrame = false;                                          // stb r27, 0x79D
        break;

    default:
        break;
    }

    // ---- THE SHARED TAIL (0x822EB0F4..0x822EB15C), reached by every arm and the default ----
    // The two physics posts (crash parity FX-RCEM4 2026-09-24, reviewer A on 65eadffe; they were
    // parked until VehicleInputInterface grew the DWARF posters :166 / :171):
    //   0x822EB0F4  lbz 0x78D ; beq          if (mbChangeCollisionState)
    //   0x822EB100  ld 0xD0 ; srdi 32        the 32-bit entity word of mHandlingBodyVolumeId
    //   0x822EB108  lbz 0x78E                mbCollisionStateToChangeTo
    //   0x822EB120  bl AddEvent on r5 + 0x202A0  == lpVehicleInterface->SetRaceCarCollision(...)
    //   0x822EB124  stb 0, 0x78D             mbChangeCollisionState = false
    //   0x822EB128  lbz 0x78F ; beq          if (mbChangeCullingGroup)
    //   0x822EB13C  lwz 0x790                mCullingGrouptoChangeTo
    //   0x822EB154  bl AddEvent on r5 + 0x202FC  == lpVehicleInterface->SetRaceCarCullingGroup(...)
    //   0x822EB158  stb 0, 0x78F             mbChangeCullingGroup = false
    // The producers are AddToCollision / RemoveFromCollision (the collision pair) and
    // UpdateCullingGroup (the group); the consumer is VehicleManager::ProcessCollisionEvents.
    // [DIAG] BRN_COLLISION_POST_DIAG -- NOT IN THE X360 BINARY. Capped proof the two posts were
    // DISPATCHED, with what they carried.
    static const bool sbCollisionPostDiag = ( getenv( "BRN_COLLISION_POST_DIAG" ) != 0 );
    static u32 suCollisionPostDiagLines = 0u;
    const bool lbLogPosts = sbCollisionPostDiag && suCollisionPostDiagLines < 64u
                         && CgsDev::Log::gpDebugPrint != 0 && ( mbChangeCollisionState || mbChangeCullingGroup );
    if( lbLogPosts )
    {
        ++suCollisionPostDiagLines;
        *CgsDev::Log::gpDebugPrint
            << "[collision-post] slot " << static_cast<s32>( meActiveRaceCarIndex )
            << " entity " << static_cast<u32>( mHandlingBodyVolumeId.muId >> 32 )
            << " collision " << ( mbChangeCollisionState ? ( mbCollisionStateToChangeTo ? "on" : "off" ) : "-" )
            << " culling " << ( mbChangeCullingGroup ? mCullingGrouptoChangeTo : -1 ) << "\n";
    }

    if( mbChangeCollisionState )
    {
        EntityId lBodyId;
        lBodyId.muValue = static_cast<u32>( mHandlingBodyVolumeId.muId >> 32 );
        lpVehicleInterface->SetRaceCarCollision( lBodyId, mbCollisionStateToChangeTo );
        mbChangeCollisionState = false;
    }
    if( mbChangeCullingGroup )
    {
        EntityId lBodyId;
        lBodyId.muValue = static_cast<u32>( mHandlingBodyVolumeId.muId >> 32 );
        lpVehicleInterface->SetRaceCarCullingGroup(
            lBodyId, static_cast<BrnPhysics::Vehicle::SetRaceCarCullingGroupEvent::CullingGroup>( mCullingGrouptoChangeTo ) );
        mbChangeCullingGroup = false;
    }

    mbCrashedIntoWater = false;                                             // 0x822EB15C  stb r27, 0x783
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
// transform -- the body, six WORLD wheels and damage-event WORLD part poses. The ordering rules
// (restore before the producers, latch after them, apply per rendered frame) and the
// reason the restore is mandatory live on PoseTrack itself; the module drives the pairing
// (RaceCarEntityModule::PostPhysicsUpdate brackets its producers with the first two).
void ActiveRaceCar::RestoreTickRenderPose()
{
    mBodyPoseTrack.Restore(mRenderParams.GetBodyTransformForWrite());
    for (u32 luWheel = 0; luWheel < KU_INTERP_WHEELS; ++luWheel)
        maWheelPoseTracks[luWheel].Restore(mRenderParams.GetWheelTransform(luWheel));
    RenderParams::DetachedPartRenderQueue& lrParts = mRenderParams.GetDetachedPartQueue();
    for (s32 liEvent = 0; liEvent < lrParts.GetLength(); ++liEvent)
    {
        DetachedPartRenderEvent& lrPart = lrParts.GetEvent(liEvent);
        CGS_ASSERT(static_cast<u32>(lrPart.miPartIndex) < KU_MAX_BODY_PARTS_PER_RACE_CAR,
                   "Invalid render part index");
        maPartPoseTracks[lrPart.miPartIndex].Restore(lrPart.mTransform);
    }
}

void ActiveRaceCar::LatchTickRenderPose()
{
    mBodyPoseTrack.Latch(mRenderParams.GetBodyTransform());
    for (u32 luWheel = 0; luWheel < KU_INTERP_WHEELS; ++luWheel)
        maWheelPoseTracks[luWheel].Latch(mRenderParams.GetWheelTransform(luWheel));
    bool labPresent[KU_MAX_BODY_PARTS_PER_RACE_CAR] = {};
    const RenderParams::DetachedPartRenderQueue& lrParts = mRenderParams.GetDetachedPartQueue();
    for (s32 liEvent = 0; liEvent < lrParts.GetLength(); ++liEvent)
    {
        const DetachedPartRenderEvent& lrPart = lrParts.GetEvent(liEvent);
        CGS_ASSERT(static_cast<u32>(lrPart.miPartIndex) < KU_MAX_BODY_PARTS_PER_RACE_CAR,
                   "Invalid render part index");
        maPartPoseTracks[lrPart.miPartIndex].Latch(lrPart.mTransform);
        labPresent[lrPart.miPartIndex] = true;
    }
    // A removed/repaired part must not reuse an old world pose if it breaks again.
    for (u32 luPart = 0; luPart < KU_MAX_BODY_PARTS_PER_RACE_CAR; ++luPart)
        if (!labPresent[luPart])
            maPartPoseTracks[luPart].Reset();
}

void ActiveRaceCar::ApplyRenderPoseInterpolation(f32 lfAlpha)
{
    mBodyPoseTrack.Apply(mRenderParams.GetBodyTransformForWrite(), lfAlpha);
    for (u32 luWheel = 0; luWheel < KU_INTERP_WHEELS; ++luWheel)
        maWheelPoseTracks[luWheel].Apply(mRenderParams.GetWheelTransform(luWheel), lfAlpha);
    RenderParams::DetachedPartRenderQueue& lrParts = mRenderParams.GetDetachedPartQueue();
    for (s32 liEvent = 0; liEvent < lrParts.GetLength(); ++liEvent)
    {
        DetachedPartRenderEvent& lrPart = lrParts.GetEvent(liEvent);
        CGS_ASSERT(static_cast<u32>(lrPart.miPartIndex) < KU_MAX_BODY_PARTS_PER_RACE_CAR,
                   "Invalid render part index");
        maPartPoseTracks[lrPart.miPartIndex].Apply(lrPart.mTransform, lfAlpha);
    }
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
//   * the centre-of-mass transform copy and its IsValid assert (leg 1 below),
//   * ResetVerletOffsets (leg 4 below),
//   * the detached-part render queue Construct,
//   * mbCanDriveAwayFromCrash / mbUncrashedThisFrame clears the module's caller makes
//     right after (they are OnRaceCarResourcesLoaded's own two trailing stores).
//
// [FLAG PC bring-up] WHAT IS NOT, and why -- the legs below, none paraphrased:
//   1. ⭐ LANDED 2026-09-24 (crash parity, reviewer B on 87d1ad23 / G62): mCentreOfMassTransform
//      <- BrnPhysics::Def(mDeformationModelResourcePtr) + 1552, 0x822EB20C..0x822EB24C:
//        mr r3, r24 (this + 0x1C90) ; bl BrnPhysics::Def (@0x822C7708) ; addi r10, r3, 0x610
//        four lvx128 [r10 + 0/0x10/0x20/0x30] -> stvx128 [this + 0x90 + ...]   (w lanes too)
//      then the per-row x/y/z self-equality cascade 0x822EB250..0x822EB3DC and ONE assert,
//      "RwMath::IsValid( mCentreOfMassTransform )" (0x8201D720, line 0x33F == :831). The DWARF
//      spelling is `mCentreOfMassTransform = mDeformationModelResourcePtr->
//      GetCarModelSpaceToHandlingBodySpaceTransform()`. The "needs the alias-list half of
//      CreateFromHandle" reason was stale: ResolveDeformationSpec (AddToScene's helper, this TU)
//      already reaches the resident spec through the handle stored two lines up. It retires the
//      promote-seam stand-in SetCentreOfMassTransformBringUp (seat wave 2026-08-05), which fed the
//      same +1552 matrix one step later, from RaceCarEntityModule::ResetActiveRaceCar.
//   2. ⭐ LANDED 2026-09-24 (crash parity, same review): the four RenderParams::SetWheelScale(i,
//      Def + 96 + 48*i) calls, 0x822EB410..0x822EB470, after the detached-part queue Construct:
//        r27 = this + 0x7E0 (mRenderParams) ; r30 = 0x60 ; r31 = 0
//        loop: mr r3, r24 ; bl BrnPhysics::Def          -- again on EVERY pass (@0x822EB42C)
//              cmpwi r31, 4 ; blt  else assert "liWheel < eNumWheels" (0x82014ED8,
//                  BrnStreamedDeformationSpec.h line 0x101) -- the inlined GetWheelSpec
//              lvx128 v1, r30, r29                    -- spec + 0x60 + 0x30*i == maWheelSpecs[i].mScale
//              mr r4, r31 ; mr r3, r27 ; bl RenderParams::SetWheelScale (@0x822CD170)
//              r30 += 0x30 ; r31 += 1 ; while r30 < 0x120
//      DWARF: `mRenderParams.SetWheelScale( luWheel, mDeformationModelResourcePtr->
//      GetWheelSpec( luWheel )->mScale )` (local u32 luWheel, :819). The old reasons were stale:
//      the Def dependency is leg 1's (resolved the same way) and the wheels have drawn since the
//      wheel-render wave. It retires the promote-seam stand-in loop in ResetActiveRaceCar
//      (wheel-transform wave 2026-08-13).
//   3. ⭐ LANDED 2026-09-24 (crash parity CHAIN-RECOLOUR / G61-D6 colour leg): the car's
//      authored default colour, 0x822EB474..0x822EB4F0, after the wheel-scale loop:
//        r3 = 0x52B81656F3ADF675 (burnoutcarasset's class key; the old "-206702987" here was
//        only its low word), r4 = r23 = luCarAssetAttribKey  ->  bl Attrib::FindCollection
//        Attrib::Instance(coll, 0) ; no layout -> DefaultDataArea(0x228)   == the inlined
//                                    burnoutcarasset(u64 key, owner) ctor
//        RefSpec @layout+0x170 (GraphicsAsset) -> GetCollection -> burnoutcargraphicsasset(c, 0)
//        miDefaultColourIndex   (+0x1C80) = layout word 1  (DWARF PlayerColourIndex(), :83)
//        miDefaultColourPalette (+0x1C84) = layout word 0  (PlayerColourPaletteIndex(), :90)
//        then both instances are destroyed (0x822EB4E8 / 0x822EB4F0).
//      Its only reader is RaceCarEntityModule::SetupCarColour @0x822F5170.
//   4. ⭐ LANDED 2026-09-23 (crash parity G61-D6): ResetVerletOffsets @0x822A4E90, `bl` at
//      0x822EB404, right before the detached-part queue Construct (0x822EB40C). The old
//      reason ("the tree has no body for it") was stale -- the body is below, at
//      ActiveRaceCar::ResetVerletOffsets.
// ============================================================================
void ActiveRaceCar::OnResourcesLoaded( const CgsResource::ResourceHandle& lrDeformationModelHandle,
                                       const CgsResource::ResourceHandle& lrGraphicsModelHandle,
                                       const Vector3&                     lrInitialVelocity,
                                       u64                                luCarAssetAttribKey )
{
    CGS_ASSERT(IsAttached(), "IsAttached()");     // BrnActiveRaceCar.cpp:821
    CGS_ASSERT(!IsActive(), "!IsActive()");       // :822

    (void)lrInitialVelocity;     // consumed by AddHandlingModel, carried for signature parity

    muState = E_STATE_WAITING;                                       // +0x740 = 2

    mDeformationModelHandle = lrDeformationModelHandle;              // +0x1CA4
    mGraphicsModelHandle    = lrGraphicsModelHandle;                 // +0x1CC4

    // 0x822EB20C..0x822EB24C (banner leg 1): Def(this + 0x1C90) + 0x610 copied whole into
    // +0x90. PC-SAFETY GUARD, not console behaviour: a spec that does not resolve fires Def's own
    // "Can not instance" assert (CgsResourcePtr.h:544) and leaves the matrix as Prepare/Attach
    // set it, where the console would read through the null resource.
    const BrnPhysics::Deformation::StreamedDeformationSpec* lpDeformationSpec =
        ResolveDeformationSpec( mDeformationModelHandle );
    CGS_ASSERT( lpDeformationSpec != 0,
                "Can not instance resource pointer - it has no main memory resource\n" );
    if( lpDeformationSpec != 0 )
    {
        mCentreOfMassTransform = lpDeformationSpec->mCarModelSpaceToHandlingBodySpaceTransform;
    }
    CGS_ASSERT( rw::math::vpu::IsValid( mCentreOfMassTransform ),
                "RwMath::IsValid( mCentreOfMassTransform )" );                         // :831

    // 0x822EB404 `mr r3, r28 ; bl ResetVerletOffsets` (G61-D6): a slot re-used for a new car
    // starts undented. muState is WAITING here, so its !IsInactive() tripwire holds.
    ResetVerletOffsets();

    // BrnWorld::DetachedPartRenderEvent<20>::Construct(this + 5520) -- the queue lives
    // inside mRenderParams and is the one member of the block this header names.
    mRenderParams.GetDetachedPartQueue().Construct();

    // 0x822EB410..0x822EB470 (banner leg 2): the four render wheel scales, once per load. Def is
    // called again on every pass (bl @0x822EB42C); the loop runs while r30 = 0x60 + 0x30*i is
    // below 0x120, i.e. four wheels. Same PC-safety guard as leg 1.
    for( u32 luWheel = 0u; luWheel < 4u; ++luWheel )
    {
        const BrnPhysics::Deformation::StreamedDeformationSpec* lpWheelDeformationSpec =
            ResolveDeformationSpec( mDeformationModelHandle );
        CGS_ASSERT( lpWheelDeformationSpec != 0,
                    "Can not instance resource pointer - it has no main memory resource\n" );
        if( lpWheelDeformationSpec != 0 )
        {
            mRenderParams.SetWheelScale(
                luWheel, lpWheelDeformationSpec->GetWheelSpec( static_cast<s32>( luWheel ) )->mScale );
        }
    }

    // 0x822EB474..0x822EB4F0 -- the authored default colour (banner leg 3).
    Attrib::Gen::burnoutcarasset lCarAsset( luCarAssetAttribKey, 0 );
    Attrib::Gen::burnoutcargraphicsasset lGraphicsAsset(
        const_cast<Attrib::Collection*>( lCarAsset.GetGraphicsAssetRefSpec()->GetCollection() ), 0 );
    miDefaultColourIndex   = lGraphicsAsset.PlayerColourIndex();          // stw +0x1C80 @0x822EB4DC
    miDefaultColourPalette = lGraphicsAsset.PlayerColourPaletteIndex();   // stw +0x1C84 @0x822EB4E4

    // [DIAG] BRN_RESLOADED_DIAG -- NOT IN THE X360 BINARY. Capped proof the Def() legs were
    // DISPATCHED on a resolved spec, with the values they stored.
    static const bool sbResLoadedDiag = ( getenv( "BRN_RESLOADED_DIAG" ) != 0 );
    static u32 suResLoadedDiagLines = 0u;
    if( sbResLoadedDiag && suResLoadedDiagLines < 32u && CgsDev::Log::gpDebugPrint != 0 )
    {
        ++suResLoadedDiagLines;
        *CgsDev::Log::gpDebugPrint
            << "[res-loaded] OnResourcesLoaded slot " << static_cast<s32>( meActiveRaceCarIndex )
            << " spec " << ( lpDeformationSpec != 0 ? 1 : 0 )
            << " mCentreOfMassTransform.w (" << mCentreOfMassTransform.wAxis.x << ", "
            << mCentreOfMassTransform.wAxis.y << ", " << mCentreOfMassTransform.wAxis.z << ")";
        for( u32 luLogWheel = 0u; luLogWheel < 4u; ++luLogWheel )
        {
            const Matrix44Affine& lrScale = mRenderParams.GetWheelScaleMatrix( luLogWheel );
            *CgsDev::Log::gpDebugPrint
                << " wheel" << luLogWheel << " scale (" << lrScale.xAxis.x << ", "
                << lrScale.yAxis.y << ", " << lrScale.zAxis.z << ")";
        }
        *CgsDev::Log::gpDebugPrint << "\n";
    }
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

    // ============ THE IDENTITY GOES WITH THE POSE ============
    // Seeding mTransform alone shipped a RaceCarState that is positioned but ANONYMOUS:
    // mEntityId stayed 0 for every car whose physics slot is never claimed. That field is
    // not decoration -- VehicleState::IsAttachedToThis @0x82683D38 is an IDENTITY TEST on
    // it, and nothing else:
    //     lwz r10, 0x428(r3)   ; state->mVehiclePhysicsData.mEntityId
    //     lwz r11, 0x3C8(r4)   ; ((RaceCarState*)attachment)->mEntityId
    //     subf ; cntlzw ; rlwinm   -> equal?
    // With every AI car reading 0, `0 == 0` made EVERY attached sound state match EVERY
    // car. In the owner's 2026-09-16 session that turned AIVehicleStateManager::UpdateParams
    // into a loop: car 1's out-of-range test resolved to car 2's state and detached it, car 2
    // re-attached on the next frame, and the two alternated 236 times (measured, strictly
    // interleaved: `[ai-sound-detach] car=1 out of range` / `[ai-sound-attach] car=2`). Each
    // re-attach re-requested 'sound\aems\InAir.bundle' and the car's engine bundle without
    // releasing, so the 16-node per-resource requester pool filled and AddTail fired
    // "We've run out of nodes." 468 times. The same all-zero mEntityId is already on record
    // one hop away -- BrnPhysicsModuleUpdateFunctions.cpp's PC-BUILD GUARD #2 measured 663
    // assert dialogs from it sailing through the console's own sentinel test.
    //
    // NOTHING IS INVENTED. This is the car's own identity, already built two ways in this
    // same class and module:
    //   * Attach (:314) does SetEntityIDOwner(E_ENTITYTYPE_RACECAR) +
    //     SetEntityIDEntityIndex(meActiveRaceCarIndex) into mHandlingBodyVolumeId;
    //   * VehicleManager::ProcessCreateEvents stores that same word into
    //     maRaceCarEntityIDs[slot], and WriteOutVehicleStats publishes it back through
    //     SetEntityID -- which is the value UpdatePhysicsState would memcpy in here the
    //     moment the physics slot IS claimed.
    // MEASURED AGREEMENT: the player's slot (the one car that does get a physics slot) reads
    // entityWord 0x01000000, owner 1, entityIndex 0 -- exactly what Set(1, 0, 0) builds.
    // So this seat writes the value the real path would write, for the cars the real path
    // has not reached yet; when it does reach them, UpdatePhysicsState overwrites it with
    // the identical word.
    // DELETE-WHEN VehicleManager::ProcessCreateEvents claims a physics slot for AI cars too
    // (the same condition this file's sibling seats already carry).
    // BrnCommonTypes.h's EntityId is the 32-bit STORAGE word only, so the packing is done by
    // the real CgsSceneManager::EntityId and the word handed over -- the same two-step
    // WriteOutVehicleStats uses in reverse (`CgsSceneManager::EntityId lEntityID(...muValue)`).
    CgsSceneManager::EntityId lSeatedEntityId;
    lSeatedEntityId.Set(static_cast<u32>(BrnWorld::E_ENTITYTYPE_RACECAR),
                        static_cast<u32>(meActiveRaceCarIndex),
                        0u);
    mPhysicsState.mEntityId.muValue = static_cast<u32>(lSeatedEntityId);

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
    for (u32 luPart = 0; luPart < KU_MAX_BODY_PARTS_PER_RACE_CAR; ++luPart)
        maPartPoseTracks[luPart].Reset();
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

    // FLAG PC diagnostic: follow the physical victim and its actual render offsets,
    // rather than treating a HUD/camera transition as proof of a visible wreck.
    static const bool sbTraceRivalDamage = getenv("BRN_RIVAL_DAMAGE_DIAG") != 0;
    static u32 sauRivalDamageFrames[E_ACTIVE_RACE_CAR_INDEX_COUNT] = {};
    if (sbTraceRivalDamage && meActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0 &&
        meActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT)
    {
        u32& lruFrame = sauRivalDamageFrames[meActiveRaceCarIndex];
        if (!mbTakenDown) lruFrame = 0;
        else if (lruFrame++ < 48 && CgsDev::Log::gpDebugPrint)
        {
            f32 lfOffsetSum = 0.0f;
            const Vector3Plus* lpOffsets = mRenderParams.GetVerletOffsets();
            for (u32 luPoint = 0; luPoint < 128; ++luPoint)
                lfOffsetSum += std::fabs(lpOffsets[luPoint].x) +
                    std::fabs(lpOffsets[luPoint].y) + std::fabs(lpOffsets[luPoint].z);
            *CgsDev::Log::gpDebugPrint << "[rival-damage] slot=" << static_cast<s32>(meActiveRaceCarIndex)
                << " crashing=" << (mPhysicsState.mbCrashing ? 1 : 0)
                << " damaged=" << (mRenderParams.IsDamaged() ? 1 : 0)
                << " displacement=" << lpCarState->GetSummedDisplacementSquared()
                << " offsets=" << lfOffsetSum
                << " pos=" << mPhysicsState.mTransform.Pos().x << "," << mPhysicsState.mTransform.Pos().y
                << "," << mPhysicsState.mTransform.Pos().z
                << " angular=" << mPhysicsState.mAngularVelocity.x << "," << mPhysicsState.mAngularVelocity.y
                << "," << mPhysicsState.mAngularVelocity.z << "\n";
        }
        // FLAG PC diagnostic ([td-flight], same gate): the 48-frame window above ends 0.8 s after
        // the takedown, before most of a wreck's flight, tumble and settling. Sample the whole
        // episode (every 6th frame, up to 900 frames = 15 s) with the linear velocity and the up
        // axis, so "flies more / reacts less" can be measured on the victim, not inferred.
        static u32 sauTakedownFlightFrames[E_ACTIVE_RACE_CAR_INDEX_COUNT] = {};
        u32& lruFlight = sauTakedownFlightFrames[meActiveRaceCarIndex];
        if (!mbTakenDown) lruFlight = 0;
        else if (lruFlight < 900 && (lruFlight++ % 6) == 0 && CgsDev::Log::gpDebugPrint)
        {
            *CgsDev::Log::gpDebugPrint << "[td-flight] slot=" << static_cast<s32>(meActiveRaceCarIndex)
                << " f=" << (lruFlight - 1)
                << " crashing=" << (mPhysicsState.mbCrashing ? 1 : 0)
                << " pos=" << mPhysicsState.mTransform.Pos().x << "," << mPhysicsState.mTransform.Pos().y
                << "," << mPhysicsState.mTransform.Pos().z
                << " vel=" << mPhysicsState.mLinearVelocity.x << "," << mPhysicsState.mLinearVelocity.y
                << "," << mPhysicsState.mLinearVelocity.z
                << " upy=" << mPhysicsState.mTransform.Up().y
                << " angular=" << mPhysicsState.mAngularVelocity.x << "," << mPhysicsState.mAngularVelocity.y
                << "," << mPhysicsState.mAngularVelocity.z
                << " displacement=" << lpCarState->GetSummedDisplacementSquared() << "\n";
        }
    }

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

            // ---- [deform-rows] THE SHAPE, NOT THE SCALAR. NOT X360; opt-in BRN_DEFORM_ROWS=1
            // (needs BRN_DEFORM_TRACE too -- it lives inside that block).
            // ⭐ WHY A WHOLE-ARRAY DUMP EXISTS AT ALL: sumVerlet / nnzVerlet / maxVerlet answer
            // "how much" and "the worst row". They cannot answer the OWNER'S question, which is
            // WHERE the displacement goes -- a dent that smears across a panel and a dent that
            // folds it have the SAME sum and can have the same max. Constant 22's rows are the
            // vehicle's only vertex mover, and each row has a fixed authored identity (the
            // console packs [every skinned TagPoint in tag order][then each IK part's driven
            // points], DeformableObject::UpdateSkinningOffsets @0x825DFA90), so a row index IS a
            // place on the car and the array IS the deformed shape.
            // Prints ONCE per car per new sumVerlet HIGH-WATER MARK, so a wreck emits a handful
            // of lines at its worst frames instead of one per frame.
            // DELETE-WHEN the deformation-shape question is closed and banked.
            {
                static s32 siRowsOn = -1;
                if (siRowsOn < 0)
                {
                    const char* lpcEnv = getenv("BRN_DEFORM_ROWS");
                    siRowsOn = (lpcEnv != 0 && atoi(lpcEnv) > 0) ? 1 : 0;
                }
                static f32 sfRowsHighWater = 0.0f;
                if (siRowsOn == 1 && IsPlayer() && lfSumVerlet > sfRowsHighWater + 0.25f
                    && CgsDev::Log::gpDebugPrint != 0)
                {
                    sfRowsHighWater = lfSumVerlet;
                    *CgsDev::Log::gpDebugPrint
                        << "[deform-rows] present " << static_cast<s32>(renderengine::guPresentCount)
                        << " crashing " << (IsCrashing() ? 1 : 0)
                        << " sumVerlet " << lfSumVerlet << " rows>=0.02m:";
                    for (u32 luRow = 0; luRow < 128u; ++luRow)
                    {
                        const f32 lfRow = std::sqrt(lpVerlet[luRow].x * lpVerlet[luRow].x
                                                  + lpVerlet[luRow].y * lpVerlet[luRow].y
                                                  + lpVerlet[luRow].z * lpVerlet[luRow].z);
                        if (lfRow >= 0.02f)
                        {
                            *CgsDev::Log::gpDebugPrint
                                << " " << static_cast<s32>(luRow) << "=("
                                << lpVerlet[luRow].x << "," << lpVerlet[luRow].y << ","
                                << lpVerlet[luRow].z << ")|" << lfRow;
                        }
                    }
                    *CgsDev::Log::gpDebugPrint << "\n";
                }
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

// -------------------------------------------------------------------------------------------------
// CalculateWheelAngularVelocities   (41 instructions)   -- wheel-blur wave 2026-09-12
//
// ⭐⭐ THE MISSING PRODUCER OF RACE-CAR WHEEL BLUR. The renderer picks a per-wheel technique
// by comparing mRenderParams.mafWheelAngularVelocities[i] against the 30 rad/s wheel-blur
// threshold; this is the ONLY thing in the game that writes that array for a race car. With it
// absent every wheel read 0 rad/s, so no race-car wheel could ever pick the blurred variant
// (traffic has no such hole -- its module derives the same quantity from the vehicle speed).
//
// The body is four unrolled copies of one statement, one per ROAD wheel: take the wheel's
// signed spin rate from the published physics snapshot, drop the sign (the blur test only
// cares how fast, not which way), and scale it by this frame's time-step multiplier so the
// smear tracks slow motion instead of ignoring it. The multiplier is the same one the module
// latches alongside its time step, handed down through Update.
//
// Both ends are named members of committed sub-objects: the source is
// mPhysicsState.maWheels[i].mfRadiansPerSecond (WheelLite +0x44, published by the vehicle
// output interface) and the destination is mRenderParams.mafWheelAngularVelocities[i]
// (RenderParams +0xD8C), reached through the accessor that already exists for it.
//
// The leading IsAttached() tripwire is the console's own.
// -------------------------------------------------------------------------------------------------
void ActiveRaceCar::CalculateWheelAngularVelocities(f32 lfTimeStepMultiplier)
{
    CGS_ASSERT( IsAttached(), "IsAttached()" );          // BrnActiveRaceCar.h:1096

    const BrnPhysics::Vehicle::WheelLite* lpaWheelArray = GetPhysicsState()->maWheels;
    RenderParams*                         lpRenderParams = GetRenderParams();

    const u32 KU_ROAD_WHEEL_COUNT = 4;
    for( u32 luWheelIndex = 0; luWheelIndex < KU_ROAD_WHEEL_COUNT; ++luWheelIndex )
    {
        lpRenderParams->SetWheelAngularVelocity(
            luWheelIndex,
            std::fabs( lpaWheelArray[luWheelIndex].mfRadiansPerSecond ) * lfTimeStepMultiplier );
    }
}

// -------------------------------------------------------------------------------------------------
// UpdateInAirRotations  (158 instructions)  -- wheel wave 2026-09-13
//
// The car-space rotation the body has swept since it last left the ground, plus the latch that
// decides whether that accumulator is live. Update calls it once a frame, immediately after
// CalculateWheelAngularVelocities and with the SAME argument -- the module's time-step
// MULTIPLIER, not its raw time step (the console does `fmr f1, f27` for both calls, where f27 is
// Update's third float parameter). The declaration reference spells the parameter lfTimeStep, so that name is
// kept; the mismatch is the console's, not this reconstruction's.
//
// ---- THE THREE LEGS, AS THE ASM ORDERS THEM -----------------------------------------------------
//  1.  airborne test: mPhysicsState.mfTimeInAir (RaceCarState @1028) > 0. Taken with
//     `fcmpu / ble` -- the arm-the-latch leg needs a STRICTLY greater ORDERED compare, so it is
//     written `> 0.0f` and a NaN falls to the grounded leg exactly as the branch does. Airborne:
//     zero the stability timer and raise mbCurrentlyRotating.
//  2.  grounded, and only while the latch is still up: walk the four road wheels and
//     decide whether the car is STABLE this frame. A wheel the car no longer has
//     (mPhysicsState.mabWheelExists[i], RaceCarState @1094) is skipped; a wheel it does have must
//     be BOTH attached and gripping (WheelLite mbAttached +0x60 / mbHasTraction +0x61) or the walk
//     stops there. Only a walk that ran all four wheels out (the console's `cmpwi r29, 4`) adds
//     this frame to mfTimeSinceLastStable; any early stop zeroes it. Once a full second of
//     unbroken stability has accumulated the latch drops and the accumulator is cleared. The
//     console SKIPS that clear with a `blt` over a 1.0f constant, and a `blt` is NOT taken when
//     the compare is unordered, so the clear must run on not-less-OR-unordered: it is written
//     `!(timer < 1.0f)` and a NaN timer is therefore reset, exactly as the branch does.
//  3.  while the latch is up, integrate: rotate the world angular velocity
//     (mPhysicsState.mAngularVelocity, RaceCarState @832) into car space through the inverse of
//     the car transform (mPhysicsState.mTransform, RaceCarState @496) and add a frame of it to
//     mCurrentInAirRotations. The console builds only the 3x3 transpose inline (vmrghw/vmrglw
//     lane merges feeding a vmaddfp cascade) because TransformVector never touches the
//  translation row -- the source shape is the pair of SDK calls the declaration reference lists for this
//     function (InverseOfMatrixWithOrthonormal3x3 then TransformVector), and the local names
//  below are the declaration reference's own (BrnActiveRaceCar.cpp).
//
// The four IsAttached() tripwires are the console's own: one per inlined GetPhysicsState().
// -------------------------------------------------------------------------------------------------
void ActiveRaceCar::UpdateInAirRotations(f32 lfTimeStep)
{
    const s32 KI_ROAD_WHEEL_COUNT = 4;

    // A full second of stable contact retires the accumulator (console constant 1.0f).
    const f32 KF_STABLE_TIME_TO_STOP_ROTATING = 1.0f;

    if( mPhysicsState.mfTimeInAir > 0.0f )
    {
        mfTimeSinceLastStable = 0.0f;                        // +0x760
        mbCurrentlyRotating   = true;                        // +0x764
    }
    else if( mbCurrentlyRotating )
    {
        s32 liWheelIndex = 0;
        for( ; liWheelIndex < KI_ROAD_WHEEL_COUNT; ++liWheelIndex )
        {
            CGS_ASSERT( IsAttached(), "IsAttached()" );      // BrnActiveRaceCar.h

            if( GetPhysicsState()->mabWheelExists[liWheelIndex] )
            {
                CGS_ASSERT( IsAttached(), "IsAttached()" );  // BrnActiveRaceCar.h

                if( !GetPhysicsState()->maWheels[liWheelIndex].mbAttached )
                {
                    break;
                }
                if( !GetPhysicsState()->maWheels[liWheelIndex].mbHasTraction )
                {
                    break;
                }
            }
        }

        if( liWheelIndex == KI_ROAD_WHEEL_COUNT )
        {
            mfTimeSinceLastStable = mfTimeSinceLastStable + lfTimeStep;
        }
        else
        {
            mfTimeSinceLastStable = 0.0f;
        }

        if( !( mfTimeSinceLastStable < KF_STABLE_TIME_TO_STOP_ROTATING ) )
        {
            mbCurrentlyRotating    = false;
            mCurrentInAirRotations = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };  // +0x750, a full 16-byte clear
        }
    }

    if( mbCurrentlyRotating )
    {
        CGS_ASSERT( IsAttached(), "IsAttached()" );          // BrnActiveRaceCar.h
        Matrix44Affine lInverseCarTransform =
            rw::math::vpu::InverseOfMatrixWithOrthonormal3x3( GetPhysicsState()->mTransform );

        CGS_ASSERT( IsAttached(), "IsAttached()" );          // BrnActiveRaceCar.h
        Vector3 lAngularVelocityInCarSpace =
            rw::math::vpu::TransformVector( lInverseCarTransform, GetPhysicsState()->mAngularVelocity );

        mCurrentInAirRotations = mCurrentInAirRotations + lAngularVelocityInCarSpace * lfTimeStep;
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
//   f2   f32                                 <- module +0x183A0  (mfSimTime)
//   f3   f32     -> lfTimeStepMultiplier ✔ USED  <- module mfTimeStepMultiplier (+0x183A4)
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
//  2-3. RESTORED: touching-world status and the crash drive-away decision/timer,
//       including route-direction inputs and the 1.5-second game-event-38 publish.
//  4. the IsOnRaceStartState(0) start-line rev RNG (0x822F7C64..0x822F7CE4) -- it needs the
//     module's RNG at +0x18490.
//  5. RaceCar::GetTransform / GetPreviousPosition / GetPosition (0x822F7D44..0x822F7DC8):
//     the console calls them and DISCARDS all three results (v102/v103/v104 are dead in the
//     decompilation) -- almost certainly an inlined body Hex-Rays lost. Dropped deliberately.
//  6. SendAddedRemovedNetworkCarForCollisionEvents (0x822F7E70) -- no definition in this tree.
//     (CalculateWheelAngularVelocities landed 2026-09-12, UpdateInAirRotations 2026-09-13 and
//      UpdateIndicators 2026-09-23 (G60-D2/G61-D5); all three are called below.)
//  7. the mbIsWaitingForDeferredReset -> RequestPlaceOnTrack countdown (0x822F7E80..0x822F7EB8).
//     RequestPlaceOnTrack exists, but the latch is only ever armed by code this build has not
//     landed, so running the countdown would be dead work with a live teleport at the end.
// ----------------------------------------------------------------------------
void ActiveRaceCar::Update(f32 lfTimeStep,
                           f32 lfTimeStepMultiplier,
                           f32 lfAcceleration,
                           f32 lfBraking,
                           bool lbIsInOnlineGameMode,
                           bool lbInCarSelectScreen,
                           s32 liGameModeType,
                           const Vector2& lrCurrentRouteNode,
                           const Vector2& lrNextRouteNode,
                           RaceCarEntityModuleIO::GameEventQueue* lpGameEvents)
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
    mbIsTouchingWorld = !lbCrashing && !(mPhysicsState.mfTimeInAir > 0.0f);

    // ARTIST 0x822F79F8..0x822F7C54: classify a settled crash and publish drive-away.
    if (lbCrashing && !mbIsInShowtime)
    {
        if (mPhysicsState.mbFullyDrivableFromCrash)
        {
            if (liGameModeType != 0 && liGameModeType != 5)
            {
                if (!mbIsWrecked)
                {
                    mbCanDriveAwayFromCrash = true;
                    mfTimeDriveableInCrash += lfTimeStep;
                }
            }
            else if (IsPlayer() && mbDriveAwayCheckRequired)
            {
                // The VMX validity checks compare each planar lane with itself (NaN test).
                if (!std::isnan(lrCurrentRouteNode.x) && !std::isnan(lrCurrentRouteNode.y) &&
                    !std::isnan(lrNextRouteNode.x) && !std::isnan(lrNextRouteNode.y))
                {
                    const Vector2 lvDirection = BrnMath::Flatten(GetDirection());
                    const f32 lfX = lrNextRouteNode.x - lrCurrentRouteNode.x;
                    const f32 lfY = lrNextRouteNode.y - lrCurrentRouteNode.y;
                    const f32 lfInvLength = 1.0f / std::sqrt(lfX * lfX + lfY * lfY);
                    const f32 lfRouteX = lfX * lfInvLength;
                    const f32 lfRouteY = lfY * lfInvLength;
                    CGS_ASSERT(!std::isnan(lfRouteX) && !std::isnan(lfRouteY),
                               "RwMathVPU::IsValid( lCurrentRouteVec )");
                    if (!(lvDirection.x * lfRouteX + lvDirection.y * lfRouteY >= 0.0f))
                    {
                        WreckLatchWitness( "Update.routeDot@0x822F7BFC",
                                           static_cast<s32>( GetActiveRaceCarIndex() ), true );
                        mbIsWrecked = true;
                        mbCanDriveAwayFromCrash = false;
                    }
                    else
                    {
                        mbCanDriveAwayFromCrash = true;
                        mfTimeDriveableInCrash += lfTimeStep;
                    }
                    mbDriveAwayCheckRequired = false;
                }
            }
        }
        else mfTimeDriveableInCrash = 0.0f;
        if (mfTimeDriveableInCrash > 1.5f)
        {
            const EActiveRaceCarIndex leIndex = GetActiveRaceCarIndex();
            lpGameEvents->AddEvent(reinterpret_cast<const CgsModule::Event*>(&leIndex), 38, sizeof(leIndex));
        }
    }

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

    // ⭐ THE WHEEL-BLUR WIRE. Unconditional, every active car, immediately after the
    // engine-state gate: this is the only producer of
    // mRenderParams.mafWheelAngularVelocities, which the renderer's per-wheel technique test
    // reads. Its argument is this frame's time-step multiplier, straight off the module.
    CalculateWheelAngularVelocities( lfTimeStepMultiplier );

    // The very next call, and the console hands it the SAME f27 -- the
    // time-step multiplier, not lfTimeStep. This is the only producer of mCurrentInAirRotations,
    // which GetCurrentInAirRotations publishes.
    UpdateInAirRotations( lfTimeStepMultiplier );

    // 0x822F7E74..0x822F7E7C: `mr r3, r31 ; fmr f1, f31 ; bl UpdateIndicators` -- f31 is Update's
    // incoming f1 (`fmr f31, f1` @0x822F78E0), i.e. lfTimeStep, NOT the multiplier the two calls
    // above take. Unconditional, every active car.
    UpdateIndicators( lfTimeStep );

    // 0x822F7EBC..0x822F7ED0.
    if( mbCrashedIntoWater )                             // +0x783
    {
        mfTimeInWater = mfTimeInWater + lfTimeStep;      // +0x784
    }
}


// -------------------------------------------------------------------------------------------------
// OnCrash @ 0x822A4D60   (75 insns)   -- crash wave 2026-09-02
//
//   0x822A4D78  CGS_ASSERT(IsActive())                       BrnActiveRaceCar.cpp:991
//   0x822A4DAC  CGS_ASSERT(lpSceneInterface != NULL)         BrnActiveRaceCar.cpp:992
//   0x822A4DD4  stb r4, 0x580(this)                          mCrashData.mbFirstCrash = lbIsPrimaryCrash
//   0x822A4DDC  !lbIsPrimaryCrash -> return
//   0x822A4DE4  CGS_ASSERT(IsAttached())                     BrnActiveRaceCar.h:1418
//   0x822A4E14  lbz 0x52A(this) -> mPhysicsState.mbCrashing: set -> the dev-log warning
//               ("Warning: crashing an active race car that is already crashing") and return
//   0x822A4E50  four lvx128/stvx128 pairs this+0x2D0 -> this+0x540:
//               mCrashData.mInitialTransform = mPhysicsState.mTransform  (RaceCarState @496)
// r5 (the event), r7 (the remove-volume bool) and f1 (sim time) are never read by the body.
// -------------------------------------------------------------------------------------------------
void ActiveRaceCar::OnCrash(bool lbIsPrimaryCrash,
                            const BrnPhysics::Vehicle::RaceCarCrashEvent& lrEvent,
                            CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface,
                            bool lbRemoveHandlingVolumeFromScene,
                            f32 lfSimTime)
{
    (void)lrEvent;
    (void)lbRemoveHandlingVolumeFromScene;
    (void)lfSimTime;

    CGS_ASSERT( IsActive(), "IsActive()" );                              // :991
    CGS_ASSERT( lpSceneInterface != 0, "lpSceneInterface != NULL" );     // :992

    mCrashData.mbFirstCrash = lbIsPrimaryCrash;                          // stb r4, 0x580

    if( !lbIsPrimaryCrash )
    {
        return;
    }

    CGS_ASSERT( IsAttached(), "IsAttached()" );                          // BrnActiveRaceCar.h:1418

    if( mPhysicsState.mbCrashing )                                       // lbz 0x52A
    {
        // The console's dev-log warning (gxMessageFilterFlags bit 0); log only, no state change.
        if( CgsDev::Log::gpDebugPrint != 0 )
        {
            *CgsDev::Log::gpDebugPrint
                << "Warning: crashing an active race car that is already crashing\n";
        }
        return;
    }

    mCrashData.mInitialTransform = mPhysicsState.mTransform;            // +0x2D0 -> +0x540, 4 rows
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
    WreckLatchWitness( "ResetAfterCrash@0x822BF4BC", static_cast<s32>( GetActiveRaceCarIndex() ), false );
    mbIsWrecked            = false;
    mfTimeDriveableInCrash = 0.0f;
    mbCrashedIntoWater     = false;
    mbIsInShowtime         = false;
}

// =================================================================================================
// [DIAG] NOT IN THE X360 BINARY -- see the banner on the declaration in BrnActiveRaceCar.h.
// Opt in with BRN_WRECK_LATCH_DIAG=1. One line per STORE to mbIsWrecked, naming the console
// address of the store it stands beside. Prints nothing when the variable is unset, which is the
// control. DELETE-WHEN-STABLE.
// =================================================================================================
void WreckLatchWitness( const char* lpcSite, s32 liActiveRaceCarIndex, bool lbNewValue )
{
    static s32 siWreckLatchDiag = -1;
    if( siWreckLatchDiag < 0 )
    {
        const char* lpcEnv = getenv( "BRN_WRECK_LATCH_DIAG" );
        siWreckLatchDiag = ( lpcEnv != 0 && lpcEnv[0] != '0' ) ? 1 : 0;
    }
    if( siWreckLatchDiag != 1 || CgsDev::Log::gpDebugPrint == 0 )
    {
        return;
    }
    *CgsDev::Log::gpDebugPrint
        << "[wrecklatch] car=" << liActiveRaceCarIndex
        << " <- " << ( lbNewValue ? 1 : 0 )
        << " site=" << lpcSite << "\n";
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


// ----------------------------------------------------------------------------
// SetIndicatorState @ 0x822A52B0. Store-for-store (raw asm, NOT the Hex-Rays rendering):
//   r4 != 0 : lbz +0x1C8D ; if 0 -> stfs flt_82001CC0 (0.0) +0x1C88
//             stb 0 +0x1C8C ; stb 1 +0x1C8D                    (0x822A52D4..0x822A52E0)
//   r5 != 0 : lbz +0x1C8C ; if 0 -> stfs 0.0 +0x1C88
//             stb 0 +0x1C8D ; stb 1 +0x1C8C                    (0x822A530C..0x822A5318)
//   else    : stfs 0.0 +0x1C88 ; stb 0 +0x1C8C ; stb 0 +0x1C8D (0x822A5320..0x822A5334)
// +0x1C8D is mbLeftIndicatorActive (UpdateIndicators copies it into mbIsIndicatingLeft, the byte
// SubmitCoronasForRaceCar treats as LEFT), so arg 1 is the LEFT request -- DWARF :47 names the
// parameters (lbLeftIndicatorOn, lbRightIndicatorOn).
// CORRECTED 2026-09-23 (crash parity G60-D1, FX-RCEM3): arm 1 used to raise
// mbRightIndicatorActive and never touch the left latch -- a copy of Hex-Rays' `*(+7308) = 1`,
// which drops the two real stores. Callers: HandleStopModeAction @0x82307C44 and
// HandleGameActions case 35 pass (0, 0) (indicators off); case 276 (UpcomingRoadChangeAction)
// passes the left/right road highlight states == 2.
// ----------------------------------------------------------------------------
void ActiveRaceCar::SetIndicatorState(bool lbLeftIndicatorOn, bool lbRightIndicatorOn)
{
    if (lbLeftIndicatorOn)
    {
        if (!mbLeftIndicatorActive)
        {
            mfIndicatorTime = 0.0f;
        }
        mbRightIndicatorActive = false;
        mbLeftIndicatorActive  = true;
    }
    else if (lbRightIndicatorOn)
    {
        if (!mbRightIndicatorActive)
        {
            mfIndicatorTime = 0.0f;
        }
        mbLeftIndicatorActive  = false;
        mbRightIndicatorActive = true;
    }
    else
    {
        mfIndicatorTime        = 0.0f;
        mbRightIndicatorActive = false;
        mbLeftIndicatorActive  = false;
    }
}

// ----------------------------------------------------------------------------
// UpdateIndicators @ 0x822A5340 (crash parity G60-D2 / G61-D5, 2026-09-23). No definition existed
// and ActiveRaceCar::Update dropped the call (0x822F7E7C), so the two render bits the corona
// producer reads had no writer on PC.
//   0x822A5340  lbz r9, +0x1C8C (right latch, read once) ; || lbz +0x1C8D (left latch)
//   0x822A5358  lfs +0x1C88 ; fadds f0, f0, f1 ; stfs +0x1C88        mfIndicatorTime += lfTimeStep
//   0x822A5368  lfs flt_820147FC (0.5) ; fcmpu ; ble -> keep          t > 0.5 -> 0.0
//               (`ble cr6` 0x40990010 == bc 4, 25: branch when cr6.GT is CLEAR, so an unordered
//               compare KEEPS a NaN timer -- corrected 2026-09-24, FX-NANPOL sweep / FX-RCEM4)
//   0x822A5374  stfs flt_82001CC0 (0.0) +0x1C88
//   0x822A5380  lfs +0x1C88 ; lfs flt_82003F40 (0.25) ; fcmpu ; blt   lbIndicatorActive = t < 0.25
//   0x822A53C4  stb (left latch && active)  -> +0x1BE9 == mRenderParams.mbIsIndicatingLeft
//   0x822A53E0  stb (right latch && active) -> +0x1BEA == mRenderParams.mbIsIndicatingRight
// ----------------------------------------------------------------------------
void ActiveRaceCar::UpdateIndicators(f32 lfTimeStep)
{
    if (mbRightIndicatorActive || mbLeftIndicatorActive)
    {
        mfIndicatorTime = mfIndicatorTime + lfTimeStep;
        if (mfIndicatorTime > 0.5f)                      // flt_820147FC; `ble` keeps -- a NaN is NOT reset
        {
            mfIndicatorTime = 0.0f;                      // flt_82001CC0
        }
    }

    const bool lbIndicatorActive = mfIndicatorTime < 0.25f;   // flt_82003F40
    mRenderParams.SetIndicatingLeft(mbLeftIndicatorActive && lbIndicatorActive);
    mRenderParams.SetIndicatingRight(mbRightIndicatorActive && lbIndicatorActive);
}

}

// ============================================================================
// FOLDED FROM BrnActiveRaceCar_wQ5_01.cpp (wave Q5) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// BrnWorld::ActiveRaceCar -- THE SCENE ADD CHAIN (wave Q5, 2026-08-18).
//
// The exact inverse of the Detach chain that already lives in BrnActiveRaceCar.cpp, and
// the missing half of car-vs-prop collision: without these six bodies a race car has no
// scene entity, no collision volume, no volume instance and no collision registration, so
// no broad-phase pair between a car and a smash gate can exist even once the scene
// manager's own volume legs land.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX, RAW ASSEMBLY (the Hex-Rays pseudocode for
// AddToScene is not usable -- it renders the whole VMX half as inline asm, drops the
// AddEntity centre vector, and mis-pairs the AddDynamicVolume id):
//   ActiveRaceCar::OnHandlingModelAdded      @ 0x822F7EF8   (41 insns)
//   ActiveRaceCar::AddToScene                @ 0x822EB768  (332 insns)
//   ActiveRaceCar::AddToCollision            @ 0x822D41F0  (138 insns)
//   ActiveRaceCar::SendSceneUpdatesPostPhysics @ 0x822EB6B0 (46 insns)
//   ActiveRaceCar::UpdateCullingGroup        @ 0x822BF5B8   (44 insns)
//   ActiveRaceCar::DetermineCullingGroup     @ 0x822BF4D8   (56 insns)
//
// A PARTFILE and not an addition to BrnActiveRaceCar.cpp: that file was already 1,557 lines
// and was live in other sessions' checkouts this wave; the same ledger TU
// (class:BrnWorld::ActiveRaceCar) owns both. MOUNTED -- tools/build/build_game_exe.bat:228,
// immediately after BrnActiveRaceCar.cpp at :227 (the line caradd.owner.md §8 asked for; the
// conductor added it 2026-08-18).
//
// ---------------------------------------------------------------------------
// BOTH OF AddToScene's FLAGGED LEGS ARE NOW LANDED. The two flags are kept as a record of
// what they were and what unblocked them -- neither is an outstanding park any more:
//
//   [FLAG BLOCKED 1] -- ⭐ RETIRED 2026-08-18 (wave Q5 finisher). The AddDynamicVolume POST
//     needed the DWARF's 64-bit form, and it now exists:
//       CgsSceneManagerIO_SceneUpdate.h:367 (dumpfile :455)
//           void AddDynamicVolume(VolumeId, const VolRef::Volume*, VolumeTypeFlags)
//     landed as `AddDynamicVolume(VolumeId, const void*, u8)` beside the committed fitted
//     32-bit `AddDynamicVolume(EntityId, const void*, u8)` form, with the same one X360 body
//     (0x822B1518, which stores the whole 64-bit r4). The call below is now live. Kept as a
//     record of WHY the EntityId form is not a substitute: the console passes the WHOLE
//     8-byte mHandlingBodyVolumeId (`ld r11,0xD0(r30) ; std ; ld r4,0(r10)`
//     @0x822EBBFC..0x822EBC04) and for a race car the entity word lives in the HIGH dword
//     while the LOW dword is IDENTICALLY ZERO (Attach seeds `muId = 0` then only
//     SetEntityIDOwner/SetEntityIDEntityIndex, both of which write the high dword --
//     BrnActiveRaceCar.cpp:290-292), so narrowing to u32 would post volume key 0 for all
//     eight cars. Same width trap wave Q4 recorded for AddVolumeInstance / AddForCollision.
//
//   [FLAG BLOCKED 2] -- ⭐ RETIRED 2026-08-18 (wave Q5 finisher). The volume's own TRANSFORM
//     row. The console re-reads the freshly built box at lpVolume+0x30 -- the Volume record's
//     translation row -- and pulls its Y down by the same half-delta it just added to the Y
//     half-extent (0x822EBBD4..0x822EBBF8: `lvx v0,[vol+0x30] ; vsubfp v0,v0,v124 ;
//     vrlimi v13,v0,4,0 ; stvx v13,[vol+0x30]`), so the box grows DOWNWARD only and its top
//     face stays on the authored handling-body top. It was blocked on the 128-byte
//     PLACEHOLDER rw::collision::BoxVolume (`{u32 muVolumeType; f32 mafParams[3];
//     u8 maPad[112];}`, no transform member at all). The REAL 96-byte
//     rw::collision::Volume/BoxVolume landed in vendor/renderware/collision/CollisionVolume.hpp
//     on 2026-08-18 (the rwcollision owner's own file), so this leg -- and the console's real
//     `static BoxVolume* Initialize(const rw::Resource&, Vector3)` @0x82BAA188 with its
//     stack-staged resource -- are both transcribed below.
//     ⚠️ MOUNT DEPENDENCY, REPORTED NOT MADE: the bodies for BoxVolume::BoxVolume
//     @0x82BAA0F0, BoxVolume::Initialize @0x82BAA188 and BoxVolume::GetBBox @0x82BA9FC8 live
//     in vendor/renderware/collision/BoxVolume.cpp, which is NEW and NOT YET on
//     tools/build/build_game_exe.bat. Until that echo line is added this TU compiles but the
//     exe cannot link (LNK2019 on BoxVolume::Initialize). GetBBox is additionally not
//     callable from here at all -- its out-parameter type rw::collision::AABBox is only
//     forward-declared in CollisionVolume.hpp on purpose (AABBox.hpp drags in the
//     rw::math::vpu vocabulary this game TU cannot take), which is why the two "Invalid
//     handling body" tripwires stay algebraically folded rather than calling it.
//
// Everything else in all six bodies is real and lands: the entity, its centre and its
// bounding radius, the box half-extents (including the deformation-spec read the old
// OnResourcesLoaded banner said was unreachable -- it is reachable through the handle), the
// two "Invalid handling body" tripwires, the volume instance, the collision registration,
// the culling group and its per-frame republish.
// ============================================================================


namespace BrnWorld
{
namespace
{
    // ------------------------------------------------------------------------
    // KF_HANDLING_BODY_Y_PAD -- MEASURED, value 0.05f.
    //
    // The console reads it as the .data VecFloat `unk_82FAD550`, which is ZERO in the
    // shipped image and is splat-filled at run time by an unnamed C++ dynamic initialiser
    // at 0x82C4C098..0x82C4C0BC (`lfs flt_8201497C ; stfs -0x10(r1) ; lvlx ; vspltw v0,v0,0
    // ; stvx128 -> unk_82FAD550`) from the rodata literal flt_8201497C == 0.05f. A
    // function-export-only scan finds no writer, which is exactly the "zero-initialised
    // unrecoverable global" trap; the value comes from a headless-IDA dump of the database
    // (scratchpad/waveQ5/caradd/q5b_out.txt, q5c_out.txt).
    //
    // Modelled as a scalar because all four splat lanes are identical and only the Y lane
    // is ever consumed (the two `vrlimi128 ...,4,0` inserts).
    //
    // Its sibling `unk_82FAD5E0` -- the Vector3 AddForCollision hands the scene as the
    // collision PADDING -- is initialised the same way at 0x82C4B030..0x82C4B064 from
    // flt_82001CC0 == 0.0f in x/y/z with a literal 0 in w, i.e. it is the ZERO vector.
    // Spelled inline at the call site as Vector3{0,0,0,0} with that attestation.
    const f32 KF_HANDLING_BODY_Y_PAD = 0.05f;

    // The "is this handling body sane" ceiling both AddToScene tripwires compare against
    // (`lfs f31, flt_82009E10` == 1000.0f, splatted and fed to `vcmpgtfp. v0, v11, v0`).
    const f32 KF_MAX_VALID_HANDLING_BODY_EXTENT = 1000.0f;

    // The entity-type flag word AddToScene stamps on the car's scene entity (`li r5, 0x484`
    // @0x822EB904 == 1156). The scene's entity-type FLAG space is a different enum from
    // BrnWorld::EEntityTypeID -- BrnTriggerEntityModule.cpp:47 already records that for
    // triggers (flag 32, owner 4) -- and that enum has no home in this tree, so the console
    // immediate is named here rather than composed out of bits that cannot be checked.
    const u32 KU_RACECAR_SCENE_ENTITY_TYPE_FLAG = 0x484u;

    // The volume-type flag AddDynamicVolume tags the car's box with (`li r6, 1`
    // @0x822EBBDC). Same flag space as the prop module's KU8_PROP_VOLUME_TYPE_FLAG == 16
    // and the trigger module's 1/2/4 -- unhomed, so the immediate is named, not decomposed.
    const u8 KU8_RACECAR_VOLUME_TYPE_FLAG = 1u;

    // DetermineCullingGroup's three results (`li r3, 6` @0x822BF55C; the AI/other pair is
    // the branchless `subfic/subfe/rlwinm 0,29,29/addi 1` tail at 0x822BF5A0..0x822BF5AC,
    // which is 5 when IsAIDriven() and 1 otherwise).
    const u32 KU_CULLING_GROUP_PLAYER_CAR = 6u;
    const u32 KU_CULLING_GROUP_AI_CAR     = 5u;
    const u32 KU_CULLING_GROUP_OTHER_CAR  = 1u;

    // Resolve the resident deformation spec from a ResourceHandle.
    //
    // WHY THIS EXISTS: the console reads the handling-body dimensions through
    // `BrnPhysics::Def...(this + 0x1C90)` @0x822C7708 -- which is
    // CgsResource::ResourcePtr<StreamedDeformationSpec>::operator-> (its baked assert is
    // "Can not instance resource pointer - it has no main memory resource" at
    // CgsResourcePtr.h:544, matching the tree's own accessor exactly) applied to
    // ActiveRaceCar::mDeformationModelResourcePtr. That member is still the opaque
    // `maPad1C90a[20]` span in BrnActiveRaceCar.h because homing the ResourcePtr wrapper
    // would give ActiveRaceCar a non-trivial ctor/dtor its 8-slot array has never had.
    //
    // What IS named is the ResourceHandle the wrapper stores at its own +0x14 --
    // mDeformationModelHandle -- and it addresses the SAME resource: OnRaceCarResourcesLoaded
    // fills it from RaceCarStreamer::GetPhysicsResourceHandle, i.e. from the very
    // ResourcePtr the console dereferences. The double deref below is the tree's established
    // handle->resource idiom, used verbatim by BrnDeformationManager.cpp:128
    // (ResolveDeformationSpec) and by CgsResource::SafeResourceHandle::operator->
    // (CgsResourceHandle.h:401); CgsBaseResourcePtr.cpp:60-90 records the runtime witness
    // that the +0x14 pair resolves through it and the +0x04 pair does not.
    //
    // Returns 0 when the car's physics resource is not resident, which is the case the
    // console's operator-> asserts on.
    const BrnPhysics::Deformation::StreamedDeformationSpec*
    ResolveDeformationSpec(const CgsResource::ResourceHandle& lrHandle)
    {
        if (lrHandle.mpResourceMemory == 0)
        {
            return 0;
        }
        return *reinterpret_cast<const BrnPhysics::Deformation::StreamedDeformationSpec* const*>(
                   lrHandle.mpResourceMemory);
    }

    // dot3 + sqrt, the console's `vmsum3fp128` followed by `vrsqrtefp` and two
    // Newton-Raphson refinements (0x822EB864..0x822EB8A4). The refined reciprocal square
    // root times the input is the square root; `vsel ... vcmpeqfp128 v9, v127, v0` returns
    // 0 for a zero input, which sqrtf(0) also does.
    f32 Length3(const Vector3& lrV)
    {
        return sqrtf(lrV.x * lrV.x + lrV.y * lrV.y + lrV.z * lrV.z);
    }
}

// ============================================================================
// OnHandlingModelAdded @ 0x822F7EF8   (41 insns)
//
// The console's only caller of AddToScene. Two tripwires, then the tail call; r4/r5/f1 are
// forwarded unchanged (0x822F7F7C..0x822F7F8C).
//
//   0x822F7F18  IsAttached()   assert BrnActiveRaceCar.cpp:0x39D == :925
//   0x822F7F50  IsActive()     assert BrnActiveRaceCar.cpp:0x39E == :926
//   0x822F7F8C  bl AddToScene(this, lpSceneInterface, lpVehicleInterface, lfTimeStep)
//
// ⭐ ITS OWN CALLER IS NOW LANDED (updated 2026-08-18, wave Q5 finisher).
// RaceCarEntityModule::ProcessCreateVehicleEvents @0x822FF620 (182 insns) is complete in
// BrnRaceCarEntityModule.cpp and calls this from PostPhysicsUpdate at the console's own slot
// (@0x823075F8); the PublishNewVehicleToDirectorWithoutPhysicsBringUp stand-in that used to
// replace it is retired. So the chain create-result -> OnHandlingModelAdded -> AddToScene ->
// AddToCollision runs in the build.
// ============================================================================
void ActiveRaceCar::OnHandlingModelAdded(
        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface,
        BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInterface,
        f32 lfTimeStep )
{
    CGS_ASSERT(IsAttached(), "IsAttached()");   // X360 :925
    CGS_ASSERT(IsActive(),   "IsActive()");     // X360 :926

    AddToScene(lpSceneInterface, lpVehicleInterface, lfTimeStep);
}

// ============================================================================
// AddToScene @ 0x822EB768   (332 insns)  -- THE CAR'S SCENE REGISTRATION.
//
// Asm order, leg by leg (every offset below is read off the listing in
// scratchpad/waveQ5/caradd/asm_0x822EB768.txt):
//
//   0x822EB78C  IsActive()                 assert :0x4D8 == :1240
//   0x822EB7C0  lpSceneInterface != NULL   assert :0x4D9 == :1241
//   0x822EB7E4  r3 = this + 0x1C90 -> BrnPhysics::Def... (ResourcePtr<StreamedDeformationSpec>
//               ::operator->)  ->  lpSpec
//   0x822EB80C  lvx128 v0, [this + 0xC0]        == mCentreOfMassTransform.wAxis (row 3)
//   0x822EB814  vspltw v10, v0, 1              == splat(COM.y)
//   0x822EB81C  lvx128 v0, [lpSpec + 0x40]     == StreamedDeformationSpec::mHandlingBodyDimensions
//                                                 (the header's own static_assert pins +64)
//   0x822EB834  vandc v12, v10, <sign mask>    == fabs(COM.y)
//   0x822EB83C  vsubfp / vmaxfp / vaddfp       == max(fabs(COM.y) - dims.y, 0) + 0.05f
//   0x822EB850  vmulfp128 v124, v13, 0.5       == the HALF-DELTA, kept in a callee-saved
//                                                 vector register for the whole body
//   0x822EB858  v0 = dims + halfDelta ; vrlimi v9, v0, 4, 0  -- ONLY the Y lane is taken
//   0x822EB860  stvx -> var_240                == the box half-extents
//   0x822EB864  vmsum3fp128 + rsqrt refine -> var_280 == Length3(halfExtents)
//   0x822EB8AC  IsAttached()                   assert BrnActiveRaceCar.h:0x441 == :1089
//   0x822EB8E4  ld [this+0xD0] ; srdi 32       == the EMBEDDED 32-BIT ENTITY WORD
//   0x822EB8F8  RaceCar::GetPosition(mpRaceCar)
//   0x822EB914  AddEntity(entityWord, 0x484, position, Length3(halfExtents))
//   0x822EB948  BoxVolume::Initialize(<128-byte image>, halfExtents)  -> lpVolume
//   0x822EB95C  BoxVolume::GetBBox(lpVolume, NULL, 1, &var_230)
//   0x822EB964  IsAttached()                   assert h:1089
//   0x822EB994  RaceCar::GetModelId -> CgsIDUnCompress(id, &var_270)  -- the assert MESSAGE only
//   0x822EBA1C  1000.0f > Length3(bbox.Min())  assert :0x4FA == :1274  "\nInvalid handling
//                                                                      body on race car: <id>"
//   0x822EBB30  1000.0f > Length3(bbox.Max())  assert :0x4FB == :1275  (same message)
//   0x822EBBB0  lpVolume != NULL               assert :0x4FF == :1279  "NULL != lpVolume"
//   0x822EBBD4  [lpVolume+0x30].y -= halfDelta.y                       [FLAG BLOCKED 2]
//   0x822EBC08  AddDynamicVolume(mHandlingBodyVolumeId, lpVolume, 1)   LANDED (wave Q5 fin.)
//   0x822EBC18  IsAttached()                   assert h:1089
//   0x822EBC54  RaceCar::GetTransform(mpRaceCar)
//   0x822EBC68  AddVolumeInstance(mHandlingBodyVolumeId, mHandlingBodyVolumeId, transform)
//   0x822EBC78  AddToCollision(lpSceneInterface, lpVehicleInterface)
//   0x822EBC80  stb 1, 0x78A                   mbAddedToScene = true
//
// FOUR THINGS A READER WILL GET WRONG, all measured:
//
//  1. THE ID PASSED TO AddVolumeInstance IS THE SAME 64-BIT WORD TWICE. `ld r5,0(r31)` and
//     `ld r4,0(r29)` (0x822EBC5C / 0x822EBC64) read two different stack slots that were both
//     just filled from the SAME `ld r11,0xD0(r30)` (0x822EBC4C / 0x822EBC50). The car's
//     VolumeInstanceId and its VolumeId are one and the same handle -- unlike a prop, which
//     carries a separate PropVolumeID.
//
//  2. THE BOX GROWS IN Y ONLY. Both `vrlimi128 ...,4,0` inserts take lane 1 (Y) and leave
//     x/z/w alone, and the pre-delta terms are splats of ONE lane each. Applying the delta to
//     all three axes -- the obvious misreading of a splat -- inflates the car's collision box
//     by up to a metre in X and Z.
//
//  3. THE ENTITY GETS THE 32-BIT ENTITY WORD, THE VOLUME/INSTANCE GET THE 64-BIT HANDLE.
//     `srdi r11,r11,32 ; clrlwi r31,r11,0` before AddEntity, a bare `ld` before the other
//     two. Exactly the asymmetry RemoveFromScene already documents in the other direction.
//
//  4. THE FLOAT ARGUMENT IS NEVER READ. f1 arrives and is never touched; the f31 spill in
//     the prologue is the callee-save of the scratch register the 1000.0f literal later
//     occupies (`lfs f31, flt_82009E10` @0x822EB9C0). It is not the parameter.
//
// PC-SAFETY GUARD (not console behaviour, stated so): the console asserts on a NULL
// lpSceneInterface and then dereferences it anyway. The early return below mirrors the one
// RemoveFromScene already carries in BrnActiveRaceCar.cpp -- same file, same convention.
// ============================================================================
void ActiveRaceCar::AddToScene(
        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface,
        BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInterface,
        f32 lfTimeStep )
{
    CGS_ASSERT(IsActive(), "IsActive()");                              // X360 :1240
    CGS_ASSERT(lpSceneInterface != 0, "lpSceneInterface != NULL");     // X360 :1241
    if (lpSceneInterface == 0)
    {
        return;
    }

    (void)lfTimeStep;   // arrives in f1; no instruction in the console body reads it

    // ---- 1. the handling-body box half-extents -------------------------------------
    const BrnPhysics::Deformation::StreamedDeformationSpec* lpSpec =
        ResolveDeformationSpec(mDeformationModelHandle);
    CGS_ASSERT(lpSpec != 0,
               "Can not instance resource pointer - it has no main memory resource\n");
    if (lpSpec == 0)
    {
        return;
    }

    const Vector3& lrHandlingBodyDimensions = lpSpec->mHandlingBodyDimensions;   // spec + 0x40

    // fabs(COM.y) is how far the handling frame's origin sits from the model origin; the
    // box is stretched by half of whatever that exceeds the authored half-height, plus the
    // 0.05 m pad, and (leg 2) its centre is dropped by the same amount so the growth is
    // downward. Note both operands are SPLATS of one lane each in the asm -- the vector
    // arithmetic is scalar arithmetic on Y.
    const f32 lfComY      = mCentreOfMassTransform.wAxis.y;                       // this + 0xC0
    const f32 lfAbsComY   = fabsf(lfComY);                                        // vandc <sign mask>
    const f32 lfOverhang  = lfAbsComY - lrHandlingBodyDimensions.y;               // vsubfp
    // `(x > 0) ? x : 0` is vmaxfp(x, 0) INCLUDING NaN polarity (gotcha 4): the VMX compare is
    // unordered-false, so a NaN operand yields the second source, which is the zero here too.
    const f32 lfHalfDelta =
        0.5f * (((lfOverhang > 0.0f) ? lfOverhang : 0.0f) + KF_HANDLING_BODY_Y_PAD); // vmaxfp/vaddfp/vmulfp

    Vector3 lHalfExtents = lrHandlingBodyDimensions;
    lHalfExtents.y = lrHandlingBodyDimensions.y + lfHalfDelta;   // vrlimi128 v9, v0, 4, 0

    const f32 lfBoundingRadius = Length3(lHalfExtents);          // vmsum3fp128 + rsqrt refine

    // ---- 2. the scene entity --------------------------------------------------------
    CGS_ASSERT(IsAttached(), "IsAttached()");                    // BrnActiveRaceCar.h:1089

    // The console hands AddEntity only the embedded 32-bit entity word (`srdi r11,r11,32`),
    // never the whole handle -- the mirror image of RemoveFromScene's RemoveEntity post.
    const CgsSceneManager::EntityId lEntityId(
        static_cast<u32>(mHandlingBodyVolumeId.muId >> 32));

    lpSceneInterface->AddEntity(lEntityId,
                                KU_RACECAR_SCENE_ENTITY_TYPE_FLAG,
                                GetGlobalRaceCar()->GetPosition(),
                                lfBoundingRadius);

    // ---- 3. the collision volume ----------------------------------------------------
    // REWRITTEN 2026-08-18 (wave Q5 finisher) against the REAL rw::collision::BoxVolume,
    // which landed in vendor/renderware/collision/CollisionVolume.hpp the same day. It
    // replaced the 128-byte placeholder whose `bool Initialize(f32,f32,f32)` this leg used to
    // call; the real one is the DWARF/asm shape
    //     static BoxVolume* Initialize(const rw::Resource&, Vector3)   @0x82BAA188
    // and it is exactly what the console calls here.
    //
    // The console stages the resource itself, on the stack, at 0x822EB918..0x822EB944:
    //     addi r11, r1, var_260        ; the rw::Resource record
    //     stw  r31(=0), 0(r11) 4(r11) 8(r11) 0xC(r11) 0x10(r11)   ; FIVE words zeroed
    //     addi r11, r1, var_1D0        ; the volume's own memory block
    //     stw  r11, var_260(r1)        ; block pointer into WORD 0
    //     lvx128 v1, r0, r10(=var_240) ; the half-extents Vector3
    //     bl   BoxVolume::Initialize   ; r3 = &var_260
    // Initialize reads only word 0 (`lwz r3, 0(r3)`) and hands it to the ctor, so the X360's
    // five-entry record is retained by the PC rw::Resource. Every word is zeroed first, exactly as the console
    // does, so nothing but word 0 is ever meaningful.
    //
    // The block stays 128 bytes even though sizeof(BoxVolume) == 96: AddDynamicVolume
    // block-copies 128 bytes FROM THE VOLUME POINTER (`li r5, 0x80` @0x822B1534), so the
    // console over-reads 32 bytes of its own stack past the record and the host must have
    // those bytes addressable too. Shrinking this to 96 is a buffer overrun, not a tidy-up.
    u8 laVolumeStorage[128];

    // (Same two lines the tree's other producer uses -- BrnTriggerEntityModule.cpp's three
    // volume branches -- so the two consumers of this SDK entry point stay spelled alike.)
    rw::Resource lVolumeResource = {};                            // stw 0, 0/4/8/0xC/0x10
    lVolumeResource.m_baseResources[0] = laVolumeStorage;         // stw r11, var_260(r1)

    rw::collision::BoxVolume* lpVolume = rw::collision::BoxVolume::Initialize(
        lVolumeResource, lHalfExtents.x, lHalfExtents.y, lHalfExtents.z);

    CGS_ASSERT(IsAttached(), "IsAttached()");                    // BrnActiveRaceCar.h:1089

    // The console composes the assert MESSAGE here -- CgsIDUnCompress(mpRaceCar->GetModelId(),
    // buf) feeding "\nInvalid handling body on race car: " << buf into the assert buffer
    // (0x822EB994..0x822EBAA4 and again at 0x822EBB74..0x822EBB98). The condition is what
    // matters; per this file's established convention (see UpdatePhysicsState's banner in
    // BrnActiveRaceCar.cpp) the runtime string composition is diagnostic-only and is not
    // rebuilt by hand.
    //
    // THE TWO CONDITIONS ARE OVER THE BOX'S OWN AABB, and it is derivable exactly here rather
    // than through rw::collision::BoxVolume::GetBBox. GetBBox @0x82BA9FC8 now HAS a body
    // (vendor/renderware/collision/BoxVolume.cpp, landed 2026-08-18) -- it is still not called
    // from here because it is NOT SPELLABLE from this translation unit: its out-parameter type
    // is rw::collision::AABBox, and CollisionVolume.hpp:89-96 deliberately only NAMES that
    // class because AABBox.hpp drags in the EATech rw::math::vpu vector vocabulary, which this
    // GAME translation unit (vendor-POD Vector3 world) cannot include. That is the rwcollision
    // owner's own stated constraint, not an avoidance.
    // The substitution is EXACT, not a convenience: GetBBox with a NULL transform argument
    // reads the volume's four transform rows, folds |row| * halfExtents + radius, and returns
    // {t - v, t + v}. The box was built one instruction earlier by BoxVolume::BoxVolume
    // @0x82BAA0F0, which writes the IDENTITY rows (gIVector / unk_82181510 / unk_82181520), a
    // ZERO translation row and a ZERO fatness (`lfs f0, flt_82001CC0 == 0.0f ; stfs f0,
    // 0x50(r3)`). So v == halfExtents, t == 0, and the AABB is exactly
    // {-halfExtents, +halfExtents}: both tripwires reduce to the same magnitude test, which is
    // why the console emits the identical VMX block twice. (The transform tweak below happens
    // AFTER GetBBox, so it does not enter either test.)
    CGS_ASSERT(KF_MAX_VALID_HANDLING_BODY_EXTENT > lfBoundingRadius,
               "Invalid handling body on race car");             // X360 :1274 (bbox.Min())
    CGS_ASSERT(KF_MAX_VALID_HANDLING_BODY_EXTENT > lfBoundingRadius,
               "Invalid handling body on race car");             // X360 :1275 (bbox.Max())

    // `cmplwi cr6, r21, 0 ; bne` @0x822EBBB0 -- the console's own null test on Initialize's
    // RESULT, now spellable exactly (the placeholder's invented bool return is gone).
    CGS_ASSERT(lpVolume != 0, "NULL != lpVolume");               // X360 :1279

    // ---- the volume's transform row ([FLAG BLOCKED 2] RETIRED 2026-08-18) ------------
    // 0x822EBBD4..0x822EBBF8, and it is scalar arithmetic on ONE lane:
    //     addi   r11, r21, 0x30     ; &lpVolume->transform.wAxis  (Volume::maTransform[3])
    //     lvx128 v0,  r0,  r11
    //     vmr    v13, v0            ; keep x/z/w
    //     vsubfp128 v0, v0, v124    ; v124 == splat(lfHalfDelta)
    //     vrlimi128 v13, v0, 4, 0   ; mask bit 4 == lane 1 == Y ONLY
    //     stvx128 v13, r0, r11
    // i.e. the box's CENTRE drops by the same half-delta its Y half-extent just gained, so it
    // grows DOWNWARD and its top face stays on the authored handling-body top
    // (top = -halfDelta + (dims.y + halfDelta) = dims.y). Applying the delta to x/z as well --
    // the obvious misreading of the splat -- would shift the car's box sideways.
    //
    // Spellable now because rw::collision::Volume carries the real `Vec4 maTransform[4]` with
    // maTransform[3] the translation row, pinned by that header's own
    // static_assert(offsetof(Volume, maTransform) == 0x00) plus the +0x30 row offset the asm
    // addresses. The console dereferences lpVolume here unconditionally (it asserts one
    // instruction earlier and carries on), and so does this -- no PC null guard is added,
    // because Initialize only returns NULL for a resource with no block and the resource above
    // always carries one.
    lpVolume->maTransform[3].y -= lfHalfDelta;

    // ---- the AddDynamicVolume post (UNPARKED 2026-08-18, wave Q5 finisher) -----------
    // 0x822EBBFC..0x822EBC08: `ld r11, 0xD0(r30) ; std ; ld r4, 0(r10)` -- the WHOLE 8-byte
    // handle -- then `li r6, 1` and the call. The DWARF's own 64-bit overload
    // (CgsSceneManagerIO_SceneUpdate.h:367, landed in that file's own TU this wave) is what
    // makes this spellable; the committed 32-bit EntityId form would narrow the handle to its
    // identically-zero low dword and post volume key 0 for all eight cars.
    lpSceneInterface->AddDynamicVolume(
        CgsSceneManager::VolumeId(mHandlingBodyVolumeId.muId),   // ld r4 -- ALL 64 BITS
        laVolumeStorage,                                        // r5
        KU8_RACECAR_VOLUME_TYPE_FLAG);                          // r6 == 1

    // ---- 4. the volume instance -----------------------------------------------------
    CGS_ASSERT(IsAttached(), "IsAttached()");                    // BrnActiveRaceCar.h:1089

    // Both ids are the SAME 64-bit handle (see note 1 in the banner).
    lpSceneInterface->AddVolumeInstance(mHandlingBodyVolumeId,
                                        CgsSceneManager::VolumeId(mHandlingBodyVolumeId.muId),
                                        GetGlobalRaceCar()->GetTransform());

    // ---- 5. the collision registration ----------------------------------------------
    AddToCollision(lpSceneInterface, lpVehicleInterface);

    mbAddedToScene = true;                                       // 0x78A
}

// ============================================================================
// AddToCollision @ 0x822D41F0   (138 insns)
//
//   0x822D4208  IsActive()                assert :0x56B == :1387
//   0x822D423C  lpSceneInterface != NULL  assert :0x56C == :1388
//   0x822D4260  lpVehicleInput  != NULL   assert :0x56D == :1389   (the console's own
//                                                                   parameter spelling)
//   0x822D4284  !mbAddedForCollision      assert :0x56E == :1390
//   0x822D42AC  lwz 0x744 ; cmpwi 0 ; beq epilogue
//                    -> the WHOLE body is gated on meOnlineState != E_ONLINE_STATE_CONNECTING
//   0x822D42C0  UpdateCullingGroup(lpSceneInterface)
//   0x822D42C4  the gxMessageFilterFlags & 1 debug line ("Adding for collision: " id "\n")
//   0x822D4334  AddForCollision(mHandlingBodyVolumeId, mCurrentCullingGroup,
//                               ACTIVE_BODY, <zero padding>, E_DO_NOT_ADD_TO_CACHE_MANAGER)
//   0x822D433C  IsAttached()              assert BrnActiveRaceCar.h:0x441 == :1089
//   0x822D437C  mpRaceCar->GetType() == E_RACE_CAR_TYPE_NETWORK -> the re-add post
//   0x822D4404  stb 1, 0x78D / 0x78E / 0x78B
//
// THE ARGUMENT CONSTANTS, and why they are NOT the prop module's:
//   * BodyState is `li r6, 4` == rw::physics::ACTIVE_BODY. PropCellManager::
//     AddPropToContactGeneration passes 2 (FROZEN_BODY) at the same slot -- a car is a live
//     body, a prop is not. Do not harmonise them.
//   * CacheOptions is `li r7, 2` == E_DO_NOT_ADD_TO_CACHE_MANAGER, the same as the props'.
//   * The padding Vector3 is `lvx128 v1, [unk_82FAD5E0]`, a .data VecFloat that the
//     dynamic initialiser at 0x82C4B030 fills with {0,0,0} + a literal 0 w -- the ZERO
//     vector, NOT the prop path's authored pad.
//   * The culling group is mCurrentCullingGroup (`lwz r5, 0x7D0`), which UpdateCullingGroup
//     has just refreshed one call earlier. It is 6/5/1, never the prop module's 2.
//
// ⚠️ REPORTED, NOT FIXED (it is a scene-manager question, not this cluster's): those 6/5/1
// groups are exactly three of the ten pairs WorldModule::Prepare stage 3 clears against the
// prop groups (`SetCullingGroupPair(7,1,0) / (7,6,0) / (7,5,0)` and the 8-side twins, asm
// 0x827D565C..0x827D5740), while PropEntityModule::InitializePropPhysicsData only ever sets
// (2,7,1) / (2,8,1). Whether that means car-vs-prop overlaps are CULLED depends on the bit
// polarity of the culling table, and the consumer that reads it (SceneSweeper::
// BuildCollidingPairs @0x828C2190) is not reconstructed, so the polarity is undetermined in
// this tree. See scratchpad/waveQ5/caradd.owner.md §6 -- it is a wave-level question.
// ============================================================================
void ActiveRaceCar::AddToCollision(
        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface,
        BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInterface )
{
    CGS_ASSERT(IsActive(), "IsActive()");                                // X360 :1387
    CGS_ASSERT(lpSceneInterface != 0, "lpSceneInterface != NULL");       // X360 :1388
    CGS_ASSERT(lpVehicleInterface != 0, "lpVehicleInput != NULL");       // X360 :1389
    CGS_ASSERT(!mbAddedForCollision, "!mbAddedForCollision");            // X360 :1390

    (void)lpVehicleInterface;   // asserted, then never read (r30 is reused at 0x822D42E8)

    if (meOnlineState == E_ONLINE_STATE_CONNECTING)
    {
        return;
    }
    if (lpSceneInterface == 0)   // PC-safety guard, as in AddToScene above
    {
        return;
    }

    UpdateCullingGroup(lpSceneInterface);

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint << "Adding for collision: "
                                   << mHandlingBodyVolumeId.muId << "\n";
    }

    lpSceneInterface->AddForCollision(
        mHandlingBodyVolumeId,
        static_cast<CgsSceneManager::SceneManagerIO::InEventAddForCollision::CullingGroup>(
            mCurrentCullingGroup),                                       // lwz 0x7D0
        rw::physics::ACTIVE_BODY,                                        // li r6, 4
        Vector3{ 0.0f, 0.0f, 0.0f, 0.0f },                               // lvx [unk_82FAD5E0]
        CgsSceneManager::SceneManagerIO::E_DO_NOT_ADD_TO_CACHE_MANAGER); // li r7, 2

    CGS_ASSERT(IsAttached(), "IsAttached()");                            // BrnActiveRaceCar.h:1089

    // [FLAG BLOCKED] the E_RACE_CAR_TYPE_NETWORK re-add post -- the exact mirror of the one
    // RemoveFromCollision @0x822BF668 already parks, and blocked on the same member:
    //   0x822D437C  if (mpRaceCar->GetType() == E_RACE_CAR_TYPE_NETWORK)
    //   0x822D4388      if (miLength >= miMaxLength) assert :0x583 == :1411
    //                       "Trying to add/remove race car more than <max> times in a frame"
    //   0x822D43FC      post { mHandlingBodyVolumeId, mbAdded = TRUE } onto THIS OBJECT'S own
    //                   mAddRemoveNetworkCarForCollisionQueue (r3 == this at the AddEvent).
    // That member is still `u8 maPad0000[144]` in BrnActiveRaceCar.h and ActiveRaceCar's
    // construction path never calls EventQueue::Construct on it, so naming it would put a
    // live mpEvents pointer at offset 0 of an unconstructed queue. The arm is UNREACHABLE on
    // this build (E_RACE_CAR_TYPE_NETWORK needs an online session). Note the polarity: the
    // ADD post sets mbAdded TRUE where RemoveFromCollision's sets it FALSE.
    // DELETE-WHEN mAddRemoveNetworkCarForCollisionQueue is named AND Construct constructs it.

    mbChangeCollisionState     = true;    // 0x78D
    mbCollisionStateToChangeTo = true;    // 0x78E   (RemoveFromCollision writes false here)
    mbAddedForCollision        = true;    // 0x78B
}

// ============================================================================
// SendSceneUpdatesPostPhysics @ 0x822EB6B0   (46 insns)
//
//   0x822EB6CC  lpSceneInterface != NULL  assert :0x443 == :1091
//   0x822EB6F8  IsActive()                assert :0x444 == :1092
//   0x822EB72C  UpdateCullingGroup(lpSceneInterface)
//   0x822EB730  lbz 0x77A ; cmplwi 1 ; bne skip     -- mbUncrashedThisFrame == true
//   0x822EB73C      lbz 0x78B ; bne skip            -- and NOT already added for collision
//   0x822EB754          AddToCollision(lpSceneInterface, lpVehicleInterface)
//   0x822EB758      stb 0, 0x77A                    -- consume the flag either way
//
// ⚠️ THE FLAG CLEAR IS OUTSIDE THE INNER TEST. A car that was un-crashed this frame while
// ALREADY added for collision still clears mbUncrashedThisFrame; only the AddToCollision
// call is skipped. Moving the clear inside the inner `if` would leave the flag latched.
//
// The float argument is again unread (f1 is never touched between prologue and epilogue);
// its caller SendRaceCarSceneUpdates @0x822F6B08 loads it from module+0x18398.
// ============================================================================
void ActiveRaceCar::SendSceneUpdatesPostPhysics(
        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface,
        BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInterface,
        f32 lfTimeStep )
{
    CGS_ASSERT(lpSceneInterface != 0, "lpSceneInterface != NULL");   // X360 :1091
    CGS_ASSERT(IsActive(), "IsActive()");                            // X360 :1092
    if (lpSceneInterface == 0)   // PC-safety guard, as in AddToScene above
    {
        return;
    }

    (void)lfTimeStep;   // arrives in f1; no instruction in the console body reads it

    UpdateCullingGroup(lpSceneInterface);

    if (mbUncrashedThisFrame)                                        // 0x77A
    {
        if (!mbAddedForCollision)                                    // 0x78B
        {
            AddToCollision(lpSceneInterface, lpVehicleInterface);
        }
        mbUncrashedThisFrame = false;
    }
}

// ============================================================================
// UpdateCullingGroup @ 0x822BF5B8   (44 insns)
//
//   0x822BF5D4  IsActive()                     assert :0x59F == :1439
//   0x822BF608  DetermineCullingGroup()
//   0x822BF60C  lwz 0x7D0 ; cmplw ; beq epilogue   -- nothing to do when unchanged
//   0x822BF620  lbz r10, 0x78B                 -- mbAddedForCollision, READ BEFORE the stores
//   0x822BF624  stw 0x7D0                      mCurrentCullingGroup     = new
//   0x822BF62C  stw 0x790                      mCullingGrouptoChangeTo  = new
//   0x822BF630  stb 1, 0x78F                   mbChangeCullingGroup     = true
//   0x822BF634  beq epilogue                   -- only publish if already added
//   0x822BF64C  SetVolumeInstanceCullingGroup(mHandlingBodyVolumeId, new)
//
// The compare is `cmplw` (UNSIGNED). It matters: RemoveFromCollision parks
// mCurrentCullingGroup at 0xFFFF, so the first UpdateCullingGroup after a re-add compares
// 0xFFFF against 1/5/6 and always takes the change path -- which is the point of the park.
// ============================================================================
void ActiveRaceCar::UpdateCullingGroup(
        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface )
{
    CGS_ASSERT(IsActive(), "IsActive()");                            // X360 :1439

    const BrnPhysics::Vehicle::SetRaceCarCullingGroupEvent::CullingGroup lCullingGroup =
        DetermineCullingGroup();

    if (lCullingGroup == static_cast<u32>(mCurrentCullingGroup))
    {
        return;
    }

    const bool lbWasAddedForCollision = mbAddedForCollision;          // read at 0x822BF620

    mCurrentCullingGroup    = static_cast<s32>(lCullingGroup);        // 0x7D0
    mCullingGrouptoChangeTo = static_cast<s32>(lCullingGroup);        // 0x790
    mbChangeCullingGroup    = true;                                   // 0x78F

    // (`lpSceneInterface != 0` is the same PC-safety guard the two callers already carry; the
    //  console's test here is the mbAddedForCollision byte alone.)
    if (lbWasAddedForCollision && lpSceneInterface != 0)
    {
        lpSceneInterface->SetVolumeInstanceCullingGroup(mHandlingBodyVolumeId,
                                                        static_cast<s32>(lCullingGroup));
    }
}

// ============================================================================
// DetermineCullingGroup @ 0x822BF4D8   (56 insns)
//
//   0x822BF4E8  IsAttached()                   assert BrnActiveRaceCar.h:0x441 == :1089
//   0x822BF524  lbz 0xA4(mpRaceCar) ; cmplwi 4 -- RaceCar::GetType()'s own inlined tripwire
//               "muType < E_RACE_CAR_TYPE_COUNT" (..\..\..\GameSource\World/EntityMod...:577)
//   0x822BF550  if (type == E_RACE_CAR_TYPE_PLAYER)  return 6
//   0x822BF56C  IsAttached()                   assert h:1089  (the second inlined GetGlobalRaceCar)
//   0x822BF598  RaceCar::IsAIDriven()
//   0x822BF5A0  subfic / subfe / rlwinm 0,29,29 / addi 1   ->   IsAIDriven() ? 5 : 1
//
// The branchless tail is `(-(bool) & 4) + 1`, i.e. 5 when AI-driven and 1 otherwise -- it is
// not a table lookup and there is no fourth value. E_RACE_CAR_TYPE_NETWORK and
// E_RACE_CAR_TYPE_INACTIVE both fall in the "1" bucket.
//
// Bodied here because it has no definition anywhere in the tree and UpdateCullingGroup above
// would otherwise be an unresolved external (gotcha 12: `cl /c` cannot see that).
// ============================================================================
BrnPhysics::Vehicle::SetRaceCarCullingGroupEvent::CullingGroup
ActiveRaceCar::DetermineCullingGroup()
{
    CGS_ASSERT(IsAttached(), "IsAttached()");                        // BrnActiveRaceCar.h:1089

    const RaceCar* lpRaceCar = GetGlobalRaceCar();
    if (lpRaceCar == 0)
    {
        // PC-SAFETY GUARD, not console behaviour: the console asserts IsAttached() (which IS
        // `mpRaceCar != NULL`) and then dereferences mpRaceCar unguarded. Unreachable whenever
        // the assert holds; the "other car" bucket is the arm a detached slot would fall into
        // anyway, and returning it keeps UpdateCullingGroup's compare well-defined.
        return KU_CULLING_GROUP_OTHER_CAR;
    }

    if (lpRaceCar->GetType() == E_RACE_CAR_TYPE_PLAYER)
    {
        return KU_CULLING_GROUP_PLAYER_CAR;
    }

    CGS_ASSERT(IsAttached(), "IsAttached()");                        // BrnActiveRaceCar.h:1089

    return lpRaceCar->IsAIDriven() ? KU_CULLING_GROUP_AI_CAR
                                   : KU_CULLING_GROUP_OTHER_CAR;
}

}
