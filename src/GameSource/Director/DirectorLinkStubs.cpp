// ============================================================================
// GameSource/Director/DirectorLinkStubs.cpp
//
// PC LINK-MOUNT STUBS for the DirectorModule mount: declaration-only callees of the
// mounted director spine whose owning TU is not in the link (the TU does not exist, or
// mounting it would drag an un-landed sub-system in behind it). Each gets a MARKED,
// QUIET stub here rather than a fabricated body inside a real header:
//
//   * every stub carries WHY it is a stub and a DELETE-WHEN note;
//   * no stub traps on a per-frame path -- a trap would make the exe unusable;
//   * a stub that must return a value returns the console's own "nothing happened" value;
//   * NO stub returns a reference to a fabricated object. Where a symbol returns a reference
//     the stub is omitted and the caller's TU is kept out of the link instead.
//
// When a real TU lands for any symbol below, DELETE its stub here (a duplicate definition
// is a link error, so the removal is enforced by the build).
//
// GROUP C -- sub-systems with no landed TU (the director DebugComponent, the
//   scene-query post-office free functions).
// GROUP D -- the vendor rw SLerp leaf.
// GROUP F -- the moment sub-system (MomentController::NewMoment).
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"

#include "GameSource/Director/BrnDirectorResourceManager.h"
#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugCompononent.h"
#include "GameSource/Director/Utils/BrnSceneQueryInterface.h"
#include "GameSource/Director/Camera/BrnBehaviourManager.h"

#include "GameSource/Director/Arbitrator/States/BrnArbStateCarSelect.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStateDriveThru.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStatePostEvent.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStateRaceIntro.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStateRankUp.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStateRoaming.h"

#include "GameSource/Director/Camera/Behaviours/BehaviourPassengerCam.h"           // the DWARF home (NOT the stale BrnBehaviourPassengerCam.h slice)

#include "GameSource/Director/Camera/Utils/BrnCameraShake.h"                       // group E
#include "GameSource/Director/Camera/Utils/BrnCameraSphericalRotationController.h" // group E

#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h"

#include "SharedClasses/Traffic/BrnTrafficSection.h"
#include "SharedClasses/Trigger/BrnGenericRegion.h"
#include "SharedClasses/Trigger/BrnRegion.h"
#include "SharedClasses/Trigger/BrnTriggerData.h"

// ----------------------------------------------------------------------------
// GROUP C -- sub-systems with no landed TU.
// ----------------------------------------------------------------------------
namespace BrnDirector
{
    // -- The three still-gated functions of the director's own CgsDev::DebugComponent page
    //    ("Camera"); the rest of the page lives in
    //    DirectorModule/BrnDirectorModuleDebugCompononent.cpp. These three index
    //    DirectorModule regions this reconstruction does not model yet.
    //    QUIET no-ops: the Camera page registers no variables and draws no overlay.
    void DebugComponent::UpdatePanoramaScreenshots(Camera::Camera* lpCamera)
    {
        (void)lpCamera;
    }

    void DebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpRender)
    {
        (void)lpRender;
    }

    void DebugComponent::OnActivate() {}

    // -- The two scene-query post-office free functions BrnSceneQueryInterface.h declares.
    //    OutEventVolumeTestDeepest mints the 16-bit query id for a staged volume test; 0 is
    //    the console's "no id" value and SceneQueryInterface treats it as a failed post.
    //    sub_8221CC98 resets slot 1's post office and returns a pointer into it.
    //    DELETE-WHEN: the two post-office TUs land.
    u32* sub_8221CC98(void* lpSlot)
    {
        return static_cast<u32*>(lpSlot);
    }
}

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    u32 OutEventVolumeTestDeepest(void* lpPostOffice, void* lpQueryParams)
    {
        (void)lpPostOffice;
        (void)lpQueryParams;
        return 0u;
    }
}
}

