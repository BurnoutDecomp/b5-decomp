#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                    // Matrix44Affine (rw::math::vpu)
#include "GameSource/Director/Camera/Utils/CameraUtils.h"      // Utils::VersionNumber

// BrnDirector::Camera::Utils::OrientationLag - a per-frame orientation smoother: it
// holds the last output transform and, when the slerp spring is enabled, spherically
// blends it toward each new input transform (translation always snaps). DWARF home
// BrnOrientationLag.h:47. Update is bodied in BrnOrientationLag.cpp (this TU);
// Construct/SetParameters/GetTransform and Parameters::Construct are their own ledger
// functions (declaration-only here).
namespace BrnDirector
{
namespace Camera
{
namespace Utils
{
    struct OrientationLag
    {
        // DWARF BrnOrientationLag.h:77. The tunables block (X360 offsets pinned by
        // Update's reads: mfSlerpSpring @+0x10 lvlx, mbUseSlerpSpring @+0x14 lbz).
        struct Parameters
        {
            VersionNumber muVersion;        // +0x00 (h:80)
            f32           mfPitchSpring;    // +0x04 (h:82)
            f32           mfYawSpring;      // +0x08 (h:83)
            f32           mfRollSpring;     // +0x0C (h:84)
            f32           mfSlerpSpring;    // +0x10 (h:86)
            bool          mbUseSlerpSpring; // +0x14 (h:87)

            // X360 visitor: `void Serialise<S>(S&)` (camera-tunings TextFile{Read,Write}Serialiser).
            // Per-instance body is a separate TU (BrnOrientationLagSerialise.cpp).
            template<class TSerialiser> void Serialise(TSerialiser& lrSerialiser);

            // DWARF h:90 / cpp:28 -- declaration-only (its own ledger function).
            void Construct();
        };

        // DWARF h:53 / h:57 / h:65 -- declaration-only (their own ledger functions).
        void Construct();
        void SetParameters(const Parameters* lpParameters);
        const rw::math::vpu::Matrix44Affine& GetTransform() const;

        // @0x82222FB8 (this TU, DWARF h:62 / cpp:58) -- fold the new transform into the
        // lagged output: first frame snaps, then either slerp-spring blend (rotation
        // only; translation snaps) or a straight copy.
        void Update(f32 lfTimestep, const rw::math::vpu::Matrix44Affine& lrTransform);

    private:
        rw::math::vpu::Matrix44Affine mLastTransform;   // +0x00 (h:69)
        const Parameters*             mpParameters;      // +0x40 (h:70)
        bool                          mbFirstFrame;      // +0x44 (h:71)
    };
}
}
}
