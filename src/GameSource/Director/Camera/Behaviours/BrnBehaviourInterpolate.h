#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_INTERPOLATE_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_INTERPOLATE_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT (the Setup asserts)
#include "GameSource/Director/Camera/Behaviours/Behaviour.h"       // THE canonical Camera::Behaviour base
#include "GameSource/Director/Camera/BrnCollisionPolicy.h"         // VisibilityCollisionPolicy (mCollisionPolicy)
#include "GameSource/Director/Camera/BrnCameraReference.h"         // THE real CameraReference (mFromCamera / mToCamera)
#include "GameSource/Director/Shots/ShotControllers/BrnCameraInterpolationController.h"  // THE real blend evaluator

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourInterpolate.h
//
// THE HOME for BrnDirector::Camera::BehaviourInterpolate -- the transition behaviour: it
// blends the camera from one source ("from") camera reference to a "to" camera reference
// over a fixed duration (slerp or rotate-about-the-player-car, with a choice of time
// mappings). The ICE movie player and the arbitrator states drive it.
//
// ----------------------------------------------------------------------------
// ⛔⛔ ODR RECONCILE, 2026-08-01 -- THIS CLASS USED TO HAVE TWO DEFINITIONS.
//
// GameSource/Director/Camera/BrnBehaviourManager.h carried a second, MUTUALLY EXCLUSIVE
// `class BehaviourInterpolate` -- a slice with no base and no members (sizeof == 1) and 11
// declaration-only methods -- and that slice, not this file, was what every consumer
// (BrnArbStateCarSelect.h, the ICE movie player, four other arbitrator states) actually saw.
// Meanwhile THIS file defined its own local `class Behaviour` and its own local
// `class CameraReference`, so the two headers could never be included together (C2011).
// The slice is retired; BrnBehaviourManager.h now forward-declares only.
//
// Two things the slice got WRONG, both now fixed against the asm:
//  * its `CameraReference` put the validity selector at +0x00 (`miSourceSelector`). The real
//    selector is `meType` at +0x168 -- BehaviourInterpolate::Setup @0x821F3E0C reads 0x418
//    (== 0x2B0 + 0x168) and @0x821F3E50 reads 0x588 (== 0x420 + 0x168). The real
//    CameraReference has a full `Camera` by value at +0x00 (sizeof(Camera) == 0x160) and has
//    had its own home, GameSource/Director/Camera/BrnCameraReference.h, all along.
//  * it had no `mpParameters` at all. It is at +0x2A0, immediately before mfDuration --
//    attested at twelve independent SetParameters store sites and at the two
//    "mpParameters" assert reads (Update @0x822449B0, PostCollisionUpdate @0x82252AD4).
//
// ⚠️ The console-offset `offsetof` static_asserts this file's .cpp used to carry are RETIRED
// with the slice. They only ever held because the fabricated members were padded to hit X360
// displacements; with the real base and the real CameraReference embedded, the x64 layout
// legitimately differs. Parity here is BY NAMED MEMBER (the project's x64 rule) -- the
// offsets below are PROVENANCE ONLY and are never used as casts.
// ----------------------------------------------------------------------------
// LAYOUT (member NAMES from the DecFIGS DWARF outline; OFFSETS pinned from the bodied
// functions' member accesses in the X360 asm:
//   GetCameraAForSetup @0x821F3D18  -> &mFromCamera   (this + 0x2B0), asserts !mbSetup
//   GetCameraBForSetup @0x821F3D70  -> &mToCamera     (this + 0x420), asserts !mbSetup
//   GetCollisionPolicy @0x821F9EE8  -> &mCollisionPolicy (this + 0x20) when mbUseCollisionPolicy
//   GetParametricTime  @0x822065F8  -> clamp(mfRunningTime/mfDuration, 0, 1); reads 0x2A4/0x590
//   SetupDuration      @0x821F3CB0  -> mfDuration = lfDuration (0x2A4), asserts !mbSetup
//   SetParameters      (inlined)    -> mpParameters (0x2A0) + Behaviour::mpcDebugParametersName (0x10)
//
//   +0x0000  Behaviour                        (base)  vtable + shared flag/state block
//   +0x0020  VisibilityCollisionPolicy        mCollisionPolicy            (GetCollisionPolicy -> &this)
//   +....    CameraInterpolationController    mInterpolator
//   +0x02A0  const Parameters*                mpParameters
//   +0x02A4  float32_t                        mfDuration
//   +0x02B0  CameraReference                  mFromCamera
//   +0x0420  CameraReference                  mToCamera
//   +0x0590  float32_t                        mfRunningTime
//   +0x0594  bool                             mbUseCollisionPolicy
//   +0x0595  bool                             mbSetup
//   +0x0596  bool                             mbHasFinished
// ============================================================================

