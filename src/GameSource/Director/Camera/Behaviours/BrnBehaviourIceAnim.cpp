#include "GameSource/Director/Camera/Behaviours/BrnBehaviourIceAnim.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"            // CgsDev::Assert (BeginAssert/FireAssert/EndAssert)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourIceAnim.cpp
//
// The seven free-function bodies of BrnDirector::Camera::BehaviourIceAnim
// (Construct, Prepare, Update, SetParameters, ChangeMovie, SetupTweaker, GetName)
// plus the class TU (the ctor, GetCollisionPolicy, GetTimeRemaining, HasFinishedOrFailed)
// -- one class spanning the two ledger entries, kept together in this .cpp.
//
// Reconstructed behaviour-faithful: members are reached BY NAME and helpers are called by
// their reconstructed names. The reference-space rows and look/eye transforms are copied
// as the named transform/vector assignments they perform.
// ============================================================================

// ----------------------------------------------------------------------------
// FLAG: minimal slices of the remaining Update-only helpers that have no reconstructed
//   home yet. Each is declared with exactly the named operation the Update body invokes,
//   accessed BY NAME; bodies land with the helpers' own Camera TUs (the per-TU `cl /c`
//   gate does not link). Replace with their real homes when those TUs are reconstructed.
// ----------------------------------------------------------------------------
namespace ICE
{
    // The per-frame reference-space cache the controller reads. Built from the shared
    // info's source-space pointer. Modelled opaque -- Update only constructs it and hands
    // it to the controller's Update.
    class CameraSpaceHandler
    {
    public:
        explicit CameraSpaceHandler(const void* lpSourceSpaces) { (void)lpSourceSpaces; }
        u8 maReserved[0x10];
    };
}

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{
    // Build a look-at affine transform from the supplied eye/target rows. Returns the
    // produced transform.
    rw::math::vpu::Matrix44Affine CreateLookAt(const void* lpArgs);

    // Depth-of-field parameters for the camera (focus distances + blur strengths).
    class DepthOfField
    {
    public:
        void SetParams(f32 lfA, f32 lfB, f32 lfC, f32 lfD);
    };
}

    // True when the produced camera can actually see the look target this frame.
    bool IsLookingAtTarget(const Camera& lrCamera, const void* lpEye, const void* lpLook);

    // ------------------------------------------------------------------------
    // FLAG: minimal-slice camera-facing operations the Update body performs on the shared
    //   per-frame camera. The Camera type's own home (Camera.h) is NOT grown here -- these
    //   are declared on this behaviour's side and routed by name. Each maps to a recorded
    //   camera write: the eye-space rows, the motion-blur amount/enable, the depth-of-field
    //   + FOV for the dedicated look space, the looker/shake post-processes, and the
    //   see-through collision request. Replace with the real Camera method set when the
    //   Camera TU recovers them; the NAMES are stable.
    // ------------------------------------------------------------------------
    namespace IceAnimCameraOps
    {
        void CopyTransformFrom(Camera& lrDst, const Camera& lrSrc);
        void SetEyeSpaceRows(Camera& lrCamera, const void* lpVehicle);
        void SetMotionBlurAmount(Camera& lrCamera, f32 lfAmount);
        void EnableMotionBlur(Camera& lrCamera);
        void SetDepthOfField(Camera& lrCamera, const Utils::DepthOfField& lrDof);
        void SetFOV(Camera& lrCamera, f32 lfFOV);
        f32  GetFOV(const Camera& lrCamera);
        void RunLooker(Camera& lrCamera, Utils::Looker& lrLooker,
                       const rw::math::vpu::Vector3& lrLookPos, f32 lfTimeStep);
        void RunShake(Camera& lrCamera, Utils::CameraShake& lrShake, f32 lfTimeStep);
        void RequestSeeThrough(Camera& lrCamera);
    }

} // namespace Camera

