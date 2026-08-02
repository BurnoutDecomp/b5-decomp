#ifndef GAMESOURCE_DIRECTOR_CAMERA_UTILS_BRN_CAMERA_SPHERICAL_ROTATION_CONTROLLER_H
#define GAMESOURCE_DIRECTOR_CAMERA_UTILS_BRN_CAMERA_SPHERICAL_ROTATION_CONTROLLER_H

// BrnDirector::Camera::Utils::CameraSphericalRotationController -- the free-look
// spherical (yaw + pitch) rotation controller driven by a look stick.
//
// Provenance: class shape / member names / method set verbatim from the DecFIGS DWARF
// (GameSource/Director/Camera/Utils/BrnCameraSphericalRotationController.{h,cpp}).
// This TU bodies Update @0x8223F808; Construct and the Get*/Is* query family are their
// own ledger functions (declaration-only here -- the per-TU `cl /c` gate does not link,
// so a declaration is all a not-yet-done callee needs).
//
// Member byte offsets pinned by the Update asm (base ptr r31 = this):
//   +0x00 mStickVector (Vector2, 16-byte SIMD slot; `stvx128 v1,r0,r31`)
//   +0x10 mfYawDegs         (0x10)     +0x14 mfYawVelocity   (0x14)
//   +0x18 mfYawReturnRate   (0x18)     +0x1C mfUnRotatedTime (0x1C)
//   +0x20 mbIsLookback      (0x20)     +0x21 mbWasLookbackLastFrame (0x21)
//   +0x22 mbIsRotated       (0x22)     +0x24 mPitchMover (SmoothMover, `addi r3,r31,0x24`)

