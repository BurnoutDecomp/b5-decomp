#ifndef GAMESOURCE_DIRECTOR_CAMERA_UTILS_BRN_LOOKER_H
#define GAMESOURCE_DIRECTOR_CAMERA_UTILS_BRN_LOOKER_H

// BrnDirector::Camera::Utils::Looker -- the camera "look-at + zoom" post-process.
//
// Provenance: X360 (Track @0x82222680, Zoom @0x82222A78, Update @0x8223FBB8,
// Parameters::Construct @0x821F8D80) is the spine; the member set / declared types /
// the EZoomType enumerators come from the DecFIGS DWARF for
// GameSource/Director/Camera/Utils/BrnLooker.h. The Parameters defaults are pinned
// store-for-store from the Parameters::Construct asm (the only goal-trace-executed
// function in this TU).
//
// The Looker keeps a target FOV / FOV-velocity band and, per frame, either tracks the
// camera orientation toward an adjusted look-at (Track) or drives the FOV toward an
// ideal computed from how big the subject should appear on screen (Zoom).

#include "types.hpp"
#include "BrnCommonTypes.h"                              // Vector2/Vector3/Matrix44Affine/VecFloat aliases
#include "GameShared/GameClasses/Numeric/CgsRandom.h"    // CgsNumeric::Random
#include "GameSource/Director/Camera/Camera.h"           // BrnDirector::Camera::Camera (FOV/transform/DOF API)
#include "GameSource/Director/Camera/Utils/CameraUtils.h" // Utils::AABBox + the FOV/look-at math helpers

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{
    // The Looker takes a Random by value (it forwards it to CreateLookAt's
    // randomised-offset machinery). The engine spells it unqualified `Random`.
    typedef CgsNumeric::Random Random;

    // DWARF BrnLooker.h:57.
    struct Looker
    {
        // DWARF BrnLooker.h:61 -- the editable parameter block. Defaults are pinned from
        // Parameters::Construct @0x821F8D80 (stfs/stb displacements off r3).
        struct Parameters
        {
            // X360 visitor: `void Serialise<S>(S&)` (camera-tunings TextFile{Read,Write}Serialiser).
            // Per-instance body is a separate TU.
            template<class TSerialiser> void Serialise(TSerialiser& lrSerialiser);

            // DWARF BrnLooker.h:99. How the ideal FOV is derived from the subject size.
            enum EZoomType
            {
                E_ZOOM_PERCEIVED_DISTANCE = 0,  // DWARF
                E_ZOOM_SCREEN_AREA        = 1,  // DWARF
                E_ZOOM_SCREEN_REGION      = 2,  // DWARF
            };

            // ⭐ Parameters::Construct @0x821F8D80 -- MOVED HERE FROM BrnLooker.cpp 2026-08-01
            // (orbit-camera wave). Every store is one stfs/stb/stw from the asm; offsets map
            // 1:1 to the DWARF member order. FULLY GROUNDED -- unchanged from the committed
            // out-of-line body, only re-homed.
            //
            // ⚠️ WHY IT IS A HEADER INLINE AND NOT AN OUT-OF-LINE BODY: its owning TU,
            //   BrnLooker.cpp, DOES NOT COMPILE (BrnLooker.cpp:189 calls the three-argument
            //   rw::math::vpu::SLerp that was replaced by the four-argument form long ago and
            //   never re-fitted -- a stale TU nobody noticed because nothing ever linked it).
            //   BehaviourRotateAboutVehicle::Parameters::Construct needs this seed on the live
            //   car-select path, and it is not worth blocking a working camera on an unrelated
            //   TU's rot. Inline, the body materialises only where a caller needs it, which is
            //   the same mount-hazard reasoning BrnVehicleRef.h's VehicleRef::Get carries.
            //   DELETE-WHEN: BrnLooker.cpp is re-fitted and mounted -- then move it back.
            void Construct()
            {
                mfInitialXLookOffsetRange       = 0.0f;          // stfs 0.0  @4
                mfInitialYLookOffsetRange       = 0.0f;          // stfs 0.0  @8
                mfTargetSubjectSize             = 0.25f;         // stfs 0.25 @0xC
                mfTargetSubjectXSize            = 0.25f;         // stfs 0.25 @0x10
                mfTargetSubjectYSize            = 0.25f;         // stfs 0.25 @0x14
                mfTargetSubjectXScreenOffset    = 0.0f;          // stfs 0.0  @0x18
                mfTargetSubjectYScreenOffset    = 0.0f;          // stfs 0.0  @0x1C
                mfTrackingTolerance             = 0.0f;          // stfs 0.0  @0x20
                mfTrackingSpeed                 = 0.2f;          // stfs 0.2  @0x24
                mfTrackingAcceleration          = 0.0099999998f; // stfs      @0x28
                mfMinFOVVelocity                = 120.0f;        // stfs      @0x2C
                mfMaxFOVVelocity                = 250.0f;        // stfs      @0x30
                mfDesiredPerceivedDistance      = 10.0f;         // stfs      @0x34
                mfDistanceToVelocityFactor      = 2.0f;          // stfs      @0x38
                mfToleranceForDistanceFromIdeal = 5.0f;          // stfs      @0x3C
                mfToleranceForDistanceFromTarget= 0.5f;          // stfs      @0x40
                mfOvershootFactor               = 1.1f;          // stfs      @0x44
                mfMinFOV                        = 10.0f;         // stfs      @0x48
                mfMaxFOV                        = 80.0f;         // stfs      @0x4C
                mfIdealFOVVelocityLerpAmount    = 0.5f;          // stfs      @0x50
                mfStaticDOF                     = 0.42500001f;   // stfs      @0x54
                mfStaticFocalLength             = 0.14f;         // stfs      @0x58
                mbUseStaticDOF                  = false;         // stb 0     @0x5C
                mbInitialiseToLookingAtTarget   = true;          // stb 1     @0x5D
                mbInitialiseToZoomedToTarget    = true;          // stb 1     @0x5E
                mbUseZoom                       = false;         // stb 0     @0x5F
                meZoomType                      = E_ZOOM_PERCEIVED_DISTANCE; // stw 0 @0x60
                // mVersion (@0x00) is left at its default-constructed value (no store in the asm).
            }

            // ---- Layout (DWARF member order; offsets X360-pinned from the Construct stfs) ----
            VersionNumber mVersion;                       // +0x00  (not written by Construct)
            f32 mfInitialXLookOffsetRange;                // +0x04  stfs 0.0  @4(r3)
            f32 mfInitialYLookOffsetRange;                // +0x08  stfs 0.0  @8(r3)
            f32 mfTargetSubjectSize;                      // +0x0C  stfs 0.25 @0xC(r3)
            f32 mfTargetSubjectXSize;                     // +0x10  stfs 0.25 @0x10(r3)
            f32 mfTargetSubjectYSize;                     // +0x14  stfs 0.25 @0x14(r3)
            f32 mfTargetSubjectXScreenOffset;             // +0x18  stfs 0.0  @0x18(r3)
            f32 mfTargetSubjectYScreenOffset;             // +0x1C  stfs 0.0  @0x1C(r3)
            f32 mfTrackingTolerance;                      // +0x20  stfs 0.0  @0x20(r3)
            f32 mfTrackingSpeed;                          // +0x24  stfs 0.2  @0x24(r3)
            f32 mfTrackingAcceleration;                   // +0x28  stfs 0.0099999998 @0x28(r3)
            f32 mfMinFOVVelocity;                         // +0x2C  stfs 120.0 @0x2C(r3)
            f32 mfMaxFOVVelocity;                         // +0x30  stfs 250.0 @0x30(r3)
            f32 mfDesiredPerceivedDistance;               // +0x34  stfs 10.0  @0x34(r3)
            f32 mfDistanceToVelocityFactor;               // +0x38  stfs 2.0   @0x38(r3)
            f32 mfToleranceForDistanceFromIdeal;          // +0x3C  stfs 5.0   @0x3C(r3)
            f32 mfToleranceForDistanceFromTarget;         // +0x40  stfs 0.5   @0x40(r3)
            f32 mfOvershootFactor;                        // +0x44  stfs 1.1   @0x44(r3)
            f32 mfMinFOV;                                 // +0x48  stfs 10.0  @0x48(r3)
            f32 mfMaxFOV;                                 // +0x4C  stfs 80.0  @0x4C(r3)
            f32 mfIdealFOVVelocityLerpAmount;             // +0x50  stfs 0.5   @0x50(r3)
            f32 mfStaticDOF;                              // +0x54  stfs 0.42500001 @0x54(r3)
            f32 mfStaticFocalLength;                      // +0x58  stfs 0.14  @0x58(r3)
            bool mbUseStaticDOF;                          // +0x5C  stb 0  @0x5C(r3)
            bool mbInitialiseToLookingAtTarget;           // +0x5D  stb 1  @0x5D(r3)
            bool mbInitialiseToZoomedToTarget;            // +0x5E  stb 1  @0x5E(r3)
            bool mbUseZoom;                               // +0x5F  stb 0  @0x5F(r3)
            EZoomType meZoomType;                         // +0x60  stw 0  @0x60(r3)
        };

        // ⭐ DWARF BrnLooker.h:118 -- BODIED 2026-08-01 (orbit-camera wave). Default-init the
        // runtime state. It used to be declaration-only ("this TU exports no standalone body
        // for it -- it is inlined at every behaviour's Construct site"), which was an accurate
        // diagnosis and an unresolved external for every caller.
        //
        // ⚠️ SINGLE WITNESS. The body below is transcribed from the ONE inlining this wave
        //   read in full: BehaviourRotateAboutVehicle::Construct @0x8222BF98..0x8222BFAC,
        //   which seeds its embedded Looker (behaviour +0x2B0) with exactly six stores --
        //   +0x2B0 (0.0f), +0x2C0 (0.2f) and the four latch bytes +0x2CC..+0x2CF ({1,1,1,0}) --
        //   landing on mfSlerpFactor, mfAssessmentTime and the four bools at this type's
        //   committed offsets. The seven fields NOT written are left at the pool's zero, which
        //   is what a placement-new'd behaviour already has.
        //   WITH ONE WITNESS A CALLER'S OWN RE-TUNE CANNOT BE TOLD APART FROM THE
        //   CONSTRUCTOR'S DEFAULT -- specifically `mfAssessmentTime = 0.2f` could belong to
        //   either. It is transcribed here because the value repeats nowhere else in that
        //   function and because the alternative (leaving this unresolved) is worse.
        //   ⚠️ Any behaviour that Constructs a Looker AND relies on a different
        //   mfAssessmentTime must set it explicitly rather than trusting this seed.
        //   DELETE-WHEN: a second embedder's Construct is transcribed (BehaviourRig @+0x410 or
        //   BehaviourIceAnim @+0x660) and the two are diffed.
        void Construct()
        {
            mfSlerpFactor           = 0.0f;    // stfs flt_82001CC0, +0x00
            mfAssessmentTime        = 0.2f;    // stfs flt_82004744, +0x10
            mbFirstFrame            = true;    // stb 1, +0x1C
            mbAssessingFOV          = true;    // stb 1, +0x1D
            mbConstructed           = true;    // stb 1, +0x1E
            mbForceZoomTargetUpdate = false;   // stb 0, +0x1F
        }

        // DWARF BrnLooker.h:128. The per-frame entry point (@0x8223FBB8). Asserts the
        // looker was constructed, runs Track, then -- when the parameters request zoom --
        // runs Zoom over the target AABB, finally clearing the first-frame latch.
        void Update(VecFloat lvTimeStep,
                    Random lRandom,
                    const Parameters& lrParams,
                    Camera& lrCamera,
                    Matrix44Affine lTarget,
                    Vector3 lTargetVel,
                    AABBox lAABB);

        // DWARF BrnLooker.h:131. Force the next Zoom to re-snap the FOV band to the target.
        // Trivial: sets the +0x1F latch the Zoom reads (mbForceZoomTargetUpdate).
        void ForceZoomTargetUpdate() { mbForceZoomTargetUpdate = true; }

    private:
        // DWARF BrnLooker.h:143. Slerp the camera orientation toward an adjusted look-at
        // (@0x82222680). When the parameters request a fixed offset it builds the look-at
        // with explicit Euler offsets; otherwise it spherically interpolates toward it,
        // capped by the cosine tolerance angle.
        void Track(VecFloat lvTimeStep,
                   Random lRandom,
                   const Parameters& lrParams,
                   Camera& lrCamera,
                   Matrix44Affine lTarget,
                   Vector3 lTargetVel);

        // DWARF BrnLooker.h:153. Drive the FOV toward the ideal that fits the subject to
        // the requested screen size/area (@0x82222A78), with an overshoot/assessment band
        // and optional static depth-of-field.
        void Zoom(VecFloat lvTimeStep,
                  Random lRandom,
                  const Parameters& lrParams,
                  Camera& lrCamera,
                  Matrix44Affine lTarget,
                  Vector3 lTargetVel,
                  AABBox lAABB);

        // ---- Layout (DWARF member order; byte offsets X360-pinned from the Zoom/Update
        // member accesses: floats at +0x00..+0x18, bool latches at +0x1C..+0x1F; the type
        // is 0x20 bytes wide, matching the embedded slice in the behaviour TUs). ----
        f32  mfSlerpFactor;             // +0x00
        f32  mfTargetFOV;               // +0x04  (Zoom: *(this+4))
        f32  mfMaxFOVVelocity;          // +0x08  (Zoom: *(this+8))
        f32  mfFOVVelocity;             // +0x0C
        f32  mfAssessmentTime;          // +0x10  (Zoom: *(this+16))
        f32  mfLastIdealFOV;            // +0x14  (Zoom: *(this+20))
        f32  mfSmoothedIdealFOVVelocity;// +0x18  (Zoom: *(this+24))
        bool mbFirstFrame;              // +0x1C  (Update clears *(this+28) at the end)
        bool mbAssessingFOV;            // +0x1D  (Zoom: *(this+29))
        bool mbConstructed;             // +0x1E  (Update asserts *(this+30))
        bool mbForceZoomTargetUpdate;   // +0x1F  (Zoom: *(this+31))
    };
} // namespace Utils
} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_UTILS_BRN_LOOKER_H