// The on-screen debug text printer the Update body writes its visibility result through.
// ⚠️ CLASS KEY: `struct`, not `class` -- the home (DirectorModule/BrnDirectorModuleDebugPrinter.h)
//   and every other forward declaration in the tree spell it `struct`, and MSVC mangles the two
//   differently, so a `class` here would silently mint a DIFFERENT symbol and only fail at link.
struct DebugPrinter
{
    static void ActualPrint(void* lpSink, const char* lpcText, s32 liColour);
};

} // namespace BrnDirector

namespace rw { namespace math { namespace vpu {
    // Spherical-linear blend between two affine transforms by a per-lane amount. Returns
    // the blended transform.
    Matrix44Affine SLerp(const Matrix44Affine& lrFrom, const Matrix44Affine& lrTo,
                         const f32* lpfAmount);
}}}

namespace BrnDirector
{
namespace Camera
{

// ----------------------------------------------------------------------------
// File-scope constants.
// ----------------------------------------------------------------------------
// The heading-space-to-look SLerp blend amount.
static const f32 KF_HEADING_SPACE_2_SLERP_AMOUNT = 1.0f;

// The source path string the asserts report.
static const char* const KPC_SOURCE_FILE =
    "..\\..\\..\\GameSource\\Director/Camera/Behaviours/BrnBehaviourIceAnim.cpp";

// The eye/look space selectors that mean "use a loose/world heading space" (the switch
// matches the controller's GetEyeSpace / GetLookSpace result against these).
static bool IsLooseHeadingSpace(s32 liSpace)
{
    return liSpace == 0 || liSpace == 10 || liSpace == 13 || liSpace == 2
        || liSpace == 6 || liSpace == 9;
}

// The selectors that mean "use the secondary (look-at) vehicle's space".
static bool IsLookAtVehicleSpace(s32 liSpace)
{
    return liSpace == 4 || liSpace == 8 || liSpace == 12;
}

// ============================================================================
// Construct
// ----------------------------------------------------------------------------
// Reset every owned field to its empty/default state, then construct the embedded camera
// and the attached-to-car collision policy. Mirrors the zero/seed sweep.
// ============================================================================
void BehaviourIceAnim::Construct()
{
    // --- base flag/state block: the six stores ARE the inlined Behaviour::Construct
    //     (see Behaviour.cpp); named base call now that the base has a home. ---
    Behaviour::Construct();
    mpCurrentTakeData = 0;

    // --- the four behaviour-mode flags + the take guid / source block ---
    miAnimGuid = -1;
    mpSourceShot = 0;
    mbUseCollisionPolicy = false;
    mbUseAttachedToCarCollisionPolicy = false;
    mbForceHeadingSpaceToBeLooseHeadingSpace = false;
    mbForceMotionBlurEverything = false;

    // --- the free visibility policy's three see-through state bytes (defaults) ---
    // (through the policy's named setters since the de-fork -- the bytes are private in the
    //  canonical BrnCollisionPolicy.h home; the console stores are unchanged.)
    mCollisionPolicy.SetSeeThroughEnabled(true);
    mCollisionPolicy.SetSeeThroughAlways(false);
    mCollisionPolicy.SetSeeThroughSuppressed(true);

    // --- the per-take reset sub-block at +0xDE0 ---
    mfReset0DE0 = 0.0f;
    maReset0DE4[0] = 0;
    maReset0DE4[1] = 0;
    maReset0DE4[2] = 0;
    maReset0DE4[3] = 0;

    // --- the produced camera + the attached-to-car collision policy ---
    mLastCamera.Construct();
    mAttachedToCarCollisionPolicy.Construct(0);
}

// ============================================================================
// SetParameters
// ----------------------------------------------------------------------------
// Assert the block is non-null and carries the iceanim class key, decode it just far
// enough to read the take guid (instance +0xC), then store the block + guid.
// ============================================================================
void BehaviourIceAnim::SetParameters(ShotReference* lpParameters)
{
    if (!lpParameters)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpParameters != NULL", KPC_SOURCE_FILE, 325);
        CgsDev::Assert::EndAssert();
    }

