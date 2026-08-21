#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3 / Matrix33 / Matrix44Affine / VecFloat (rw::math::vpu)

// ⛔ LOAD-BEARING INCLUDE, same hazard CameraUtils.h documents at length: `VecFloat` inside
// `namespace BrnDirector` resolves to TWO different types depending on include order -- the
// global `typedef rw::math::vpu::Vector4 VecFloat` from BrnCommonTypes.h, or
// `BrnDirector::VecFloat` from BrnDirectorTimestep.h -- and the two MANGLE DIFFERENTLY. The
// Interpolate overloads below were declaration-only until now, so no TU ever had to agree
// with another; the moment they got bodies (BrnInterpolater.cpp, which reaches
// BrnDirectorTimestep.h through CameraUtils.h) a caller that had not seen that header would
// have emitted a different symbol and read as a missing body rather than a type mismatch.
// Forcing it here puts every consumer on BrnDirector::VecFloat.
#include "GameSource/Director/Utils/BrnDirectorTimestep.h"   // BrnDirector::VecFloat

// BrnDirector::Camera::Utils::Interpolater - a direction-preserving orientation
// interpolator: it slerps between two rotations while remembering the rotation axis it
// used last frame (mLastAxis) and whether it had to invert it (mbWasInvertedLastTime),
// so successive frames never flip to the antipodal axis mid-blend. DWARF home
// BrnInterpolater.h:47.
//
// Both Interpolate overloads are DECLARATION-ONLY (their bodies -- the X360
// BrnDirector::Camera::Utils::DirectionPreservingSLerp helper, which receives
// &mLastAxis / &mbWasInvertedLastTime as its two trailing state pointers -- are their
// own ledger functions). CameraInterpolationController embeds two of these by value and
// RotateAboutPivotParams::Interpolate @0x8221E9D0 drives one per matrix pair.
namespace BrnDirector
{
namespace Camera
{
namespace Utils
{
    struct Interpolater
    {
        // DWARF h:51 -- reset the remembered axis state.
        void Construct();

        // DWARF h:57 / h:63 -- direction-preserving slerp between two orientations by
        // lvT, updating the remembered axis state.
        Matrix33       Interpolate(Matrix33 lFrom, Matrix33 lTo, VecFloat lvT);
        Matrix44Affine Interpolate(Matrix44Affine lFrom, Matrix44Affine lTo, VecFloat lvT);

        // The console reaches these two as LOOSE POINTERS -- CameraInterpolationController::
        // Update hands DirectionPreservingSLerp this object and this+0x10 directly rather
        // than going through an Interpolate method (asm 0x82251520 method-0 arm). Exposed by
        // name so that call site stays off raw offsets.
        Vector3& GetLastAxis() { return mLastAxis; }
        bool&    GetWasInvertedLastTime() { return mbWasInvertedLastTime; }

    private:
        Vector3 mLastAxis;              // h:67 (+0x00)
        bool    mbWasInvertedLastTime;  // h:68 (+0x10)
    };
}
}
}
