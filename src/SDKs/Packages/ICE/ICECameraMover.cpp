// ============================================================================
// SDKs/Packages/ICE/ICECameraMover.cpp
//
// Runtime bodies for ICE::ICECameraMover -- the system that drives the in-game (ICE)
// camera from a recorded camera take. The type layout (member NAMES/OFFSETS) lives in
// the home GameSource/Director/Camera/ICECameraMover.h; members are accessed BY NAME
// here. The eleven functions in this TU:
//
//   Construct                  build the mover from an anchor + camera + take, seed the
//                              cubic followers / transform / bungee state
//   UpdateFrameEnd             per-frame: while a take is bound, refresh transform /
//                              forward / lens / focus / hard-cuts / fade / overlay /
//                              bloom, then push the world->camera matrix to the camera
//   UpdateTransformationMatrix sample eye/look channels, transform through the anchor
//                              space, build the dutch-rolled look-at into mWorldToCamera
//   UpdateForwardVector        blend the camera forward toward the car forward (cubics)
//   UpdateLens                 lens length channel -> FOV angle -> camera FOV
//   UpdateFocus                depth-of-field channels -> camera DOF params
//   UpdateHardCuts             snap the bungee pos/forward to the car on a hard cut
//   UpdateFade                 fade level + fade-colour-mode channels -> camera fade
//   UpdateOverlay              overlay-id channel -> camera overlay / hide-overlay
//   UpdateBloom                bloom channel -> camera bloom threshold/intensity
//   UpdateSimTime              sim-time channel -> clamp -> mfSimTime + camera multiplier
//
// The per-element values come from the active ICE take via its named GetValueFloat /
// GetValueInt(channel) accessors (ICEData.hpp). The take inlines those reads as a
// value-type test + raw mValues[] load; the de-optimized form here calls the named
// accessors -- semantic parity by channel index, not by offset.
// ============================================================================

#include "GameSource/Director/Camera/ICECameraMover.h"   // the mover home (this type)

#include "SDKs/Packages/ICE/ICEMath.hpp"                 // ICE::ICEMath, ICE::Angle, Matrix4
#include "GameShared/GameClasses/Core/CgsAssert.h"       // CGS_ASSERT

namespace ICE
{

// ----------------------------------------------------------------------------
// Take channel indices the mover samples (per-element slots in the active take's
// decoded value table; same numbering the take's GetValue* accessors use).
// ----------------------------------------------------------------------------
namespace
{
    // Eye / look position elements (camera-space points the take animates).
    const s32 KI_CHANNEL_EYE_X        = 0;
    const s32 KI_CHANNEL_EYE_Y        = 1;
    const s32 KI_CHANNEL_EYE_Z        = 2;
    const s32 KI_CHANNEL_LOOK_X       = 3;
    const s32 KI_CHANNEL_LOOK_Y       = 4;
    const s32 KI_CHANNEL_LOOK_Z       = 5;
    const s32 KI_CHANNEL_DUTCH        = 6;   // camera roll (degrees)

    // Depth-of-field elements.
    const s32 KI_CHANNEL_FOCUS_PERFECT_START = 12;
    const s32 KI_CHANNEL_FOCUS_PERFECT_END   = 13;
    const s32 KI_CHANNEL_FOCUS_FALLOFF       = 14;
    const s32 KI_CHANNEL_FOCUS_BLURRINESS    = 15;

    // Lens / scalar elements.
    const s32 KI_CHANNEL_LENS_LENGTH  = 9;   // lens focal length -> FOV (mValues[9])
    // FLAG: UpdateBloom and UpdateFade both sample mValues[21] -- the same per-element
    // slot holds both the bloom amount and the fade level; kept as one shared channel
    // constant.
    const s32 KI_CHANNEL_POSTFX_LEVEL = 21;  // bloom amount / fade level (mValues[21])
    const s32 KI_CHANNEL_SIM_TIME     = 19;  // sim-time scale (mValues[19])

