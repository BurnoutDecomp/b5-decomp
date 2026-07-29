// ============================================================================
// GameSource/Director/Camera/Camera.cpp
//
// Compilation home for the BrnDirector::Camera::Camera member functions this TU owns:
//   - Camera::Construct                    @0x82255E68  (default-init the camera)
//   - Camera::ValidateTransformWithDebugInfo @0x8220A850 (debug transform validation)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (offset/behaviour authority) + the DecFIGS
// DWARF (declaration shape). The X360 inlines the sub-block constructors (CameraState /
// DepthOfField / CameraEffects) into Construct as flat store runs; they are de-inlined
// here to the named sub-object Construct calls, matching the asm store set field-for-field.
//
// FLAG (assert machinery): both ValidateTransformWithDebugInfo asserts originally built a
//   dynamic message through CgsDev::StrStream over CgsDev::Assert::gpcMessageBuffer (the
//   off_82000D00 / BasePriorityQueue::Clear / off_82000D08 sink dance), streaming the
//   camera name from mpDebugInfoBehaviour->GetName() ("Camera name unknown" when null) and
//   a transform dump. Per the project rule that replaces the gpcMessageBuffer machinery with
//   CGS_ASSERT, the dynamic message build (and with it the debug-only mpDebugInfoBehaviour
//   name lookup, a virtual call reached only on the failure path) is folded to a static
//   assert message. The validity/position predicates -- the only non-debug side effects --
//   are preserved exactly.
// ============================================================================

#include "GameSource/Director/Camera/Camera.h"
#include "rw/math/vpu/vector3_operation.h"          // rw::math::vpu::IsValid / Abs (Vector3)
#include "rw/math/vpu/matrix44affine_operation.h"   // rw::math::vpu::IsValid (Matrix44Affine)
#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT (gpcMessageBuffer substitute)
#include "GameSource/Director/Camera/Utils/CameraUtils.h"   // Utils::GetZoomFromFOVDegs (GetLodZoomFactor)
#include <cstddef>                                  // offsetof

