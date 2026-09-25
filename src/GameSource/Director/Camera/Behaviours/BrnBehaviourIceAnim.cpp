#include "GameSource/Director/Camera/Behaviours/BrnBehaviourIceAnim.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"            // CgsDev::Assert (BeginAssert/FireAssert/EndAssert)
#include "GameShared/GameClasses/Graphics/CgsCamera.h"        // CgsGraphics::Camera + CameraRwFrustum (IsLookingAtTarget)
#include "vendor/renderware/collision/Frustum.hpp"            // rw::collision::Frustum::IsBoxInFrustum
#include "rw/math/vpu/matrix44affine_operation.h"             // rw::math::vpu::SLerp (X360 0x82216858), the heading blend
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
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // [DIAG] CgsDev::Log::WriteToLog (the BYSTANDER look witness)
#include <cstdio>                                             // [DIAG] snprintf
#include <cstdlib>                                            // [DIAG] getenv (BRN_CRASHCAM_DIAG)
#include <math.h>                                             // [DIAG] sqrtf / acosf (the witness's aim angle)

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
// Ten bodyless free functions used to sit here as a NAMING DEVICE for camera writes the
// original compiler inlined into Update. Every one of them was real code, but the naming
// was wrong in ways that mattered, so all ten are gone and each write now goes through the
// API that actually owns it:
//
//   CopyTransformFrom      -> `mLastCamera = lrCamera;`  (Camera::operator= -- a
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
//   SetFOV / GetFOV        -> `lrCamera.SetFOV(mLastCamera.GetFOV())` (the getter was a
//                             single inlined field load).
//   RunLooker              -> Looker::Parameters::Construct on a stack block, 11
//                             named overrides, then Utils::Looker::Update.
//   RunShake               -> Utils::CameraShake::Update over the camera's own
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
    // IsLookingAtTarget -- true when the produced camera's frustum still contains
    // the target's oriented bounding box.
    //
    // ⚠️ THE PARAMETER NAMES ARE MISLEADING and are corrected here. They were inherited from
    // a retired fork that read the pair as "eye target / look target"; the attested accesses
    // say otherwise.
    // The second argument is read at +0x00/+0x10/+0x20/+0x30 -- four rows, i.e. a
    // Matrix44Affine -- and the third at +0x00/+0x10 only, i.e. an {min,max} box. They are the
    // TARGET's world transform and the TARGET's local bounds. The caller passes the shared
    // info's mPlayerInfo.mRaceCarState.mTransform (info +0x250) and mPlayerInfo.mAABB (info
    // +0x500), which independently confirms both types.
    // ⭐ NOW PROPERLY TYPED (2026-08-01): BehaviourSharedInfo::GetEyeTarget/GetLookTarget
    // return the real sub-objects of the embedded mPlayerInfo, so the untyped `const void*`
    // pair the retired fork forced is gone.
    // DECLARATION-ONLY; body below.
    bool IsLookingAtTarget(const Camera& lrCamera,
                           const rw::math::vpu::Matrix44Affine& lrTargetTransform,
                           const AABBox& lrTargetBounds);

    // ------------------------------------------------------------------------
    // The HEADING-SPACE frame of an anchor vehicle: a look-at built at the vehicle's world
    // position, aimed along its forward axis FLATTENED to horizontal. Attested inside
    // BehaviourIceAnim::Update: it takes the vehicle's world
    // transform (vehicle +0x1F0), splats its at-row's x and z lanes into {at.x, 0, at.z, 0},
    // adds that to the position row, and calls Utils::CreateLookAt(position, position + that).
    //
    // The untyped parameter is what BrnDirector::VehicleRef::Get hands back. Its real pointee
    // IS reconstructed -- BrnDirector::Camera::VehicleInfo (SharedIO/BrnPlayerInfo.h), whose
    // mRaceCarState.mTransform sits at the attested +0x1F0 -- so the bodies below
    // cast to it by NAME instead of forming displacements. The DECLARATIONS keep `const void*`
    // because VehicleRef::Get's own return type is still untyped.
    rw::math::vpu::Matrix44Affine CreateHeadingSpaceLookAt(const void* lpVehicle);
    rw::math::vpu::Vector3        GetVehicleWorldPosition(const void* lpVehicle);

    // ------------------------------------------------------------------------
    // CreateHeadingSpaceLookAt -- BODIED 2026-08-01.
    //
    // There is no standalone symbol for it: the original build inlines it TWICE inside
    // BehaviourIceAnim::Update, which is why a name search finds nothing. Both inlined copies
    // are identical and both store the result into mHeadingSpaceTransform, which is what pins
    // the identity. The attested sequence is:
    //
    //   take &mRaceCarState.mTransform (vehicle +0x1F0)
    //   eye    = transform.wAxis                        (the +0x30 row, whole 16-byte lane)
    //   flat   = Vector3(zAxis.x, 0, zAxis.z)           (built with the standard
    //                                                    three-float Vector3 construction)
    //   target = eye + flat
    //   CreateLookAt(eye, target)
    //
    // The construction idiom is Vector3's three-float constructor (verified against
    // BrnGui::MapTransform::MakeCoordSpaceFromRect, which uses the same pair three
    // times with distinct sources). So the added vector is Vector3(zAxis.x, 0, zAxis.z) -- the
    // forward axis flattened to horizontal, NOT normalised (CreateLookAt normalises).
    // Argument order is attested inside CreateLookAt, which asserts
    // "IsValid(lEyePosition)" on its first vector and "IsValid(lTargetPosition)" on its second.
    // ------------------------------------------------------------------------
    inline rw::math::vpu::Matrix44Affine CreateHeadingSpaceLookAt(const void* lpVehicle)
    {
        const rw::math::vpu::Matrix44Affine& lrTransform =
            static_cast<const VehicleInfo*>(lpVehicle)->mRaceCarState.mTransform;

        // Vector3(zAxis.x, 0, zAxis.z) -- the forward axis flattened to horizontal.
        const rw::math::vpu::Vector3 lFlatHeading =
            { lrTransform.zAxis.x, 0.0f, lrTransform.zAxis.z, 0.0f };

        const rw::math::vpu::Vector3 lEye    = lrTransform.wAxis;
        const rw::math::vpu::Vector3 lTarget = { lEye.x + lFlatHeading.x,   // target = eye + flat
                                                 lEye.y + lFlatHeading.y,
                                                 lEye.z + lFlatHeading.z,
                                                 0.0f };

        return Utils::CreateLookAt(lEye, lTarget);
    }

    // ------------------------------------------------------------------------
    // GetVehicleWorldPosition -- BODIED 2026-08-01.
    //
    // Also inlined, immediately before the second CreateHeadingSpaceLookAt: one 16-byte lane
    // read, no branches, no asserts. It reads &mRaceCarState.mTransform (vehicle +0x1F0),
    // steps to its .wAxis row (+0x30) and copies that whole lane into the behaviour's
    // mHeadingSpaceTransform.wAxis (behaviour +0x640).
    // ------------------------------------------------------------------------
    inline rw::math::vpu::Vector3 GetVehicleWorldPosition(const void* lpVehicle)
    {
        const rw::math::vpu::Matrix44Affine& lrTransform =
            static_cast<const VehicleInfo*>(lpVehicle)->mRaceCarState.mTransform;

        return lrTransform.wAxis;   // the whole 16-byte lane, as the single load/store pair does
    }

    // ------------------------------------------------------------------------
    // IsLookingAtTarget -- BODIED 2026-08-01. The attested sequence is:
    //   CopyToCgsCamera(camera, &lCgsCamera)
    //   CgsGraphics::Camera::GetFrustumPerspective(&lCgsCamera, &lFrustum, false)
    //   load the scale constant K and splat it across all four lanes
    //   read lrTargetBounds.mMin (+0x00) and lrTargetBounds.mMax (+0x10)
    //   read transform.xAxis / .yAxis / .zAxis / .wAxis (+0x00 / +0x10 / +0x20 / +0x30)
    //   lHi = max * K ; lLo = min * K   (whole 4-lane multiply, w lane included)
    //   eight corners = wAxis + xAxis*sel.x + yAxis*sel.y + zAxis*sel.z
    //   rw::collision::Frustum::IsBoxInFrustum(&lFrustum, laCorners)
    //   return (result != 0)
    //
    // ⭐ K == 0x3F400000 == 0.75f. This value had ONE witness and a prior wave flagged it
    // "get a second before shipping" -- correctly, because the reader that produced it had
    // silently drifted (its own sanity check fails today). Recalibrated this wave: the constant
    // mapping was off by 1594 bytes; the new calibration is agreed by NINE independent function
    // prologues and reproduces two known 1.0f / 0.0f constants plus five constants this
    // subsystem had already derived by other means. 0.75f confirmed.
    // The box is therefore SHRUNK to three quarters before the test -- the target has to be
    // comfortably inside the frustum, not merely clipping its edge.
    //
    // ⚠️ TRANSCRIPTION TRAP: the fused multiply-add in the corner loop is printed by the
    // disassembler in raw field order, not operand order. Read literally it says
    // "row0 * position + splat(x)", which is nonsense; the real operation is
    // xAxis*splat(sel.x) + wAxis. That misreading is what makes this function look
    // unrecoverable.
    // ------------------------------------------------------------------------
    bool IsLookingAtTarget(const Camera& lrCamera,
                           const rw::math::vpu::Matrix44Affine& lrTargetTransform,
                           const AABBox& lrTargetBounds)
    {
        // The fraction of the target's own bounds the test uses.
        const f32 KF_TARGET_BOX_SCALE = 0.75f;

        CgsGraphics::Camera lCgsCamera;
        lrCamera.CopyToCgsCamera(&lCgsCamera);

        CgsGraphics::CameraRwFrustum lFrustum;
        lCgsCamera.GetFrustumPerspective(lFrustum, false);

        // Both bounds are scaled by K as whole 4-lane vectors, so the w lane is scaled too
        // (it is never read downstream).
        const rw::math::vpu::Vector3& lrMin = lrTargetBounds.mMin;
        const rw::math::vpu::Vector3& lrMax = lrTargetBounds.mMax;
        const f32 lfLoX = lrMin.x * KF_TARGET_BOX_SCALE;
        const f32 lfLoY = lrMin.y * KF_TARGET_BOX_SCALE;
        const f32 lfLoZ = lrMin.z * KF_TARGET_BOX_SCALE;
        const f32 lfHiX = lrMax.x * KF_TARGET_BOX_SCALE;
        const f32 lfHiY = lrMax.y * KF_TARGET_BOX_SCALE;
        const f32 lfHiZ = lrMax.z * KF_TARGET_BOX_SCALE;

        // The console emits the eight corners in this exact order (the store sequence runs
        // in ascending stack address).
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
// The real one is a NON-static, PRIVATE member `void DebugPrinter::ActualPrint(const
// char*, CgsDev::RGBA)`; the implicit first argument at those two call sites is the printer
// itself, not an explicit parameter. Spelling it `static ActualPrint(void*, const char*, s32)` minted a
// completely different mangled symbol that no TU in the tree could ever define -- it would
// have stayed unresolved for ever while looking, in the source, like a call that just needed
// its home mounted. The faithful spelling is the PUBLIC forwarder
// `DebugPrinter::Print(text, colour)`, which the console inlines to exactly that direct
// ActualPrint call (the forwarder is now bodied in the home header for that reason).
// ============================================================================

} // namespace BrnDirector

