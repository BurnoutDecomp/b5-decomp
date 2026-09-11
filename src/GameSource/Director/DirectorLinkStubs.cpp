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
// GROUP A -- arbitrator states whose .cpp is not mounted: each entered state produces no
//   camera and reports "released". DELETE-WHEN: the state's sub-system lands -> mount the
//   state's .cpp, delete its block here.
// GROUP B -- the two dev-menu behaviours BehaviourManager::AllocateBehaviour<T> forces
//   vtables for (their real TUs pull the Tweaker mapping API + panorama screenshot callback).
// GROUP C -- sub-systems with no landed TU (the director DebugComponent, the
//   scene-query post-office free functions, rw SLerp).
// GROUP D -- declared-only leaves of already-mounted TUs.
// GROUP F -- the moment sub-system (MomentController::NewMoment).
// GROUP G -- dev-only trap leaves ArbStateCrashing reaches.
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"

#include "GameSource/Director/BrnDirectorResourceManager.h"
#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugCompononent.h"
#include "GameSource/Director/Utils/BrnSceneQueryInterface.h"
#include "GameSource/Director/Camera/BrnBehaviourManager.h"

#include "GameSource/Director/Arbitrator/States/BrnArbStateCarSelect.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStateCrashMode.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStateCrashNav.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStateDriveThru.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStateOnlineCarSelect.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStateOnlineRaceIntro.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStatePostEvent.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStateRaceIntro.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStateRankUp.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStateRoaming.h"

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourDebugFlyWorld.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourDebugOrbitPlayer.h"
#include "GameSource/Director/Camera/Behaviours/BehaviourPassengerCam.h"           // the DWARF home (NOT the stale BrnBehaviourPassengerCam.h slice)

#include "GameSource/Director/Camera/Utils/BrnCameraShake.h"                       // group E
#include "GameSource/Director/Camera/Utils/BrnCameraSphericalRotationController.h" // group E

#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h"

#include "SharedClasses/Traffic/BrnTrafficSection.h"
#include "SharedClasses/Trigger/BrnGenericRegion.h"
#include "SharedClasses/Trigger/BrnRegion.h"
#include "SharedClasses/Trigger/BrnTriggerData.h"

// ----------------------------------------------------------------------------
// GROUP A -- the off-path arbitrator states.
//
// Every stub below mirrors the BASE ArbitratorState default (see
// Arbitrator/BrnDirectorArbitratorState.cpp), which is what an un-entered state does anyway:
//   Construct() -- build the state's camera and clear the base flags;
//   Prepare()   -- "ready" (the arbitrator only calls it after CanRun said yes);
//   Update()    -- drive nothing;
//   Release()   -- "already released" (ReleaseAll asserts the result);
//   Destruct()  -- own nothing;
//   GetName()   -- the state's own console name literal.
// ----------------------------------------------------------------------------
#define BRN_DIRECTOR_STUB_ARBSTATE(CLASS, NAME_LITERAL)                             \
    void CLASS::Construct()                                                         \
    {                                                                               \
        ArbitratorState::Construct();                                               \
    }                                                                               \
    bool CLASS::Prepare(ArbStateSharedInfo& lrSharedInfo)                           \
    {                                                                               \
        (void)lrSharedInfo;                                                         \
        return true;                                                                \
    }                                                                               \
    void CLASS::Update(ArbStateSharedInfo& lrSharedInfo)                            \
    {                                                                               \
        (void)lrSharedInfo;                                                         \
    }                                                                               \
    bool CLASS::Release(ArbStateSharedInfo& lrSharedInfo)                           \
    {                                                                               \
        (void)lrSharedInfo;                                                         \
        return true;                                                                \
    }                                                                               \
    const char* CLASS::GetName() const                                              \
    {                                                                               \
        return NAME_LITERAL;                                                        \
    }

