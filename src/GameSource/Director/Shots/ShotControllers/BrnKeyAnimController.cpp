// ============================================================================
// GameSource/Director/Shots/ShotControllers/BrnKeyAnimController.cpp
//
// BrnDirector::KeyAnimController -- the ICE take evaluator. THIS is the code that turns an
// authored ICE ("In-game Camera Editor") camera take into a camera picture; the retail game
// intro fly-by runs entirely through it.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//     Prepare                    @0x821F7E10   (cpp:62)
//     UpdateTransformationMatrix @0x8221E2C8   (cpp:98)   -- static (see the header)
//     UpdateFocus                @0x821F7F50   (cpp:154)  -- static
//     UpdateLens                 @0x821F8068   (cpp:200)  -- static
//     Update                     @0x8223D020   (cpp:224)
//     UpdateCameraFromICE        @0x8221E630   (cpp:288)  -- static
//     SetParametricTime0To1      @0x821F80F8   (cpp:331)
//     GetParametricTime0To1      @0x8220AD50   (cpp:357)
//     GetEyeSpace                @0x821F4138   (h:107)
//     GetLookSpace               @0x821F4190   (h:110)
//     GetLookPos                 @0x821F41E8   (h:113)
//     HasFinished                @0x821F4258   (h:155)
//
// Every floating-point literal below was read out of the X360 image itself (the decompiled
// float/vector pools at flt_82001CC0 / flt_82001C98 / flt_82001C94 / flt_82001748 /
// flt_82002138 / flt_82001B04 / flt_82001B08 / dbl_82004640 / flt_82CDA5E0), not guessed.
//
// The `fsel`-based Min/Max/Clamp idioms the console emits are written here as the
// rw::math::fpu Min/Max/Clamp the rest of the camera family already uses -- the same
// branchless semantics (they differ from `fsel` only for NaN inputs, which every one of
// these lanes has already been asserted / clamped out of).
// ============================================================================

#include "GameSource/Director/Shots/ShotControllers/BrnKeyAnimController.h"

// BrnDirectorResourceManager.h's header-inline GetKeyAnim / GetShakeTakes bodies dereference
// the ICE wrapper, so its home has to be complete first (the same include pair every other
// consumer of the manager uses -- see DirectorLinkStubs.cpp:49-50).
#include "GameSource/Director/BrnDirectorICEWrapper.h"
#include "GameSource/Director/BrnDirectorResourceManager.h"    // DirectorResourceManager::GetKeyAnimFromGuid / GetIceResourceManager
#include "GameSource/Director/Camera/Camera.h"                 // BrnDirector::Camera::Camera
#include "GameSource/Director/Camera/Utils/CameraUtils.h"      // Utils::CreateLookAt
#include "GameSource/Director/Utils/BrnDirectorTimestep.h"     // BrnDirector::Timestep
#include "SDKs/Packages/ICE/ICECameraSpaceHandler.hpp"         // ICE::CameraSpaceHandler::TransformToWorld
#include "rw/math/vpu/vector3_operation.h"                     // Vector3 operator* / operator+ / operator-
#include "rw/math/fpu/scalar_operation.h"                      // rw::math::fpu Min / Max / Clamp / Sin / Cos
#include "GameShared/GameClasses/Development/Log/CgsLog.h"     // [DIAG] gpDebugPrint (bring-up only)
#include <cmath>                                               // atan2
#include <cstdlib>                                             // getenv ([DIAG] BRN_ICE_TRACE)

