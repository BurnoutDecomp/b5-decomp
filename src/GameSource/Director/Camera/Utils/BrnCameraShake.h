#ifndef GAMESOURCE_DIRECTOR_CAMERA_UTILS_BRN_CAMERA_SHAKE_H
#define GAMESOURCE_DIRECTOR_CAMERA_UTILS_BRN_CAMERA_SHAKE_H

#include "types.hpp"
#include "BrnCommonTypes.h"                            // Matrix44Affine
#include "GameShared/GameClasses/Numeric/CgsRandom.h"  // CgsNumeric::Random

// ============================================================================
// GameSource/Director/Camera/Utils/BrnCameraShake.h
//
// CANONICAL HOME for the director camera SHAKE family:
//   BrnDirector::Camera::Utils::CameraShake              (+ ::Parameters)
//   BrnDirector::Camera::Utils::CameraImpactEffect       (+ ::Parameters)
//   BrnDirector::Camera::Utils::CameraShakeICEController
//
// WHY THIS FILE EXISTS: the DecFIGS DWARF names BrnCameraShake.h as the home of all four
// types, but the tree only ever carried private SLICES of CameraShake -- one inside
// Behaviours/BehaviourRig.h (four f32 + Parameters, complete) and one inside
// Behaviours/BrnBehaviourIceAnim.h (a 16-byte reserved span). CameraShakeICEController had
// NO definition anywhere at all, which is why the two SHARED GAMEPLAY CAMERA behaviours
// (BehaviourGameplayBumper / BehaviourGameplayExternal), both of which embed one by value,
// could not be re-based onto Camera::Behaviour and kept a raw `void* mpVTable` head instead.
// That fork is what made SharedCameraContainer::Prepare crash: placement-new into a pool slot
// installs no vtable for a non-polymorphic class, so BehaviourHelper::Prepare's slot-0
// dispatch read a null vptr.
//
// LAYOUT AUTHORITY: the DecFIGS DWARF for BrnCameraShake.h (CameraShake :56/:74-:77,
// Parameters :83/:86-:90, CameraImpactEffect :105/:126-:127, CameraShakeICEController
// :160/:179-:186). Member ORDER and NAMES are the DWARF's; the two console SIZES that are
// independently pinned are quoted per type below.
//
// x64 NOTE: parity here is BY NAMED MEMBER (the project rule). No consumer indexes any of
// these by offset; the console displacements quoted below are provenance only.
// ============================================================================

namespace BrnDirector
{
    // Threaded into CameraShakeICEController::Update by pointer (it resolves the shake take
    // out of the director's authored data).
    class DirectorResourceManager;

namespace Camera
{
    struct Camera;   // Camera/Camera.h -- CameraImpactEffect::Update writes into one

namespace Utils
{
    // The engine spells the RNG unqualified `Random` inside Camera::Utils. This is the SAME
    // typedef BrnLooker.h declares (a redundant typedef to the same type is well-formed);
    // it is repeated here so this header stands alone.
    typedef CgsNumeric::Random Random;

    // ------------------------------------------------------------------------
    // CameraShake (DWARF BrnCameraShake.h:56) -- adds XY/Z wobble to a camera matrix each
    // frame. Four f32 of live wobble state; console size 0x10.
    //
    // MOVED HERE (2026-07-29) from Behaviours/BehaviourRig.h, byte-identically. That copy is
    // retired in favour of this home (BehaviourRig.h now includes this file), because
    // BrnBehaviourManager.cpp pulls BehaviourRig.h (through BrnBehaviourAftertouchCam.h) AND
    // the two gameplay-camera headers in one TU -- two definitions of
    // Camera::Utils::CameraShake in one TU is C2011.
    // ------------------------------------------------------------------------
    struct CameraShake
    {
        // DWARF BrnCameraShake.h:83. The authored tunings sub-block; embedded by value in
        // every behaviour parameter block that shakes (aftertouch cam/crash, bystander cam,
        // gameplay bumper, gameplay external -- each of whose serialisers drives it as one
        // nested "Shake Params" section).
        struct Parameters
        {
            // X360 visitor: `void Serialise<S>(S&)` (camera-tunings
            // TextFile{Read,Write}Serialiser / DebugMenuSerialiser). Per-instance body is
            // its own TU.
            template<class TSerialiser> void Serialise(TSerialiser& lrSerialiser);

            f32  mfXYShakeMagnitudeDegs;   // :86  +0x00
            f32  mfZShakeMagnitudeDegs;    // :87  +0x04
            f32  mfXYWobbleMagnitudeDegs;  // :89  +0x08
            f32  mfWobbleCenteringFactor;  // :90  +0x0C

            void Construct();              // :93  (declaration-only -- its own ledger fn)
        };

        // :62 -- clear the live wobble state. BODIED (inline) and X360-attested: the console
        // INLINES it at every owner's Construct, and BehaviourGameplayBumper::Construct
        // @0x82242418 shows the whole body -- `*(this+32)=*(this+36)=*(this+40)=*(this+44)=0.0f`
        // over the shake it embeds at +0x20. BehaviourGameplayExternal::Construct @0x82224A18
        // emits the identical quadruple twice (its +0x2A0 air shake and +0x2B0 impact shake).
        void Construct()
        {
            mfCurrentWobbleX    = 0.0f;
            mfCurrentWobbleY    = 0.0f;
            mfCurrentWobbleXVel = 0.0f;
            mfCurrentWobbleYVel = 0.0f;
        }

