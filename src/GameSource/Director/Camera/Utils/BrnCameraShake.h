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

            // ⭐ :93 -- BODIED 2026-08-01 (orbit-camera wave). It used to be declaration-only
            // ("its own ledger fn"), which made it an unresolved external for any caller.
            // The console has NO standalone body for it: it is INLINED at every one of the
            // nineteen sites that seed a shake block, and the four defaults are attested by
            // the three rodata literals those sites all load
            // (flt_820047B8 = 0.06, flt_82001CC0 = 0.0, flt_820047BC = 1.15,
            //  flt_820047C0 = 0.11), in this member order.
            // THREE INDEPENDENT WITNESSES, two of them already committed in this tree:
            //   BehaviourGameplayExternal::Parameters::Construct  (BrnBehaviourGameplayExternal.cpp:58-61)
            //   BehaviourGameplayBumper::Parameters::Construct    (BrnBehaviourGameplayBumper.cpp:59-62)
            //   BehaviourRotateAboutVehicle::Parameters::Construct @0x821FB328..0x821FB358
            // Each of the three then OVERRIDES a subset immediately afterwards, which is how
            // the seed can be told apart from the caller's own tunings: the seed is the store
            // that is dead (overwritten) at the sites that re-tune, and live at the ones that
            // do not.
            void Construct()
            {
                mfXYShakeMagnitudeDegs  = 0.06f;   // stfs flt_820047B8
                mfZShakeMagnitudeDegs   = 0.0f;    // stfs flt_82001CC0
                mfXYWobbleMagnitudeDegs = 1.15f;   // stfs flt_820047BC
                mfWobbleCenteringFactor = 0.11f;   // stfs flt_820047C0
            }
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
    // runs an authored ICETake ("shake take") resolved through the director resource manager,
    // adds a procedural vibration to the take's pitch, and publishes the result as a matrix
    // the owning behaviour post-multiplies.
    // ⚠️ THE OLD BANNER SAID "or falls back to the procedural CameraShake". IT DOES NOT --
    //   the procedural value is the take arm's INPUT and has exactly one consumer, inside the
    //   arm. When a gate trips the console publishes nothing at all. Corrected 2026-08-02.
    //
    // ⭐ ALL THREE OF ITS METHODS ARE NOW BODIED (2026-08-02, ICE-shake wave):
    //   ::Construct -- COMPLETE, in Camera/Utils/BrnCameraShakeICEController.cpp. It used to
    //     be an empty `{}` in Director/DirectorLinkStubs.cpp whose stated reason ("a real
    //     console call whose body was never dumped") was FALSE: 0x8223EBF0 is a fully exported
    //     186-line function. That stub was ARMED, not merely incomplete -- see below.
    //   ::Update    -- HEAD + THE THREE GATES, arm flagged and made LOUD. Same TU.
    //   ::GetMatrix -- inline below.
    //
    // ⭐ CONSOLE SIZE 0x7E0 (2016) -- pinned, not guessed: BehaviourGameplayBumper embeds one
    // between mImpactShake and mLastCameraAngles, and the surrounding offsets are all
    // asm-attested (mpParameters @+0x828 from SetParameters @0x821F39C0, walking back the
    // three trailing members mfDampenedAcceleration/+0x824, mfLastSpeed/+0x820 and the
    // 16-byte-aligned Vector3 mLastCameraAngles/+0x810; the controller therefore spans
    // +0x30..+0x80F).
    //
    // ⭐⭐ SCOUTED 2026-08-02 (final-camera wave) FROM ::Update's OWN ASM @0x8223EEC8, which
    // is the first time anything has read INSIDE this type. Two things came out of it:
    //
    // (1) THE INTERNAL SPLIT IN THIS HEADER IS WRONG, and the tail is where it is wrong.
    //     ::Update touches, by X360 offset off `this`:
    //       +0x798  `stb r23, 0x798(r30)`        -> mu8ActiveShake  (the last statement)
    //       +0x7A0  `addi r11, r30, 0x7A0` then `lfsx/stwx r8,r11` (idx*4), `ld/std 0x20(r11)`
    //               (a u64 multiplied by 0x5851F42D4C957F2D and incremented) and
    //               `lwz 0x28(r11); addi 1; clrlwi ...,29` (index &= 7)
    //               -> mRandom: EIGHT floats +0x7A0..+0x7BF, u64 seed +0x7C0, u32 index +0x7C8.
    //               That is exactly CgsNumeric::Random's eight-slot ring, the same shape the
    //               rotate-helper wave pinned from Random::Construct/RandomFloat.
    //       +0x7D0  `lfs/fadds/stfs` with the timestep  -> mfShotRunningTime
    //       +0x7D4  the integrated procedural bump      -> mfBumpValue
    //     ⇒ mShakeTake is +0x060..+0x797 (0x738), NOT 0x770, and the tail is 0x48 bytes, not
    //       0x10. The two still sum to 0x7E0 (0x798 +1, pad to 8 for the u64 seed, +0x30
    //       Random, +4 +4, pad to 16 for mMatrix), which is why nothing has noticed: this
    //       build matches BY NAMED MEMBER, so only the PROVENANCE was wrong.
    //     ⭐⭐ SETTLED 2026-08-02, AND BY A SECOND, INDEPENDENT WITNESS: `ICE::ICETake` DOES
    //       have a homed layout in this tree -- SDKs/Packages/ICE/ICEData.hpp:202 (DWARF
    //       ICEData.hpp:236) -- and its last two members are `void* mUndoList[2]` @0x72C and
    //       `u32 muUndoUsedBytes` @0x734, i.e. a console sizeof of EXACTLY 0x738. Two
    //       derivations that share no evidence -- one from which offsets ::Update touches off
    //       `this`, one from the ICE SDK's own DWARF member list -- land on the same number.
    //       The size below is corrected to 0x738 and is no longer "whatever makes the total
    //       come out right".
    //     ⭐ ::Construct @0x8223EBF0 WAS re-checked against these offsets while bodying it,
    //       and it agrees store for store (+0x40..+0x4C zeroed, +0x50..+0x5C the four params,
    //       +0x798 the byte, +0x7A0 the ring, +0x7D0/+0x7D4 the two f32, +0x00..+0x3F the
    //       identity). Nothing writes inside mShakeTake.
    //
    // (2) THE ICE ARM IS A SELF-CONTAINED EARLY-OUT, and the procedural value is its INPUT --
    //     NOT, as the banner above says, a fallback that gets published on its own.
    //     ⚠️ THAT CORRECTION MATTERS, and I got it wrong on the first read before checking
    //     where the value goes. ::Update computes the procedural bump FIRST
    //     (0x8223EEE4..0x8223F010: one Random draw off the ring below, an integrate-and-clamp
    //     into mfBumpValue, scaled by flt_82CDAD48/4C and by lfMagnitude, broadcast into
    //     lane 0 of v118), and v118's only consumer is INSIDE the arm --
    //     `vaddfp128 v1, v1, v118` @0x8223F6C8. Nothing publishes it by itself.
    //     Then three conditions are tested in a row, each jumping STRAIGHT to the epilogue
    //     (0x8223F7F0, which is nothing but the register restore):
    //         lfMagnitude == 0.0f                                   (0x8223F014)
    //         lu8ActiveShake == 0                                   (0x8223F020)
    //         lu8ActiveShake > Attrib::Gen::shotgroup::Num_ShotList (0x8223F034)
    //     ⇒ WHEN ANY GATE TRIPS THE CONSOLE PUBLISHES NOTHING: `r27 == this+0x30` is not even
    //       formed until 0x8223F624, inside the arm, so mMatrix keeps its previous value and
    //       even mu8ActiveShake (stored at 0x8223F7E0) is left alone. Before the gates the
    //       function writes ONLY mfShotRunningTime, mfBumpValue and the Random ring.
    //     ⇒ so a wave that needs this to LINK can body the head + the three gates and FLAG
    //       the arm, and that partial is BEHAVIOURALLY IDENTICAL to the console on every
    //       frame a gate trips -- it differs only when an authored take actually resolves.
    //       That is a defensible documented partial; it is NOT a silent drop, and it must be
    //       written as such. ⚠️ Update .cpp:445 passes ln8ShakeType == 2, i.e. non-zero, so
    //       the arm WOULD be taken whenever the vehicle's shotgroup resolves.
    //     ⭐ AND ::GetMatrix IS REALLY CONSUMED: Update inlines it as four `lvx128` off
    //       mBoostShake immediately after the call (0x82241C70..0x82241C8C) and multiplies
    //       the result into the camera transform. It is a one-line `return mMatrix;` and it
    //       is a hard link dependency exactly like ::Update.
    // ------------------------------------------------------------------------
    struct CameraShakeICEController
    {
        // :164 / @0x8223EBF0 -- BODIED 2026-08-02 in Camera/Utils/BrnCameraShakeICEController.cpp.
        void Construct();

        // :172 / @0x8223EEC8 -- advance the shot clock and the procedural vibration, then run
        // the authored take. BODIED 2026-08-02 as a DOCUMENTED PARTIAL (head + the three
        // gates; the ICE take arm is flagged AND announces itself once at runtime). Same TU.
        //
        // ⚠️ THE PARAMETER NAMES BELOW ARE THE DWARF'S OWN. The two f32 used to be called
        //   lfMagnitude / lfFrequency here, which was a guess. THREE witnesses now: the
        //   DecFIGS PS3 disassembly labels the registers with these names; the PS3 mangling
        //   gives the types; and `work postmortem GameSource/Director/Camera/Utils/
        //   BrnCameraShake.cpp` prints the whole declaration, names and all.
        // ⚠️ THE ARITY IS ASM-RECOVERED, not Hex-Rays'. The X360 map is r3 this /
        //   r4 manager / f1 timestep / r6 the u8 / f2 amplitude / f3 frequency -- r5 is
        //   SKIPPED because the first float argument still consumes its GPR slot, and that is
        //   what pins the u8 to the FOURTH parameter. The PS3 mangling agrees outright:
        //   `...6UpdateEPKNS_23DirectorResourceManagerEfhff`.
        void Update(const BrnDirector::DirectorResourceManager* lpDirectorResourceManager,
                    f32 lfTimestep, u8 lu8ShakeType, f32 lfShakeAmplitude, f32 lfShakeFrequency);

        // :175 -- the published take-space matrix. BODIED 2026-08-02, inline, because that is
        // where both consoles put it: it has no standalone symbol on either export, and
        // BehaviourGameplayExternal::Update inlines it as four `lvx128` off mBoostShake at
        // 0x82241C70..0x82241C8C before multiplying the result into the camera transform.
        // ⚠️ WHICH IS WHY Construct() MATTERS. Left to the pool's `new (slot) T()`
        //   zero-initialisation this returns the ALL-ZERO matrix, and an all-zero matrix does
        //   not "do nothing" to a post-multiply -- it annihilates the camera transform.
        //   Construct() sets it to the identity.
        const Matrix44Affine& GetMatrix() const { return mMatrix; }

    private:
        // FLAG (named opaque sub-object) -- ⭐ ITS SIZE IS NOW PINNED BY TWO INDEPENDENT
        //   WITNESSES, and it is no longer "whatever makes the total come out right":
        //     (a) which offsets ::Update @0x8223EEC8 touches off `this` (the tail starts at
        //         +0x798, so the take ends at +0x797 -> 0x738), and
        //     (b) `ICE::ICETake`'s OWN homed layout at SDKs/Packages/ICE/ICEData.hpp:202,
        //         whose last members are `void* mUndoList[2]` @0x72C + `u32 muUndoUsedBytes`
        //         @0x734 -> sizeof 0x738.
        //   Still a NAMED, sized sub-object rather than a console window: nothing in this
        //   class reads or writes inside it, and its interior is not fabricated.
        //   ⛔ DELETE-WHEN: the ICE take arm of ::Update lands. The swap to a real
        //   `ICE::ICETake mShakeTake;` is DEFERRED, not blocked -- ICEData.hpp is a heavy
        //   include and this header is pulled by six camera headers (BehaviourRig.h,
        //   BrnBehaviourGameplayBumper.h, BrnBehaviourGameplayExternal.h, BrnBehaviourIceAnim.h,
        //   BrnBehaviourRotateAboutVehicle.h, BrnCameraImpactEffect.h), so the swap should be
        //   made by the wave that actually needs to reach inside the take, and measured then.
        //   ⚠️ On x64 the real ICETake is LARGER than 0x738 (its pointers widen). That is
        //   expected and harmless: parity in this build is BY NAMED MEMBER, and nothing
        //   indexes this class by offset.
        struct OpaqueICETake { u8 maOpaque[0x738]; };

        Matrix44Affine          mMatrix;                 // :179  +0x000  (64B, 16-aligned)
        CameraShake             mProceduralShake;        // :180  +0x040
        CameraShake::Parameters mProceduralShakeParams;  // :181  +0x050
        OpaqueICETake           mShakeTake;              // :182  +0x060..+0x797 (0x738)
        u8                      mu8ActiveShake;          // :183  +0x798  stb, ::Update's last store
        Random                  mRandom;                 // :184  +0x7A0  ring +0x7A0, seed +0x7C0, idx +0x7C8
        f32                     mfShotRunningTime;       // :185  +0x7D0
        f32                     mfBumpValue;             // :186  +0x7D4
    };

} // namespace Utils
} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_UTILS_BRN_CAMERA_SHAKE_H
