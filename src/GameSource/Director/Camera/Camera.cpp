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
// RECONSTRUCTED THIS WAVE (7 funcs -- the BehaviourHandle<T> "is-waiting-to-prepare" query
// family). Bodied as real template members on the homed BehaviourHandle<TBehaviour> in
// BrnBehaviourManager.h (BehaviourHandle<>::IsWaitingToPrepare / ::IsReadyToPrepare),
// reaching the homed BehaviourManager by its NAMED public IsBehaviourWaitingToPrepare(u32)
// -- no raw offsets, no fabrication. All six "waiting" siblings are byte-identical asm:
//   IsWaitingToPrepare():  BehaviourGyroCam @0x82212510, BehaviourIceAnim @0x822128A0,
//                          BehaviourInterpo @0x82212150, BehaviourLooseAt @0x82212A68,
//                          BehaviourRoadRun @0x82212AC8, BehaviourRotateA @0x82212B98
//   IsReadyToPrepare():    BehaviourAfterto @0x8222D0E8  (the `== 0` negation)
//
// DECLARATION-ONLY + FLAGGED (22 funcs). Each below is un-bodyable WITHOUT fabricating an
// un-homed owner-class layout, raw-offset-poking a committed/opaque aggregate, or homing an
// un-recovered free function. Per the project anti-fabrication rule they are left un-bodied:
//
//   -- BehaviourHandle<T>::SetUp factory glue (reaches the UN-HOMED free resolver `BrnDirec`
//      i.e. GetBehaviourSlotFromHandle's underlying lookup, storing *(handle+0x10)): -------
//        BehaviourBystand  @0x8222F688   BehaviourFailsaf @0x822302F8
//        BehaviourHeliCam  @0x82230240   BehaviourPasseng @0x8222F5D0
//        BehaviourRenderM  @0x822303B0   BehaviourSpirall @0x8222FF60
//      (FLAG: the *(handle+0x10) = *BrnDirec(manager,key) tail needs the un-homed behaviour-
//       slot resolver; the declared GetBehaviourSlotFromHandle<T> has no body yet.)
//
//   -- BehaviourHandle<T>::AttachTweaker glue (reaches BehaviourManager::AttachTweaker, which
//      is a DIFFERENT (2-arg, private) overload than the declared private AttachTweaker; the
//      glue also reaches the OPAQUE mTweakerHelper interior): -----------------------------
//        BehaviourDebugFl @0x82213AD0   BehaviourDebugOr @0x82213998   BehaviourFixedCa @0x82212970
//
//   -- BehaviourHandle<T>::SetUpdatesDuringPause glue (reaches BehaviourManager::
//      SetBehaviourUpdatesDuringPause with the 2-arg (key,bool) form the glue uses, not the
//      declared (BehaviourHelperIndex,bool) overload): -----------------------------------
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