namespace BrnDirector
{
namespace Camera
{

class BehaviourInterpolate : public Behaviour
{
public:
    // DWARF BrnBehaviourInterpolate.h:189/:190 -- the two selector enums the per-take
    // Parameters record carries. ArbStateCarSelect::Construct @0x8225B0C0 seeds
    // {method = 0, mapping = 1}, i.e. slerp + sinusoidal.
    enum EInterpolationMethod
    {
        E_METHOD_SLERP                   = 0,
        E_METHOD_ROTATE_ABOUT_PLAYER_CAR = 1,
    };

    enum EInterpolationMapping
    {
        E_MAPPING_LINEAR                  = 0,
        E_MAPPING_SINUSOIDAL              = 1,
        E_MAPPING_EXPONENTIAL_SYMMETRICAL = 2,
        E_MAPPING_EXPONENTIAL_OUT_X_CUBED = 3,
    };

    // ------------------------------------------------------------------------
    // Per-take interpolation curve/easing parameters (DWARF BrnBehaviourInterpolate.h:186-193;
    // MomentPlayerJumping holds one BY VALUE @X360 +0x36C). It IS a Behaviour::Parameters --
    // the record's head is the shared {type tag, debug name} pair, which is exactly what
    // SetParameters reads from (`lwz rX, 4(lpParams)` == the base's mpcDebugName).
    // The inlined default construct (X360 ArbStateCarSelect::Construct @0x8225B0C0..0x8225B0EC
    // stores {8, 0, 0, 1}): the interpolate behaviour-type tag, no debug name, slerp method,
    // sinusoidal mapping.
    // ⚠️ WHICH of +0x08 / +0x0C is "method" and which is "mapping" is INFERRED, not proven:
    //   the two offsets are hard-attested (PostCollisionUpdate @0x82252C7C..0x82252CA0 reads
    //   params[+0xC] -> camera +0x11D and params[+0x8] -> camera +0x11E), but the labels come
    //   from the seeded pair {0, 1} matching E_METHOD_SLERP / E_MAPPING_SINUSOIDAL.
    // ------------------------------------------------------------------------
    class Parameters : public Behaviour::Parameters
    {
    public:
        void Construct()
        {
            mType                  = 8;                     // +0x00 the interpolate behaviour-type tag
            SetDebugName(0);                                // +0x04
            meInterpolationMethod  = E_METHOD_SLERP;        // +0x08 (:189)
            meInterpolationMapping = E_MAPPING_SINUSOIDAL;  // +0x0C (:190)
        }

        EInterpolationMethod  GetInterpolationMethod()  const { return meInterpolationMethod; }
        EInterpolationMapping GetInterpolationMapping() const { return meInterpolationMapping; }

        EInterpolationMethod  meInterpolationMethod;   // +0x08 (:189)
        EInterpolationMapping meInterpolationMapping;  // +0x0C (:190)
    };

    // ------------------------------------------------------------------------
    // ⭐⭐ THE FIVE VIRTUALS (2026-08-01, car-select hand-off wave). Until now this class
    // declared NONE of them, so every one resolved to Camera::Behaviour's shape-only default
    // -- and `Behaviour::Update` is `return true;` WITHOUT TOUCHING THE CAMERA. That is the
    // whole reason ArbStateCarSelect's hand-off collapsed: the moment
    // GAME_INTRO_PART_THREE published `mToGameplayInterpolater.GetCamera()`, it published a
    // camera that nothing had ever written -- i.e. exactly what BehaviourHelper::Prepare's
    // `Camera::Construct` left there, an identity basis at the origin -- and mbHasFinished was
    // never set, so meState never left PART_THREE either.
    //
    // ⚠️ The base's own FLAG said those defaults were "never dispatched on the live path (every
    // pooled behaviour is a concrete leaf)". BehaviourInterpolate IS a concrete leaf that was
    // pooled and dispatched -- the note had expired. (Behaviour.cpp's FLAG is corrected there.)
    //
    // X360: Construct @0x82255FC8, Prepare @0x82252A10, Update @0x82244998,
    //       PostCollisionUpdate @0x82252AB8, Release @0x82252CB8, GetName @0x821F9F00.
    // ------------------------------------------------------------------------
    void        Construct() override;                                                 // slot 0
    bool        Prepare(const BehaviourSharedPrepareReleaseInfo& lrInfo) override;     // slot 1
    bool        Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo) override;  // slot 2
    bool        PostCollisionUpdate(Camera& lrCamera,
                                    const BehaviourSharedInfo& lrInfo) override;       // slot 3
    void        Release(const BehaviourSharedPrepareReleaseInfo& lrInfo) override;      // slot 4
    const char* GetName() const override;                                              // slot 9

    // 0 == cut / no pause updates, 2 == updates-during-pause. (Own ledger function.)
    void SetInterpolationMode(s32 liMode);