namespace BrnDirector
{
    // ArbStateCrashMode is NOT the crash camera the game enters (that is ArbStateCrashing,
    // mounted); it is only reached from ArbStateCrashing::ProcessPossibleStateChanges.
    // Mounting BrnArbStateCrashMode.cpp needs its Prepare, which BrnArbStateCrashMode.h
    // declares with no definition. Blocker on that Prepare: the SetParameters argument is
    // BehaviourParameterBank + 0x7C, not yet carved as BehaviourAftertouchCrash::Parameters.
    // Polarity: CrashMode's IsReadyToPrepare callee already negates -- return it raw (AttractMode
    // negates at the call site instead).
    // DELETE-WHEN: that Prepare is bodied -> mount BrnArbStateCrashMode.cpp, delete this line.
    BRN_DIRECTOR_STUB_ARBSTATE(ArbStateCrashMode,       "ArbStateCrashMode")
    BRN_DIRECTOR_STUB_ARBSTATE(ArbStateOnlineCarSelect, "ArbStateOnlineCarSelect")
    BRN_DIRECTOR_STUB_ARBSTATE(ArbStateOnlineRaceIntro, "ArbStateOnlineRaceIntro")
    BRN_DIRECTOR_STUB_ARBSTATE(ArbStatePostEvent,       "ArbStatePostEvent")
    BRN_DIRECTOR_STUB_ARBSTATE(ArbStateRaceIntro,       "ArbStateRaceIntro")
    BRN_DIRECTOR_STUB_ARBSTATE(ArbStateRankUp,          "ArbStateRankUp")

    // These five are silent-drop stubs for a state that is fully reconstructed but not mounted:
    // Arbitrator/States/BrnArbStateCrashNav.cpp owns the real Construct/Prepare/Update/
    // Release/GetName. While they stand, Update does nothing and Release always succeeds, so
    // un-gating the arbitrator's crash-nav trigger would enter a state that publishes a camera
    // nothing ever writes.
    // ⇒ ALL FIVE COME OUT TOGETHER WITH THE MOUNT, never one at a time (LNK2005 x5 otherwise).
    // Re-measured 2026-09-11 (dumpbin over that TU's object against the mounted symbol set):
    // it needs exactly TEN externals this link does not provide, all of them ICE:
    //     8  ICEMoviePlayer::{Construct,Prepare,Update,Stop,Loop,GetCamera,InterpolateFrom,
    //        CutToInterpolateOut} -- written, in the unmounted Utils/BrnICEMoviePlayer.cpp.
    //     2  ICEWrapper::{GetCurrentMovie,PlayMovie} -- written, unmounted
    //        SDKs/Packages/ICE/ICEWrapper.cpp.
    // The two non-ICE ones are closed: SharedCameraContainer::GetGameplayCameraHelperIndex
    // (Camera/BrnSharedCameraContainer.cpp mounted) and Camera::Camera::SetRequestedBorderPostFX
    // (bodied in Camera/Camera.cpp). ⇒ these five come out with the ICE mount, nothing else.
    void ArbStateCrashNav::Construct()                          { ArbitratorState::Construct(); }
    // Prepare forwards to the base: the override declared in BrnArbStateCrashNav.h adds a vtable
    // slot MainDirector references, so it must be defined even while the TU is unmounted.
    bool ArbStateCrashNav::Prepare(ArbStateSharedInfo& lrInfo)  { return ArbitratorState::Prepare(lrInfo); }
    void ArbStateCrashNav::Update(ArbStateSharedInfo& lrInfo)   { (void)lrInfo; }
    bool ArbStateCrashNav::Release(ArbStateSharedInfo& lrInfo)  { (void)lrInfo; return true; }
    const char* ArbStateCrashNav::GetName() const               { return "ArbStateCrashNav"; }

    // Two states declare an explicit Destruct() override that has no body in the tree.
    void ArbStateOnlineRaceIntro::Destruct() {}
    void ArbStatePostEvent::Destruct()       {}
}

#undef BRN_DIRECTOR_STUB_ARBSTATE

// ----------------------------------------------------------------------------
// GROUP B -- the two dev-menu behaviours the manager's AllocateBehaviour<T> forces vtables
// for. Their real TUs (BrnBehaviourDebugFlyWorld.cpp / BrnBehaviourDebugOrbitPlayer.cpp) pull
// the whole Tweaker mapping API + the panorama screenshot callback. Neither is allocated on
// the fly-by path -- only ArbStateAttractMode's BehaviourRoadRunner is.
// Update() returns FALSE = "I produced no camera this frame", the same answer the console's
// own behaviours give when they have nothing to say.
// DELETE-WHEN: Camera/Utils/BrnCameraTweaker.cpp lands -> mount both real TUs, delete this.
// ----------------------------------------------------------------------------
namespace BrnDirector
{
namespace Camera
{
    void BehaviourDebugFlyWorld::Construct()
    {
        // NOTE: these two classes are still PRE-BASE forks -- they carry their own
        // `void* mpVTable` at +0x00 instead of deriving from Camera::Behaviour, so there is no
        // base Construct to chain to. (Retiring those two forks the way the road runner's was
        // retired is a separate job.)
    }