        // ⭐ :70 / @0x82221310 -- advance the wobble one frame and fold it into lrTransform.
        // BODIED 2026-08-01 in BrnCameraShake.cpp, from the full 300-line asm; the banner
        // there carries the walk, the draw identification and the arity recovery.
        // ⚠️ The signature below is CONFIRMED against the asm register-by-register (r3 this /
        //   r4 transform / r5 params / r6 random / f1 the wobble integration timestep / f2 the
        //   scale on the final angle). The two f32 NAMES are the pre-existing committed ones
        //   and remain INFERRED -- see the note in the .cpp; `lfSpeedRatio` reads more like a
        //   shake amount at the two call sites that were checked.
        // ⚠️ NOT dormant: SIXTEEN console callers dispatch it (every camera behaviour's
        //   Update, ImpactShakeController, KeyAnimShakeController and
        //   BehaviourGameplayExternal::ApplyJumpEffects). The old "nothing dispatches it"
        //   note described the PC build's gating, not the console.
        void Update(Matrix44Affine& lrTransform, const Parameters& lrParams,
                    Random& lrRandom, f32 lfTimestep, f32 lfSpeedRatio);

    private:
        f32  mfCurrentWobbleX;             // :74  +0x00
        f32  mfCurrentWobbleY;             // :75  +0x04
        f32  mfCurrentWobbleXVel;          // :76  +0x08
        f32  mfCurrentWobbleYVel;          // :77  +0x0C
    };

    // NOTE -- the DWARF's third BrnCameraShake.h type, CameraImpactEffect (:105, members
    // :126/:127; Parameters :133), is NOT redeclared here: it already has a committed home at
    // Utils/BrnCameraImpactEffect.h (with its own TU bodying RegisterImpact @0x821F3648 and
    // the three Parameters::Serialise<S> visitors). That home now includes THIS file for
    // CameraShake instead of reaching through Behaviours/BehaviourRig.h -- which is what its
    // own FLAG asked for ("adopt the real member when CameraShake migrates to its canonical
    // BrnCameraShake.h home").

    // ------------------------------------------------------------------------
    // CameraShakeICEController (DWARF BrnCameraShake.h:160) -- the ICE-authored shake: it
    // either runs an authored ICETake ("shake take") resolved through the director resource
    // manager, or falls back to the procedural CameraShake, and publishes the result as a
    // matrix the owning behaviour post-multiplies.
    //
    // ⭐ CONSOLE SIZE 0x7E0 (2016) -- pinned, not guessed: BehaviourGameplayBumper embeds one
    // between mImpactShake and mLastCameraAngles, and the surrounding offsets are all
    // asm-attested (mpParameters @+0x828 from SetParameters @0x821F39C0, walking back the
    // three trailing members mfDampenedAcceleration/+0x824, mfLastSpeed/+0x820 and the
    // 16-byte-aligned Vector3 mLastCameraAngles/+0x810; the controller therefore spans
    // +0x30..+0x80F).
    // ------------------------------------------------------------------------
    struct CameraShakeICEController
    {
        void Construct();                                                     // :164

        // :172 -- advance the take (or the procedural fallback) one frame.
        void Update(const BrnDirector::DirectorResourceManager* lpResourceManager,
                    f32 lfTimestep, u8 lu8ActiveShake, f32 lfMagnitude, f32 lfFrequency);

        const Matrix44Affine& GetMatrix() const;                              // :175

    private:
        // FLAG (named opaque sub-object): ICE::ICETake has no homed layout anywhere in the
        //   tree (the ICE take/resource cluster is entirely gated -- see
        //   GameSource/Director/DirectorLinkStubs.cpp group C). It is modelled as a NAMED,
        //   sized sub-object -- never a console window: nothing reads or writes inside it,
        //   and its interior is not fabricated. Its SIZE is derived, not invented: it is
        //   whatever makes this controller match the console's pinned 0x7E0 (see the banner),
        //   i.e. 0x7E0 - (0x40 mMatrix + 0x10 mProceduralShake + 0x10 mProceduralShakeParams
        //   + 0x10 for the mu8ActiveShake/mRandom/mfShotRunningTime/mfBumpValue tail) = 0x770.
        //   DELETE-WHEN: ICE::ICETake gets a homed layout -- then this becomes
        //   `ICE::ICETake mShakeTake;`.
        struct OpaqueICETake { u8 maOpaque[0x770]; };

        Matrix44Affine          mMatrix;                 // :179  +0x000  (64B, 16-aligned)
        CameraShake             mProceduralShake;        // :180  +0x040
        CameraShake::Parameters mProceduralShakeParams;  // :181  +0x050
        OpaqueICETake           mShakeTake;              // :182  +0x060
        u8                      mu8ActiveShake;          // :183
        Random                  mRandom;                 // :184
        f32                     mfShotRunningTime;       // :185
        f32                     mfBumpValue;             // :186
    };

} // namespace Utils
} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_UTILS_BRN_CAMERA_SHAKE_H