namespace BrnDirector
{

// [DIAG BRN_ICE_TRACE] BRING-UP SCAFFOLDING, NOT CONSOLE CODE. Remove with the bring-up path.
bool KeyAnimController::sbIceTrace = false;

// ----------------------------------------------------------------------------
// File-scope constants.
// ----------------------------------------------------------------------------

// The source path string the asserts report.
static const char* const KPC_SOURCE_FILE =
    "..\\..\\..\\GameSource\\Director/Shots/ShotControllers/BrnKeyAnimController.cpp";

// The two depth-of-field falloff constants the DWARF names at BrnKeyAnimController.cpp:20/:21
// (`KI_DEPTH_OF_FIELD_MAX_FALLOFF_DISTANCE` / `KI_DEPTH_OF_FIELD_MIN_FALLOFF_DISTANCE`).
// VALUES are X360-attested: flt_82001B04 == 50.0f (the metres the authored BLUR_FALLOFF
// percentage scales into) and flt_82001B08 == 0.001f (the minimum separation kept between
// two adjacent focus-band edges). FLAG: which DWARF name goes with which value is INFERRED
// from their roles -- the 50 m span is the MAX falloff distance, the 1 mm epsilon the MIN.
static const f32 KI_DEPTH_OF_FIELD_MAX_FALLOFF_DISTANCE = 50.0f;      // flt_82001B04
static const f32 KI_DEPTH_OF_FIELD_MIN_FALLOFF_DISTANCE = 0.001f;     // flt_82001B08

// ICE authors the DUTCH (roll) element in TURNS, over [-0.25, +0.25]; the camera wants
// radians. flt_82001C94 == 6.2831855f.
static const f32 KF_TURNS_TO_RADIANS = 6.2831855f;

// flt_82001748. Radians -> degrees (the camera FOV is in degrees).
static const f32 KF_RADIANS_TO_DEGREES = 57.29578f;

// dbl_82004640. HALF the camera aperture in millimetres: FOV/2 = atan(aperture/2 / lens).
// The console evaluates this arm in DOUBLE precision (lfd + atan2 + frsp), reproduced here.
static const f64 KF_HALF_APERTURE_MM = 15.96000051498413;

// flt_82002138. Several ICE elements are authored as percentages (0..100) and consumed as
// unit scalars.
static const f32 KF_PERCENT_TO_UNIT = 0.01f;

// flt_82CDA5E0 -- the letterbox / race-end effect amount raised whenever the take authors a
// non-zero LETTERBOX. Read out of the X360 .data image: 0.15f. (NOT 1.0f: four other
// reconstructions in this tree carry `flt_82CDA5E0` as a placeholder 1.0f -- see the note in
// the report; this one is measured.)
static const f32 KF_LETTERBOX_EFFECT_AMOUNT = 0.15f;

// The two camera-state flags UpdateCameraFromICE / UpdateTransformationMatrix drive. The
// console ORs/ANDs the raw bit into the current-flag qword at camera +0x140; the indices are
// those bits (0x40 == 1<<6, 0x2000 == 1<<13). FLAG: the flag NAMES are inferred from what
// raises them -- a take whose sub-take changed this frame is a hard CUT, and an eye/look
// point authored in eICE_SCENE_SPACE is a world-anchored ("scene") shot.
static const u32 KU_CAMERA_FLAG_CUT         = 6;    // camera +0x140 bit 0x40
static const u32 KU_CAMERA_FLAG_SCENE_SPACE = 13;   // camera +0x140 bit 0x2000

// ----------------------------------------------------------------------------
// The ICE element indices this controller samples. Indices into the engine's
// ICE::ICEElementDescriptions[48] table (SDKs/Packages/ICE/ICEData.cpp); the names are that
// table's own `mpTag` strings.
// ----------------------------------------------------------------------------
enum EICEElement
{
    E_ICE_EYE_X               = 0,
    E_ICE_EYE_Y               = 1,
    E_ICE_EYE_Z               = 2,
    E_ICE_LOOK_X              = 3,
    E_ICE_LOOK_Y              = 4,
    E_ICE_LOOK_Z              = 5,
    E_ICE_DUTCH               = 6,
    E_ICE_LENS_LENGTH         = 9,
    E_ICE_CAMERA_BLEND_AMOUNT = 10,
    E_ICE_CAMERA_LAG_AMOUNT   = 11,
    E_ICE_NEAR_FOCUS          = 12,
    E_ICE_FAR_FOCUS           = 13,
    E_ICE_BLUR_FALLOFF        = 14,
    E_ICE_BLUR_INTENSITY      = 15,
    E_ICE_SHAKE_AMPLITUDE     = 17,
    E_ICE_SHAKE_FREQUENCY     = 18,
    E_ICE_TIME_SCALE          = 19,
    E_ICE_LETTERBOX           = 20,
    E_ICE_SPACE_EYE           = 30,
    E_ICE_SPACE_LOOK          = 31,
    E_ICE_BLEND_CURVE         = 36,
    E_ICE_INTERPOLATE_TYPE    = 37,
    E_ICE_RAWFOCUS_OVERRIDE   = 39,
    E_ICE_SHAKE_TYPE          = 40,
    E_ICE_POSTFX_HOOK         = 44
};

// ============================================================================
// Prepare @0x821F7E10  (cpp:62)
// ----------------------------------------------------------------------------
// Bind the controller to the take named by liAnimGuid. Always returns true.
// ============================================================================
bool KeyAnimController::Prepare(const DirectorResourceManager& lrResourceManager, s32 liAnimGuid)
{
    // The take evaluator needs a take-data provider to resolve sub-takes through.
    mPlaybackTake.Construct(lrResourceManager.GetIceResourceManager());

    // Resolve the take data. The console INLINES the manager's own two-step resolve here
    // (`ICEAuthor::FindEditedTakeFromGuid(*(rm+560) + 10064, guid)`, i.e. the editor copy
    // inside the ICE WRAPPER at rm+560, falling back to
    // `ICEList::GetICETakeDataFromGuid(*(rm+544), guid)`); that pair IS
    // DirectorResourceManager::GetKeyAnimFromGuid @0x821F69A8, called by name here.
    ICE::ICETakeData* lpTakeData = lrResourceManager.GetKeyAnimFromGuid(liAnimGuid);

    CGS_ASSERT(lpTakeData != 0, "lpTakeData != NULL");                       // cpp:68

    // [DIAG BRN_ICE_TRACE] BRING-UP SCAFFOLDING, NOT CONSOLE CODE. Which take guid the shot
    // reference decoded to, and what the ICE list handed back for it. Remove with the
    // bring-up path.
    if (getenv("BRN_ICE_TRACE") != 0 && CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint
            << "[ice] Prepare guid " << liAnimGuid
            << " -> takeData " << (lpTakeData != 0 ? 1 : 0)
            << " len " << (lpTakeData != 0 ? lpTakeData->GetLength() : 0.0f) << "\n";
    }

    // [DIAG -- BRING-UP SCAFFOLDING, NOT CONSOLE CODE, capped at 8 lines.] Names the take
    // whose SetParameter below trips the ICEData.cpp:1851 key-bounds assert (12 of the 15
    // whole-run asserts as of 2026-08-02). Paired with the [ice-bounds] line in
    // ICETake::GetValue, this identifies the take AND the offending element/channel in one
    // run. Remove with the ICE-camera campaign.
    {
        static s32 s_iPrepareReports = 0;
        if (s_iPrepareReports < 8 && CgsDev::Log::gpDebugPrint != 0)
        {
            ++s_iPrepareReports;
            *CgsDev::Log::gpDebugPrint
                << "[ice-prepare] guid " << liAnimGuid
                << " takeData " << (lpTakeData != 0 ? 1 : 0)
                << " len " << (lpTakeData != 0 ? lpTakeData->GetLength() : 0.0f) << "\n";
        }
    }

    mPlaybackTake.SetDataPointers(lpTakeData, false);

    // Rewind to the start of the take, forcing the seek and taking the simple all-channel
    // path (no sub-take blend on the first frame).
    mPlaybackTake.SetParameter(0.0f, true, true);

    mEyeSpace  = static_cast<ICE::eICESpace>(mPlaybackTake.GetValueInt(E_ICE_SPACE_EYE));
    mLookSpace = static_cast<ICE::eICESpace>(mPlaybackTake.GetValueInt(E_ICE_SPACE_LOOK));

    mbPrepared = true;

    // ⚠️ REPRODUCED AS-IS: Prepare seeds mLookPos from the EYE elements (0/1/2), where Update
    // fills it from the LOOK elements (3/4/5). That reads like a copy-paste in the original,
    // but it is what the X360 does (the three GetValueFloat calls at 0x821F7EE8/0x821F7EF8/
    // 0x821F7F08 pass 2, 1 and 0). It is harmless in practice -- the first Update overwrites
    // it before anything reads it. Do not "fix" it.
    mLookPos.x = mPlaybackTake.GetValueFloat(E_ICE_EYE_X);
    mLookPos.y = mPlaybackTake.GetValueFloat(E_ICE_EYE_Y);
    mLookPos.z = mPlaybackTake.GetValueFloat(E_ICE_EYE_Z);
    mLookPos.w = 0.0f;

    return true;
}

// ============================================================================
// Update @0x8223D020  (cpp:224)
// ----------------------------------------------------------------------------
// Advance the take by this frame's GAME timestep and write the camera out of it.
// ============================================================================
void KeyAnimController::Update(const ShotContext& lrContext, Camera::Camera* lpCamera)
{
    CGS_ASSERT(mbPrepared, "mbPrepared");                                     // cpp:226

    if (!mbPaused)
    {
        // The take plays on GAME time (the slow-mo-scaled flavour): the console reads
        // mafTimestep[2] off the context's Timestep, which is E_GAME.
        const f32 lfTimeStep = lrContext.mpTimestep->Get(Timestep::E_GAME);

        mfPlaybackTimer = mbReversed ? (mfPlaybackTimer - lfTimeStep)
                                     : (lfTimeStep + mfPlaybackTimer);

        if (mbIsLooping)
        {
            // Wrap the timer back into [0, length) -- fmod by truncation, then one length
            // added back when the reverse direction has taken it negative.
            const f32 lfLength = GetLength();
            const s32 liWholeLoops = static_cast<s32>(mfPlaybackTimer / lfLength);
            mfPlaybackTimer = mfPlaybackTimer - (static_cast<f32>(liWholeLoops) * lfLength);

            if (mfPlaybackTimer < 0.0f)
                mfPlaybackTimer = mfPlaybackTimer + GetLength();
        }

        mfPlaybackTimer = rw::math::fpu::Clamp(mfPlaybackTimer, 0.0f, GetLength());
    }

    // The camera's running time IS the take's playback timer.
    lpCamera->mfRunningTime = mfPlaybackTimer;

    // Seek the take. The parameter is the played fraction, capped at 1.0 (the console does
    // NOT floor it here -- the clamp above already guarantees it is non-negative).
    const f32 lfParameter = rw::math::fpu::Min(mfPlaybackTimer / GetLength(), 1.0f);
    mPlaybackTake.SetParameter(lfParameter, false, false);

    // [DIAG BRN_ICE_TRACE] BRING-UP SCAFFOLDING, NOT CONSOLE CODE. One line per sampled frame
    // per take, keyed by the take LENGTH (the three game-intro movies are 40.02 / 6.38 / 1.36 s,
    // so the length identifies which behaviour is speaking). The whole point is the pair
    // (dtGame, timer): if the GAME timestep is zero the take can never advance and every
    // element stays at whatever the seek in Prepare left it, which looks exactly like "the take
    // evaluates to nothing".
    {
        // ⚠️ THE SAMPLE WINDOW IS A BURST, NOT A STRIDE. This counter counts CALLS, and there
        // are N live ICE behaviours per frame, so `% 60` samples call 0, 60, 120 ... -- with
        // N == 2 or 3 that is ALWAYS THE SAME BEHAVIOUR and the other takes are invisible.
        // (That aliasing cost this wave a whole build/boot cycle: the 40 s game-intro take
        // looked as if it never ran.) A short burst catches every live behaviour in one frame.
        static s32 siTraceFrame = 0;
        sbIceTrace = (getenv("BRN_ICE_TRACE") != 0) && ((siTraceFrame % 300) < 4);
        if (sbIceTrace && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[ice] f" << siTraceFrame
                << " len " << GetLength()
                << " dtGame " << lrContext.mpTimestep->Get(Timestep::E_GAME)
                << " timer " << mfPlaybackTimer
                << " param " << lfParameter
                << " asmIntervals " << static_cast<s32>(mPlaybackTake.GetNumIntervals(10))
                << " ch0Intervals " << static_cast<s32>(mPlaybackTake.GetNumIntervals(0))
                << " ch0Keys " << static_cast<s32>(mPlaybackTake.GetNumKeys(0))
                // elements 47 CONTAINS_SUBTAKE / 46 TAKE_NUMBER -- the assembly track's own
                // sub-take gate and guid (see ICEData.cpp's public SetParameter).
                << " hasSub " << mPlaybackTake.GetValueInt(47)
                << " subGuid " << mPlaybackTake.GetValueInt(46)
                << " subParam " << mPlaybackTake.GetSubParameter() << "\n";
        }
        ++siTraceFrame;
    }

    // Evaluate the whole camera out of the take.
    UpdateCameraFromICE(mPlaybackTake, *lrContext.mpCameraSpaceHandler, lpCamera);

    // Re-cache the reference spaces + the authored look point for this frame's consumers
    // (BehaviourIceAnim::Update reads all three straight after calling us).
    mEyeSpace  = static_cast<ICE::eICESpace>(mPlaybackTake.GetValueInt(E_ICE_SPACE_EYE));
    mLookSpace = static_cast<ICE::eICESpace>(mPlaybackTake.GetValueInt(E_ICE_SPACE_LOOK));

    mLookPos.x = mPlaybackTake.GetValueFloat(E_ICE_LOOK_X);
    mLookPos.y = mPlaybackTake.GetValueFloat(E_ICE_LOOK_Y);
    mLookPos.z = mPlaybackTake.GetValueFloat(E_ICE_LOOK_Z);
    mLookPos.w = 0.0f;
}

// ============================================================================
// UpdateCameraFromICE @0x8221E630  (cpp:288)  [static]
// ----------------------------------------------------------------------------
// Sample every camera-facing element of the take into the camera: transform, lens, focus,
// then the effects block.
// ============================================================================
void KeyAnimController::UpdateCameraFromICE(const ICE::ICETake& lrTake,
                                            const ICE::CameraSpaceHandler& lrSpaces,
                                            Camera::Camera* lpCamera)
{
    UpdateTransformationMatrix(lrTake, lrSpaces, lpCamera);
    UpdateLens(lrTake, lpCamera);
    UpdateFocus(lrTake, lpCamera);

    Camera::CameraEffects& lrEffects = lpCamera->GetEffects();

    // TIME_SCALE is authored as a percentage of normal speed.
    lrEffects.mfSimTimeScale = lrTake.GetValueFloat(E_ICE_TIME_SCALE) * KF_PERCENT_TO_UNIT;

    // The shake request triple.
    lrEffects.mu8ShakeType     = static_cast<u8>(lrTake.GetValueInt(E_ICE_SHAKE_TYPE));
    lrEffects.mfShakeFrequency = lrTake.GetValueFloat(E_ICE_SHAKE_FREQUENCY);
    lrEffects.mfShakeAmplitude = lrTake.GetValueFloat(E_ICE_SHAKE_AMPLITUDE);

    // LETTERBOX is a gate, not an amount: any positive authored value raises the fixed
    // effect amount, anything else clears it.
    const f32 lfLetterbox = lrTake.GetValueFloat(E_ICE_LETTERBOX);
    lrEffects.mfRaceEndEffectAmount = (lfLetterbox > 0.0f) ? KF_LETTERBOX_EFFECT_AMOUNT : 0.0f;

    // A take that swapped to a new sub-take this frame is a hard CUT.
    lpCamera->GetState().SetFlag(KU_CAMERA_FLAG_CUT, lrTake.IsNewSubTakeThisFrame());

    // The game-camera blend / lag request pair (both authored as percentages) and the two
    // blend-shape selectors that go with them.
    lrEffects.mfGameCameraBlend  = lrTake.GetValueFloat(E_ICE_CAMERA_BLEND_AMOUNT) * KF_PERCENT_TO_UNIT;
    lrEffects.mu8BlendCurve      = static_cast<u8>(lrTake.GetValueInt(E_ICE_BLEND_CURVE));
    lrEffects.mu8InterpolateType = static_cast<u8>(lrTake.GetValueInt(E_ICE_INTERPOLATE_TYPE));
    lrEffects.mfCameraLag        = lrTake.GetValueFloat(E_ICE_CAMERA_LAG_AMOUNT) * KF_PERCENT_TO_UNIT;

    // The authored post-FX hook id.
    lrEffects.muRequestedPostFxId = static_cast<u32>(lrTake.GetValueInt(E_ICE_POSTFX_HOOK));
}

// ============================================================================
// UpdateTransformationMatrix @0x8221E2C8  (cpp:98)  [static]
// ----------------------------------------------------------------------------
// Project the take's authored eye/look points out of their reference spaces into world
// space, build the look-at frame, roll it by the authored dutch angle, and publish the look
// point as the camera's subject.
// ============================================================================
void KeyAnimController::UpdateTransformationMatrix(const ICE::ICETake& lrTake,
                                                   const ICE::CameraSpaceHandler& lrSpaces,
                                                   Camera::Camera* lpCamera)
{
    rw::math::vpu::Vector3 lEyePosition;
    lEyePosition.x = lrTake.GetValueFloat(E_ICE_EYE_X);
    lEyePosition.y = lrTake.GetValueFloat(E_ICE_EYE_Y);
    lEyePosition.z = lrTake.GetValueFloat(E_ICE_EYE_Z);
    lEyePosition.w = 0.0f;

    rw::math::vpu::Vector3 lLookPosition;
    lLookPosition.x = lrTake.GetValueFloat(E_ICE_LOOK_X);
    lLookPosition.y = lrTake.GetValueFloat(E_ICE_LOOK_Y);
    lLookPosition.z = lrTake.GetValueFloat(E_ICE_LOOK_Z);
    lLookPosition.w = 0.0f;

    const ICE::eICESpace leEyeSpace =
        static_cast<ICE::eICESpace>(lrTake.GetValueInt(E_ICE_SPACE_EYE));
    const rw::math::vpu::Vector3 lWorldEyePosition =
        lrSpaces.TransformToWorld(lEyePosition, leEyeSpace);

    const ICE::eICESpace leLookSpace =
        static_cast<ICE::eICESpace>(lrTake.GetValueInt(E_ICE_SPACE_LOOK));
    const rw::math::vpu::Vector3 lWorldLookPosition =
        lrSpaces.TransformToWorld(lLookPosition, leLookSpace);

    // A shot authored against the SCENE space is a world-anchored shot.
    lpCamera->GetState().SetFlag(KU_CAMERA_FLAG_SCENE_SPACE,
                                 leEyeSpace == ICE::eICE_SCENE_SPACE
                                 || leLookSpace == ICE::eICE_SCENE_SPACE);

    const rw::math::vpu::Matrix44Affine lLookAt =
        Camera::Utils::CreateLookAt(lWorldEyePosition, lWorldLookPosition);

    // The dutch (roll) angle, authored in turns over [-0.25, +0.25].
    //
    // The console does NOT call sin/cos here: it inlines the X360 XMVectorSinCos pair --
    // range-reduce the angle by 2*pi against the {pi, 2*pi, 1/pi, 1/(2*pi)} block at
    // unk_82000C60, then two 12-term minimax polynomials whose coefficient blocks
    // (unk_82000BD0.. = {1, -1/6, 1/120, -1/5040, ...} and unk_82000C00.. =
    // {1, -1/2, 1/24, -1/720, ...}) are exactly the odd sine and even cosine series. Read
    // out of the image and confirmed term by term; de-optimised here to the scalar pair.
    const f32 lfDutchRadians = lrTake.GetValueFloat(E_ICE_DUTCH) * KF_TURNS_TO_RADIANS;
    const f32 lfSin = rw::math::fpu::Sin(lfDutchRadians);
    const f32 lfCos = rw::math::fpu::Cos(lfDutchRadians);

    // Roll the look-at frame about its own at-axis:
    //   right' = right*cos + up*sin        up' = up*cos - right*sin
    // (the at/position rows pass through untouched).
    rw::math::vpu::Matrix44Affine& lrTransform = lpCamera->mTransform;
    lrTransform.zAxis = lLookAt.zAxis;
    lrTransform.wAxis = lLookAt.wAxis;
    lrTransform.xAxis = lLookAt.xAxis * lfCos + lLookAt.yAxis * lfSin;
    lrTransform.yAxis = lLookAt.yAxis * lfCos - lLookAt.xAxis * lfSin;

    lpCamera->ValidateTransformWithDebugInfo();

    // The take's look point IS the camera's subject.
    lpCamera->mbHasSubject = true;
    lpCamera->mSubject = lWorldLookPosition;

    // [DIAG BRN_ICE_TRACE] BRING-UP SCAFFOLDING, NOT CONSOLE CODE. What the authored take
    // actually asks for, and what the reference-space projection makes of it. This is the
    // measurement that separates "the take evaluates to nothing" from "the space it is
    // authored against projects to nothing". Off unless the env var is set; remove with the
    // bring-up path.
    if (KeyAnimController::sbIceTrace && CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint
            << "  eyeSpace " << static_cast<s32>(leEyeSpace)
            << " lookSpace " << static_cast<s32>(leLookSpace)
            << " rawEye (" << lEyePosition.x << ", " << lEyePosition.y
            << ", " << lEyePosition.z << ")"
            << " -> world (" << lWorldEyePosition.x << ", " << lWorldEyePosition.y
            << ", " << lWorldEyePosition.z << ")"
            << " rawLook (" << lLookPosition.x << ", " << lLookPosition.y
            << ", " << lLookPosition.z << ")"
            << " -> world (" << lWorldLookPosition.x << ", " << lWorldLookPosition.y
            << ", " << lWorldLookPosition.z << ")\n";
    }
}

// ============================================================================
// UpdateLens @0x821F8068  (cpp:200)  [static]
// ----------------------------------------------------------------------------
// Turn the take's authored lens length (millimetres) into the camera's FOV (degrees).
// ============================================================================
void KeyAnimController::UpdateLens(const ICE::ICETake& lrTake, Camera::Camera* lpCamera)
{
    const f32 lfLensLength = lrTake.GetValueFloat(E_ICE_LENS_LENGTH);

    // Half-angle from the half-aperture over the focal length, in degrees. (The FOV > 0
    // tripwire the console emits here belongs to Camera::SetFOV -- its assert cites
    // Camera.h:424, not this file -- so it comes along with the named call.)
    const f32 lfFOV =
        static_cast<f32>(std::atan2(KF_HALF_APERTURE_MM, static_cast<f64>(lfLensLength)))
        * KF_RADIANS_TO_DEGREES;

    lpCamera->SetFOV(lfFOV);
}

// ============================================================================
// UpdateFocus @0x821F7F50  (cpp:154)  [static]
// ----------------------------------------------------------------------------
// Build the camera's depth-of-field band out of the take's raw-focus channel. A take that
// does not override raw focus (or authors no blur) hands the camera an all-zero band.
// ============================================================================
void KeyAnimController::UpdateFocus(const ICE::ICETake& lrTake, Camera::Camera* lpCamera)
{
    f32 lfFocusStartDistanceMeters        = 0.0f;
    f32 lfPerfectFocusStartDistanceMeters = 0.0f;
    f32 lfPerfectFocusEndDistanceMeters   = 0.0f;
    f32 lfFocusEndDistanceMeters          = 0.0f;
    f32 lfBlurriness                      = 0.0f;

    if (lrTake.GetValueInt(E_ICE_RAWFOCUS_OVERRIDE) != 0)
    {
        const f32 lfBlurIntensity = lrTake.GetValueFloat(E_ICE_BLUR_INTENSITY);
        if (lfBlurIntensity > 0.0f)
        {
            const f32 lfBlurFalloff = lrTake.GetValueFloat(E_ICE_BLUR_FALLOFF);
            const f32 lfNearFocus   = lrTake.GetValueFloat(E_ICE_NEAR_FOCUS);
            const f32 lfFarFocus    = lrTake.GetValueFloat(E_ICE_FAR_FOCUS);

            // The authored falloff percentage becomes a distance either side of the
            // perfectly-focused band.
            const f32 lfFalloffMeters = lfBlurFalloff * KI_DEPTH_OF_FIELD_MAX_FALLOFF_DISTANCE;

            lfBlurriness = rw::math::fpu::Min(lfBlurIntensity, 1.0f);

            // Then force the four band edges into a strictly increasing order, keeping at
            // least the minimum separation between each adjacent pair.
            lfFocusStartDistanceMeters =
                rw::math::fpu::Max(lfNearFocus - lfFalloffMeters, 0.0f);
            lfPerfectFocusStartDistanceMeters =
                rw::math::fpu::Max(lfFocusStartDistanceMeters + KI_DEPTH_OF_FIELD_MIN_FALLOFF_DISTANCE,
                                   lfNearFocus);
            lfPerfectFocusEndDistanceMeters =
                rw::math::fpu::Max(lfPerfectFocusStartDistanceMeters, lfFarFocus);
            lfFocusEndDistanceMeters =
                rw::math::fpu::Max(lfPerfectFocusEndDistanceMeters + KI_DEPTH_OF_FIELD_MIN_FALLOFF_DISTANCE,
                                   lfFarFocus + lfFalloffMeters);
        }
    }

    lpCamera->GetDepthOfField().SetParams(lfFocusStartDistanceMeters,
                                          lfPerfectFocusStartDistanceMeters,
                                          lfPerfectFocusEndDistanceMeters,
                                          lfFocusEndDistanceMeters,
                                          lfBlurriness);
}

// ============================================================================
// SetParametricTime0To1 @0x821F80F8  (cpp:331)
// ============================================================================
void KeyAnimController::SetParametricTime0To1(f32 lfParametricTime0To1)
{
    CGS_ASSERT(mbPrepared, "mbPrepared");                                                // cpp:333
    CGS_ASSERT(lfParametricTime0To1 >= 0.0f && lfParametricTime0To1 <= 1.0f,
               "lfParametricTime0To1 >= 0.0f && lfParametricTime0To1 <= 1.0f");          // cpp:334
    CGS_ASSERT(GetLength() > 0.0f, "GetLength() > 0.0f");                                // cpp:335

    mfPlaybackTimer = mbReversed ? ((1.0f - lfParametricTime0To1) * GetLength())
                                 : (GetLength() * lfParametricTime0To1);
}

// ============================================================================
// GetParametricTime0To1 @0x8220AD50  (cpp:357)
// ============================================================================
f32 KeyAnimController::GetParametricTime0To1() const
{
    CGS_ASSERT(GetLength() > 0.0f, "GetLength() > 0.0f");                                // cpp:361

    const f32 lfPlayed = mfPlaybackTimer / GetLength();
    const f32 lfParametricTime0To1 = mbReversed ? (1.0f - lfPlayed) : lfPlayed;

    return rw::math::fpu::Clamp(lfParametricTime0To1, 0.0f, 1.0f);
}

// ============================================================================
// GetEyeSpace @0x821F4138  (h:107)
// ============================================================================
ICE::eICESpace KeyAnimController::GetEyeSpace() const
{
    CGS_ASSERT(mbPrepared, "mbPrepared");
    return mEyeSpace;
}

// ============================================================================
// GetLookSpace @0x821F4190  (h:110)
// ============================================================================
ICE::eICESpace KeyAnimController::GetLookSpace() const
{
    CGS_ASSERT(mbPrepared, "mbPrepared");
    return mLookSpace;
}

// ============================================================================
// GetLookPos @0x821F41E8  (h:113)
// ----------------------------------------------------------------------------
// Returned BY VALUE -- the console copies the 16-byte lane into the caller's sret slot.
// ============================================================================
rw::math::vpu::Vector3 KeyAnimController::GetLookPos() const
{
    CGS_ASSERT(mbPrepared, "mbPrepared");
    return mLookPos;
}

// ============================================================================
// HasFinished @0x821F4258  (h:155)
// ============================================================================
bool KeyAnimController::HasFinished() const
{
    CGS_ASSERT(mbPrepared, "mbPrepared");

    if (mbReversed)
        return mfPlaybackTimer <= 0.0f;

    return mfPlaybackTimer >= GetLength();
}

} // namespace BrnDirector