// ----------------------------------------------------------------------------
// GROUP D -- the vendor leaf with no reconstructed body.
// ----------------------------------------------------------------------------
namespace rw
{
namespace math
{
namespace vpu
{
    // The vendor affine-matrix SLerp (declared in rw/math/vpu/matrix44affine_operation.h,
    // body owned by the SDK and not reconstructed). ONE caller reaches it:
    // InertiaController::Update, and only on the branch where the camera has
    // requested LAG (`1 - CameraEffects::mfCameraLag < 1`, i.e. lag > 0). With no lag the
    // console returns before the call, which is the state on every frame of this build.
    // The stub therefore returns lrTo unchanged -- exactly the t == 1 endpoint, i.e. "adopt
    // the freshly-finalised transform", which is what the no-lag path already does. It is the
    // inert answer, not an approximation of the interpolation.
    // DELETE-WHEN: the vendor op is reconstructed (then camera lag starts working).
    Matrix44Affine SLerp(const Matrix44Affine& lrFrom, const Matrix44Affine& lrTo,
                         const float* lpafBlend)
    {
        (void)lrFrom;
        (void)lpafBlend;
        return lrTo;
    }
}
}
}

// GROUP F -- THE MOMENT SUB-SYSTEM (the establishing-shot / jump-cutaway camera).
// ONE gate remains: MomentController::NewMoment, below. The two trap stubs that used to stand
// with it (MomentBystanderSeesAction::SetPerceivedDistanceModificationFactor,
// MomentTumbling::SignalIsGoodTimeToPlant) are gone: both TUs are mounted.
//
// It is a camera blocker: NewMoment allocates nothing, so every MomentHandle stays
// !IsAllocated(), MomentSelector::Update's classification loop skips them all, muValidMoments
// is pinned at 0, and ArbStateRoaming::Update's DRIVING arm never calls SelectBestMoment.
//
// Mounting the rest of the closure (BrnMomentControllerNewMoment.cpp + the two Moments/*.cpp
// that are still unmounted -- PlayerJumping and TakedownLookback) costs 77 non-CRT unresolved
// externals. Re-measured 2026-09-11 against a baseline built by RECOMPILING every one of the
// 2,343 mounted TUs with the shipping flags and archiving them into one library: the previous
// census read 71 off the checked-in object directory, which still held objects for TUs whose
// source had moved on, so it credited the link with bodies it no longer has. 77 is the honest
// number for the same closure; nothing regressed between the two readings.
// PlayerStunt is out of the closure entirely -- it measures ZERO and is mounted.
// By family:
//     37  per-moment-class virtuals and privates (Destruct / SetParameters / Prepare /
//         GetInstanceType / MomentFailSafe::Release, plus MomentPlayerJumping::UpdateCamera)
//     24  BehaviourCollection<T,P,N> methods -- six template methods x four instantiations
//      6  BehaviourRig's virtual set + Parameters::Construct. Camera/Behaviours/BehaviourRig.cpp
//         HOLDS all six and now COMPILES (the four const VehicleInfo* conversions and the
//         Looker::Update VecFloat fork are repaired). It is still unmounted, and its whole
//         remaining cost is TWO symbols: Utils::CameraRig::Construct, which has no body
//         anywhere in the tree, and Utils::PositionLag::Update, whose home BrnPositionLag.cpp
//         is itself blocked on six unbodied camera-serialiser leaf overloads.
//      5  the four vector-deleting destructors the takedown look-back's rig handle emits
//         (Behaviour / BehaviourRig / CollisionPolicy / VisibilityCollisionPolicy) plus
//         BehaviourRig::GetCollisionPolicy
//      3  detail:: reach shims -- the takedown look-back's two resolved-vehicle lanes
//         (Vehicle_GetPosition / Vehicle_GetSegmentReference, which want VehicleRef::Get's
//         return type carved) and BehaviourRig_StartLookingAtRaceCarSnapped
//      2  BehaviourParameterBank accessors -- and only the two INDEXED ones are left
//         (GetPlayerJumpingRigShotParams / GetPlayerJumpingBystanderShotParams). The four
//         single-block moment accessors are bodied; see that header's RECORD MAP for why the
//         indexed pair needs its call sites renumbered before it can follow.
// (operator delete(void*,size_t) and type_info's vftable also come up unresolved against the
// object list and are NOT counted above: the whole build already leaves both to the CRT.)
// Per-TU, each measured alone against that recompiled baseline:
//     NewMoment 36 | PlayerJumping 27 | TakedownLookback 14 | PlayerStunt 0 (mounted).
// Every `AllocateVoid<MomentXxx>()` arm placement-constructs a MomentXxx, which emits its
// vftable, which needs every virtual of that class defined at link -- so the twelve-arm switch
// drags all twelve subclasses in whole. That is what makes this ONE gate cost 49 on its own
// while the individual moment TUs cost nothing: mounted without NewMoment, a moment TU emits no
// vftable and so drags none of its siblings' virtuals in.
//
// The closure alone would not make a cutaway play: nothing in this tree ticks a moment
// (MomentController::UpdateAllMoments has no body, MainDirector::UpdateMoments is
// declaration-only, and its call is commented out in MainDirector::Update).
//
// DELETE-WHEN, in dependency order -- do NOT start at the bottom:
//   0. DONE 2026-09-11: the layout-stubbed MomentHardStop is retired and its header deleted.
//      The parameter bank now holds the real Moments/BrnMomentHardStop.h record, so
//      MomentHardStop has exactly one definition tree-wide and step 4 can no longer fold a
//      one-liner Prepare over the real body.
//   1. DONE 2026-09-11: the MomentSharedInfo shim family is bodied against the homed record
//      (BrnMomentSharedInfo.cpp). All of it: the last two, which the census called uncarved,
//      were a named VehicleTracker member with its own accessor and a profile-data flag two
//      mounted arbitrator arms already read through the same byte blob. Seven moment TUs are
//      mounted off the back of it.
//   2. Body MomentController::UpdateAllMoments and MainDirector::UpdateMoments, then un-gate
//      the commented-out call in MainDirector::Update. Those two are NOT in this file set.
//   3. Body the six BehaviourCollection<> template methods and the per-class virtuals, and
//      repair BehaviourRig.cpp so its seven bodies can mount. 65 of the remaining 71.
//   4. Mount the closure TUs and delete the gate below.
//
// The moment pool's bucket is widened on this x64 host (static_assert per moment type; see
// the HOST BUCKET WIDENING banner in BrnMomentController.h).
// ============================================================================
#include "GameSource/Director/MomentController/BrnMomentController.h"   // MomentController