    bool BehaviourDebugFlyWorld::Prepare(const BehaviourSharedPrepareReleaseInfo& lrInfo)
    {
        (void)lrInfo;
        return true;
    }

    bool BehaviourDebugFlyWorld::Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo)
    {
        (void)lrCamera;
        (void)lrInfo;
        return false;
    }

    void BehaviourDebugFlyWorld::SetupTweaker(Utils::Tweaker& lrTweaker)
    {
        (void)lrTweaker;
    }

    const char* BehaviourDebugFlyWorld::GetName() const
    {
        return "BehaviourDebugFlyWorld";
    }

    void BehaviourDebugFlyWorld::WarpToLookAt(Vector3 lEye, Vector3 lLookAt)
    {
        (void)lEye;
        (void)lLookAt;
    }

    // Concrete Behaviour vtables measure EIGHT slots, not the TEN Behaviour.h's banner lists:
    // its slots 6/7 (GetParameters/SetParameters) are very likely not virtual. Behaviour.h's lane.

    void BehaviourDebugOrbitPlayer::Construct()
    {
        // (same pre-base fork note as BehaviourDebugFlyWorld::Construct above)
    }

    bool BehaviourDebugOrbitPlayer::Prepare(const BehaviourSharedPrepareReleaseInfo& lrInfo)
    {
        (void)lrInfo;
        return true;
    }

    bool BehaviourDebugOrbitPlayer::Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo)
    {
        (void)lrCamera;
        (void)lrInfo;
        return false;
    }

    void BehaviourDebugOrbitPlayer::SetupTweaker(Utils::Tweaker& lrTweaker)
    {
        (void)lrTweaker;
    }

    const char* BehaviourDebugOrbitPlayer::GetName() const
    {
        return "BehaviourDebugOrbitPlayer";
    }
}
}

// ----------------------------------------------------------------------------
// GROUP C -- sub-systems with no landed TU.
// ----------------------------------------------------------------------------
namespace BrnDirector
{
    // -- The director's own CgsDev::DebugComponent page ("Camera"). Its five recovered
    //    functions (DirectorModule/BrnDirectorModuleDebugCompononent.cpp, unmounted) all index
    //    DirectorModule regions this reconstruction does not model yet.
    //    QUIET no-ops: the debug menu simply has an empty Camera page.
    void DebugComponent::Construct(DirectorModule* lpDirectorModule)
    {
        (void)lpDirectorModule;
    }

    void DebugComponent::UpdatePanoramaScreenshots(Camera::Camera* lpCamera)
    {
        (void)lpCamera;
    }

