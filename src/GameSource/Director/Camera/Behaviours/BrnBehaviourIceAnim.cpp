#include "GameSource/Director/Camera/Behaviours/BrnBehaviourIceAnim.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"            // CgsDev::Assert (BeginAssert/FireAssert/EndAssert)
#include "GameShared/GameClasses/Graphics/CgsCamera.h"        // CgsGraphics::Camera + CameraRwFrustum (IsLookingAtTarget)
#include "vendor/renderware/collision/Frustum.hpp"            // rw::collision::Frustum::IsBoxInFrustum
#include "GameSource/Director/Camera/Utils/CameraUtils.h"     // Camera::Utils::CreateLookAt (the real home)
#include "SDKs/Packages/ICE/ICECameraSpaceHandler.hpp"        // ICE::CameraSpaceHandler (the real home)
#include "SDKs/Packages/ICE/ICEAuthor.hpp"                    // ICE::ICEAuthor::FindEditedTakeFromGuid --
                                                              //   THE home (2026-07-31). The header's own
                                                              //   `class ICEAuthor` slice is retired; the
                                                              //   real one is a `struct`, which mangles
                                                              //   differently, so this include is what makes
                                                              //   the two calls below link.
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"
                                                              // BrnDirector::Camera::VehicleInfo -- the
                                                              //   real pointee of what VehicleRef::Get
                                                              //   hands back (its mRaceCarState.mTransform
                                                              //   IS the +0x1F0 the two heading-space
                                                              //   helpers below form on the console).
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

    // ------------------------------------------------------------------------
    // IsLookingAtTarget @0x822331F0 -- true when the produced camera's frustum still contains
    // the target's oriented bounding box.
    //
    // ⚠️ THE PARAMETER NAMES ARE MISLEADING and are corrected here. They were inherited from
    // a retired fork that read the pair as "eye target / look target"; the asm says otherwise.
    // The second argument is read at +0x00/+0x10/+0x20/+0x30 -- four rows, i.e. a
    // Matrix44Affine -- and the third at +0x00/+0x10 only, i.e. an {min,max} box. They are the
    // TARGET's world transform and the TARGET's local bounds. The caller passes the shared
    // info's mPlayerInfo.mRaceCarState.mTransform (info +592) and mPlayerInfo.mAABB (info
    // +1280), which independently confirms both types.
    // ⭐ NOW PROPERLY TYPED (2026-08-01): BehaviourSharedInfo::GetEyeTarget/GetLookTarget
    // return the real sub-objects of the embedded mPlayerInfo, so the untyped `const void*`
    // pair the retired fork forced is gone.
    // DECLARATION-ONLY; body below.
    bool IsLookingAtTarget(const Camera& lrCamera,
                           const rw::math::vpu::Matrix44Affine& lrTargetTransform,
                           const AABBox& lrTargetBounds);

    // ------------------------------------------------------------------------
    // The HEADING-SPACE frame of an anchor vehicle: a look-at built at the vehicle's world
    // position, aimed along its forward axis FLATTENED to horizontal. X360-attested
    // (BehaviourIceAnim::Update @0x82247270..0x822472B0): it takes the vehicle's world
    // transform (vehicle +0x1F0), splats its at-row's x and z lanes into {at.x, 0, at.z, 0},
    // adds that to the position row, and calls Utils::CreateLookAt(position, position + that).
    //
    // The untyped parameter is what BrnDirector::VehicleRef::Get hands back. Its real pointee
    // IS reconstructed -- BrnDirector::Camera::VehicleInfo (SharedIO/BrnPlayerInfo.h), whose
    // mRaceCarState.mTransform sits at +0x1F0 == the 496 the asm forms -- so the bodies below
    // cast to it by NAME instead of forming displacements. The DECLARATIONS keep `const void*`
    // because VehicleRef::Get's own return type is still untyped.
    rw::math::vpu::Matrix44Affine CreateHeadingSpaceLookAt(const void* lpVehicle);
    rw::math::vpu::Vector3        GetVehicleWorldPosition(const void* lpVehicle);

    // ------------------------------------------------------------------------
    // CreateHeadingSpaceLookAt -- BODIED 2026-08-01.
    //
    // No standalone X360 symbol: the console inlines it TWICE inside
    // BehaviourIceAnim::Update (0x82247270-0x822472B0 and 0x822472D8-0x8224733C), which is why
    // a name search finds nothing. Both blocks are identical and both store the result into
    // mHeadingSpaceTransform, which is what pins the identity.
    //
    //   addi r11, r3, 0x1F0            ; &mRaceCarState.mTransform
    //   vspltisw v11, 0                ; 0.0f
    //   v13 = splat(zAxis, lane 0)     ; zAxis.x
    //   v12 = splat(zAxis, lane 2)     ; zAxis.z
    //   v1  = wAxis                    ; lvx r11, 0x30
    //   v0  = vperm(v13, v11, mask@0x82CDA350)   )  the standard rw Vector3(x,y,z)
    //   v0  = vrlimi128(v0, v12, 2, 0)           )  construction codegen
    //   v2  = v1 + v0
    //   CreateLookAt(eye = v1, target = v2)
    //
    // The vperm+vrlimi pair is Vector3's three-float constructor (verified against
    // BrnGui::MapTransform::MakeCoordSpaceFromRect @0x82450460, which uses the same pair three
    // times with distinct sources). So the added vector is Vector3(zAxis.x, 0, zAxis.z) -- the
    // forward axis flattened to horizontal, NOT normalised (CreateLookAt normalises).
    // Argument order is attested inside CreateLookAt @0x8220C4F8, which asserts
    // "IsValid(lEyePosition)" on its first vector and "IsValid(lTargetPosition)" on its second.
    // ------------------------------------------------------------------------
    inline rw::math::vpu::Matrix44Affine CreateHeadingSpaceLookAt(const void* lpVehicle)
    {
        const rw::math::vpu::Matrix44Affine& lrTransform =
            static_cast<const VehicleInfo*>(lpVehicle)->mRaceCarState.mTransform;

        // v0 = Vector3(zAxis.x, 0, zAxis.z) -- the forward axis flattened to horizontal.
        const rw::math::vpu::Vector3 lFlatHeading =
            { lrTransform.zAxis.x, 0.0f, lrTransform.zAxis.z, 0.0f };

        const rw::math::vpu::Vector3 lEye    = lrTransform.wAxis;      // v1
        const rw::math::vpu::Vector3 lTarget = { lEye.x + lFlatHeading.x,   // v2 = v1 + v0
                                                 lEye.y + lFlatHeading.y,
                                                 lEye.z + lFlatHeading.z,
                                                 0.0f };

        return Utils::CreateLookAt(lEye, lTarget);
    }

    // ------------------------------------------------------------------------
    // GetVehicleWorldPosition -- BODIED 2026-08-01.
    //
    // Also inlined, at 0x822472E4-0x82247308 (immediately before the second
    // CreateHeadingSpaceLookAt): one 16-byte lane read, no branches, no asserts.
    //   addi r11, r3, 0x1F0   ; &mRaceCarState.mTransform
    //   addi r10, r11, 0x30   ; &.wAxis
    //   lvx128 v0, r0, r10  /  stvx128 v0, r31, r9   ; r9 == 0x640 == mHeadingSpaceTransform.wAxis
    // ------------------------------------------------------------------------
    inline rw::math::vpu::Vector3 GetVehicleWorldPosition(const void* lpVehicle)
    {
        const rw::math::vpu::Matrix44Affine& lrTransform =
            static_cast<const VehicleInfo*>(lpVehicle)->mRaceCarState.mTransform;

        return lrTransform.wAxis;   // the whole 16-byte lane, as the single lvx128/stvx128 pair does
    }

    // ------------------------------------------------------------------------
    // IsLookingAtTarget @0x822331F0 -- BODIED 2026-08-01. Full asm walk:
    //   0x82233210  bl CopyToCgsCamera(camera, &lCgsCamera)
    //   0x82233220  bl CgsGraphics::Camera::GetFrustumPerspective(&lCgsCamera, &lFrustum, false)
    //   0x82233240  lfs f0, flt_82CDAD44          ; K, spilled twice as {K,0,0,0} then vspltw'd
    //   0x8223322C  lvx128 v12, r0,  r30          ; lrTargetBounds.mMin
    //   0x8223325C  lvx128 v0,  r30, 0x10         ; lrTargetBounds.mMax
    //   0x82233234  lvx128 v11, r0,  r31          ; transform.xAxis
    //   0x8223327C  lvx128 v10, r31, 0x10         ; transform.yAxis
    //   0x82233298  lvx128 v13, r31, 0x20         ; transform.zAxis
    //   0x82233270  lvx128 v9,  r31, 0x30         ; transform.wAxis
    //   0x8223329C  v0  = max * K                 ; lHi
    //   0x822332A4  v12 = min * K                 ; lLo
    //   ... eight corners = wAxis + xAxis*sel.x + yAxis*sel.y + zAxis*sel.z ...
    //   0x82233354  bl rw::collision::Frustum::IsBoxInFrustum(&lFrustum, laCorners)
    //   0x82233358  cntlzw/extrwi/xori            ; return (result != 0)
    //
    // ⭐ K == flt_82CDAD44 == 0x3F400000 == 0.75f. This value had ONE witness and a prior wave
    // flagged it "get a second before shipping" -- correctly, because the reader that produced
    // it had silently drifted (its own sanity check fails today). Recalibrated this wave: the
    // .id1 address mapping was off by 1594 bytes; the new calibration is agreed by NINE
    // independent function prologues and reproduces flt_82001C98 == 1.0f, flt_82001CC0 == 0.0f
    // and five constants this subsystem had already derived by other means. 0.75f confirmed.
    // The box is therefore SHRUNK to three quarters before the test -- the target has to be
    // comfortably inside the frustum, not merely clipping its edge.
    //
    // ⚠️ TRANSCRIPTION TRAP: IDA prints `vmaddfp` in RAW FIELD ORDER (VD,VA,VB,VC), not
    // mnemonic order. `vmaddfp v7, v11, v9, v7` is v7 = v11*v7 + v9, i.e.
    // xAxis*splat(sel.x) + wAxis. Read literally it says "row0 * position + splat(x)", which
    // is nonsense -- that misreading is what makes this function look unrecoverable.
    // ------------------------------------------------------------------------
    bool IsLookingAtTarget(const Camera& lrCamera,
                           const rw::math::vpu::Matrix44Affine& lrTargetTransform,
                           const AABBox& lrTargetBounds)
    {
        // The fraction of the target's own bounds the test uses (flt_82CDAD44).
        const f32 KF_TARGET_BOX_SCALE = 0.75f;

        CgsGraphics::Camera lCgsCamera;
        lrCamera.CopyToCgsCamera(&lCgsCamera);

        CgsGraphics::CameraRwFrustum lFrustum;
        lCgsCamera.GetFrustumPerspective(lFrustum, false);

        // vmulfp128 v12, v12, splat(K) / vmulfp128 v0, v0, splat(K) -- the whole 4-lane
        // register is scaled, so the w lane is scaled too (it is never read downstream).
        const rw::math::vpu::Vector3& lrMin = lrTargetBounds.mMin;
        const rw::math::vpu::Vector3& lrMax = lrTargetBounds.mMax;
        const f32 lfLoX = lrMin.x * KF_TARGET_BOX_SCALE;
        const f32 lfLoY = lrMin.y * KF_TARGET_BOX_SCALE;
        const f32 lfLoZ = lrMin.z * KF_TARGET_BOX_SCALE;
        const f32 lfHiX = lrMax.x * KF_TARGET_BOX_SCALE;
        const f32 lfHiY = lrMax.y * KF_TARGET_BOX_SCALE;
        const f32 lfHiZ = lrMax.z * KF_TARGET_BOX_SCALE;

        // The console emits the eight corners in this exact order (the stvx128 sequence at
        // 0x82233318..0x82233350, ascending stack address).
        const f32 laSelectX[8] = { lfHiX, lfHiX, lfHiX, lfLoX, lfLoX, lfLoX, lfHiX, lfLoX };
        const f32 laSelectY[8] = { lfHiY, lfHiY, lfLoY, lfHiY, lfLoY, lfHiY, lfLoY, lfLoY };
        const f32 laSelectZ[8] = { lfHiZ, lfLoZ, lfHiZ, lfHiZ, lfHiZ, lfLoZ, lfLoZ, lfLoZ };

        const rw::math::vpu::Vector3& lrX = lrTargetTransform.xAxis;
        const rw::math::vpu::Vector3& lrY = lrTargetTransform.yAxis;
        const rw::math::vpu::Vector3& lrZ = lrTargetTransform.zAxis;
        const rw::math::vpu::Vector3& lrW = lrTargetTransform.wAxis;

        rw::collision::Frustum::Vec4 laCorners[8];
        for (s32 liCorner = 0; liCorner < 8; ++liCorner)
        {
            const f32 lfSX = laSelectX[liCorner];
            const f32 lfSY = laSelectY[liCorner];
            const f32 lfSZ = laSelectZ[liCorner];

            laCorners[liCorner].x = lrW.x + lrX.x * lfSX + lrY.x * lfSY + lrZ.x * lfSZ;
            laCorners[liCorner].y = lrW.y + lrX.y * lfSX + lrY.y * lfSY + lrZ.y * lfSZ;
            laCorners[liCorner].z = lrW.z + lrX.z * lfSX + lrY.z * lfSY + lrZ.z * lfSZ;
            laCorners[liCorner].w = lrW.w + lrX.w * lfSX + lrY.w * lfSY + lrZ.w * lfSZ;
        }

        return rw::collision::Frustum::IsBoxInFrustum(
                   reinterpret_cast<const rw::collision::Frustum::Vec4*>(lFrustum.maPlanes),
                   laCorners) != 0;
    }

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

    // ------------------------------------------------------------------------
    // ⭐⭐ THE THREE ANCHOR VEHICLE REFERENCES. RESTORED 2026-08-01 -- THEY WERE MISSING,
    // and their absence is why every ICE-anim camera in the game produced nothing.
    //
    // Update @0x82247108 opens with `IsValid(mPrimaryVehicleRef)` / `IsValid(mSecondary-
    // VehicleRef)` and takes Behaviour::Fail on either failure; VehicleRef::IsValid tests
    // mbSet FIRST. With these seeds absent the refs were whatever the behaviour pool slot
    // happened to hold (zero), so Update failed out on its first line every frame and
    // mLastCamera stayed exactly as BehaviourHelper::Prepare's Camera::Construct left it --
    // identity basis at the world origin. That IS the "eye (0,0,0) at (0,0,1)" symptom the
    // director trace showed to the last frame.
    //
    // The console's stores (Construct @0x82256100, 0x82256154..0x8225618C) are the inlined
    // VehicleRef::Construct + VehicleRef::Set pair for each ref, in this order:
    //     mPrimaryVehicleRef    stb 0/1 +0xDFC ; stw 0 +0xDF0 ; stw 0 +0xDF8 ; stw -1 +0xDF4
    //     mSecondaryVehicleRef  stb 0/1 +0xE0C ; stw 2 +0xE00 ; stw -1 +0xE04 ; stw 1 +0xE08
    //     mBystanderRef         stb 0/1 +0xE1C ; stw 0 +0xE10 ; stw 0 +0xE18 ; stw -1 +0xE14
    // i.e. the EYE anchor and the BYSTANDER anchor are the PLAYER's car, and the LOOK-AT
    // anchor is the nearest race car at RANK 1 (E_RACE_CAR_NEAREST_PLAYER, muRef == 1).
    // Written through the named Construct/Set pair so nothing is poked by offset; the
    // leading Construct is the console's own `stb 0` before the `stb 1`, not decoration.
    // ------------------------------------------------------------------------
    mPrimaryVehicleRef.Construct();
    mPrimaryVehicleRef.Set(BrnDirector::VehicleRef::E_PLAYER_CAR,
                           E_ACTIVE_RACE_CAR_INDEX_INVALID, 0u);

    mSecondaryVehicleRef.Construct();
    mSecondaryVehicleRef.Set(BrnDirector::VehicleRef::E_RACE_CAR_NEAREST_PLAYER,
                             E_ACTIVE_RACE_CAR_INDEX_INVALID, 1u);

    mBystanderRef.Construct();
    mBystanderRef.Set(BrnDirector::VehicleRef::E_PLAYER_CAR,
                      E_ACTIVE_RACE_CAR_INDEX_INVALID, 0u);

    // --- the embedded controller's playback reset (X360 @0x82256190..0x822561A0) ---
    // The console re-bases onto the controller (`this+0x680`) and stores 0.0f -> +0x760
    // (mfPlaybackTimer) + 0 -> the four playback flags. The previous slice mis-homed this as
    // a fabricated behaviour-level "reset block" at +0xDE0; see ResetPlayback's banner.
    mKeyAnimController.ResetPlayback();

    // --- the produced camera + the attached-to-car collision policy ---
    mLastCamera.Construct();
    mAttachedToCarCollisionPolicy.Construct(0);
}