    // The class-key test compares the block's STORED class key (its leading 8-byte tag)
    // against the generated iceanim class key.
    if (lpParameters->GetClassKey() != Attrib::Gen::iceanim::ClassKey())
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpParameters->GetClassKey()==Attrib::Gen::iceanim::ClassKey()", KPC_SOURCE_FILE, 326);
        CgsDev::Assert::EndAssert();
    }

    // Decode the parameter block to reach the take guid carried at the instance's +0xC.
    miAnimGuid = lpParameters->GetAnimGuid();
    mpSourceShot = lpParameters;
}

// ============================================================================
// ChangeMovie
// ----------------------------------------------------------------------------
// Re-take the parameters, reset the per-take camera/collision state, re-prepare the
// controller, then resolve the take data (editor-edited take first, then the ICE list by
// guid) and bind it.
// ============================================================================
void BehaviourIceAnim::ChangeMovie(ShotReference* lpParameters,
                                   const DirectorResourceManager& lrResourceManager)
{
    SetParameters(lpParameters);

    // Reset the per-take sub-block at +0xDE0 (the f32 to 0.0f and the four trailing bytes
    // to 0). ChangeMovie touches THIS block only -- it does NOT reset the behaviour-mode
    // flags at +0xE28..+0xE2B.
    mfReset0DE0 = 0.0f;
    maReset0DE4[0] = 0;
    maReset0DE4[1] = 0;
    maReset0DE4[2] = 0;
    maReset0DE4[3] = 0;

    if (!mKeyAnimController.Prepare(lrResourceManager, miAnimGuid))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "mKeyAnimController.Prepare(lResourceManager, miAnimGuid)", KPC_SOURCE_FILE, 347);
        CgsDev::Assert::EndAssert();
    }

    // Editor-edited take first, then the on-disk ICE list keyed by guid.
    ICE::ICETakeData* lpTakeData =
        lrResourceManager.GetICEAuthor().FindEditedTakeFromGuid(miAnimGuid);
    if (!lpTakeData)
        lpTakeData = const_cast<ICE::ICETakeData*>(
            lrResourceManager.GetICEList().GetICETakeDataFromGuid(miAnimGuid));

    if (!lpTakeData)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpTakeData != NULL", KPC_SOURCE_FILE, 351);
        CgsDev::Assert::EndAssert();
    }

    // Bind the take-data pointer offset by +0xC (past the node base).
    mpCurrentTakeData = reinterpret_cast<u8*>(lpTakeData) + 0xC;
}

// ============================================================================
// GetName
// ============================================================================
const char* BehaviourIceAnim::GetName() const
{
    return "BehaviourIceAnim";
}

// ============================================================================
// SetupTweaker
// ----------------------------------------------------------------------------
// Tail-call into the camera-utils tweaker's Construct on the supplied tweaker.
// ============================================================================
void BehaviourIceAnim::SetupTweaker(Utils::Tweaker& lrTweaker)
{
    // X360 `Tweaker::Construct(a2)` -- the canonical home (Utils/BrnCameraTweaker.h) has that
    // @0x821F8588 as a MEMBER with this == a2, so the console's one-argument call IS this.
    lrTweaker.Construct();
}

// ============================================================================
// Prepare
// ----------------------------------------------------------------------------
// Assert the shared-info resource manager is present, prepare the controller for the
// current guid, resolve the take data (edited-first, then by guid), bind it, and clear the
// produced-camera flag. Always returns true.
// ============================================================================
bool BehaviourIceAnim::Prepare(const BehaviourSharedPrepareReleaseInfo& lrInfo)
{
    const DirectorResourceManager* lpResourceManager = lrInfo.GetDirectorResourceManager();
    if (!lpResourceManager)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lSharedInfo.mpDirectorResourceManager", KPC_SOURCE_FILE, 80);
        CgsDev::Assert::EndAssert();
    }

    if (!mKeyAnimController.Prepare(*lpResourceManager, miAnimGuid))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "mKeyAnimController.Prepare(*lSharedInfo.mpDirectorResourceManager, miAnimGuid)",
            KPC_SOURCE_FILE, 81);
        CgsDev::Assert::EndAssert();
    }

    ICE::ICETakeData* lpTakeData =
        lpResourceManager->GetICEAuthor().FindEditedTakeFromGuid(miAnimGuid);
    if (!lpTakeData)
        lpTakeData = const_cast<ICE::ICETakeData*>(
            lpResourceManager->GetICEList().GetICETakeDataFromGuid(miAnimGuid));

    if (!lpTakeData)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpTakeData != NULL", KPC_SOURCE_FILE, 85);
        CgsDev::Assert::EndAssert();
    }

    mpCurrentTakeData = reinterpret_cast<u8*>(lpTakeData) + 0xC;
    mbIsPrepared = false;
    return true;
}