    // The source / destination camera references the blend runs between (populated before
    // the no-arg Setup via these accessors). The asm asserts the behaviour has NOT yet been
    // set up before handing the reference out. @0x821F3D18 / @0x821F3D70.
    CameraReference& GetCameraAForSetup();
    CameraReference& GetCameraBForSetup();

    // Return this behaviour's active collision policy: &mCollisionPolicy when
    // mbUseCollisionPolicy is set, otherwise NULL. @0x821F9EE8. (Body in the .cpp.)
    CollisionPolicy* GetCollisionPolicy();

    // The clamped blend parameter t = clamp(mfRunningTime / mfDuration, 0, 1). Asserts the
    // duration is non-zero first. @0x822065F8. (Body in the .cpp.)
    f32 GetParametricTime() const;

    void SetParameters(const Parameters* lpParams);
    void SetupDuration(f32 lfDuration);
    void SetupCameraAFromCamera(const Camera& lrCamera);
    void SetupCameraAFromHelper(BehaviourHelperIndex lHelper, BehaviourManager& lrManager);
    void SetupCameraBFromCamera(const Camera& lrCamera);
    void SetupCameraBFromHelper(BehaviourHelperIndex lHelper, BehaviourManager& lrManager);
    void Setup();

    bool HasFinished() const { return mbHasFinished; }

    // The camera this interpolator is producing this frame. (Own ledger function.)
    const Camera& GetCamera() const;

    // Layout members are public-of-layout so consumers reach them BY NAME.
    VisibilityCollisionPolicy mCollisionPolicy;   // +0x020

    // The per-frame blend evaluator, by value. Was a 4-byte OpaqueInterpolationController
    // stand-in until 2026-08-20; the real type is two Utils::Interpolater sub-objects (the
    // "two 16-byte blocks + a trailing byte" Construct below zeroes), and it is what turns
    // the parametric time into an actual blended camera in PostCollisionUpdate.
    CameraInterpolationController mInterpolator;

    // ⭐ +0x2A0. NOT initialised by BehaviourInterpolate::Construct @0x82255FC8 (that
    // function's store set does not touch +0x2A0) -- which is precisely why Update
    // @0x822449B0 and PostCollisionUpdate @0x82252AD4 both open with an "mpParameters"
    // assert. Left uninitialised here for the same reason.
    const Parameters* mpParameters; // +0x2A0

    f32             mfDuration;     // +0x2A4  blend duration (SetupDuration writes; GetParametricTime reads)
    CameraReference mFromCamera;    // +0x2B0  the "from" camera (GetCameraAForSetup)
    CameraReference mToCamera;      // +0x420  the "to" camera   (GetCameraBForSetup)
    f32             mfRunningTime;  // +0x590  elapsed blend time (GetParametricTime reads)