namespace BrnDirector
{
    // ------------------------------------------------------------------------
    // MomentController::NewMoment. Real body: MomentController/BrnMomentControllerNewMoment.cpp
    // (complete; it is the MOUNT that is blocked, not the code).
    //
    // The console allocates a moment out of mMomentPool, hands it to the handle, pushes the
    // bank's parameters onto it and returns TRUE unconditionally. Here: allocate nothing and
    // return the same TRUE, leaving the handle !IsAllocated(). TRUE is load-bearing: a FALSE
    // clears MomentSelector::Prepare's mbPrepared, which is ArbStateRoaming::Prepare's return
    // value, and meState would never leave E_STATE_PREPARING. An unallocated handle is safe
    // downstream (MomentSelector::Update skips it; MomentHandle::Release is a no-op on one).
    //
    // DELETE-WHEN: the five-step plan in the GROUP F banner above, in that order.
    // ------------------------------------------------------------------------
    bool MomentController::NewMoment(Moment::EType leMomentType,
                                     MomentParameterBank::EMomentParamID leMomentParamID,
                                     MomentHandle& lrMomentHandleInOut,
                                     Camera::BehaviourManager& lrBehaviourManager)
    {
        (void)leMomentType;
        (void)leMomentParamID;
        (void)lrMomentHandleInOut;   // deliberately left !IsAllocated()
        (void)lrBehaviourManager;
        return true;
    }
}