    // Integer-mode elements.
    const s32 KI_CHANNEL_SPACE        = 30;  // reference space for the eye point
    const s32 KI_CHANNEL_LOOK_SPACE   = 31;  // reference space for the look point
    const s32 KI_CHANNEL_OVERLAY      = 42;  // overlay id
    const s32 KI_CHANNEL_FOCUS_ENABLE = 39;  // depth-of-field enable
    const s32 KI_CHANNEL_FADE_COLOUR  = 43;  // fade tint mode

    // The hard-cut channel whose current interval the mover tracks.
    const s32 KI_CHANNEL_HARD_CUT     = 0;
    const s32 KI_ELEMENT_HARD_CUT     = 0;

    // Post-effect scaling constants. The percent channels (bloom amount, fade level,
    // sim-time scale) are authored 0..100 and mapped to the unit interval by the 0.01
    // factor.
    const f32 KF_PERCENT_TO_UNIT      = 0.01f;
    const f32 KF_BLOOM_THRESHOLD_BASE = 0.2f;
    const f32 KF_BLOOM_INTENSITY_RATE = 0.02f;
}

// ----------------------------------------------------------------------------
// ICE::ICECameraMover::Construct
//
// Build the mover from a freshly resolved camera take. liViewIndex is the view slot;
// lpCar is the anchor (the car/reference-space source, stored at mpCar); lpICECamera
// is the ICE camera this mover drives; lpTake is the active take. lpShakeGroup /
// lpResourceMgr are the take-pipeline context (not stored by this entry point).
//
// Seeds: the two cubic followers (mAccelOffset, mForward) to a rest/identity-ish
// state, mfSimTime to 1.0, the hard-cut interval to -1 (invalid), the bungee/old-tag
// state to 0, marks the camera overlay-enabled, and snaps the bungee position/forward
// to the anchor's current car-to-world (forward == zAxis, position == wAxis).
// ----------------------------------------------------------------------------
void ICECameraMover::Construct(s32 /*liViewIndex*/, ICECameraAnchor* lpCar,
                               ICECamera* lpICECamera, ICETake* lpTake,
                               ICEGroup* /*lpShakeGroup*/,
                               const IResourceManager* /*lpResourceMgr*/)
{
    mpCar       = lpCar;
    mpICECamera = lpICECamera;
    mpTake      = lpTake;

    // Per-frame hysteresis state: invalid hard-cut interval, no event tag / overlay,
    // full sim-time scale until the first UpdateSimTime.
    miHardCutInterval = -1;
    muOldTag          = 0;
    miOldOverlay      = 0;
    mfSimTime         = 1.0f;

    // Reset the two cubic followers to rest: two zeroed 132-byte blocks (each three
    // Cubic1D) with the "duration" lanes set to 1.0 -- the Cubic1D default state. The
    // two blocks differ only in the flags lane: mAccelOffset keeps flags=1 (the
    // Cubic1D default), while mForward is seeded flags=0 so its seek state machine
    // settles to "running" instead of restarting every frame.
    for (s32 liComponent = 0; liComponent < 3; ++liComponent)
    {
        mAccelOffset.maComponents[liComponent] = Cubic1D();
        mForward.maComponents[liComponent]     = Cubic1D();
        mForward.maComponents[liComponent].SetFlags(0);
    }

    // Mark the camera's overlay enabled for this take (clears the ICE camera's
    // hide-overlay flag).
    mpICECamera->SetHideOverlay(false);

    // Snap the bungee position / forward to the anchor's current car geometry so the
    // first frame has no spurious bungee delta.
    mBungeeCarPos = mpCar->GetGeometryPosition();
    mBungeeCarFwd = mpCar->GetForwardVector();
}

// ----------------------------------------------------------------------------
// ICE::ICECameraMover::Destruct
//
// Tear down the mover. No owned heap resources -- the cubic followers and the
// transform are plain members, and the anchor / camera / take are non-owning
// pointers. (The body is empty.)
// ----------------------------------------------------------------------------
void ICECameraMover::Destruct()
{
}

// ----------------------------------------------------------------------------
// ICE::ICECameraMover::UpdateSimTime
//
// Advance the per-frame sim-time scale from the take's sim-time channel: read the
// channel value, scale it to the unit interval, clamp to [0,1], store it in mfSimTime
// and push it into the ICE camera's sim-time multiplier. (Consumer: ICEWrapper::Update.)
// ----------------------------------------------------------------------------
void ICECameraMover::UpdateSimTime(f32 /*lfTimeStep*/)
{
    const f32 lfRaw = mpTake->GetValueFloat(KI_CHANNEL_SIM_TIME);

    // value * 0.01, clamped into [0,1].
    const f32 lfScaled = ICEMath::Clamp(lfRaw * KF_PERCENT_TO_UNIT, 0.0f, 1.0f);

    mfSimTime = lfScaled;
    mpICECamera->SetSimTimeMultiplier(lfScaled);
}

// ----------------------------------------------------------------------------
// ICE::ICECameraMover::UpdateBloom
//
// Drive the camera bloom from the take's bloom channel: read the bloom amount, derive
// a threshold (0.2 baseline biased down by the amount, clamped) and an intensity
// (amount-scaled), and push both into the ICE camera.
// ----------------------------------------------------------------------------
void ICECameraMover::UpdateBloom(f32 /*lfTimeStep*/)
{
    const f32 lfBloom = mpTake->GetValueFloat(KI_CHANNEL_POSTFX_LEVEL);

    // Threshold: bias the 0.2 baseline down by the bloom amount, clamp into [0,1],
    // then map back so a larger amount lowers the threshold.
    const f32 lfClamped   = ICEMath::Clamp(KF_BLOOM_THRESHOLD_BASE - lfBloom * KF_PERCENT_TO_UNIT,
                                           0.0f, 1.0f);
    const f32 lfThreshold = (lfClamped - KF_BLOOM_THRESHOLD_BASE) * -2.0f;

    // Intensity scales directly with the bloom amount.
    const f32 lfIntensity = lfBloom * KF_BLOOM_INTENSITY_RATE;

    mpICECamera->SetBloom(lfThreshold, lfIntensity);
}

// ----------------------------------------------------------------------------
// ICE::ICECameraMover::UpdateFade
//
// Drive the camera screen fade from two channels: the fade level (clamped to [0,1] for
// the alpha) and the fade-colour mode (which selects the tint). The mode picks black /
// white / red / green / blue; the alpha is the clamped fade level.
// ----------------------------------------------------------------------------
void ICECameraMover::UpdateFade(f32 /*lfTimeStep*/)
{
    // Fade alpha: value * 0.01, clamped into [0,1].
    const f32 lfFade = ICEMath::Clamp(mpTake->GetValueFloat(KI_CHANNEL_POSTFX_LEVEL) * KF_PERCENT_TO_UNIT,
                                      0.0f, 1.0f);

    f32 lfRed   = 0.0f;
    f32 lfGreen = 0.0f;
    f32 lfBlue  = 0.0f;

    const s32 liColourMode = mpTake->GetValueInt(KI_CHANNEL_FADE_COLOUR);
    switch (liColourMode)
    {
    case 0:   // black tint
        lfRed = 0.0f; lfGreen = 0.0f; lfBlue = 0.0f;
        break;
    case 1:   // white tint
        lfRed = 1.0f; lfGreen = 1.0f; lfBlue = 1.0f;
        break;
    case 2:   // red tint
        lfRed = 1.0f; lfGreen = 0.0f; lfBlue = 0.0f;
        break;
    case 3:   // green tint
        lfRed = 0.0f; lfGreen = 1.0f; lfBlue = 0.0f;
        break;
    case 4:   // blue tint
        lfRed = 0.0f; lfGreen = 0.0f; lfBlue = 1.0f;
        break;
    default:
        // Unknown mode: leave the fade untouched.
        return;
    }

    mpICECamera->SetFadeColor(lfRed, lfGreen, lfBlue, lfFade);
}

// ----------------------------------------------------------------------------
// ICE::ICECameraMover::UpdateFocus
//
// Drive the camera depth-of-field from the take's focus channels. If DOF is disabled
// (enable channel == 0) or the blurriness is non-positive, push a zeroed DOF (off).
// Otherwise compute the focus start/end distances from the perfect-focus window and
// the falloff, and push the five DOF params into the camera.
// ----------------------------------------------------------------------------
void ICECameraMover::UpdateFocus(f32 /*lfTimeStep*/)
{
    if (mpTake->GetValueInt(KI_CHANNEL_FOCUS_ENABLE) == 0)
    {
        mpICECamera->SetDepthOfField(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    const f32 lfBlurriness = mpTake->GetValueFloat(KI_CHANNEL_FOCUS_BLURRINESS);
    if (lfBlurriness <= 0.0f)
    {
        mpICECamera->SetDepthOfField(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    const f32 lfFalloff      = mpTake->GetValueFloat(KI_CHANNEL_FOCUS_FALLOFF);
    const f32 lfPerfectStart = mpTake->GetValueFloat(KI_CHANNEL_FOCUS_PERFECT_START);
    const f32 lfPerfectEnd   = mpTake->GetValueFloat(KI_CHANNEL_FOCUS_PERFECT_END);

    // Blurriness is clamped to >= 0; the falloff is widened by 50m and biased so the
    // focus window brackets the perfect-focus window. The +0.001 nudges keep the
    // start/end distances strictly ordered (a chain of select/max clamps).
    const f32 lfClampedBlur  = ICEMath::Max(lfBlurriness, 0.0f);
    const f32 lfFalloffMeters = lfFalloff * 50.0f;

    const f32 lfFocusStart = ICEMath::Max(lfPerfectStart - lfFalloffMeters, 0.0f);
    const f32 lfNearGap    = ICEMath::Max((lfFocusStart + 0.001f) - lfPerfectStart, 0.0f);
    const f32 lfPerfectEndOrdered = ICEMath::Max(lfPerfectEnd, lfPerfectStart + lfNearGap);
    const f32 lfFocusEnd   = ICEMath::Max((lfPerfectEndOrdered + 0.001f),
                                          lfPerfectEnd + lfFalloffMeters);

    mpICECamera->SetDepthOfField(lfClampedBlur, lfFocusStart, lfPerfectStart,
                                 lfPerfectEndOrdered, lfFocusEnd);
}

// ----------------------------------------------------------------------------
// ICE::ICECameraMover::UpdateOverlay
//
// Drive the camera overlay from the take's overlay-id channel. If the take parameter
// has run out (<= 0) the remembered overlay is cleared. A non-positive overlay id (or
// no change) hides the overlay or leaves it; a positive id in range (<= 2) sets it.
// miOldOverlay remembers the last id applied so a repeated id is a no-op.
// ----------------------------------------------------------------------------
void ICECameraMover::UpdateOverlay(f32 /*lfTimeStep*/)
{
    // When the take has finished playing, forget the remembered overlay.
    if (mpTake->GetParameter() <= 0.0f)
    {
        miOldOverlay = 0;
    }

    const s32 liOverlay = mpTake->GetValueInt(KI_CHANNEL_OVERLAY);

    if (liOverlay <= 0)
    {
        // id 0 hides the overlay; a negative id just records the (cleared) state.
        if (liOverlay == 0)
        {
            mpICECamera->SetHideOverlay(true);
        }
        miOldOverlay = liOverlay;
        return;
    }

    // A positive id that has not changed needs no work.
    if (liOverlay == miOldOverlay)
    {
        miOldOverlay = liOverlay;
        return;
    }

    // The overlay changed: clear the current one, then apply the new id when it is in
    // range (<= 2). Out-of-range ids only update the remembered value.
    mpICECamera->ClearOverlay();
    if (liOverlay <= 2)
    {
        mpICECamera->SetOverlay(liOverlay);
    }
    miOldOverlay = liOverlay;
}

// ----------------------------------------------------------------------------
// ICE::ICECameraMover::UpdateHardCuts
//
// Track the hard-cut channel's current interval; when it changes, check whether the
// new interval is a hard cut and, if so, snap the bungee position / forward to the
// car's current geometry (so the camera does not bungee across the cut).
// ----------------------------------------------------------------------------
void ICECameraMover::UpdateHardCuts(f32 /*lfTimeStep*/)
{
    const s32 liInterval = mpTake->GetCurrentInterval(KI_CHANNEL_HARD_CUT);
    if (liInterval == miHardCutInterval)
    {
        return;
    }

    miHardCutInterval = liInterval;

    if (mpTake->IsHardCut(KI_CHANNEL_HARD_CUT, KI_ELEMENT_HARD_CUT))
    {
        // Snap both bungee references to the car's current position: on a hard cut the
        // car position vector is copied into both the position and forward slots.
        const Vector3 lvCarPos = mpCar->GetGeometryPosition();
        mBungeeCarPos = lvCarPos;
        mBungeeCarFwd = lvCarPos;
    }
}

// ----------------------------------------------------------------------------
// ICE::ICECameraMover::UpdateLens
//
// Drive the camera field-of-view from the take's lens-length channel: convert the
// lens length to an FOV angle, then degrees, and push it into the camera. Asserts the
// computed FOV is positive (the camera requires a positive FOV).
// ----------------------------------------------------------------------------
void ICECameraMover::UpdateLens(f32 /*lfTimeStep*/)
{
    const f32 lfLensLength = mpTake->GetValueFloat(KI_CHANNEL_LENS_LENGTH);

    // Lens length -> FOV angle (the homed ICEMath conversion, which inlines an
    // ATan-based conversion) then map the resulting Angle to degrees.
    const Angle leFov = ICEMath::ConvertLensLengthToFovAngle(lfLensLength);
    const f32   lfFovDegrees = ICEMath::Angles::AngToDeg(leFov);

    CGS_ASSERT(lfFovDegrees > 0.0f, "lfFOV > 0.0f");

    mpICECamera->SetFieldOfView(lfFovDegrees);
}

// ----------------------------------------------------------------------------
// ICE::ICECameraMover::UpdateForwardVector
//
// Blend the camera forward toward the car forward. Reads the car forward, projects it,
// renormalises (flipping a back-facing result), then drives the three forward-follower
// cubics toward the new components over the frame's blended sim time.
// ----------------------------------------------------------------------------
void ICECameraMover::UpdateForwardVector(f32 lfTimeStep)
{
    // The car's current forward direction (zAxis of the car-to-world affine).
    Vector3 lvCarForward = mpCar->GetForwardVector();

    // Project / renormalise: a back-facing forward (negative self-dot) is flipped so the
    // follower targets a forward-facing direction.
    Vector3 lvTarget;
    lvTarget.SetZero();
    ICEMath::Normalize(&lvTarget, &lvCarForward);

    // Drive each forward-follower component toward the renormalised target. A component
    // whose new target differs from the follower's CURRENT value restarts that cubic
    // (state 2) -- the compare is against the live Val, not the previous target, so an
    // unchanged target still restarts a follower that is mid-seek. The followers advance
    // over the frame sim time (mfSimTime * lfTimeStep).
    f32* lpafTarget[3] = { &lvTarget.x, &lvTarget.y, &lvTarget.z };
    for (s32 liComponent = 0; liComponent < 3; ++liComponent)
    {
        Cubic1D& lrCubic = mForward.maComponents[liComponent];
        const f32 lfNewDesired = *lpafTarget[liComponent];
        if (lfNewDesired != lrCubic.Val)
        {
            lrCubic.SetState(2);
        }
        lrCubic.SetValDesired(lfNewDesired);
    }

    const f32 lfBlendTime = mfSimTime * lfTimeStep;
    for (s32 liComponent = 0; liComponent < 3; ++liComponent)
    {
        mForward.maComponents[liComponent].Update(lfBlendTime, 0.0f, 0.0f);
    }
}

// ----------------------------------------------------------------------------
// ICE::ICECameraMover::UpdateTransformationMatrix
//
// Build the world->camera transform for the frame. Sample the eye and look points from
// the take (three position channels each), transform both through the anchor's
// reference space (the per-point space is itself a take channel), apply the dutch roll
// (channel 6, degrees), and build the look-at matrix into mWorldToCamera.
//
// FLAG: this selects between two external look-at builders
// (BrnDirector::Camera::Utils::CreateLookAt and a sibling matrix builder) on the eye
// space id, and applies the dutch roll with an inline SIMD rotation. Neither external
// builder has a reconstructed home; this routes both through the mover's own homed
// CreateLookAtMatrix helper (declaration-only; its body lands with the follow-on
// ICECameraMover work), which is the in-family dutch-rolled look-at builder -- semantic
// parity by the same eye/look/dutch inputs.
// ----------------------------------------------------------------------------
void ICECameraMover::UpdateTransformationMatrix(f32 /*lfTimeStep*/)
{
    // Eye / look points in their authored spaces.
    Vector3 lvEye;
    lvEye.x = mpTake->GetValueFloat(KI_CHANNEL_EYE_X);
    lvEye.y = mpTake->GetValueFloat(KI_CHANNEL_EYE_Y);
    lvEye.z = mpTake->GetValueFloat(KI_CHANNEL_EYE_Z);
    lvEye.w = 0.0f;

    Vector3 lvLook;
    lvLook.x = mpTake->GetValueFloat(KI_CHANNEL_LOOK_X);
    lvLook.y = mpTake->GetValueFloat(KI_CHANNEL_LOOK_Y);
    lvLook.z = mpTake->GetValueFloat(KI_CHANNEL_LOOK_Z);
    lvLook.w = 0.0f;

    // Project the eye and look points into world space through the anchor's reference
    // spaces (each point carries its own space id channel).
    const CameraSpaceHandler& lrSpace = mpCar->GetSpace();

    const u8 lu8EyeSpace  = static_cast<u8>(mpTake->GetValueInt(KI_CHANNEL_SPACE));
    const u8 lu8LookSpace = static_cast<u8>(mpTake->GetValueInt(KI_CHANNEL_LOOK_SPACE));

    Vector3 lvWorldEye  = lrSpace.TransformToWorld(lvEye, static_cast<eICESpace>(lu8EyeSpace));
    Vector3 lvWorldLook = lrSpace.TransformToWorld(lvLook, static_cast<eICESpace>(lu8LookSpace));

    // Dutch roll: channel 6 in degrees, converted to the ICE angle unit.
    const f32   lfDutchDegrees = mpTake->GetValueFloat(KI_CHANNEL_DUTCH);
    const Angle leDutch        = ICEMath::Angles::DegToAng(lfDutchDegrees);

    // Build the world->camera look-at (with the dutch roll) into mWorldToCamera.
    CreateLookAtMatrix(&mWorldToCamera, lvWorldEye, lvWorldLook, leDutch);
}

// ----------------------------------------------------------------------------
// ICE::ICECameraMover::UpdateFrameEnd
//
// Finish the per-frame update. While a take is bound, refresh the whole take->camera
// pipeline (transform, forward, lens, focus, hard-cuts, fade, overlay, bloom) then push
// the built world->camera transform into the ICE camera. (Consumer: ICEWrapper::Update.)
// ----------------------------------------------------------------------------
void ICECameraMover::UpdateFrameEnd(f32 lfTimeStep)
{
    if (mpTake == 0)
    {
        return;
    }

    UpdateTransformationMatrix(lfTimeStep);
    UpdateForwardVector(lfTimeStep);
    UpdateLens(lfTimeStep);
    UpdateFocus(lfTimeStep);
    UpdateHardCuts(lfTimeStep);
    UpdateFade(lfTimeStep);
    UpdateOverlay(lfTimeStep);
    UpdateBloom(lfTimeStep);

    // Push the built world->camera transform into the ICE camera. mWorldToCamera is
    // ICE's Matrix4 (four 16-byte SIMD rows); the camera's SetCameraMatrix takes the
    // bit-identical RenderWare affine matrix -- bridge the two SIMD matrix vocabularies
    // (layout-identical 64-byte, 16-aligned), not an offset poke.
    mpICECamera->SetCameraMatrix(
        reinterpret_cast<const rw::math::vpu::Matrix44Affine&>(mWorldToCamera));
}

} // namespace ICE