// ============================================================================
// Update
// ----------------------------------------------------------------------------
// Advance the take and produce the camera. Control flow:
//   1. If either anchor ref is invalid -> raise "give up following" flags and bail.
//   2. (Optionally) re-prepare the controller, then raise the "follow" flag.
//   3. Build the heading-space look-at, SLerp it against the take's heading space.
//   4. Run the controller (its vtable Update) to evaluate the take into the camera.
//   5. Seed the produced camera once; pick the eye space (heading / look-at vehicle);
//      pick the look space; set the motion-blur amount + enable.
//   6. When the look space is the dedicated "11" space, drive depth-of-field, FOV, the
//      looker and the camera shake.
//   7. Gate the see-through collision flag, mark "can't switch from me now" until finished,
//      and print the visibility result.
// ============================================================================
bool BehaviourIceAnim::Update(Camera& lrCamera, const BehaviourSharedInfo& lrSharedInfo)
{
    BehaviourSharedInfo& lrInfo = const_cast<BehaviourSharedInfo&>(lrSharedInfo);
    const void* lpWorld = lrSharedInfo.GetWorld();

    if (!mPrimaryVehicleRef.IsValid(lpWorld) || !mSecondaryVehicleRef.IsValid(lpWorld))
    {
        // Neither anchor resolves -> give up following. The X360 block here (account
        // SetFlag(11) on camera +0x138, `camera+0x140 &= ~2`, then the three base flag
        // stores) is the INLINED Behaviour::Fail @0x822063E8 -- expressed as the named base
        // call now that the base has a home. Reason 11 is the fork's own `+312 |= 0x800`
        // (bit 11) written as the flag index.
        Fail(lrCamera, 11);
        return true;
    }

    if (lrSharedInfo.ShouldRePrepareController()
        && !mKeyAnimController.Prepare(*lrSharedInfo.GetDirectorResourceManager(), miAnimGuid))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "mKeyAnimController.Prepare(*lrSharedInfo.mpDirectorResourceManager, miAnimGuid)",
            KPC_SOURCE_FILE, 116);
        CgsDev::Assert::EndAssert();
    }

    // The fork's `RequestFollow()` is the camera-state write `camera+0x140 |= 2` -- the
    // exact bit Behaviour::Fail clears. Expressed through CameraState's named ClearFlag/
    // SetFlag pair so no offset is poked (see the FLAG on the flag id in Behaviour.cpp).
    lrCamera.GetState().SetFlag(1u, true);
    mbTweakerAttached = true;      // +0x0A -- see the FLAG in BrnBehaviourIceAnim.h

    // The reference-space rows the controller reads, taken from the shared info.
    void* lpSpaceArgs = lrSharedInfo.GetSpaceArgs();

    // Heading-space look-at: build it for the secondary (look-at) vehicle, then SLerp the
    // behaviour's stored heading-space transform towards it on the first prepared frame.
    if (!mbIsPrepared)
    {
        mSecondaryVehicleRef.Get(lpWorld);
        mLastCamera.mTransform = Utils::CreateLookAt(lpSpaceArgs);
    }

    mSecondaryVehicleRef.Get(lpWorld);
    rw::math::vpu::Matrix44Affine lLookAt = Utils::CreateLookAt(lpSpaceArgs);
    f32 lfSlerpAmount = KF_HEADING_SPACE_2_SLERP_AMOUNT;
    mLastCamera.mTransform =
        rw::math::vpu::SLerp(mLastCamera.mTransform, lLookAt, &lfSlerpAmount);

    ICE::CameraSpaceHandler lSpaces(lrSharedInfo.GetSourceSpaces());
    mPrimaryVehicleRef.Get(lpWorld);
    mSecondaryVehicleRef.Get(lpWorld);

    // Run the controller (dispatched through its vtable) to evaluate the take.
    mKeyAnimController.Update(&lSpaces, lrSharedInfo.GetInfoPointer());

    // Seed the behaviour's stored camera from the shared camera the first time only
    // (Camera::operator=(mLastCamera, lrCamera): dst = mLastCamera, src = lrCamera).
    if (!mbIsPrepared)
    {
        IceAnimCameraOps::CopyTransformFrom(mLastCamera, lrCamera);
        mbIsPrepared = true;
    }

    // --- Pick the eye space ---
    const s32 liEyeSpace = mKeyAnimController.GetEyeSpace();
    bool lbHasEyeSpace;
    if (IsLooseHeadingSpace(liEyeSpace))
    {
        IceAnimCameraOps::SetEyeSpaceRows(mLastCamera, mPrimaryVehicleRef.Get(lpWorld));
        lbHasEyeSpace = true;
        mbUseAttachedToCarCollisionPolicy = true;
    }
    else if (IsLookAtVehicleSpace(liEyeSpace))
    {
        IceAnimCameraOps::SetEyeSpaceRows(mLastCamera, mSecondaryVehicleRef.Get(lpWorld));
        lbHasEyeSpace = true;
        mbUseAttachedToCarCollisionPolicy = true;
    }
    else
    {
        lbHasEyeSpace = false;
        mbUseAttachedToCarCollisionPolicy = false;
    }

    // --- Pick the look space + motion-blur amount ---
    const s32 liLookSpace = mKeyAnimController.GetLookSpace();
    const bool lbHasLookSpace = IsLooseHeadingSpace(liLookSpace) || IsLookAtVehicleSpace(liLookSpace);

    if ((lbHasEyeSpace || lbHasLookSpace) && !mbForceMotionBlurEverything)
        IceAnimCameraOps::SetMotionBlurAmount(lrCamera, 0.0f);
    else
        IceAnimCameraOps::SetMotionBlurAmount(lrCamera, 1.0f);
    IceAnimCameraOps::EnableMotionBlur(lrCamera);

    // --- The dedicated look space (11) drives depth-of-field, FOV, looker + shake ---
    if (liLookSpace == 11)
    {
        Utils::DepthOfField lDepthOfField;
        lDepthOfField.SetParams(1.0f, 0.0f, 0.0f, 0.0f);
        IceAnimCameraOps::SetDepthOfField(lrCamera, lDepthOfField);
        IceAnimCameraOps::SetFOV(lrCamera, IceAnimCameraOps::GetFOV(mLastCamera));

        const rw::math::vpu::Vector3& lLookPos = mKeyAnimController.GetLookPos();
        const f32 lfTimeStep = lrSharedInfo.GetTimestep().Get(Timestep::E_WORLD);
        mBystanderRef.Get(lpWorld);
        IceAnimCameraOps::RunLooker(lrCamera, mLooker, lLookPos, lfTimeStep);
    }

    // Copy the shared camera into the behaviour's stored camera
    // (Camera::operator=(mLastCamera, lrCamera): dst = mLastCamera, src = lrCamera).
    IceAnimCameraOps::CopyTransformFrom(mLastCamera, lrCamera);

    if (liLookSpace == 11)
    {
        const f32 lfShakeTimeStep = lrSharedInfo.GetTimestep().Get(Timestep::E_WORLD);
        IceAnimCameraOps::RunShake(lrCamera, mShake, lfShakeTimeStep);
    }

    // --- See-through collision-flag gate ---
    if (mbUseCollisionPolicy)
    {
        // Inner guard: only raise the see-through flag when the free visibility policy's
        // three state bytes say so:
        //   mbSeeThroughAlways || (mbSeeThroughEnabled && !mbSeeThroughSuppressed).
        if (mCollisionPolicy.ShouldRaiseSeeThrough())
        {
            // Request the see-through flag on the camera and clear the motion-blur gate.
            IceAnimCameraOps::RequestSeeThrough(lrCamera);
            mbCanSwitchToMeNow = false;
        }
    }

    // SetCantSwitchFromMeNow's first argument is the CAMERA (the account it stamps lives at
    // camera +0x138) -- the retired fork threaded the shared info there by mistake. Reason 29
    // sits inside the account's attested no-cut-from band [27,31).
    if (!HasFinishedOrFailed() && !HasFailed())
        SetCantSwitchFromMeNow(lrCamera, 29);

    // --- Debug visibility readout ---
    void* lpDebugSink = lrSharedInfo.GetDebugSink();
    if (!IsLookingAtTarget(lrCamera, lrSharedInfo.GetEyeTarget(), lrSharedInfo.GetLookTarget()))
        DebugPrinter::ActualPrint(lpDebugSink, "Can't see player", static_cast<s32>(0xFFF0F0FF));
    else
        DebugPrinter::ActualPrint(lpDebugSink, "Can see player", static_cast<s32>(0xFFF0FFF0));

    return true;
}

} } // namespace BrnDirector::Camera

