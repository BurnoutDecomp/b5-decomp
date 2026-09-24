// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourFixedCam.cpp
//
// BrnDirector::Camera::BehaviourFixedCam -- the behaviour's own bodies, transcribed from the ARTIST
// X360 asm (DWARF homes all of them in this file):
//   Parameters::Construct  (cpp:34)  -- inlined by BehaviourParameterBank::Construct @0x8223DC90
//   Construct              (cpp:51)  @0x82229D20
//   Prepare                (cpp:69)  @0x821FAD28
//   Update                 (cpp:87)  @0x82229DE0
//   GetCollisionPolicy     (cpp:162) @0x821FB588
//   SetupTweaker           (cpp:176) @0x821FAD48
//   GetName                (cpp:198) @0x821FAE48
// SetParameters (h:122, @0x821F4518) is a header inline. The Parameters::Serialise<S> visitor lives in
// the partfile BrnBehaviourFixedCamSerialise.cpp, because its serialisers are not in the exe's link.
//
// ⭐ 2026-09-24 (FX-DIRECTOR): these bodies are what turn this class from a hollow shell into a real
// Camera::Behaviour -- see the banner in the header for the crash they end.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourFixedCam.h"
#include "GameSource/Director/Camera/Utils/CameraUtils.h"               // Utils::CreateLookAt
#include "GameSource/Director/Camera/Utils/BrnCameraTweaker.h"          // Utils::Tweaker (SetupTweaker)
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                   // CgsNumeric::Random::RandomFloat
#include "rw/math/vpu/vector3_operation.h"                              // MagnitudeSquared / IsZero / NormalizeFast
#include "rw/math/vpu/matrix44affine_operation.h"                       // Mult / MakeRotationZ

namespace BrnDirector
{
namespace Camera
{

namespace
{
    // Update's constants, each read from the image at its load site.
    const f32 KF_TIME_AHEAD            = 1.0f;          // lTimeAhead    flt_82001C98 (0x3F800000)
    const f32 KF_MIN_DISTANCE          = 5.0f;          // lMinDistance  flt_8200426C (0x40A00000)
    const f32 KF_MAX_DISTANCE_SQUARED  = 400.0f;        // the fail test flt_8200889C (0x43C80000)
    const f32 KF_ZERO_TOLERANCE        = 1.1920929e-7f; // IsZero        flt_82001770 (0x34000000)
    const f32 KF_DEGREES_TO_RADIANS    = 0.017453292f;  // the dutch     flt_82001744 (0x3C8EFA35)

    // The validity-account flags Update raises (names from the account's recovered table).
    const s32 KI_FAILED_POSITION_TOO_FAR_FROM_SUBJECT = 7;    // `li r5, 7` into Behaviour::Fail
    const s32 KI_NOCUTTO_SUBJECT_LEFT_FRAME           = 16;   // `oris r11, r11, 1` on camera +0x138

    // The camera-state flag Update raises while the behaviour has not failed (`ori r11, r11, 2` on
    // camera +0x140) -- the same bit every live behaviour raises (GyroCam, ICECamera).
    const s32 KI_CAMERA_STATE_FLAG_BEHAVIOUR_LIVE = 2;

    // Parameters::Construct's authored defaults, read off the bank's inlined copy
    // (0x8223DC90: stfs 70.0 -> +9020, stfs 10.0 -> +9024).
    const f32 KF_DEFAULT_FOV       = 70.0f;
    const f32 KF_DEFAULT_MAX_DUTCH = 10.0f;

