#include "GameSource/Director/Camera/Behaviours/BrnBehaviourIceAnim.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"            // CgsDev::Assert (BeginAssert/FireAssert/EndAssert)
#include "GameSource/Director/Camera/Utils/CameraUtils.h"     // Camera::Utils::CreateLookAt (the real home)
#include "SDKs/Packages/ICE/ICECameraSpaceHandler.hpp"        // ICE::CameraSpaceHandler (the real home)
#include "SDKs/Packages/ICE/ICEAuthor.hpp"                    // ICE::ICEAuthor::FindEditedTakeFromGuid --
                                                              //   THE home (2026-07-31). The header's own
                                                              //   `class ICEAuthor` slice is retired; the
                                                              //   real one is a `struct`, which mangles
                                                              //   differently, so this include is what makes
                                                              //   the two calls below link.
#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugPrinter.h"
                                                              // BrnDirector::DebugPrinter -- THE home
                                                              //   (2026-08-01). Same story as ICEAuthor
                                                              //   above: this file's own DebugPrinter slice
                                                              //   declared ActualPrint as a STATIC 3-arg
                                                              //   function, which is a different mangled
                                                              //   symbol from the real non-static 2-arg
                                                              //   member. See the RETIRED note below.

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
// ============================================================================
// RETIRED (2026-07-31) -- the `BrnDirector::Camera::IceAnimCameraOps` namespace.
//
// Ten bodyless free functions used to sit here as a NAMING DEVICE for camera writes the X360
// compiler inlined into Update @0x82247108. Every one of them was real code, but the naming
// was wrong in ways that mattered, so all ten are gone and each write now goes through the
// API that actually owns it:
//
//   CopyTransformFrom      -> `mLastCamera = lrCamera;`  (Camera::operator= @0x82233A80 -- a
//                             WHOLE-camera memberwise copy, not a transform copy)
//   SetEyeSpaceRows        -> CollisionPolicyAttachedToVehicle::SetVehicleRef.  It never
//                             touched a Camera: the four-word copy lands at behaviour +0x480
//                             == the attached-to-car POLICY's +0x220 (policy base +0x260).
//   SetMotionBlurAmount  \
//   EnableMotionBlur     /  -> ONE call, `Camera::RequestMotionBlur(amount, 1.0f)` -- the two
//                             ops are the two halves of that one operation (amount at
//                             mEffects +0x44, blend 1.0f at +0x48, both enables at +0x4C/+0x4D).
//   SetDepthOfField        -> `lrCamera.GetDepthOfField().SetParams(...)` directly on the
//                             SHARED camera (there is no local DepthOfField and no copy), with
//                             the blurriness lane taken from mLastCamera's own band.
//   SetFOV / GetFOV        -> `lrCamera.SetFOV(mLastCamera.GetFOV())` (Camera::SetFOV
//                             @0x821F26B8; the getter was a single inlined field load).
//   RunLooker              -> Looker::Parameters::Construct @0x821F8D80 on a stack block, 11
//                             named overrides, then Utils::Looker::Update @0x8223FBB8.
//   RunShake               -> Utils::CameraShake::Update @0x82221310 over the camera's own
//                             transform, with a stack CameraShake::Parameters.
//   RequestSeeThrough      -> `SetCantSwitchToMeNow(lrCamera, 16)`. Nothing "see-through" is
//                             written: the two stores are the validity-account bit 16 at
//                             camera +0x138 plus `mbCanSwitchToMeNow = false`, which is
//                             exactly Behaviour::SetCantSwitchToMeNow's body. This is that
//                             method's first attested call site.
//
// The set also MISSED a real write, restored below: the orientation-only copy of
// mLastCamera's transform rows 0..2 into the shared camera, immediately before the
// depth-of-field call in the look-space-11 arm. (Row 3, the position, is deliberately not
// copied -- that is what the console does.)
// ============================================================================

namespace BrnDirector
{
namespace Camera
{

    // True when the produced camera can actually see the look target this frame.
    bool IsLookingAtTarget(const Camera& lrCamera, const void* lpEye, const void* lpLook);