    bool            mbUseCollisionPolicy; // +0x594  selects whether GetCollisionPolicy returns the policy
    bool            mbSetup;        // +0x595  latched once Setup has run
    bool            mbHasFinished;  // +0x596  the blend has reached its end
};



// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourInterpolate::GetCameraAForSetup @0x821F3D18
//   lbz  r11, 0x595(r31)     ; mbSetup
//   ... assert !mbSetup ("Can't setup camera A after Setup() has been called") ...
//   addi r3, r31, 0x2B0      ; return &mFromCamera
// ----------------------------------------------------------------------------
inline CameraReference&
BehaviourInterpolate::GetCameraAForSetup()
{
    CGS_ASSERT(!mbSetup, "Can't setup camera A after Setup() has been called");
    return mFromCamera;
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourInterpolate::GetCameraBForSetup @0x821F3D70
//   lbz  r11, 0x595(r31)     ; mbSetup
//   ... assert !mbSetup ("Can't setup camera B after Setup() has been called") ...
//   addi r3, r31, 0x420      ; return &mToCamera
// ----------------------------------------------------------------------------
inline CameraReference&
BehaviourInterpolate::GetCameraBForSetup()
{
    CGS_ASSERT(!mbSetup, "Can't setup camera B after Setup() has been called");
    return mToCamera;
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourInterpolate::SetParameters -- X360 header-inline (no
// standalone symbol; the assert-file string of this class's inlines is the .h, distinct from
// the .cpp string its Prepare/Update/Release use). Recovered from TWELVE identical inline
// sites -- InterpolaterHelper::Prepare @0x8226628C, ICEMoviePlayer::InterpolateFrom
// @0x82263BB4, ICEMoviePlayer::Update @0x82267140, ArbStateRoaming::Update @0x82264784,
// ArbStateOnlineRaceIntro::Update @0x82273804/@0x82273980/@0x82273B24,
// ArbStateTakedown::Prepare @0x8226DBA4, MomentNewCarJoined::Update @0x82266DC4, and the
// three takedown-player moments -- every one of them exactly:
//   lwz  rX, 4(lpParams)        ; lpParams->mpcDebugName
//   stw  lpParams, 0x2A0(this)  ; mpParameters = lpParams
//   stw  rX,       0x10(this)   ; Behaviour::mpcDebugParametersName = lpParams->GetDebugName()
//
// ⚠️ THERE IS NO TYPE-TAG ASSERT, and that is a real difference from the siblings, not an
// omission: BehaviourAftertouchCam::SetParameters @0x821F3EA0 has the
// "lpParameters->GetType() == e..." assert AND both stores, BehaviourRoadRunner::SetParameters
// @0x821F5550 has the assert and only ONE store. All twelve interpolate sites are a bare
// lwz/stw/stw with no compare and no branch.
// ----------------------------------------------------------------------------
inline void
BehaviourInterpolate::SetParameters(const Parameters* lpParams)
{
    mpParameters           = lpParams;
    mpcDebugParametersName = lpParams->GetDebugName();
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourInterpolate::SetupDuration @0x821F3CB0
//   fmr  f31, f1             ; latch lfDuration
//   lbz  r11, 0x595(r31)     ; mbSetup
//   ... assert !mbSetup ("Can't setup duration after Setup() has been called") ...
//   stfs f31, 0x2A4(r31)     ; mfDuration = lfDuration
// ----------------------------------------------------------------------------
inline void
BehaviourInterpolate::SetupDuration(f32 lfDuration)
{
    CGS_ASSERT(!mbSetup, "Can't setup duration after Setup() has been called");
    mfDuration = lfDuration;
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourInterpolate::SetupCameraAFromHelper / ...BFromHelper
// -- X360 header-inlines, recovered from the sites where the compiler left the composition
// out of line. ICEMoviePlayer::Update is the fully decomposed shape:
//   0x82267188  bl  BehaviourInterpolate::GetCameraBForSetup   ; r3 = this + 0x420
//   0x8226718C  mr  r4, r29     ; the BehaviourHelperIndex
//   0x82267190  mr  r5, r28     ; the BehaviourManager*
//   0x82267194  bl  CameraReference::Setup(BehaviourHelperIndex, const BehaviourManager*)
// The A-variant is the same three registers at ICEMoviePlayer::InterpolateFrom
// @0x82263C60..0x82263C94 against this + 0x2B0.
//
// ⚠️ CameraReference::Setup is NOT just a selector write: it asserts the reference is still
// E_TYPE_INVALID, stores meType = E_TYPE_BEHAVIOUR and the helper index, and then SNAPSHOTS
// the helper's current camera into the reference's own `mCamera`. (@0x8223E990.)
// ----------------------------------------------------------------------------
inline void
BehaviourInterpolate::SetupCameraAFromHelper(BehaviourHelperIndex lHelper, BehaviourManager& lrManager)
{
    GetCameraAForSetup().Setup(lHelper, &lrManager);
}

inline void
BehaviourInterpolate::SetupCameraBFromHelper(BehaviourHelperIndex lHelper, BehaviourManager& lrManager)
{
    GetCameraBForSetup().Setup(lHelper, &lrManager);
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourInterpolate::Setup @0x821F3DC8
//   The no-arg overload: both camera references must already be valid. The console body
//   asserts !mbSetup, then mFromCamera valid, then mToCamera valid, then latches mbSetup.
//   (The asserts carry a copy-pasted "Can't setup twice" message in the shipped strings.)
//   The two validity reads are @0x821F3E0C (0x2B0 + 0x168) and @0x821F3E50 (0x420 + 0x168),
//   i.e. CameraReference::meType -- which is what pins the real reference layout.
//
// ⚠️ THE CONSOLE ALSO HAS A COMBINED OVERLOAD, Setup(f32, BehaviourHelperIndex,
//   BehaviourHelperIndex, BehaviourManager&) @0x8224EE58, and InterpolaterHelper::Prepare
//   @0x822662DC calls THAT rather than the four-call decomposition the PC reconstruction
//   uses. The combined form writes mfDuration, latches mbSetup FIRST, then Setups both
//   references directly (no GetCameraXForSetup, no IsValid asserts). The decomposed shape
//   reaches an identical end state and fires no assert (mbSetup is still false while the
//   references are written); it just evaluates five asserts the console path skips.
//   Recorded rather than "fixed" -- swapping to the combined overload is a behaviour-neutral
//   shape change and belongs with the InterpolaterHelper TU.
// ----------------------------------------------------------------------------
inline void
BehaviourInterpolate::Setup()
{
    CGS_ASSERT(!mbSetup, "Can't setup twice");
    CGS_ASSERT(mFromCamera.IsValid(), "Can't setup twice");
    CGS_ASSERT(mToCamera.IsValid(), "Can't setup twice");
    mbSetup = true;
}

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_INTERPOLATE_H