    // SetupTweaker's scale (stfs 0.5 into both mappings' mfScale).
    const f32 KF_TWEAK_SCALE = 0.5f;
}

// ----------------------------------------------------------------------------
// Parameters::Construct (cpp:34). Inlined once, over the bank's fixed-cam block:
//     stw 0,    +9016   the base's debug name   } Behaviour::Parameters::Construct
//     stfs 70.0, +9020  mfFOV
//     stw 15,   +9012   the type tag
//     stfs 10.0, +9024  mfMaxDutch
// ----------------------------------------------------------------------------
void BehaviourFixedCam::Parameters::Construct()
{
    Behaviour::Parameters::Construct();
    mType      = eBehaviourFixedCam;
    mfFOV      = KF_DEFAULT_FOV;
    mfMaxDutch = KF_DEFAULT_MAX_DUTCH;
}

// ----------------------------------------------------------------------------
// Construct @0x82229D20. Three inlined blocks and one store:
//   * Behaviour::Construct -- +0x04 = 0, +0x08..+0x0C = 0, +0x10 = 0;
//   * VisibilityCollisionPolicy::Construct over +0x20 -- the failed flag, the two predictors, the
//     visibility-test bytes (1/0/1 at policy +0x1A0..+0x1A2), -1.0 / 1.5 / 0.5 at +0x210 / +0x234 /
//     +0x238, mbCanFail / mbFirstFrame = 1, mbTargetSet = 0, the zero velocity, +0x23C = 0;
//   * the DWARF's VisibilityCollisionPolicy::SetTestLookingAt(true) -- the SECOND `stb 1` to policy
//     +0x1A0 after the Construct block;
//   * mpParameters = 0 (`stw 0, 0x2A0`).
// ----------------------------------------------------------------------------
void BehaviourFixedCam::Construct()
{
    Behaviour::Construct();
    mCollisionPolicy.Construct();
    mCollisionPolicy.SetTestLookingAt(true);
    mpParameters = 0;
}

// ----------------------------------------------------------------------------
// Prepare @0x821FAD28: `stb 1, 8(r3)` (SetPrepared), `stb 0, 0x2A8(r3)` (mbPositionSet = false),
// `li r3, 1`. The next Update plants the shot afresh.
// ----------------------------------------------------------------------------
bool BehaviourFixedCam::Prepare(const BehaviourSharedPrepareReleaseInfo& /*lrInfo*/)
{
    SetPrepared();
    mbPositionSet = false;
    return true;
}

// ----------------------------------------------------------------------------
// Update @0x82229DE0. The subject is always the PLAYER (lrInfo.mPlayerInfo).
//   0x82229E00..0x82229E9C  the inlined VisibilityCollisionPolicy::SetTarget: the player's transform
//                           (info +0x250), its AABB (+0x500) and entity id (+0x428).
//   0x82229EA0              if (!mbPositionSet)  -- plant the shot once:
//     lOffset   = the player's linear velocity (+0x390), Y zeroed (`vrlimi128 v12, v0, 4` of v*0),
//                 times lTimeAhead (1.0);
//     |lOffset|^2 > 400     -> Fail(7) "Position too far from subject" (the shot is still planted);
//     not zero AND |lOffset|^2 < lMinDistance^2 -> NormalizeFast(lOffset) * lMinDistance
//     else if zero          -> the car's zAxis (+0x270) * lMinDistance;
//     mBaseTranform = CreateLookAt(car position (+0x280) + lOffset, car position);
//     mfDutch = random in [-mfMaxDutch, mfMaxDutch] (the inlined Random::RandomFloat on +0x5D4);
//     unless the tweaker is attached (+0x0A), roll mBaseTranform by mfDutch degrees about Z
//     (the inlined Matrix44AffineFromZRotationAngle + Mult, 0x8222A110..0x8222A318);
//     mbPositionSet = true.
//   0x8222A320  not failed -> camera state flag bit 1.
//   0x8222A338  the policy's visibility interrupted -> "NoCutTo: Subject left frame" (bit 16 of the
//               camera's account) and mbCanSwitchToMeNow = 0 -- the inlined
//               Behaviour::SetCantSwitchToMeNow the DWARF lists (no assert in the console's copy).
//   0x8222A384  Camera::SetFOV(mfFOV) (its "lfFOV > 0.0f" assert, Camera.h:424), the transform,
//               Camera::ValidateTransformWithDebugInfo.
//   0x8222A3F8  tweaker attached -> the camera shows mBaseTranform rolled by mfMaxDutch instead.
//   return true.
// ----------------------------------------------------------------------------
bool BehaviourFixedCam::Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo)
{
    using namespace rw::math::vpu;

    const BrnPhysics::Vehicle::RaceCarState& lrTarget = lrInfo.mPlayerInfo.mRaceCarState;

    mCollisionPolicy.SetTarget(lrTarget.mTransform, lrInfo.mPlayerInfo.mAABB,
                               CgsSceneManager::EntityId(lrTarget.mEntityId.muValue));

    if (!mbPositionSet)
    {
        Vector3 lOffset = lrTarget.mLinearVelocity;
        lOffset.y *= 0.0f;
        lOffset = lOffset * KF_TIME_AHEAD;

        if (MagnitudeSquared(lOffset) > KF_MAX_DISTANCE_SQUARED)
        {
            Fail(lrCamera, KI_FAILED_POSITION_TOO_FAR_FROM_SUBJECT);
        }

        if (!IsZero(lOffset, KF_ZERO_TOLERANCE) &&
            MagnitudeSquared(lOffset) < KF_MIN_DISTANCE * KF_MIN_DISTANCE)
        {
            lOffset = NormalizeFast(lOffset) * KF_MIN_DISTANCE;
        }
        else if (IsZero(lOffset, KF_ZERO_TOLERANCE))
        {
            lOffset = lrTarget.mTransform.zAxis * KF_MIN_DISTANCE;
        }

        const Vector3 lTargetPosition = lrTarget.mTransform.wAxis;
        mBaseTranform = Utils::CreateLookAt(lTargetPosition + lOffset, lTargetPosition);

        mfDutch = lrInfo.GetRandom()->RandomFloat(-mpParameters->mfMaxDutch, mpParameters->mfMaxDutch);

        if (!IsTweakerAttached())
        {
            mBaseTranform = Mult(MakeRotationZ(mfDutch * KF_DEGREES_TO_RADIANS), mBaseTranform);
        }

        mbPositionSet = true;
    }

    if (!HasFailed())
    {
        lrCamera.mState_uFlags |= KI_CAMERA_STATE_FLAG_BEHAVIOUR_LIVE;
    }

    if (mCollisionPolicy.IsVisibilityInterrupted())
    {
        SetCantSwitchToMeNow(lrCamera, KI_NOCUTTO_SUBJECT_LEFT_FRAME);
    }

    lrCamera.SetFOV(mpParameters->mfFOV);
    lrCamera.SetTransform(mBaseTranform);
    lrCamera.ValidateTransformWithDebugInfo();

    if (IsTweakerAttached())
    {
        lrCamera.mTransform = Mult(MakeRotationZ(mpParameters->mfMaxDutch * KF_DEGREES_TO_RADIANS),
                                   mBaseTranform);
    }

    return true;
}

// ----------------------------------------------------------------------------
// GetCollisionPolicy @0x821FB588: `addi r3, r3, 0x20 ; blr` (ICF-shared with BehaviourRig's).
// ----------------------------------------------------------------------------
CollisionPolicy* BehaviourFixedCam::GetCollisionPolicy()
{
    return &mCollisionPolicy;
}

// ----------------------------------------------------------------------------
// SetupTweaker @0x821FAD48. Tweaker::Construct, assert mpParameters (cpp:180), then two inlined
// constant-scale Tweaker::AddMapping calls, in this order:
//   "Roll" -> &mpParameters->mfMaxDutch, scale 0.5, into maAxisMapping[NORMAL][UPPER_TRIGGERS] (+0x8C)
//   "FOV"  -> &mpParameters->mfFOV,      scale 0.5, into maAxisMapping[NORMAL][LOWER_TRIGGERS] (+0x78)
// ("FOV" is the rodata at 0x820051C0, "Roll" the string after it.) The console tweaks the shared
// parameter block in place, through the const pointer.
// ----------------------------------------------------------------------------
void BehaviourFixedCam::SetupTweaker(Utils::Tweaker& lrTweaker)
{
    lrTweaker.Construct();

    CGS_ASSERT(mpParameters != 0, "mpParameters");   // cpp:180

    Parameters* lpParameters = const_cast<Parameters*>(mpParameters);
    lrTweaker.AddMapping("Roll", &lpParameters->mfMaxDutch, KF_TWEAK_SCALE,
                         Utils::Tweaker::E_AXIS_UPPER_TRIGGERS, Utils::Tweaker::E_MAP_NORMAL);
    lrTweaker.AddMapping("FOV", &lpParameters->mfFOV, KF_TWEAK_SCALE,
                         Utils::Tweaker::E_AXIS_LOWER_TRIGGERS, Utils::Tweaker::E_MAP_NORMAL);
}

// ----------------------------------------------------------------------------
// GetName @0x821FAE48.
// ----------------------------------------------------------------------------
const char* BehaviourFixedCam::GetName() const
{
    return "BehaviourFixedCam";
}

} // namespace Camera
} // namespace BrnDirector