// ============================================================================
// RETIRED (2026-09-26, crash parity FX-LASTFIX): the local re-declaration
//     namespace rw { namespace math { namespace vpu {
//         Matrix44Affine SLerp(const Matrix44Affine&, const Matrix44Affine&, const f32* lpfAmount); }}}
// that used to sit here is GONE, with its mounted link stub (DirectorLinkStubs.cpp GROUP D), which returned `lrTo`.
// No such overload exists on the console: Update's `bl 0x82247354` goes to rw::math::vpu::SLerp @0x82216858, the
// four-argument body in rw/math/vpu/matrix44affine_operation.h (the amount a splat in v1, the angle out in r6). With
// the stub and an amount of 1.0f the heading space SNAPPED to the look-at every frame; the console eases it 20% a frame.
// ============================================================================

namespace BrnDirector
{
namespace Camera
{

// ----------------------------------------------------------------------------
// File-scope constants.
// ----------------------------------------------------------------------------
// The heading-space-to-look SLerp blend amount: 0.2 a frame. The console's is the class-static
// `const VecFloat BehaviourIceAnim::KF_HEADING_SPACE_2_SLERP_AMOUNT` (DWARF BrnBehaviourIceAnim.h:181, defined at
// BrnBehaviourIceAnim.cpp:21) -- the .bss splat unk_82FAA6F0, which its CRT dynamic initializer 0x82C49580 fills from
// flt_82004744 (0x3E4CCCCD): lfs, stfs, lvlx, vspltw 0, stvx128. Update loads it into v127 at 0x82247334 and passes it
// as SLerp's v1 at 0x82247340; SLerp reads its lanes as the one scalar amount.
static const f32 KF_HEADING_SPACE_2_SLERP_AMOUNT = 0.2f;

// The BYSTANDER look space's Looker block (Update 0x82247714..0x82247760): the tracking / distance-from-target
// tolerance and the screen-offset scale (f31 = flt_82004014 == 0x3DCCCCCD), the FOV velocity band
// (flt_820054CC == 20.0, flt_8200544C == 130.0), the distance-from-ideal tolerance (flt_8200426C == 5.0) and the
// subject size (flt_82001DA0 == 0.5).
static const f32 KF_BYSTANDER_LOOKER_TOLERANCE           = 0.1f;
static const f32 KF_BYSTANDER_LOOKER_MIN_FOV_VELOCITY    = 20.0f;
static const f32 KF_BYSTANDER_LOOKER_MAX_FOV_VELOCITY    = 130.0f;
static const f32 KF_BYSTANDER_LOOKER_DISTANCE_FROM_IDEAL = 5.0f;
static const f32 KF_BYSTANDER_LOOKER_SUBJECT_SIZE        = 0.5f;

// [DIAG] BRN_CRASHCAM_DIAG -- NOT IN THE X360 BINARY. The BYSTANDER look space's live witness (CC-13): how many degrees
// the camera's forward (its z row) is off the bystander's position before and after the looker, and the FOV the looker
// left, with the take's guid and name. One line on each of a take's first four frames, then every 30th. Reads only.
struct BystanderLookDiag
{
    bool        mbOn;
    const void* mpBehaviour;
    s32         miGuid;
    u32         muFrame;
    f32         mfAimBefore;
    f32         mfFovBefore;
};

static BystanderLookDiag& BrnDiag_BystanderLookState()
{
    static BystanderLookDiag sDiag = { getenv("BRN_CRASHCAM_DIAG") != 0, 0, -1, 0u, 0.0f, 0.0f };
    return sDiag;
}

static f32 BrnDiag_BystanderDistance(const Camera& lrCamera, const rw::math::vpu::Matrix44Affine& lrTarget)
{
    const f32 lfX = lrTarget.wAxis.x - lrCamera.mTransform.wAxis.x;
    const f32 lfY = lrTarget.wAxis.y - lrCamera.mTransform.wAxis.y;
    const f32 lfZ = lrTarget.wAxis.z - lrCamera.mTransform.wAxis.z;
    return sqrtf(lfX * lfX + lfY * lfY + lfZ * lfZ);
}

static f32 BrnDiag_BystanderAimDegrees(const Camera& lrCamera, const rw::math::vpu::Matrix44Affine& lrTarget)
{
    const rw::math::vpu::Vector3& lrForward = lrCamera.mTransform.zAxis;
    const f32 lfForward = sqrtf(lrForward.x * lrForward.x + lrForward.y * lrForward.y + lrForward.z * lrForward.z);
    const f32 lfDistance = BrnDiag_BystanderDistance(lrCamera, lrTarget);
    if (!(lfForward > 0.0f) || !(lfDistance > 0.0f))
        return -1.0f;
    f32 lfCos = ((lrTarget.wAxis.x - lrCamera.mTransform.wAxis.x) * lrForward.x
                 + (lrTarget.wAxis.y - lrCamera.mTransform.wAxis.y) * lrForward.y
                 + (lrTarget.wAxis.z - lrCamera.mTransform.wAxis.z) * lrForward.z) / (lfForward * lfDistance);
    lfCos = (lfCos > 1.0f) ? 1.0f : ((lfCos < -1.0f) ? -1.0f : lfCos);
    return acosf(lfCos) * 57.2957795f;
}

static void BrnDiag_BystanderLookBefore(const Camera& lrCamera, const rw::math::vpu::Matrix44Affine& lrTarget)
{
    BystanderLookDiag& lrDiag = BrnDiag_BystanderLookState();
    if (!lrDiag.mbOn)
        return;
    lrDiag.mfAimBefore = BrnDiag_BystanderAimDegrees(lrCamera, lrTarget);
    lrDiag.mfFovBefore = lrCamera.GetFOV();
}

static void BrnDiag_BystanderLookAfter(const void* lpBehaviour, const KeyAnimController& lrController,
                                       const Camera& lrCamera, const rw::math::vpu::Matrix44Affine& lrTarget)
{
    BystanderLookDiag& lrDiag = BrnDiag_BystanderLookState();
    if (!lrDiag.mbOn)
        return;
    const ICE::ICETakeData* lpTake = lrController.GetTake().GetData();
    const s32 liGuid = (lpTake != 0) ? lpTake->miGuid : -1;
    if (lpBehaviour != lrDiag.mpBehaviour || liGuid != lrDiag.miGuid)
    {
        lrDiag.mpBehaviour = lpBehaviour;
        lrDiag.miGuid      = liGuid;
        lrDiag.muFrame     = 0u;
    }
    const u32 luFrame = lrDiag.muFrame++;
    if (luFrame >= 4u && (luFrame % 30u) != 0u)
        return;
    char lacLine[320];
    snprintf(lacLine, sizeof(lacLine),
             "[iceanim] bystander look take %d '%s' frame %u: aim %.2f -> %.2f deg, fov %.4f -> %.4f, "
             "bystander (%.1f, %.1f, %.1f) at %.1f m\n",
             liGuid, (lpTake != 0) ? lpTake->macTakeName : "?", luFrame, lrDiag.mfAimBefore,
             BrnDiag_BystanderAimDegrees(lrCamera, lrTarget), lrDiag.mfFovBefore, lrCamera.GetFOV(),
             lrTarget.wAxis.x, lrTarget.wAxis.y, lrTarget.wAxis.z, BrnDiag_BystanderDistance(lrCamera, lrTarget));
    CgsDev::Log::WriteToLog(lacLine);
}

// [DIAG] BRN_CRASHCAM_DIAG -- NOT IN THE X360 BINARY. The heading space's live witness (FX-LASTFIX): on a take's frames
// 0..5, how many degrees mHeadingSpaceTransform's forward (its z row) is off the look-at's before and after the SLerp,
// and the SLerp's own remaining angle (its angle out, angle - angle * 0.2). The console eases 20% a frame: on a frame
// whose look-at moved, after == 0.8 x before (the arc arm; under 2 degrees the lerp arm is within a hair of it). The
// old link stub made every after 0. 96 lines a run at most. Reads only.
struct HeadingEaseDiag
{
    bool        mbOn;
    const void* mpBehaviour;
    s32         miGuid;
    u32         muFrame;
    u32         muLines;
    f32         mfBefore;
    f32         mfAfter;
    f32         mfRemaining;
};

static HeadingEaseDiag& BrnDiag_HeadingEaseState()
{
    static HeadingEaseDiag sDiag = { getenv("BRN_CRASHCAM_DIAG") != 0, 0, -1, 0u, 0u, 0.0f, 0.0f, 0.0f };
    return sDiag;
}

static f32 BrnDiag_ForwardDegrees(const rw::math::vpu::Matrix44Affine& lrA, const rw::math::vpu::Matrix44Affine& lrB)
{
    const rw::math::vpu::Vector3& lrZA = lrA.zAxis;
    const rw::math::vpu::Vector3& lrZB = lrB.zAxis;
    const f32 lfCrossX = lrZA.y * lrZB.z - lrZA.z * lrZB.y;
    const f32 lfCrossY = lrZA.z * lrZB.x - lrZA.x * lrZB.z;
    const f32 lfCrossZ = lrZA.x * lrZB.y - lrZA.y * lrZB.x;
    const f32 lfSin = sqrtf(lfCrossX * lfCrossX + lfCrossY * lfCrossY + lfCrossZ * lfCrossZ);
    const f32 lfCos = lrZA.x * lrZB.x + lrZA.y * lrZB.y + lrZA.z * lrZB.z;
    return atan2f(lfSin, lfCos) * 57.2957795f;
}

static void BrnDiag_HeadingEaseBefore(const rw::math::vpu::Matrix44Affine& lrHeading,
                                      const rw::math::vpu::Matrix44Affine& lrLookAt)
{
    HeadingEaseDiag& lrDiag = BrnDiag_HeadingEaseState();
    if (lrDiag.mbOn)
        lrDiag.mfBefore = BrnDiag_ForwardDegrees(lrHeading, lrLookAt);
}

static void BrnDiag_HeadingEaseAfter(const rw::math::vpu::Matrix44Affine& lrHeading,
                                     const rw::math::vpu::Matrix44Affine& lrLookAt,
                                     const rw::math::vpu::Vector3& lrAngleOut)
{
    HeadingEaseDiag& lrDiag = BrnDiag_HeadingEaseState();
    if (!lrDiag.mbOn)
        return;
    lrDiag.mfAfter     = BrnDiag_ForwardDegrees(lrHeading, lrLookAt);
    lrDiag.mfRemaining = lrAngleOut.x * 57.2957795f;
}

static void BrnDiag_HeadingEaseReport(const void* lpBehaviour, const KeyAnimController& lrController,
                                      ICE::eICESpace leEyeSpace, ICE::eICESpace leLookSpace)
{
    HeadingEaseDiag& lrDiag = BrnDiag_HeadingEaseState();
    if (!lrDiag.mbOn)
        return;
    const ICE::ICETakeData* lpTake = lrController.GetTake().GetData();
    const s32 liGuid = (lpTake != 0) ? lpTake->miGuid : -1;
    if (lpBehaviour != lrDiag.mpBehaviour || liGuid != lrDiag.miGuid)
    {
        lrDiag.mpBehaviour = lpBehaviour;
        lrDiag.miGuid      = liGuid;
        lrDiag.muFrame     = 0u;
    }
    const u32 luFrame = lrDiag.muFrame++;
    if (luFrame >= 6u || lrDiag.muLines >= 96u)
        return;
    ++lrDiag.muLines;
    char lacLine[256];
    snprintf(lacLine, sizeof(lacLine),
             "[iceanim] heading2 ease take %d '%s' frame %u: %.3f -> %.3f deg (slerp remaining %.3f deg), "
             "eye space %d look space %d\n",
             liGuid, (lpTake != 0) ? lpTake->macTakeName : "?", luFrame, lrDiag.mfBefore, lrDiag.mfAfter,
             lrDiag.mfRemaining, static_cast<s32>(leEyeSpace), static_cast<s32>(leLookSpace));
    CgsDev::Log::WriteToLog(lacLine);
}

// [DIAG] BRN_CRASHCAM_DIAG -- NOT IN THE X360 BINARY. The take's reference spaces (FX-LASTFIX item 1b): on a take's frames
// 0..5, where the handler the take evaluator reads puts CAR and CAR2 -- metres off the behaviour's primary / secondary
// vehicle, for the take's copy and for the MainDirector's shared handler (the player / the race car nearest the player)
// -- and the yaw of HEADING2 in the copy (the eased heading space), in the shared handler (the nearest car's raw
// transform) and of the secondary vehicle itself. 96 lines a run at most. Reads only.
struct SpacesDiag
{
    bool        mbOn;
    const void* mpBehaviour;
    s32         miGuid;
    u32         muFrame;
    u32         muLines;
};

static f32 BrnDiag_Metres(const rw::math::vpu::Vector3& lrA, const rw::math::vpu::Vector3& lrB)
{
    const f32 lfX = lrA.x - lrB.x;
    const f32 lfY = lrA.y - lrB.y;
    const f32 lfZ = lrA.z - lrB.z;
    return sqrtf(lfX * lfX + lfY * lfY + lfZ * lfZ);
}

static f32 BrnDiag_YawDegrees(const rw::math::vpu::Matrix44Affine& lrM)
{
    return atan2f(lrM.zAxis.x, lrM.zAxis.z) * 57.2957795f;
}

static void BrnDiag_SpacesReport(const void* lpBehaviour, const KeyAnimController& lrController,
                                 ICE::eICESpace leEyeSpace, ICE::eICESpace leLookSpace,
                                 const ICE::CameraSpaceHandler& lrShared, const ICE::CameraSpaceHandler& lrTake,
                                 const VehicleInfo& lrPrimary, const VehicleInfo& lrSecondary, bool lbForcedLoose)
{
    static SpacesDiag sDiag = { getenv("BRN_CRASHCAM_DIAG") != 0, 0, -1, 0u, 0u };
    if (!sDiag.mbOn)
        return;
    const ICE::ICETakeData* lpTake = lrController.GetTake().GetData();
    const s32 liGuid = (lpTake != 0) ? lpTake->miGuid : -1;
    if (lpBehaviour != sDiag.mpBehaviour || liGuid != sDiag.miGuid)
    {
        sDiag.mpBehaviour = lpBehaviour;
        sDiag.miGuid      = liGuid;
        sDiag.muFrame     = 0u;
    }
    const u32 luFrame = sDiag.muFrame++;
    if (luFrame >= 6u || sDiag.muLines >= 96u)
        return;
    ++sDiag.muLines;
    const rw::math::vpu::Vector3& lrPrimaryAt   = lrPrimary.mRaceCarState.mTransform.wAxis;
    const rw::math::vpu::Vector3& lrSecondaryAt = lrSecondary.mRaceCarState.mTransform.wAxis;
    char lacLine[400];
    snprintf(lacLine, sizeof(lacLine),
             "[iceanim] spaces take %d '%s' frame %u: eye %d look %d | CAR primary id %u: take %.2f m, shared %.2f m"
             " | CAR2 secondary id %u: take %.2f m, shared %.2f m | HEADING2 yaw take %.2f, shared %.2f, secondary"
             " %.2f deg | forced loose %d\n",
             liGuid, (lpTake != 0) ? lpTake->macTakeName : "?", luFrame, static_cast<s32>(leEyeSpace),
             static_cast<s32>(leLookSpace), lrPrimary.mRaceCarState.mEntityId.muValue,
             BrnDiag_Metres(lrTake.GetTransformToWorld(ICE::eICE_CAR_SPACE).wAxis, lrPrimaryAt),
             BrnDiag_Metres(lrShared.GetTransformToWorld(ICE::eICE_CAR_SPACE).wAxis, lrPrimaryAt),
             lrSecondary.mRaceCarState.mEntityId.muValue,
             BrnDiag_Metres(lrTake.GetTransformToWorld(ICE::eICE_CAR2_SPACE).wAxis, lrSecondaryAt),
             BrnDiag_Metres(lrShared.GetTransformToWorld(ICE::eICE_CAR2_SPACE).wAxis, lrSecondaryAt),
             BrnDiag_YawDegrees(lrTake.GetTransformToWorld(ICE::eICE_HEADING2_SPACE)),
             BrnDiag_YawDegrees(lrShared.GetTransformToWorld(ICE::eICE_HEADING2_SPACE)),
             BrnDiag_YawDegrees(lrSecondary.mRaceCarState.mTransform), lbForcedLoose ? 1 : 0);
    CgsDev::Log::WriteToLog(lacLine);
}

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