namespace BrnDirector
{
namespace Camera
{

// ----------------------------------------------------------------------------
// Near/far-clip class constants (DWARF Camera.h:200-202).
//
// FLAG (un-recovered rodata): the X360 GetNearClipDistance @0x82205B68 returns the
// floats at flt_82CDA55C (small-near-clip) and flt_82CDA560 (default-near-clip); the
// far-clip constant is the third member. None of these leaf magnitudes appear in any
// available rodata dump, so their VALUES are NOT reconstructed. Defined here with a
// flagged 0.0f placeholder rather than a fabricated magnitude -- the GetNearClipDistance
// SELECTION logic below is fully X360-attested; only these three leaf values are unknown.
// Replace with the real magnitudes when the .rodata at 0x82CDA55C is recovered.
const f32 Camera::KF_SMALL_NEAR_CLIP_DISTANCE   = 0.0f;  // FLAG placeholder (flt_82CDA55C)
const f32 Camera::KF_DEFAULT_NEAR_CLIP_DISTANCE = 0.0f;  // FLAG placeholder (flt_82CDA560)
const f32 Camera::KF_DEFAULT_FAR_CLIP_DISTANCE  = 0.0f;  // FLAG placeholder

// Pointer-size-independent facts the X360 asm pins (these hold on the x64 gate too).
// CameraEffects has no pointer members, so its 0xBC stride -- the gap the Construct asm
// proves between the camera's effects block (+0x68) and depth-of-field (+0x124) -- is the
// same on console and host. The transform stays at the head on both. (The interior member
// offsets that ride behind the 4-vs-8-byte pointer members are NOT asserted; parity there
// is by named member -- see Camera.h.)
static_assert(sizeof(CameraEffects) == 0xBC, "CameraEffects must be 0xBC (camera +0x68..+0x124)");
static_assert(offsetof(Camera, mTransform) == 0x00, "transform @ +0x00");

// ----------------------------------------------------------------------------
// BrnDirector::Camera::Camera::Construct @0x82255E68
//
// Default-construct the camera. The asm:
//   1. CameraState::Construct(this+0x138)               -> mState.Construct()
//   2. five stfs into the DOF sub-object at +0x124..+0x134 (0.1/0.2/0.3/0.4/0.0,
//      no range asserts) -> the inlined DepthOfField default-init -> mDepthOfField.Construct()
//   3. the inlined CameraEffects default-init (zeroes the hook-name heads, the motion-blur
//      block, the background-effect/fade/post-fx scalars, sets the two blend amounts at
//      +0x9C/+0xB0 to 1.0) over the +0x68 block -> mEffects.Construct()
//   4. tail-call Camera::Clear(this)                    -> Clear()
// (The asm interleaves 2 and 3 across the shared base register r11=this+0x68, but the
//  effects/DOF blocks are disjoint, so the de-inlined order is behaviourally identical.)
// ----------------------------------------------------------------------------
void Camera::Construct()
{
    mState.Construct();          // CameraState::Construct(this+0x138)
    mDepthOfField.Construct();   // inlined DOF default-init (+0x124..+0x134)
    mEffects.Construct();        // inlined CameraEffects default-init (+0x68 block)
    Clear();                     // tail call -> Camera::Clear(this)
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::Camera::ValidateTransformWithDebugInfo @0x8220A850
//
// Debug-build transform validation, run after each transform install. Two checks:
//   1. Every row of mTransform is finite (the asm runs a per-row, per-lane vcmpeqfp
//      self-equality NaN test over xAxis/yAxis/zAxis/wAxis, ANDs the four results) ->
//      IsValid(mTransform). If invalid, fire "Camera has invalid transform".
//   2. The position (mTransform.Pos(), the wAxis row) is "reasonable": the asm Abs's the
//      row (vandc against the sign-bit mask) and compares each xyz lane against a cached
//      broadcast 1,000,000 (vcmpgefp); a lane >= 1,000,000 makes the position unreasonable
//      -> fire "Camera has unreasonable position". (The broadcast constant is a function-
//      local one-time-initialised static -- the dword_82FAAD20 & 1 guard; modelled as the
//      static sOneMillion below.)
//
// Returns the validated-transform pointer the X360 forwards to ICECamera::SetCameraMatrix.
// The asm's r3/r24 ("result") is the camera `this`, and the transform sits at this+0x00,
// so the validated-transform pointer is &mTransform. (On a failing assert the X360 returns
// EndAssert()'s pointer instead; that is part of the dropped gpcMessageBuffer machinery, so
// the validated-transform pointer is returned unconditionally here -- see the file FLAG.)
// ----------------------------------------------------------------------------
rw::math::vpu::Matrix44Affine* Camera::ValidateTransformWithDebugInfo()
{
    // 1. No NaN/Inf in any transform row.
    CGS_ASSERT(rw::math::vpu::IsValid(mTransform),
               "Camera has invalid transform, originated from: ");

    // 2. Position within +/- 1,000,000 on every axis. One-time-initialised broadcast
    //    bound (the X360 dword_82FAAD20 & 1 cache guard).
    static const rw::math::vpu::Vector3 sOneMillion = { 1000000.0f, 1000000.0f, 1000000.0f, 1000000.0f };
    const rw::math::vpu::Vector3 lAbsPosition = rw::math::vpu::Abs(mTransform.Pos());
    CGS_ASSERT(lAbsPosition.x < sOneMillion.x
            && lAbsPosition.y < sOneMillion.y
            && lAbsPosition.z < sOneMillion.z,
               "Camera has unreasonable position, originated from: ");

    return &mTransform;
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::Camera::Camera(const Camera&) @0x821F3B88
//
// The copy constructor. The X360 asm is a flat field-by-field copy of the WHOLE camera
// (no behaviour beyond the copy): five vmx loads/stores cover mTransform (64B) + mSubject
// (16B); a run of word stores copies mpDebugInfoBehaviour..mpCrashAnalysis; a memcpy of
// 0xBC (188) bytes copies mEffects; a 5-word loop copies mDepthOfField (0x14); a 6-word
// loop copies mState (0x18); and the tail copies mfCustomNearClipDistance, the two
// mShotSelectionInfo words, and the mbHasSubject / mbHasCustomNearClipDistance bytes.
// That is exactly a memberwise copy of every member, so the de-optimised human form is
// the implicit memberwise copy. (Console offsets +0x00..+0x15D; member parity is by name
// on the x64 gate -- see Camera.h.)
// ----------------------------------------------------------------------------
Camera::Camera(const Camera& lrOther) = default;

// ----------------------------------------------------------------------------
// BrnDirector::Camera::Camera::SetFOV @0x821F26B8
//
// Set the camera field-of-view. The asm asserts the new FOV is strictly greater than 0
// (fcmpu f31, 0.0; bgt skips the assert) -- Camera.h:424 "lfFOV > 0.0f" -- then stores it
// to mfFOV (+0x58). The dynamic gpcMessageBuffer assert is replaced by CGS_ASSERT per the
// project rule; the store side effect is preserved exactly.
// ----------------------------------------------------------------------------
void Camera::SetFOV(f32 lfFOV)
{
    CGS_ASSERT(lfFOV > 0.0f, "lfFOV > 0.0f");
    mfFOV = lfFOV;
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::Camera::GetFOV / ::GetTransform / ::SetTransform
//
// The three field accessors the X360 always INLINES (no exported symbol -- every consumer
// shows a direct load/store of mfFOV @+0x58 or the mTransform block @+0x00). They are
// declared in Camera.h precisely so no consumer forms those offsets itself; the bodies are
// the loads/stores themselves, so there is nothing to transcribe and nothing to flag.
// ----------------------------------------------------------------------------
f32 Camera::GetFOV() const
{
    return mfFOV;
}

const rw::math::vpu::Matrix44Affine& Camera::GetTransform() const
{
    return mTransform;
}

void Camera::SetTransform(const rw::math::vpu::Matrix44Affine& lrTransform)
{
    mTransform = lrTransform;
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::Camera::GetNearClipDistance @0x82205B68
//
// The active near-clip distance. The asm:
//   if (mbHasCustomNearClipDistance)          return mfCustomNearClipDistance;  // +0x15D / +0x150
//   if (mState_uFlags & 0x10000)              return KF_SMALL_NEAR_CLIP_DISTANCE; // +0x140
//   else                                      return KF_DEFAULT_NEAR_CLIP_DISTANCE;
// (The flags load is a 64-bit ld of mState's current-flag word at +0x140, masked to the
// single 0x10000 bit.) FLAG: the two constant magnitudes are un-recovered rodata -- see
// the KF_*_NEAR_CLIP_DISTANCE definitions above; the branch logic here is X360-exact.
// ----------------------------------------------------------------------------
f32 Camera::GetNearClipDistance() const
{
    if (mbHasCustomNearClipDistance)
    {
        return mfCustomNearClipDistance;
    }
    if ((mState_uFlags & 0x10000) != 0)
    {
        return KF_SMALL_NEAR_CLIP_DISTANCE;
    }
    return KF_DEFAULT_NEAR_CLIP_DISTANCE;
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::Camera::Clear @0x8223CE70 (destub wave 2026-07-26)
//
// Reset the camera to its defaults. Store-for-store against the asm:
//   * mTransform = the identity basis with a zero pos row (the four stvx128 of
//     {1,0,0,0}/{0,1,0,0}/{0,0,1,0}/{0,0,0,0}) -- Matrix44Affine::SetIdentity;
//   * NULL the three pointers (+0x50/+0x54/+0x64), clear both has-flags
//     (+0x15C/+0x15D);
//   * mfFOV = 90 (flt_82004F64), mfAspectRatio = 16/9 (flt_82009A78);
//   * the DOF default band (+0x124..+0x134) -> mDepthOfField.Construct() (the
//     same inlined store set as Camera::Construct);
//   * mState.Clear() (the out-of-line @0x82220950 call);
//   * the effects-block default store run (r11 = this+0x68) ->
//     mEffects.Construct() (same inlined store set as Camera::Construct);
//   * mShotSelectionInfo = { -1, -1 }.
// NOT touched by the X360 body: mSubject, mfRunningTime,
// mfCustomNearClipDistance.
// ----------------------------------------------------------------------------
void Camera::Clear()
{
    mTransform.SetIdentity();            // stvx128 x4 @ +0x00..+0x30 (pos row zero)

    mpDebugInfoBehaviour = 0;            // stw 0 @+0x50
    mbHasSubject = false;                // stb 0 @+0x15C
    mfFOV = 90.0f;                       // flt_82004F64
    mpSourceShot = 0;                    // stw 0 @+0x54
    mfAspectRatio = 1.7777778f;          // flt_82009A78
    mpCrashAnalysis = 0;                 // stw 0 @+0x64
    mbHasCustomNearClipDistance = false; // stb 0 @+0x15D

    mDepthOfField.Construct();           // the five stfs @+0x124..+0x134
    mState.Clear();                      // bl @0x82220950 (this+0x138)
    mEffects.Construct();                // the store run @ r11 = this+0x68

    mShotSelectionInfo.miType = -1;      // stw -1 @+0x154
    mShotSelectionInfo.miId   = -1;      // stw -1 @+0x158
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::Camera::CopyToCgsCamera @0x8220AC48 (destub wave 2026-07-26)
//
// Publish this director camera into a graphics camera:
//   1. ValidateTransformWithDebugInfo() (NaN / unreasonable-position tripwires);
//   2. reset the target camera (the @0x827F94E8 defaults reset + the empty
//      ICF-folded teardown stub) -> lpOutCamera->Release();
//   3. fov: degrees -> radians (0.017453292), clamped to [1 deg, 160 deg]
//      (2.7925267) with the two fsel arms, then SetFovHorizontal;
//   4. LookAt(eye = mTransform.Pos() [+0x30], up = mTransform.Up() [+0x10],
//      target = Pos + At [+0x30 + +0x20], the vaddfp);
//   5. near clip: the custom-override / small-flag / default selection --
//      exactly GetNearClipDistance() (+0x15D / +0x150 / the +0x144 & 0x10000
//      flag test) -> scalar store + UpdatePerspectiveProjectionMatrix;
//   6. far clip: KF_DEFAULT_FAR_CLIP_DISTANCE (flt_82CDA564; FLAG magnitude
//      un-recovered, see the definition above) -> scalar store +
//      UpdatePerspectiveProjectionMatrix again.
// ----------------------------------------------------------------------------
void Camera::CopyToCgsCamera(CgsGraphics::Camera* lpOutCamera) const
{
    const_cast<Camera*>(this)->ValidateTransformWithDebugInfo();

    lpOutCamera->Release();   // sub_827F94E8 + the empty folded Destruct

    // fovRad = clamp(mfFOV * DEG2RAD, 1 deg, 160 deg) -- the two fsel arms.
    f32 lfFovRadians = mfFOV * 0.017453292f;
    if (0.017453292f - lfFovRadians >= 0.0f)
    {
        lfFovRadians = 0.017453292f;
    }
    if (2.7925267f - lfFovRadians < 0.0f)
    {
        lfFovRadians = 2.7925267f;
    }
    lpOutCamera->SetFovHorizontal(lfFovRadians);

    // target = Pos + At (the vaddfp of +0x30 and +0x20).
    rw::math::vpu::Vector3 lTarget;
    lTarget.x = mTransform.Pos().x + mTransform.At().x;
    lTarget.y = mTransform.Pos().y + mTransform.At().y;
    lTarget.z = mTransform.Pos().z + mTransform.At().z;
    lTarget.w = mTransform.Pos().w + mTransform.At().w;
    lpOutCamera->LookAt(mTransform.Pos(), mTransform.Up(), lTarget);

    // Near clip (the inlined GetNearClipDistance selection), then rebuild.
    lpOutCamera->maProjectionScalars[7] = GetNearClipDistance();   // m_nearClipPlane
    lpOutCamera->UpdatePerspectiveProjectionMatrix();

    // Far clip (flt_82CDA564 == KF_DEFAULT_FAR_CLIP_DISTANCE), then rebuild.
    lpOutCamera->maProjectionScalars[8] = KF_DEFAULT_FAR_CLIP_DISTANCE;  // m_farClipPlane
    lpOutCamera->UpdatePerspectiveProjectionMatrix();
}

// ----------------------------------------------------------------------------
// The frame-camera reads the WorldModule render feeds inline on the X360
// (destub wave 2026-07-26):
//   * GetPosition / GetDirection: the transform's pos / at rows (+0x30 / +0x20
//     -- the lvx128 reads in WorldModule::GenerateDispatchLists @0x827D1CE8).
//   * IsInJunkyard: the camera-state current-flag word & 0x400000 (the ld at
//     +0x140 masked in the junkyard lighting latch).
//   * GetLodZoomFactor @0x827BAC40: forwards the camera FOV (degrees) into
//     Utils::GetZoomFromFOVDegs (the 1/tan(fov/2) zoom; @0x821F23E8's xref
//     list attests the call edge). FLAG: the 0x827BAC40 export JSON is absent
//     from the dump set, so the body is recovered from the call edge + the
//     degrees-domain contract (GetZoomFromFOVDegs(90 deg) == 1.0, the neutral
//     LOD zoom at the camera's default FOV).
// ----------------------------------------------------------------------------
Vector3 Camera::GetPosition() const
{
    return mTransform.Pos();     // +0x30
}

Vector3 Camera::GetDirection() const
{
    return mTransform.At();      // +0x20
}

bool Camera::IsInJunkyard() const
{
    return (mState_uFlags & 0x400000) != 0;
}

f32 Camera::GetLodZoomFactor() const
{
    return Utils::GetZoomFromFOVDegs(mfFOV);
}

// ============================================================================
// FLAG MANIFEST -- class:BrnDirector::Camera postmortem bucket (29 functions)
//
// The postmortem bucket for this class TU groups 29 functions under TRUNCATED demangled
// symbols ("BrnDirector::Camera::Beha", "...::BehaviourGyroCam", "...::BehaviourRenderMe",
// ...). They are NOT BrnDirector::Camera::Camera member functions. The truncated symbols
// collapse THREE distinct, un-homed-at-source owner families:
//   (1) BehaviourHandle<TBehaviour> template-member instantiations (one body, many T),
//   (2) internal Array<T,N>::operator[] / PushBack bounds machinery (per element type),
//   (3) per-behaviour-subclass getters in GameSource/Director/Camera/Behaviours/* whose
//       headers are not yet homed, plus two free helpers and a vector-ctor-iterator.
//
// RECONSTRUCTED PRIOR WAVE (7 funcs -- the BehaviourHandle<T> "is-waiting-to-prepare" query
// family). Bodied as real template members on the homed BehaviourHandle<TBehaviour> in
// BrnBehaviourManager.h (BehaviourHandle<>::IsWaitingToPrepare / ::IsReadyToPrepare),
// reaching the homed BehaviourManager by its NAMED public IsBehaviourWaitingToPrepare(u32)
// -- no raw offsets, no fabrication. All six "waiting" siblings are byte-identical asm:
//   IsWaitingToPrepare():  BehaviourGyroCam @0x82212510, BehaviourIceAnim @0x822128A0,
//                          BehaviourInterpo @0x82212150, BehaviourLooseAt @0x82212A68,
//                          BehaviourRoadRun @0x82212AC8, BehaviourRotateA @0x82212B98
//   IsReadyToPrepare():    BehaviourAfterto @0x8222D0E8  (the `== 0` negation)
//
// RESOLVED THIS WAVE (6 funcs -- the BehaviourHandle<T>::Prepare "SetUp factory" family).
// These are NOT new hand-written bodies: each is a per-T instantiation of the SINGLE inline
// BehaviourHandle<TBehaviour>::Prepare(u32 key, u32 helperIndex, BehaviourManager*) already
// committed in BrnBehaviourManager.h. They were previously FLAGGED un-bodyable because the
// final `*(handle+0x10) = *BrnDirec(handle+8, handle+4)` tail reached an UN-HOMED free
// resolver. That resolver is now the homed (declared) static template
// BehaviourManager::GetBehaviourSlotFromHandle<TBehaviour>(u32 helperIndex, u32 key), which
// the committed inline Prepare already calls BY NAME -- so all six instantiations now compile
// and link against the inline body with zero fabrication. The asm of each is byte-identical
// to Prepare (guard-on-mbAllocated UnSet, store key/helperIndex/manager, SetBehaviourUsedBy-
// Handle(key), IsAllocated() assert, mpBehaviour = *GetBehaviourSlotFromHandle<T>(...)):
//   BehaviourBystand  @0x8222F688 (BehaviourBystanderCam)   BehaviourFailsaf @0x822302F8 (BehaviourFailsafe)
//   BehaviourHeliCam  @0x82230240 (BehaviourHeliCam)        BehaviourPasseng @0x8222F5D0 (BehaviourPassengerCam)
//   BehaviourRenderM  @0x822303B0 (BehaviourRenderMetrics)  BehaviourSpirall @0x8222FF60 (BehaviourSpirallingDeathcam)
// (Proven this wave by a scratch instantiation TU over the now-declared resolver + homed
// manager; the TU compiled STATUS=pass and was deleted. No new code lands in Camera.cpp --
// the 6 bodies live in the committed inline Prepare, this entry records that they now resolve.)
//
// DECLARATION-ONLY + FLAGGED (16 funcs). Each below is STILL un-bodyable WITHOUT fabricating
// an un-homed owner-class layout, raw-offset-poking a committed/opaque aggregate, or homing an
// un-recovered free function / un-reconciled manager overload. Per the anti-fabrication rule
// they are left un-bodied:
//
//   -- BehaviourHandle<T>::AttachTweaker glue (mpManager->AttachTweaker(muAllocationKey) on a
//      KEY, not the declared private AttachTweaker(BehaviourHelperIndex) on a helper word; the
//      glue also reaches the OPAQUE mTweakerHelper interior). STILL un-bodyable: adding a
//      key-based manager overload collides with BehaviourHelperIndex's implicit s32 conversion
//      (ambiguity) on a shared committed header other TUs consume -- deferred until the manager
//      tweaker overload set is reconciled. NOT a missing type; a shared-header overload hazard:
//        BehaviourDebugFl @0x82213AD0   BehaviourDebugOr @0x82213998   BehaviourFixedCa @0x82212970
//
//   -- BehaviourHandle<T>::SetUpdatesDuringPause glue (mpManager->SetBehaviourUpdatesDuringPause
//      (muAllocationKey, bool) -- the 2-arg (key,bool) form, not the declared
//      (BehaviourHelperIndex,bool) overload). Same shared-header overload-ambiguity hazard as
//      AttachTweaker above; deferred until reconciled:
//        BehaviourGamepla @0x82212360
//
//   -- internal Array<T,N> bounds/PushBack machinery over un-homed element types (raw 24*i /
//      4*i / 29*i strides into containers whose element layout is this glue's own concern):
//        Beha   @0x82200460 (stride 24)   Behav  @0x821FFF30 (stride 4)
//        BehaviourB @0x8222F740 (cap 2)   BehaviourI @0x8222FBE8 (cap 5)
//        BehaviourR @0x8222F890 (cap 4)   BehaviourHelperIndex_28 @0x82212008 (cap 28, stride 29)
//
//   -- per-behaviour-subclass trivial getters; owner headers in Behaviours/* NOT homed (the
//      returned member offsets are into un-homed subclass layouts): ----------------------
//        BehaviourRenderMe @0x821FB280 (return *(this+0xB8))
//        BehaviourRotateAb @0x821FB418 (return *(this+0x374))
//        BehaviourSpiralli @0x821FB590 (return *(this+0x2D0))
//
//   -- free / special helpers reaching un-homed code or un-homed layouts: ----------------
//        B @0x8222CBD8 -- a BrnDirector::MomentPlayerJumping::UpdateCamera helper that reaches
//          the un-homed free `BrnDirector::Cam` + `BrnDirec` and pokes MomentPlayerJumping
//          fields (+0x40/+0x41/+0x3C) by raw offset (MomentPlayerJumping is not homed here).
//        BehaviourHelperIndex_28__28_ @0x827DE450 -- the Array<Array<BehaviourHelperIndex,28>,28>
//          vector-constructor-iterator (28x BaseCollisionGenerator::Destruct sweep, stride
//          116); a compiler-generated ctor-iterator, not a hand-written method.
//        EnsureEffectIsStopped @0x821F2808 -- reaches the un-homed
//          BrnDirector::HookNameStringWrapper::Set and raw-offset-pokes Camera-adjacent
//          fields (+287/+288/+104/+137 on `this`, +3383/+3316 on a second un-homed object).
//
// Replace each FLAGGED entry with a real body when its owner family is homed: the behaviour
// subclasses (Behaviours/*), the un-homed free resolvers (BrnDirec / BrnDirector::Cam /
// HookNameStringWrapper), and the BehaviourManager's 2-arg private tweaker / pause overloads.
// ============================================================================

} // namespace Camera
} // namespace BrnDirector
