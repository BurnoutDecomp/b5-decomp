#ifndef GAMESOURCE_DIRECTOR_SHOTS_SHOTCONTROLLERS_BRN_KEY_ANIM_CONTROLLER_H
#define GAMESOURCE_DIRECTOR_SHOTS_SHOTCONTROLLERS_BRN_KEY_ANIM_CONTROLLER_H

#include "types.hpp"
#include "rw/math/vpu/types.h"                              // rw::math::vpu::Vector3 (mLookPos)
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT (the mbPrepared guards)
#include "SDKs/Packages/ICE/ICEData.hpp"                    // ICE::ICETake / ICETakeData
#include "SDKs/Packages/ICE/ICEDataEnums.hpp"               // ICE::eICESpace
#include "GameSource/Director/Shots/BrnShotController.h"    // BrnDirector::ShotController / ShotContext

// ============================================================================
// GameSource/Director/Shots/ShotControllers/BrnKeyAnimController.h
//
// BrnDirector::KeyAnimController -- THE ICE take evaluator. It is the shot controller that
// plays one recorded ICE ("In-game Camera Editor") camera take: it owns the live
// ICE::ICETake, advances a playback timer each frame, samples the take's 48 authored
// elements at the resulting parameter, and writes the whole camera out of them -- transform
// (eye/look points projected through the reference spaces, plus the dutch roll), lens (FOV
// from the authored lens length), focus (the depth-of-field band), and the effects block
// (time scale, shake, letterbox, blend/lag, post-FX hook).
//
// This is the code that turns an authored ICE take into a camera picture. The retail game
// intro's fly-by (BrnArbStateCarSelect -> BehaviourIceAnim -> this) runs entirely through it.
//
// ----------------------------------------------------------------------------
// LAYOUT (DecFIGS DWARF BrnKeyAnimController.h:45, member-for-member; the offsets are
// X360-attested by every body in BrnKeyAnimController.cpp):
//
//   +0x000  ShotController                  (base: vptr, 16-byte aligned for the Vector3)
//   +0x010  rw::math::vpu::Vector3 mLookPos
//   +0x020  ICE::ICETake           mPlaybackTake       (mpTakeData at +0x024 == take +0x04)
//   +0x758  ICE::eICESpace         mEyeSpace
//   +0x75C  ICE::eICESpace         mLookSpace
//   +0x760  f32                    mfPlaybackTimer
//   +0x764  bool                   mbIsLooping
//   +0x765  bool                   mbPrepared
//   +0x766  bool                   mbPaused
//   +0x767  bool                   mbReversed
//
// (The previous slice in this file modelled +0x024 as a "clip pointer" into an invented
// `KeyAnimClip` with a duration at +0x2C, and typed the two space selectors as opaque
// `Space*`. Both readings are retired: +0x024 is the embedded ICETake's own mpTakeData and
// +0x2C of THAT is ICETakeData::mfLength, which is exactly what GetLength() returns; the
// selectors are ICE::eICESpace enums, which is why BehaviourIceAnim switches on them by
// integer value.)
//
// x64 note: the console is a 4-byte-pointer build, so mEyeSpace lands at exactly +0x758.
// Here sizeof(ICETake) differs (its interior pointers widen), so the byte offsets are NOT
// reproduced -- parity is BY NAMED MEMBER, per the project's x64 gate. Declaration ORDER is
// the DWARF's.
// ============================================================================

namespace BrnDirector
{

// The director resource manager Prepare resolves takes through (its ICE resource manager,
// its editor author and its on-disk ICE dictionary list). Declared by name only -- the full
// home is GameSource/Director/BrnDirectorResourceManager.h, which this header deliberately
// does NOT pull in (the manager includes the whole attrib-vault vocabulary).
class DirectorResourceManager;

// DWARF BrnKeyAnimController.h:45.
class KeyAnimController : public ShotController
{
public:
    // ------------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------------

    // Bring the controller up empty (BrnKeyAnimController.cpp:38). Declaration-only: it is
    // its own X360 function and lands with the controller-construct slice.
    void Construct();

    // Bind the controller to the take named by liAnimGuid, resolved through the director
    // resource manager: the ICE editor's edited copy first, then the on-disk ICE dictionary.
    // Seeds the playback take, rewinds it to parameter 0, caches the eye/look reference
    // spaces and the authored look position. Always returns true. @0x821F7E10 (cpp:62).
    bool Prepare(const DirectorResourceManager& lrResourceManager, s32 liAnimGuid);

    // ------------------------------------------------------------------------
    // The per-frame entry point (ShotController's virtual)
    // ------------------------------------------------------------------------