    // --- the free visibility policy: its WHOLE Construct, inlined by the console ---
    // ⭐ CORRECTED 2026-09-25 (FX-DIRECTOR2). The three "see-through" stores that stood here
    // (policy +0x1A0/+0x1A1/+0x1A2) are only the visibility-test tail of an inlined
    // VisibilityCollisionPolicy::Construct: 0x822561F0..0x8225625C writes the base latch (+0x04),
    // the geometry predictor (+0x80/+0x90/+0xE4), the vehicle predictor (+0x70), the visibility
    // test (+0xF0..+0x1A2), the volume box (+0x1B0), the ground constraint (+0x1C0/+0x210), the two
    // timeouts (+0x234 = 1.5 / +0x238 = 0.5), mbCanFail = 1 (+0x08), the velocity (+0x220),
    // mbFirstFrame = 1 (+0x09), mbTargetSet = 0 (+0x0A) and mbUseGroundConstraint = 0 (+0x23C) --
    // store for store the same set BehaviourGyroCam::Construct inlines. Without it mbCanFail /
    // mbFirstFrame / the timeouts held whatever the behaviour pool slot held.
    mCollisionPolicy.Construct();

    // ------------------------------------------------------------------------
    // ⭐⭐ THE THREE ANCHOR VEHICLE REFERENCES. RESTORED 2026-08-01 -- THEY WERE MISSING,
    // and their absence is why every ICE-anim camera in the game produced nothing.
    //
    // Update opens with `IsValid(mPrimaryVehicleRef)` / `IsValid(mSecondary-
    // VehicleRef)` and takes Behaviour::Fail on either failure; VehicleRef::IsValid tests
    // mbSet FIRST. With these seeds absent the refs were whatever the behaviour pool slot
    // happened to hold (zero), so Update failed out on its first line every frame and
    // mLastCamera stayed exactly as BehaviourHelper::Prepare's Camera::Construct left it --
    // identity basis at the world origin. That IS the "eye (0,0,0) at (0,0,1)" symptom the
    // director trace showed to the last frame.
    //
    // The console's stores inside Construct are the inlined VehicleRef::Construct +
    // VehicleRef::Set pair for each ref, in this order (byte then three words):
    //     mPrimaryVehicleRef    0 then 1 -> +0xDFC ; 0 -> +0xDF0 ; 0 -> +0xDF8 ; -1 -> +0xDF4
    //     mSecondaryVehicleRef  0 then 1 -> +0xE0C ; 2 -> +0xE00 ; -1 -> +0xE04 ; 1 -> +0xE08
    //     mBystanderRef         0 then 1 -> +0xE1C ; 0 -> +0xE10 ; 0 -> +0xE18 ; -1 -> +0xE14
    // i.e. the EYE anchor and the BYSTANDER anchor are the PLAYER's car, and the LOOK-AT
    // anchor is the nearest race car at RANK 1 (E_RACE_CAR_NEAREST_PLAYER, muRef == 1).
    // Written through the named Construct/Set pair so nothing is poked by offset; the
    // leading Construct is the console's own clear-then-set of mbSet, not decoration.
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