// ============================================================================
// SetParameters @0x8220F5C0
// ----------------------------------------------------------------------------
// Assert the shot reference is non-null and carries the iceanim class key, RESOLVE it into
// a live attribute instance, read the take guid out of that instance's layout block, and
// store the reference + guid.
//
// ⭐ THE DECODE, WHICH WAS WRONG UNTIL 2026-08-01. lpParameters is one element of a
// shotgroup's ShotList -- a raw 24-byte Attrib::RefSpec {mClassKey, mCollectionKey,
// mpCollectionPtr}. It is NOT an already-constructed Attrib::Instance, so the take guid is
// not reachable off it directly. The console makes that explicit:
//
//     0x8220F63C  li   r5, 0                          ; owner = 0
//     0x8220F640  mr   r4, r30                        ; the RefSpec
//     0x8220F644  addi r3, r1, var_30                 ; a STACK iceanim
//     0x8220F648  bl   Attrib__Gen__iceanim__iceanim  ; resolve the ref
//     0x8220F64C  lwz  r11, var_2C(r1)                ; temporary + 4 == mpAttributeData
//     0x8220F654  lwz  r11, 0xC(r11)                  ; the LAYOUT block's +0xC
//     0x8220F658  stw  r30, 0xE24(r29)                ; mpSourceShot = lpParameters
//     0x8220F65C  stw  r11, 0xE20(r29)                ; miAnimGuid
//     0x8220F660  bl   Attrib__Instance___Instance    ; ~iceanim (drops the handle's ref)
//
// i.e. the guid is at the RESOLVED LAYOUT block +0xC, reached through a temporary instance
// built over the ref -- never at RefSpec+0xC and never at RefSpec+4. The previous
// reconstruction called GetAnimGuid() straight on the parameter, which took RefSpec+4 (the
// high half of mCollectionKey) for mpAttributeData and dereferenced it: a garbage take guid
// -- a camera behaviour that links and boots and plays nothing. The class-key assert below
// did NOT catch it: a RefSpec's class key IS its leading qword, so the check passed for the
// wrong reason.
//
// The temporary is a real ref-counted handle and is destroyed at the end of the statement,
// exactly as the console destroys its stack copy. It is deliberately NOT cached: mpSourceShot
// keeps the REFERENCE (whose lifetime is the shot group's), and Prepare/ChangeMovie re-resolve
// from miAnimGuid.
// ============================================================================
void BehaviourIceAnim::SetParameters(ShotReference* lpParameters)
{
    if (!lpParameters)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpParameters != NULL", KPC_SOURCE_FILE, 325);
        CgsDev::Assert::EndAssert();
    }

    // The class-key test compares the reference's STORED class key (its leading 8-byte tag,
    // X360 `ld r11, 0(r30)`) against the generated iceanim class key.
    if (static_cast<s64>(lpParameters->GetClassKey()) != Attrib::Gen::iceanim::ClassKey())
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpParameters->GetClassKey()==Attrib::Gen::iceanim::ClassKey()", KPC_SOURCE_FILE, 326);
        CgsDev::Assert::EndAssert();
    }

    // Resolve the reference into a live iceanim instance and read the guid off its layout
    // block. The instance is a scoped temporary, as on the console.
    const Attrib::Gen::iceanim lShot(*lpParameters, 0);

    mpSourceShot = lpParameters;
    miAnimGuid   = lShot.GetAnimGuid();
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

    // ⭐ Reset the embedded controller's playback state for the NEW take (X360
    // @0x8220F698..0x8220F6AC: `addi r3, this, 0x680` then 0.0f -> controller +0x760
    // mfPlaybackTimer + 0 -> the four playback flags). It does NOT touch the behaviour-mode
    // flags at +0xE28..+0xE2B. THIS REWIND IS THE GAME-INTRO REVEAL (2026-08-05): the
    // previous slice zeroed a fabricated behaviour-level block instead, so a take changed
    // after the long-held 40 s intro shot inherited a saturated timer, clamped to the new
    // length on its first Update, and reported HasFinished() immediately -- the authored
    // junkyard reveal collapsed to one frozen top-down frame.
    mKeyAnimController.ResetPlayback();

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

    const VehicleInfo* lpLookAtVehicle = mSecondaryVehicleRef.Get(lpWorld);
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
        (void)lfShakeTimeStep;

        // [GATED 2026-08-01 -- the bystander-space wobble post-process]
        //   mShake.Update(lrCamera.mTransform, lShakeParams, *lrSharedInfo.GetRandom(),
        //                 lfShakeTimeStep, 1.0f);
        // WHY: Utils::CameraShake::Update @0x82221310 IS bodied, in
        // Camera/Utils/BrnCameraShake.cpp -- but that TU is not in the link and mounting it
        // TODAY costs +5 net unresolved (MEASURED 2026-08-01 with scratchpad\ice5_net.py):
        // CgsNumeric::Random::{RandomFloat,RandomVector}, Utils::
        // RotateMatrix44AffineByEulerAnglesZXY, and the three
        // <Serialiser>::Serialise(const char*, f32&) overloads its Parameters::Serialise<S>
        // instantiations pull in. Without this gate, mounting BrnBehaviourIceAnim.cpp -- which
        // is what makes the REAL junkyard / game-intro ICE cameras run -- is blocked on a
        // camera WOBBLE.
        // COST: ICE shots authored in eICE_BYSTANDER_SPACE render without their 1-degree
        // procedural wobble. Nothing else: CameraShake::Update only post-multiplies the
        // transform it is handed, and mShake carries no state any other code reads.
        // ⚠️ This is the ONLY thing in the ICE-anim behaviour that is gated; every other
        // branch of RunShake and of Update is live.
        // DELETE-WHEN: BrnCameraShake.cpp joins the link (body the three Serialise overloads
        // and the two Random draws first, then the mount is free).
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