// ============================================================================
// The class TU (class:BrnDirector::Camera::BehaviourIceAnim): ctor + the three
// const/leaf accessors. Same class, second ledger entry -- kept in this same .cpp.
// ============================================================================
namespace BrnDirector
{
namespace Camera
{

// ----------------------------------------------------------------------------
// BehaviourIceAnim()
// ----------------------------------------------------------------------------
// Install the behaviour + collision-policy vtables, then default-construct the embedded
// key-anim controller's ICETake. (The ctor writes the four vtable pointers at
// +0x00 / +0x20 / +0x260 / +0x680 and constructs the ICETake at +0x6A0.) The embedded
// members install their own vtables through their default construction; the ICETake is
// constructed by KeyAnimController's member init below.
// ----------------------------------------------------------------------------
BehaviourIceAnim::BehaviourIceAnim()
{
}

// ----------------------------------------------------------------------------
// GetCollisionPolicy  (ledger name "GetCollisio" -- truncated)
// ----------------------------------------------------------------------------
// No policy when collision is off; the attached-to-car policy when its flag is set;
// otherwise the free visibility policy.
// ----------------------------------------------------------------------------
CollisionPolicy* BehaviourIceAnim::GetCollisionPolicy()
{
    if (!mbUseCollisionPolicy)
        return 0;
    if (mbUseAttachedToCarCollisionPolicy)
        return &mAttachedToCarCollisionPolicy;
    return &mCollisionPolicy;
}

// ----------------------------------------------------------------------------
// GetTimeRemaining
// ----------------------------------------------------------------------------
// The un-played fraction of the parametric time times the bound take's length (0 length
// when no take is bound).
// ----------------------------------------------------------------------------
f32 BehaviourIceAnim::GetTimeRemaining()
{
    const ICE::ICETakeData* lpTakeData = mKeyAnimController.mTake.GetData();
    f32 lfLength = lpTakeData ? lpTakeData->GetLength() : 0.0f;
    return (1.0f - mKeyAnimController.GetParametricTime0To1()) * lfLength;
}

// ----------------------------------------------------------------------------
// HasFinishedOrFailed
// ----------------------------------------------------------------------------
// Finished when the controller has finished, or failed when the base failure flag is set.
// ----------------------------------------------------------------------------
bool BehaviourIceAnim::HasFinishedOrFailed() const
{
    if (mKeyAnimController.HasFinished())
        return true;
    if (HasFailed())
        return true;
    return false;
}

} } // namespace BrnDirector::Camera