    // --- the embedded controller's playback reset ---
    // The console re-bases onto the controller (+0x680) and stores 0.0f -> +0x760
    // (mfPlaybackTimer) + 0 -> the four playback flags. The previous slice mis-homed this as
    // a fabricated behaviour-level "reset block" at +0xDE0; see ResetPlayback's banner.
    mKeyAnimController.ResetPlayback();

    // --- the produced camera + the attached-to-car collision policy ---
    mLastCamera.Construct();
    mAttachedToCarCollisionPolicy.Construct(0);
}

// ============================================================================
// SetParameters
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
//     construct a STACK Attrib::Gen::iceanim over the RefSpec, owner 0   ; resolve the ref
//     read the temporary's +0x4 (mpAttributeData), then that block's +0xC ; the take guid
//     store mpSourceShot = lpParameters                                  ; behaviour +0xE24
//     store miAnimGuid   = that guid                                     ; behaviour +0xE20
//     destroy the temporary iceanim                                      ; drops the handle's ref
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
    // read from the RefSpec's +0x0) against the generated iceanim class key.
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

    // ⭐ Reset the embedded controller's playback state for the NEW take (the console
    // re-bases onto the controller at +0x680, then stores 0.0f -> controller +0x760
    // mfPlaybackTimer and 0 -> the four playback flags). It does NOT touch the behaviour-mode
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
    // The console calls Tweaker::Construct with the tweaker as its only argument; the
    // canonical home (Utils/BrnCameraTweaker.h) declares it as a MEMBER, so that
    // one-argument call IS this member call on lrTweaker.
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

