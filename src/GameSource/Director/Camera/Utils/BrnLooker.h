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

            // Reset every field to its construction default (asm @0x821F8D80).
            void Construct();

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

        // DWARF BrnLooker.h:118. Default-init the runtime state (declaration-only here:
        // this TU exports no standalone body for it -- it is inlined at every behaviour's
        // Construct site, so no X360 asm body is attributed to this TU).
        void Construct();

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