    void DebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpRender)
    {
        (void)lpRender;
    }

    const char* DebugComponent::GetName() const
    {
        return "Camera";   // the page name the console registers (DWARF + the header's note)
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
// GROUP D -- declared-only leaves of TUs that ARE in the link.
// ----------------------------------------------------------------------------
namespace BrnDirector
{
namespace Camera
{
    // BehaviourManager::DebugDumpToTTY -- walks every helper slot and prints GetDebugFullName.
    // Called from the manager's allocation-failure path only; that failure still asserts
    // through its own CGS_ASSERT, so a quiet no-op loses nothing.
    // FLAG PC-platform leaf: BehaviourHelper::GetDebugFullName is declaration-only.
    // DELETE-WHEN: BehaviourHelper::GetDebugFullName lands.
    void BehaviourManager::DebugDumpToTTY() const {}
}
}

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
// Mounting the rest of the closure (BrnMomentControllerNewMoment.cpp + the eleven Moments/*.cpp
// that are still unmounted) costs 135 non-CRT unresolved externals, re-measured 2026-09-11 by
// compiling every candidate with the shipping flags and subtracting the defined-symbol set of
// the whole current object list. By family:
//     39  detail::MomentSharedInfo_* reach shims. The record itself IS homed now
//         (MomentController/BrnMomentSharedInfo.h) and eleven of its shims are bodied; these
//         are the rest, each still a declaration at the head of the TU that reads it.
//     33  per-moment-class virtuals and privates (Destruct / GetInstanceType / SetParameters /
//         Prepare / Release, plus MomentPlayerJumping::UpdateCamera)
//     24  BehaviourCollection<T,P,N> methods -- six template methods x four instantiations
//     24  other subsystems (BehaviourRig's virtual set + Parameters::Construct, six
//         vector-deleting destructors, BehaviourPassengerCam::SetParameters,
//         BehaviourLooseAttachment::Parameters::Construct, Camera::SetRequestedBorderPostFX,
//         ShotSelector::GetCrashShot, DirectorResourceManager::GetKeyAnim)
//     13  other detail:: reach shims (ICETakeData_*, IceAnimShotData_*, Vehicle_*,
//         BehaviourRig_*, CameraState_AppendToDebugLog, ...)
//      2  BehaviourParameterBank accessors -- and only the two INDEXED ones are left
//         (GetPlayerJumpingRigShotParams / GetPlayerJumpingBystanderShotParams). The four
//         single-block moment accessors are bodied; see that header's RECORD MAP for why the
//         indexed pair needs its call sites renumbered before it can follow.
// Every `AllocateVoid<MomentXxx>()` arm placement-constructs a MomentXxx, which emits its
// vftable, which needs every virtual of that class defined at link -- so the twelve-arm switch
// drags all twelve subclasses in whole. That is what makes this ONE gate cost 135 symbols while
// the individual moment TUs cost nothing: mounted without NewMoment, a moment TU emits no
// vftable and so drags none of its siblings' virtuals in.
//
// The closure alone would not make a cutaway play: nothing in this tree ticks a moment
// (MomentController::UpdateAllMoments has no body, MainDirector::UpdateMoments is
// declaration-only, and its call is commented out in MainDirector::Update).
//
// DELETE-WHEN, in dependency order -- do NOT start at the bottom:
//   1. Body the rest of the MomentSharedInfo shims against the homed record. Each one is a
//      named member read; what takes the time is checking the role name the call site coined
//      against the member it lands on -- two of the eleven already bodied turned out to be
//      misnamed, and both are recorded in the record's own header.
//   2. Body MomentController::UpdateAllMoments and MainDirector::UpdateMoments, then un-gate
//      the commented-out call in MainDirector::Update. Those two are NOT in this file set.
//   3. Body the six BehaviourCollection<> template methods and the per-class virtuals.
//   4. Mount the closure TUs and delete the gate below.
//
// The moment pool's bucket is widened on this x64 host (static_assert per moment type; see
// the HOST BUCKET WIDENING banner in BrnMomentController.h).
// ============================================================================
#include "GameSource/Director/MomentController/BrnMomentSelector.h"     // MomentSelector
#include "GameSource/Director/MomentController/BrnMomentController.h"   // MomentController
#include "GameSource/Director/Camera/BrnCameraValidityAccount.h"        // group G: ValidityAccount::Print x2
#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugPrinter.h" // group G: DebugPrinter / DebugLog

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

    // ------------------------------------------------------------------------
    // GROUP G -- THE THREE DEV-ONLY LEAVES BrnArbStateCrashing.cpp REACHES. All three are TRAP
    // stubs, not quiet ones, and that is deliberate: every call site is inside
    // `if (IsDebugDisplayActive())`, and ArbStateCrashing::Construct seeds that flag FALSE
    // (the console only raises it from a dev tool), so retail never reaches any of them.
    //
    //   MomentSelector::ActualDebugRender -- walks the handle + description arrays and prints
    //       one line per candidate through the DebugPrinter.
    //   ValidityAccount::Print(DebugPrinter&) / Print(DebugLog&) -- both walk the raised
    //       reason bits and print the name of each; the reason-NAME table is what is missing
    //       (the 31 enumerators are in BrnCameraValidityAccount.h).
    //
    // DELETE-WHEN: ActualDebugRender is bodied, and the ValidityAccount reason-name table is
    // recovered.
    // ------------------------------------------------------------------------
    void MomentSelector::ActualDebugRender(DebugPrinter& lrDebugPrinter) const
    {
        (void)lrDebugPrinter;
        CGS_ASSERT(false, "MomentSelector::ActualDebugRender is not reconstructed");
        __debugbreak();
    }
}

namespace BrnDirector
{
namespace Camera
{
    // See the GROUP G banner above -- both overloads are dev-only read-outs.
    void ValidityAccount::Print(DebugPrinter& lrDebugPrinter) const
    {
        (void)lrDebugPrinter;
        CGS_ASSERT(false, "ValidityAccount::Print(DebugPrinter&) is not reconstructed");
        __debugbreak();
    }

    void ValidityAccount::Print(DebugLog& lrDebugLog) const
    {
        (void)lrDebugLog;
        CGS_ASSERT(false, "ValidityAccount::Print(DebugLog&) is not reconstructed");
        __debugbreak();
    }
}
}