    if (!mPrimaryVehicleRef.IsValid(*lpWorld) || !mSecondaryVehicleRef.IsValid(*lpWorld))
    {
        // Neither anchor resolves -> give up following. The block here (account
        // SetFlag(11) on camera +0x138, clearing bit 1 of camera +0x140, then the three base
        // flag stores) is the INLINED Behaviour::Fail -- expressed as the named base call now
        // that the base has a home. Reason 11 is the fork's own bit 11 of +0x138 written as
        // the flag index.
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

    // ⭐ CORRECTED 2026-09-25 (FX-DIRECTOR2) -- THE POLICY IS RE-TARGETED AT THE PLAYER EVERY FRAME.
    // The byte at behaviour +0x2A (the free visibility policy's +0x0A) is mbTargetSet, and it is
    // only the first store of an inlined VisibilityCollisionPolicy::SetTarget (0x82247204..
    // 0x82247258): the player's transform (shared info +0x250) into policy +0x10, the player's
    // bounds (+0x500) into policy +0x50, and the player's entity id (+0x428) into policy +0x230.
    // The old spelling, SetSeeThroughEnabled(true), wrote the visibility test's mbTestLookingAt
    // instead and left the policy without a target -- its GenerateSceneQueries / ProcessScene-
    // QueryResults open with the "mbTargetSet" tripwire (BrnCollisionPolicy.cpp:807 / :867).
    mCollisionPolicy.SetTarget(lrSharedInfo.mPlayerInfo.mRaceCarState.mTransform,
                               lrSharedInfo.mPlayerInfo.mAABB,
                               CgsSceneManager::EntityId(lrSharedInfo.mPlayerInfo.mRaceCarState.mEntityId.muValue));

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

    // ⭐ (2026-09-26, crash parity FX-LASTFIX) SLerp @0x82216858, `bl` at 0x82247354: r3 the sret (var_360), r4 = r29 =
    // &mHeadingSpaceTransform (this+0x610) -- FROM, r5 = CreateLookAt's sret -- TO, v1 = v127 = the 0.2 splat, r6 = r22 =
    // var_390 -- an angle-out slot Update never reads (the looker block re-uses var_390 as scratch). The four result rows
    // go back into mHeadingSpaceTransform (0x82247360..0x8224737C). It used to call a pointer-amount overload whose
    // mounted link stub returned `to`, with an amount of 1.0f: the space snapped to the look-at every frame.
    rw::math::vpu::Vector3 lUnusedAngle;
    BrnDiag_HeadingEaseBefore(mHeadingSpaceTransform, lLookAt);                            // [DIAG] NOT X360
    mHeadingSpaceTransform =
        rw::math::vpu::SLerp(mHeadingSpaceTransform, lLookAt, KF_HEADING_SPACE_2_SLERP_AMOUNT, &lUnusedAngle);
    BrnDiag_HeadingEaseAfter(mHeadingSpaceTransform, lLookAt, lUnusedAngle);               // [DIAG] NOT X360

    // The take evaluator resolves its reference spaces through its own copy of the shared
    // per-frame handler (through its copy constructor, 0x82247384 into var_2B0).
    //
    // ⭐ (2026-09-26, crash parity FX-LASTFIX item 1b) THE TAKE'S OWN SPACES. The console overrides four of the COPY's
    // spaces before the take evaluator reads it -- the MainDirector's shared handler is untouched, and nothing reads
    // the copy before the controller's Update (0x822474A8):
    //   0x82247388..0x822473CC  mCarToWorld (+0x00)      <- mPrimaryVehicleRef.Get (r20, this+0xDF0) +0x1F0, the
    //                                                       vehicle's mRaceCarState.mTransform
    //   0x822473D0..0x82247418  mCar2ToWorld (+0x40)     <- mSecondaryVehicleRef.Get (r23, this+0xE00) +0x1F0
    //   0x822473DC..0x8224744C  mHeading2ToWorld (+0x1C0) <- mHeadingSpaceTransform (r29, this+0x610), the space the
    //                                                       SLerp above just eased
    //   0x822473D4 lbz +0xE2A (mbForceHeadingSpaceToBeLooseHeadingSpace), 0x822473E4 cmplwi 0, 0x82247450 beq:
    //     clear -> T = 0x8224748C, no write;
    //     set   -> F = 0x82247454..0x82247488, mHeadingToWorld (+0x140) <- world +0x80, the player's loose heading
    //              space (AllVehicleData::mPlayerLooseHeadingSpace).
    // Neither Get result is tested: VehicleRef::Get @0x822335A0 returns a record on every arm, and the only guard is
    // the IsValid pair at the top of Update (0x82247148 / 0x82247164 -> the Fail exit 0x822479BC). The PC used to
    // discard both Get results and write nothing, so every take read the shared handler's spaces: CAR = the player,
    // CAR2 = the race car nearest the player, HEADING2 = that car's raw transform (MainDirector's Construct), and the
    // TAKEDOWN / REVERSE_TAKEDOWN / BYSTANDER spaces derived from them.
    ICE::CameraSpaceHandler lSpaces(*lrSharedInfo.GetCameraSpaceHandler());
    const VehicleInfo* lpPrimaryVehicle = mPrimaryVehicleRef.Get(lpWorld);
    lSpaces.SetCarToWorld(lpPrimaryVehicle->mRaceCarState.mTransform);
    const VehicleInfo* lpSecondaryVehicle = mSecondaryVehicleRef.Get(lpWorld);
    lSpaces.SetCar2ToWorld(lpSecondaryVehicle->mRaceCarState.mTransform);
    lSpaces.SetHeading2ToWorld(mHeadingSpaceTransform);
    if (mbForceHeadingSpaceToBeLooseHeadingSpace)
        lSpaces.SetHeadingToWorld(lpWorld->GetPlayerLooseHeadingSpace());

    // Run the take evaluator: it advances the ICE take and writes the whole camera out of it
    // (transform, FOV, depth of field, the effects block). Dispatched through the
    // ShotController vtable.
    ShotContext lShotContext;
    lShotContext.mpAllVehicleData     = lpWorld;
    lShotContext.mpCameraSpaceHandler = &lSpaces;
    lShotContext.mpTimestep           = &lrSharedInfo.GetTimestep();
    mKeyAnimController.Update(lShotContext, &lrCamera);

    // Seed the behaviour's stored camera from the shared camera the first time only. This is
    // Camera::operator= -- a WHOLE-camera memberwise copy.
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
    BrnDiag_HeadingEaseReport(this, mKeyAnimController, leEyeSpace, leLookSpace);         // [DIAG] NOT X360
    BrnDiag_SpacesReport(this, mKeyAnimController, leEyeSpace, leLookSpace,                // [DIAG] NOT X360
                         *lrSharedInfo.GetCameraSpaceHandler(), lSpaces, *lpPrimaryVehicle, *lpSecondaryVehicle,
                         mbForceHeadingSpaceToBeLooseHeadingSpace);

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

        // The default focus band (0.1 / 0.2 / 0.3 / 0.4 metres -- all four are constants in
        // the original build), keeping the blurriness the take already produced.
        lrCamera.GetDepthOfField().SetParams(0.1f, 0.2f, 0.3f, 0.4f,
                                             mLastCamera.GetDepthOfField().GetBlurriness());
        lrCamera.SetFOV(mLastCamera.GetFOV());

        // ⭐ (2026-09-25, FX-DIRECTOR2 CHAINCHECK2 CC-13) THE LOOKER TRACKS THE BYSTANDER (0x82247710..0x82247888).
        // It was a FLAG and did not run: the camera kept the take's orientation and never aimed, framed or zoomed
        // onto the bystander vehicle. 36 retail takes look in space 11: Takedown_ICE_1 (the shutdown takedown,
        // whose Prepare binds mBystanderRef to the victim), the 21 World_Win_* and 13 World_Signature_*.
        //   Looker::Parameters::Construct over a stack block, then eleven overrides (block +offset):
        //     +0x20 mfTrackingTolerance 0.1 and +0x40 mfToleranceForDistanceFromTarget 0.1 (f31, flt_82004014);
        //     +0x5F mbUseZoom 1 (r21); +0x2C / +0x30 the FOV velocity band 20 / 130 (flt_820054CC / flt_8200544C);
        //     +0x60 meZoomType 2 E_ZOOM_SCREEN_REGION; +0x3C mfToleranceForDistanceFromIdeal 5 (flt_8200426C);
        //     +0x10 / +0x14 the subject X / Y size 0.5 (flt_82001DA0);
        //     +0x18 / +0x1C the subject X / Y screen offset = GetLookPos().x / .y * 0.1 (one vmulfp128 each,
        //     0x8224779C / 0x822477E4: one f32 rounding, ROUNDING_RULE rule 4).
        //   The timestep is the behaviour's own (Timestep::Get(shared +0x550, meTimestepType at +0x04)), splatted.
        //   The bystander is resolved three times (VehicleRef::Get 0x82247808 / 0x82247818 / 0x82247828): its AABB
        //   (+0x4A0, the 0x20-byte memcpy), its linear velocity (+0x330) and its transform (+0x1F0).
        //   Looker::Update(this + 0x660, the timestep, the shared Random BY VALUE (six qwords from shared +0x5D4),
        //   the block, the camera, the transform, the velocity, the AABB).
        Utils::Looker::Parameters lLookerParams;
        lLookerParams.Construct();
        lLookerParams.mfTrackingTolerance              = KF_BYSTANDER_LOOKER_TOLERANCE;
        lLookerParams.mfToleranceForDistanceFromTarget = KF_BYSTANDER_LOOKER_TOLERANCE;
        lLookerParams.mbUseZoom                        = true;
        lLookerParams.mfMinFOVVelocity                 = KF_BYSTANDER_LOOKER_MIN_FOV_VELOCITY;
        lLookerParams.mfMaxFOVVelocity                 = KF_BYSTANDER_LOOKER_MAX_FOV_VELOCITY;
        lLookerParams.meZoomType                       = Utils::Looker::Parameters::E_ZOOM_SCREEN_REGION;
        lLookerParams.mfToleranceForDistanceFromIdeal  = KF_BYSTANDER_LOOKER_DISTANCE_FROM_IDEAL;
        lLookerParams.mfTargetSubjectXSize             = KF_BYSTANDER_LOOKER_SUBJECT_SIZE;
        lLookerParams.mfTargetSubjectYSize             = KF_BYSTANDER_LOOKER_SUBJECT_SIZE;
        lLookerParams.mfTargetSubjectXScreenOffset     = mKeyAnimController.GetLookPos().x * KF_BYSTANDER_LOOKER_TOLERANCE;
        lLookerParams.mfTargetSubjectYScreenOffset     = mKeyAnimController.GetLookPos().y * KF_BYSTANDER_LOOKER_TOLERANCE;

        const f32 lfLookerTimeStep = lrSharedInfo.GetTimestep().Get(GetTimestepType());
        const VehicleInfo& lrBystanderBox      = mBystanderRef.GetVehicle(lrSharedInfo);
        const VehicleInfo& lrBystanderVelocity = mBystanderRef.GetVehicle(lrSharedInfo);
        const VehicleInfo& lrBystanderPlace    = mBystanderRef.GetVehicle(lrSharedInfo);
        BrnDiag_BystanderLookBefore(lrCamera, lrBystanderPlace.mRaceCarState.mTransform);   // [DIAG] NOT X360
        mLooker.Update(VecFloat(lfLookerTimeStep), *lrSharedInfo.GetRandom(), lLookerParams, lrCamera,
                       lrBystanderPlace.mRaceCarState.mTransform, lrBystanderVelocity.mRaceCarState.mLinearVelocity,
                       lrBystanderBox.mAABB);
        BrnDiag_BystanderLookAfter(this, mKeyAnimController, lrCamera,                      // [DIAG] NOT X360
                                   lrBystanderPlace.mRaceCarState.mTransform);
        (void)lpWorld;
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

        // [CORRECTED 2026-09-25, FX-DIRECTOR2] The behaviour's own timestep (`lwz r4, 4(r31)`, meTimestepType
        // at 0x822478AC), as for the looker above -- not a fixed E_WORLD.
        const f32 lfShakeTimeStep = lrSharedInfo.GetTimestep().Get(GetTimestepType());

        mShake.Update(lrCamera.mTransform, lShakeParams, *lrSharedInfo.GetRandom(),
                      lfShakeTimeStep, 1.0f);
    }