    // ------------------------------------------------------------------------
    // The HEADING-SPACE frame of an anchor vehicle: a look-at built at the vehicle's world
    // position, aimed along its forward axis FLATTENED to horizontal. X360-attested
    // (BehaviourIceAnim::Update @0x82247270..0x822472B0): it takes the vehicle's world
    // transform (vehicle +0x1F0), splats its at-row's x and z lanes into {at.x, 0, at.z, 0},
    // adds that to the position row, and calls Utils::CreateLookAt(position, position + that).
    //
    // FLAG: BrnDirector::VehicleRef::Get returns an UNTYPED vehicle pointer -- the vehicle's
    //   own type has no reconstructed accessor set yet, so the two reads live behind these
    //   named helpers rather than being formed as offsets here. DECLARATION-ONLY; the bodies
    //   land with the vehicle TU. DELETE-WHEN: the anchor vehicle's transform accessor lands.
    rw::math::vpu::Matrix44Affine CreateHeadingSpaceLookAt(const void* lpVehicle);
    rw::math::vpu::Vector3        GetVehicleWorldPosition(const void* lpVehicle);

} // namespace Camera

// ============================================================================
// RETIRED (2026-08-01): the private
//     struct DebugPrinter { static void ActualPrint(void* lpSink, const char*, s32); };
// slice that used to sit here is GONE, and the two Update call sites now go through the real
// home (DirectorModule/BrnDirectorModuleDebugPrinter.h, included above).
//
// ⚠️ IT WAS AN ARITY + STATICNESS FORK, the species that only ever surfaces as LNK2019.
// The real @0x821F71D8 is a NON-static, PRIVATE member `void DebugPrinter::ActualPrint(const
// char*, CgsDev::RGBA)`; the console's `r3` at those two call sites is the printer itself,
// not a first argument. Spelling it `static ActualPrint(void*, const char*, s32)` minted a
// completely different mangled symbol that no TU in the tree could ever define -- it would
// have stayed unresolved for ever while looking, in the source, like a call that just needed
// its home mounted. The faithful spelling is the PUBLIC forwarder
// `DebugPrinter::Print(text, colour)`, which the console inlines to exactly that direct
// ActualPrint call (the forwarder is now bodied in the home header for that reason).
// ============================================================================

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
static bool IsLooseHeadingSpace(ICE::eICESpace leSpace)
{
    return leSpace == ICE::eICE_CAR_SPACE || leSpace == ICE::eICE_HEADING_SPACE
        || leSpace == ICE::eICE_LOOSE_HEADING_SPACE || leSpace == ICE::eICE_HYBRID_SPACE
        || leSpace == ICE::eICE_TAKEDOWN_SPACE || leSpace == ICE::eICE_GAMEPLAY_SPACE;
}

// The selectors that mean "use the secondary (look-at) vehicle's space".
static bool IsLookAtVehicleSpace(ICE::eICESpace leSpace)
{
    return leSpace == ICE::eICE_CAR2_SPACE || leSpace == ICE::eICE_REVERSE_TAKEDOWN_SPACE
        || leSpace == ICE::eICE_HEADING2_SPACE;
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
    const AllVehicleData* lpWorld = lrSharedInfo.GetWorld();

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

    // ⚠️ CORRECTED 2026-07-31: the store here is `stb r21, 0xA(r11)` with r11 == this+0x20 --
    // behaviour +0x2A, i.e. the free VISIBILITY COLLISION POLICY's +0x0A, NOT the base's
    // mbTweakerAttached at behaviour +0x0A. (That resolves the open FLAG this header carried
    // about "why does an ICE take raise mbTweakerAttached every frame": it never did.)
    mCollisionPolicy.SetSeeThroughEnabled(true);

    // Heading-space look-at: build it for the secondary (look-at) vehicle, then SLerp the
    // behaviour's own heading-space frame towards it. (⚠️ CORRECTED 2026-07-31: the console
    // writes behaviour +0x610, which is mHeadingSpaceTransform -- mLastCamera ENDS at +0x610.
    // The previous reconstruction aimed these stores 0x160 bytes low, into the produced
    // camera's transform, which would have overwritten the camera every frame. It also called
    // a one-argument `Utils::CreateLookAt(spaceArgs)` that does not exist: the console builds
    // the look-at from the anchor vehicle's position + flattened heading, see the helper.)
    if (!mbIsPrepared)
        mHeadingSpaceTransform = CreateHeadingSpaceLookAt(mSecondaryVehicleRef.Get(lpWorld));

    void* lpLookAtVehicle = mSecondaryVehicleRef.Get(lpWorld);
    mHeadingSpaceTransform.wAxis = GetVehicleWorldPosition(lpLookAtVehicle);

    rw::math::vpu::Matrix44Affine lLookAt = CreateHeadingSpaceLookAt(lpLookAtVehicle);
    f32 lfSlerpAmount = KF_HEADING_SPACE_2_SLERP_AMOUNT;
    mHeadingSpaceTransform =
        rw::math::vpu::SLerp(mHeadingSpaceTransform, lLookAt, &lfSlerpAmount);

    // The take evaluator resolves its reference spaces through its own copy of the shared
    // per-frame handler (X360 copy ctor @0x821FAA88).
    ICE::CameraSpaceHandler lSpaces(*lrSharedInfo.GetCameraSpaceHandler());
    mPrimaryVehicleRef.Get(lpWorld);
    mSecondaryVehicleRef.Get(lpWorld);

    // Run the take evaluator: it advances the ICE take and writes the whole camera out of it
    // (transform, FOV, depth of field, the effects block). Dispatched through the
    // ShotController vtable.
    ShotContext lShotContext;
    lShotContext.mpAllVehicleData     = lpWorld;
    lShotContext.mpCameraSpaceHandler = &lSpaces;
    lShotContext.mpTimestep           = &lrSharedInfo.GetTimestep();
    mKeyAnimController.Update(lShotContext, &lrCamera);

    // Seed the behaviour's stored camera from the shared camera the first time only. This is
    // Camera::operator= @0x82233A80 -- a WHOLE-camera memberwise copy.
    if (!mbIsPrepared)
    {
        mLastCamera = lrCamera;
        mbIsPrepared = true;
    }

    // --- Pick the eye space ---
    const ICE::eICESpace leEyeSpace = mKeyAnimController.GetEyeSpace();
    bool lbHasEyeSpace;
    if (IsLooseHeadingSpace(leEyeSpace))
    {
        mAttachedToCarCollisionPolicy.SetVehicleRef(mPrimaryVehicleRef);
        lbHasEyeSpace = true;
        mbUseAttachedToCarCollisionPolicy = true;
    }
    else if (IsLookAtVehicleSpace(leEyeSpace))
    {
        mAttachedToCarCollisionPolicy.SetVehicleRef(mSecondaryVehicleRef);
        lbHasEyeSpace = true;
        mbUseAttachedToCarCollisionPolicy = true;
    }
    else
    {
        lbHasEyeSpace = false;
        mbUseAttachedToCarCollisionPolicy = false;
    }

    // --- Pick the look space + motion-blur amount ---
    const ICE::eICESpace leLookSpace = mKeyAnimController.GetLookSpace();
    const bool lbHasLookSpace = IsLooseHeadingSpace(leLookSpace) || IsLookAtVehicleSpace(leLookSpace);

    // A take anchored to a car gets NO extra motion blur; a free/world take gets it all. The
    // two amounts and both enable flags are one operation on the camera's effects block.
    if ((lbHasEyeSpace || lbHasLookSpace) && !mbForceMotionBlurEverything)
        lrCamera.RequestMotionBlur(0.0f, 1.0f);
    else
        lrCamera.RequestMotionBlur(1.0f, 1.0f);

    // --- The BYSTANDER look space drives depth-of-field, FOV, the looker + the shake ---
    if (leLookSpace == ICE::eICE_BYSTANDER_SPACE)
    {
        // Hand the shared camera the behaviour's produced ORIENTATION (rows 0..2 only -- the
        // console does not copy the position row here) before re-focusing it.
        lrCamera.mTransform.xAxis = mLastCamera.mTransform.xAxis;
        lrCamera.mTransform.yAxis = mLastCamera.mTransform.yAxis;
        lrCamera.mTransform.zAxis = mLastCamera.mTransform.zAxis;

        // The default focus band (0.1 / 0.2 / 0.3 / 0.4 metres -- all four read out of the
        // X360 rodata), keeping the blurriness the take already produced.
        lrCamera.GetDepthOfField().SetParams(0.1f, 0.2f, 0.3f, 0.4f,
                                             mLastCamera.GetDepthOfField().GetBlurriness());
        lrCamera.SetFOV(mLastCamera.GetFOV());

        // Then let the looker track the bystander. FLAG (not yet re-expressed): the console
        // builds a Looker::Parameters on the stack (Parameters::Construct @0x821F8D80 then
        // eleven named overrides -- the two subject sizes 0.5, the two screen offsets
        // GetLookPos().x/.y * 0.1, tracking tolerance 0.1, FOV velocity band 20..130, the two
        // distance tolerances 5.0 / 0.1, mbUseZoom, meZoomType = E_ZOOM_SCREEN_REGION) and
        // calls Utils::Looker::Update @0x8223FBB8 with the bystander's transform (+0x1F0),
        // velocity (+0x330) and AABB (+0x4A0). Left as the named call with the parameter block
        // still to be filled: the offsets into the bystander vehicle are attested but the
        // vehicle type they index has no reconstructed accessor set yet, and inventing three
        // more offset reads would be exactly the kind of guess this wave is retiring.
        // DELETE-WHEN: the race-car/vehicle accessors those three reads need have names.
        const f32 lfTimeStep = lrSharedInfo.GetTimestep().Get(Timestep::E_WORLD);
        (void)lfTimeStep;
        mBystanderRef.Get(lpWorld);
    }

    // Copy the shared camera back into the behaviour's stored camera (Camera::operator=).
    mLastCamera = lrCamera;

    if (leLookSpace == ICE::eICE_BYSTANDER_SPACE)
    {
        // The wobble/shake post-process, over the camera's own transform. The console builds
        // a CameraShake::Parameters on the stack -- {XY shake 0, Z shake 0, XY wobble 1.0,
        // wobble centering 0.25} -- and passes the shared info's Random plus a 1.0 speed ratio.
        Utils::CameraShake::Parameters lShakeParams;
        lShakeParams.mfXYShakeMagnitudeDegs  = 0.0f;
        lShakeParams.mfZShakeMagnitudeDegs   = 0.0f;
        lShakeParams.mfXYWobbleMagnitudeDegs = 1.0f;
        lShakeParams.mfWobbleCenteringFactor = 0.25f;

        const f32 lfShakeTimeStep = lrSharedInfo.GetTimestep().Get(Timestep::E_WORLD);
        mShake.Update(lrCamera.mTransform, lShakeParams, *lrSharedInfo.GetRandom(),
                      lfShakeTimeStep, 1.0f);
    }

    // --- "Can't cut TO me" gate ---
    if (mbUseCollisionPolicy)
    {
        // Inner guard: only raise it when the free visibility policy's three state bytes say
        // so: mbSeeThroughAlways || (mbSeeThroughEnabled && !mbSeeThroughSuppressed).
        if (mCollisionPolicy.ShouldRaiseSeeThrough())
        {
            // The console's two stores here -- validity-account bit 16 at camera +0x138, then
            // `mbCanSwitchToMeNow = false` -- ARE Behaviour::SetCantSwitchToMeNow's body. This
            // is that method's first attested call site, and it pins flag 16 into the
            // no-cut-TO band that BrnCameraValidityAccount.h flags as unattested.
            SetCantSwitchToMeNow(lrCamera, 16);
        }
    }

    // SetCantSwitchFromMeNow's first argument is the CAMERA (the account it stamps lives at
    // camera +0x138) -- the retired fork threaded the shared info there by mistake. Reason 29
    // sits inside the account's attested no-cut-from band [27,31).
    if (!HasFinishedOrFailed() && !HasFailed())
        SetCantSwitchFromMeNow(lrCamera, 29);

    // --- Debug visibility readout ---
    // The console's `r3` here is the shared info's own DebugPrinter (info +1488), which the
    // committed BehaviourSharedInfo already exposes by its real type -- GetDebugPrinter(), not
    // the untyped GetDebugSink() the retired fork used to pair with its fabricated static
    // ActualPrint. See the RETIRED note at the top of this file.
    BrnDirector::DebugPrinter* lpDebugPrinter = lrSharedInfo.GetDebugPrinter();
    if (!IsLookingAtTarget(lrCamera, lrSharedInfo.GetEyeTarget(), lrSharedInfo.GetLookTarget()))
        lpDebugPrinter->Print("Can't see player", static_cast<CgsDev::RGBA>(0xFFF0F0FF));
    else
        lpDebugPrinter->Print("Can see player", static_cast<CgsDev::RGBA>(0xFFF0FFF0));

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
    const ICE::ICETakeData* lpTakeData = mKeyAnimController.GetTake().GetData();
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