    // Advance the playback timer by the context's GAME timestep (honouring pause / reverse /
    // looping), clamp it into the take, publish it as the camera's running time, seek the
    // take, then write the whole camera out of the take. @0x8223D020 (cpp:224).
    virtual void Update(const ShotContext& lrContext, Camera::Camera* lpCamera);

    // ------------------------------------------------------------------------
    // Playback state
    // ------------------------------------------------------------------------

    // The bound take's authored length in seconds (0 when nothing is bound). Header-inline
    // in the original (h:69) -- every caller has it folded in.
    f32 GetLength() const
    {
        const ICE::ICETakeData* lpTakeData = mPlaybackTake.GetData();
        return lpTakeData ? lpTakeData->GetLength() : 0.0f;
    }

    // Seek to lfParametricTime0To1 (0..1) of the take. @0x821F80F8 (cpp:331).
    void SetParametricTime0To1(f32 lfParametricTime0To1);

    // The normalised playback parameter, clamped to [0,1]. @0x8220AD50 (cpp:357).
    f32 GetParametricTime0To1() const;

    // h:85 / h:88 / h:91 -- header-inline playback controls.
    void Reverse() { mbReversed = !mbReversed; }
    void Pause()   { mbPaused = true; }
    void Resume()  { mbPaused = false; }

    // True once the take has run off its end (forward: timer >= length; reverse: timer <= 0).
    // @0x821F4258 (h:155).
    bool HasFinished() const;

    // h:97 / h:101 / h:104 -- header-inline state accessors.
    bool IsPrepared() const           { return mbPrepared; }
    void SetLooping(bool lbLooping)   { mbIsLooping = lbLooping; }
    bool IsLooping() const            { return mbIsLooping; }

    // The take's current eye / look reference-space selectors. @0x821F4138 / @0x821F4190
    // (h:107 / h:110). BehaviourIceAnim::Update switches on these to decide which vehicle
    // the produced camera anchors to.
    ICE::eICESpace GetEyeSpace() const;
    ICE::eICESpace GetLookSpace() const;

    // The take's current authored look-at point. @0x821F41E8 (h:113). Returned BY VALUE --
    // the X360 copies the 16-byte lane out of +0x10 into the caller's sret slot.
    rw::math::vpu::Vector3 GetLookPos() const;

    // The live take itself (BehaviourIceAnim::GetTimeRemaining reads its bound take data).
    const ICE::ICETake& GetTake() const { return mPlaybackTake; }

private:
    // ------------------------------------------------------------------------
    // The four take->camera writers. All FOUR ARE STATIC: the X360 passes the ICETake in r3
    // with no instance register left over (UpdateCameraFromICE is called as
    // `UpdateCameraFromICE(this+0x20, ctx->mpCameraSpaceHandler, camera)`), and they touch no
    // controller member. Same precedent as PerlinShakeController::Update
    // (BrnPerlinShakeController.h) -- the asm arbitrates the calling convention.
    // ------------------------------------------------------------------------

    // Sample every camera-facing element of the take into the camera. @0x8221E630 (cpp:288).
    static void UpdateCameraFromICE(const ICE::ICETake& lrTake,
                                    const ICE::CameraSpaceHandler& lrSpaces,
                                    Camera::Camera* lpCamera);

    // The camera's world transform: project the authored eye/look points through their
    // reference spaces, build the look-at frame, roll it by the authored dutch angle, and
    // publish the look point as the camera's subject. @0x8221E2C8 (cpp:98).
    static void UpdateTransformationMatrix(const ICE::ICETake& lrTake,
                                           const ICE::CameraSpaceHandler& lrSpaces,
                                           Camera::Camera* lpCamera);

    // The camera's depth-of-field band, from the take's raw-focus channel. @0x821F7F50
    // (cpp:154). (The ledger homes this function under BrnDepthOfField.h; that is a TU-path
    // misattribution -- the DWARF puts it in BrnKeyAnimController.cpp, where it lives here.)
    static void UpdateFocus(const ICE::ICETake& lrTake, Camera::Camera* lpCamera);

    // The camera's FOV, from the take's authored lens length. @0x821F8068 (cpp:200).
    static void UpdateLens(const ICE::ICETake& lrTake, Camera::Camera* lpCamera);

    // ---- Layout (DWARF member order) ----------------------------------------
    rw::math::vpu::Vector3 mLookPos;          // h:133
    ICE::ICETake           mPlaybackTake;     // h:135
    ICE::eICESpace         mEyeSpace;         // h:136
    ICE::eICESpace         mLookSpace;        // h:137
    f32                    mfPlaybackTimer;   // h:138
    bool                   mbIsLooping;       // h:140
    bool                   mbPrepared;        // h:141
    bool                   mbPaused;          // h:142
    bool                   mbReversed;        // h:143
};

} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_SHOTS_SHOTCONTROLLERS_BRN_KEY_ANIM_CONTROLLER_H