    // --- "Can't cut TO me" gate ---
    if (mbUseCollisionPolicy)
    {
        // Inner guard: only raise it when the free visibility policy's visibility test says the
        // target is not visible: mbOccluded || (mbTestLookingAt && !mbIsOnScreen) -- the DWARF's
        // VisibilityCollisionPolicy::IsVisibilityInterrupted (BrnCollisionPolicy.h:404).
        if (mCollisionPolicy.IsVisibilityInterrupted())
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

// ----------------------------------------------------------------------------
// SetSecondaryVehicleRefToRaceCar
// ----------------------------------------------------------------------------
// ⭐ ADDED 2026-08-29 (crash-camera wave). The de-inlined form of the console's
//     VehicleRef::SetToRaceCar(behaviour + 0xE00, raceCarIndex)
// -- one real out-of-line call, emitted by ArbStateCrashing::Update when the player
// is taken down mid-crash, so the takedown ICE camera anchors on the killer's car. The member is
// private, so the setter lives here rather than letting the arbitrator state form the offset.
// ----------------------------------------------------------------------------
void BehaviourIceAnim::SetSecondaryVehicleRefToRaceCar(EActiveRaceCarIndex leRaceCar)
{
    mSecondaryVehicleRef.SetToRaceCar(leRaceCar);
}

// ----------------------------------------------------------------------------
// SetPrimaryVehicleRefToRaceCar
// ----------------------------------------------------------------------------
// The PRIMARY-ref sibling of the setter above, for mPrimaryVehicleRef (+0xDF0). The de-inlined
// form of the shipped build's VehicleRef::SetToRaceCar(behaviour + 0xDF0, raceCarIndex) -- one
// real out-of-line call, emitted twice by the rank-up arbitrator state (ArbStateRankUp::Prepare
// seeds the first rival's take, ArbStateRankUp::Update re-anchors every time the game moves to
// the next rival), so the rank-up ICE camera frames that rival's car. Same distinction as the
// secondary setter: this is the real out-of-line VehicleRef method (bodied in
// Utils/BrnVehicleRef.cpp), NOT the inlined four-word {kind=1, index, 0, valid=1} seed that
// SetPrimaryVehicleRefToRaceCarIndex models. The member is private, so the setter lives here
// rather than letting the arbitrator state form the offset.
// ----------------------------------------------------------------------------
void BehaviourIceAnim::SetPrimaryVehicleRefToRaceCar(EActiveRaceCarIndex leRaceCar)
{
    mPrimaryVehicleRef.SetToRaceCar(leRaceCar);
}

// ----------------------------------------------------------------------------
// SetBystanderRefForPostEvent
// ----------------------------------------------------------------------------
// The BYSTANDER-ref seed the post-event take runs with, for mBystanderRef (+0xE10). Unlike the
// two SetToRaceCar setters above this one has no out-of-line callee: the shipped build INLINES
// the four field writes straight into ArbStatePostEvent::Prepare, in the order
//     word +0x00 = 0 ; word +0x08 = 0 ; byte +0x0C = 1 ; word +0x04 = -1
// (the store order is the scheduler's; the values are what matter). Written here through the
// ref's named fields: the reference kind is the PLAYER CAR, it binds no race-car index, it
// names no nearest-player rank, and it is marked populated -- i.e. the post-event take frames
// the player, which is what a "who is watching the winner" bystander anchor means here.
// FLAG: the four fields' VALUES are attested; their individual ROLES in the bystander case are
// the base VehicleRef's, taken over unchanged.
// The member is private, so the setter lives here rather than letting the arbitrator state
// form the offset.
// ----------------------------------------------------------------------------
void BehaviourIceAnim::SetBystanderRefForPostEvent()
{
    mBystanderRef.meType         = BrnDirector::VehicleRef::E_PLAYER_CAR;   // +0x00 = 0
    mBystanderRef.miRaceCarIndex = -1;                                      // +0x04 = -1
    mBystanderRef.muRef          = 0;                                       // +0x08 = 0
    mBystanderRef.mbSet          = true;                                    // +0x0C = 1
}

// ----------------------------------------------------------------------------
// SetPrimaryVehicleRefToRaceCarIndex / SetSecondaryVehicleRefToRaceCarIndex
// ----------------------------------------------------------------------------
// BODIED 2026-09-12 (were declaration-only). The online-race-intro "show" takes anchor both the
// primary (eye) and the secondary (look) reference of a rival take onto that rival's race car.
// The shipped build inlines the four-field race-car seed into
// ArbStateOnlineRaceIntro::SetupRivalMovie, twice, once per ref:
//     ref kind word = 1 (a race-car reference) ; index word = liRaceCarIndex ;
//     nearest-player ref word = 0 ; set-flag byte = 1
// which is exactly VehicleRef::SetToRaceCar -- so each setter is the one named call, with the
// race-car-index tripwire the console fires right after the stores (BrnVehicleRef.h:222,
// non-gating like every folded assert here). The refs are private, so the setters live in this
// behaviour's own TU rather than letting the arbitrator state form the offsets.
// ----------------------------------------------------------------------------
void BehaviourIceAnim::SetPrimaryVehicleRefToRaceCarIndex(s32 liRaceCarIndex)
{
    mPrimaryVehicleRef.SetToRaceCar(static_cast<EActiveRaceCarIndex>(liRaceCarIndex));
    CGS_ASSERT(liRaceCarIndex < 8, "meRaceCarIndex < BrnPhysics::Vehicle::ku8MaxNumRaceCars");
}

void BehaviourIceAnim::SetSecondaryVehicleRefToRaceCarIndex(s32 liRaceCarIndex)
{
    mSecondaryVehicleRef.SetToRaceCar(static_cast<EActiveRaceCarIndex>(liRaceCarIndex));
    CGS_ASSERT(liRaceCarIndex < 8, "meRaceCarIndex < BrnPhysics::Vehicle::ku8MaxNumRaceCars");
}

// ----------------------------------------------------------------------------
// SetBystanderRefToRaceCarIndex
// ----------------------------------------------------------------------------
// BODIED (was declaration-only tree-wide, and its only call site was parked because of that).
// The simple-ICE takedown take anchors BOTH its secondary (look) reference and its BYSTANDER
// reference onto the SAME race car -- the car the player just took down. The shipped build
// inlines the four-field race-car seed into SimpleIceTakedownPlayer::Prepare twice in a row,
// once per ref, reading the index out of the shared context both times:
//     mSecondaryVehicleRef (+0xE00) : kind word = 1 ; index word = the race car ;
//                                     nearest-player ref word = 0 ; set-flag byte = 1
//     mBystanderRef        (+0xE10) : the identical four stores, 0x10 further on
// with the race-car-index tripwire fired after each seed (non-gating, like every folded assert
// here). Those four fields ARE VehicleRef::SetToRaceCar, so this setter is the one named call
// plus the trailing tripwire -- byte-for-byte the sibling above, on the bystander ref instead
// of the secondary one. The ref is private, so the setter lives in this behaviour's own TU
// rather than letting the arbitrator state form the offset.
// ----------------------------------------------------------------------------
void BehaviourIceAnim::SetBystanderRefToRaceCarIndex(s32 liRaceCarIndex)
{
    mBystanderRef.SetToRaceCar(static_cast<EActiveRaceCarIndex>(liRaceCarIndex));
    CGS_ASSERT(liRaceCarIndex < 8, "meRaceCarIndex < BrnPhysics::Vehicle::ku8MaxNumRaceCars");
}

// ----------------------------------------------------------------------------
// SetPrimaryVehicleRefToPlayer / SetSecondaryVehicleRefToPlayer
// ----------------------------------------------------------------------------
// BODIED 2026-09-12 (were declaration-only). The online-race-intro PLAYER "show" take anchors
// both refs onto the player instead of a numbered race car. Like the bystander seed above, the
// shipped build inlines the four field writes straight into ArbStateOnlineRaceIntro::Update
// (both the SHOWING_PLAYER setup and its SHOWING_PLAYER_AGAIN twin), in the order
//     set-flag byte = 1 ; kind word = 0 ; nearest-player ref word = 0 ; index word = -1
// -- the reference kind is the PLAYER CAR, it binds no race-car index, it names no
// nearest-player rank, and it is marked populated. There is no race-car tripwire on this path
// (no index is bound). Written through the ref's named fields; the refs are private, so the
// setters live in this behaviour's own TU.
// ----------------------------------------------------------------------------
void BehaviourIceAnim::SetPrimaryVehicleRefToPlayer()
{
    mPrimaryVehicleRef.meType         = BrnDirector::VehicleRef::E_PLAYER_CAR;
    mPrimaryVehicleRef.miRaceCarIndex = -1;
    mPrimaryVehicleRef.muRef          = 0;
    mPrimaryVehicleRef.mbSet          = true;
}

void BehaviourIceAnim::SetSecondaryVehicleRefToPlayer()
{
    mSecondaryVehicleRef.meType         = BrnDirector::VehicleRef::E_PLAYER_CAR;
    mSecondaryVehicleRef.miRaceCarIndex = -1;
    mSecondaryVehicleRef.muRef          = 0;
    mSecondaryVehicleRef.mbSet          = true;
}

} } // namespace BrnDirector::Camera