#include "types.hpp"
#include "BrnCommonTypes.h"                                   // Vector2 alias
#include "GameSource/Director/Camera/Utils/BrnCameraSmoothMover.h"  // SmoothMover mPitchMover

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{
    // DWARF BrnCameraSphericalRotationController.h:57.
    struct CameraSphericalRotationController
    {
        // ⭐ DWARF h:61 -- BODIED 2026-08-01 (orbit-camera wave), in this class's own .cpp.
        // It used to be declaration-only here AND an EMPTY STUB in
        // GameSource/Director/DirectorLinkStubs.cpp, whose justification ("nothing reads
        // either one ... the only path that would dispatch it is gated") EXPIRED twice over:
        // the post-scene behaviour pass was un-gated on 2026-08-01, and
        // BehaviourRotateAboutVehicle::BecomeSimilarTo calls it on the live car-select path
        // specifically to throw away accumulated stick state -- with the empty stub the stale
        // rotation survived every re-seat.
        void Construct();

        // ---- the query family (DWARF h:73..:103) ------------------------------------
        // ⭐ THE FOUR THE ORBIT CAMERA NEEDS ARE BODIED INLINE BELOW (2026-08-01). Each is a
        // single load off a member this header already names at its asm-attested offset, and
        // each is INLINED at every console call site -- BehaviourRotateAboutVehicle::Update
        // @0x822494E0..0x82249518 emits `lbz +0x20` / `lfs +0x10` / `lfs +0x2C` directly, and
        // @0x82249454 emits `lvx128 +0x00` on the shared info's own controller. Bodying them
        // inline is what keeps that behaviour off raw offsets; leaving them declaration-only
        // would have made every one an unresolved external.
        // ⚠️ ONE WITNESS EACH. They are one-line member fetches whose names match the members
        //   exactly, so the risk is naming-only -- but GetPitchRotationAngleDegs reaching
        //   mPitchMover.mfCurrentValue (controller +0x2C) is the one worth re-checking if a
        //   second witness ever disagrees.
        bool    IsLookback() const { return mbIsLookback; }                       // +0x20
        f32     GetYawRotationAngleDegs() const { return mfYawDegs; }             // +0x10
        f32     GetPitchRotationAngleDegs() const { return mPitchMover.GetCurrentValue(); } // +0x2C
        Vector2 GetRawStickVector() const { return mStickVector; }                // +0x00

        // ⭐⭐ THE RADIANS PAIR IS BODIED AS OF 2026-08-02 (final-helpers wave), and NOT by
        // assuming it is "the Degs one times pi/180" -- the LOOKBACK SPECIAL CASE lives
        // inside these two and would have been lost by that assumption.
        // Attested from BehaviourGameplayExternal::ApplySlideyEffects, which inlines both:
        //   X360 @0x82226650  lbz this+0x40 (== controller +0x20, mbIsLookback)
        //        @0x8222665C  lookback  -> 0.0f
        //        @0x82226664  otherwise -> lfs this+0x4C (== mPitchMover.mfCurrentValue)
        //        @0x8222667C  fmuls by flt_82001744 (DUMPED 0.017453292 == pi/180)
        //   X360 @0x822266B0  the same byte again
        //        @0x822266D4  lookback  -> flt_820025FC (DUMPED 180.0f)
        //        @0x822266E0  otherwise -> lfs this+0x30 (== mfYawDegs)
        //        @0x822266E8  fmuls by the same pi/180
        // ⭐ CORROBORATED BY THE OTHER BUILD AT THE ACCESSOR'S OWN SOURCE LINES: DecFIGS
        // charges those loads to BrnCameraSphericalRotationController.h:82 and :88 -- i.e.
        // the work really is inside these two members, not open-coded in the caller -- and it
        // shows the PS3 compiler having CONSTANT-FOLDED both lookback arms (yaw's arm loads
        // PI outright, pitch's folds to zero), which is the same source read two ways.
        // ⇒ lookback flips the camera to look BACKWARDS (180 degs of yaw) and levels the
        //   pitch, and it does so for every consumer of these two, not just the chase cam.
        f32 GetYawRotationAngleRads() const
        {
            const f32 KF_DEGS_TO_RADS = 0.017453292f;   // flt_82001744, dumped
            return (mbIsLookback ? 180.0f : mfYawDegs) * KF_DEGS_TO_RADS;
        }

        f32 GetPitchRotationAngleRads() const
        {
            const f32 KF_DEGS_TO_RADS = 0.017453292f;   // flt_82001744, dumped
            return (mbIsLookback ? 0.0f : mPitchMover.GetCurrentValue()) * KF_DEGS_TO_RADS;
        }

        // ⭐⭐ THE LOOKBACK EDGE PAIR + IsRotated ARE BODIED AS OF 2026-08-02 (final-camera
        // wave). All three are inlined at every console call site -- they have NO standalone
        // symbol on either export -- and BehaviourGameplayExternal::UpdateLooking inlines all
        // three, which is where the shapes below are read from:
        //   IsRotated()                    X360 @0x82225A3C  `lbz r11, 0x40+2(this)`, i.e.
        //                                  controller +0x22 == mbIsRotated, tested against 0.
        //   IsEndingLookbackThisFrame()    X360 @0x82225EE8..0x82225F08 AND @0x82225FF4..
        //                                  0x82226018 -- TWO independent inlinings in the one
        //                                  function, identical both times:
        //                                    lbz +0x20 ; bne -> 0   (mbIsLookback  => false)
        //                                    lbz +0x21 ; bne -> 1   (mbWasLookback => true)
        //   IsStartingLookbackThisFrame()  X360 @0x82225F14..0x82225F34 AND @0x82226024..
        //                                  0x82226040 -- likewise twice, the mirror image:
        //                                    lbz +0x20 ; beq -> 0   (!mbIsLookback => false)
        //                                    lbz +0x21 ; bne -> 0   ( mbWasLookback => false)
        // ⭐ CORROBORATED BY THE OTHER BUILD AT THE ACCESSORS' OWN SOURCE LINES: DecFIGS
        // charges both edge tests to BrnCameraSphericalRotationController.h:100..:103 (an
        // `[inlined ...h:100..103 x7]` run immediately before BehaviourGameplayExternal.cpp
        // :761 and again before :782) -- so the work really is inside these two members.
        // ⇒ the pair is a RISING/FALLING EDGE on mbIsLookback, which Update's own frame
        //   bookkeeping (mbWasLookbackLastFrame, written by ::Update @0x8223F808) provides.
        //   Both are used as "do not smooth across this frame" latches by the chase camera.
        bool IsRotated() const { return mbIsRotated; }                            // +0x22
        bool IsStartingLookbackThisFrame() const                                  // +0x20/+0x21
        {
            return mbIsLookback && !mbWasLookbackLastFrame;
        }
        bool IsEndingLookbackThisFrame() const                                    // +0x20/+0x21
        {
            return !mbIsLookback && mbWasLookbackLastFrame;
        }

        // Still declaration-only (their own ledger functions; no caller in this tree yet).
        Vector2  GetAdjustedStickVector() const;
        f32      GetUnRotatedTime() const;

        // @0x8223F808 (this TU, DWARF h:70) -- advance one frame: stash the look stick,
        // update lookback edge flags, drive yaw (free-spring when live, integrated when
        // paused) and pitch (via mPitchMover), then refresh the rotated / un-rotated-time
        // tracking.
        void Update(f32 lfTimestep, Vector2 lStick, bool lbLookback, bool lbPaused,
                    f32 lfMinPitch, f32 lfMaxPitch);

    private:
        Vector2     mStickVector;             // h:111  +0x00
        f32         mfYawDegs;                 // h:112  +0x10
        f32         mfYawVelocity;             // h:113  +0x14
        f32         mfYawReturnRate;           // h:114  +0x18
        f32         mfUnRotatedTime;           // h:115  +0x1C
        bool        mbIsLookback;             // h:116  +0x20
        bool        mbWasLookbackLastFrame;   // h:117  +0x21
        bool        mbIsRotated;              // h:118  +0x22
        SmoothMover mPitchMover;              // h:120  +0x24
    };
}
}
}

#endif // GAMESOURCE_DIRECTOR_CAMERA_UTILS_BRN_CAMERA_SPHERICAL_ROTATION_CONTROLLER_H
