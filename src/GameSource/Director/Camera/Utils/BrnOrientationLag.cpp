#include "GameSource/Director/Camera/Utils/BrnOrientationLag.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"     // CGS_ASSERT
#include "rw/math/vpu/matrix44affine_operation.h"      // rw::math::vpu::SLerp

// BrnDirector::Camera::Utils::OrientationLag -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (DWARF primary file GameSource/Director/Camera/Utils/BrnOrientationLag.cpp):
//   OrientationLag::Update @0x82222FB8  (called by BehaviourFailsafe::Update and
//                                        BehaviourRig::Update)
//   and the four the X360 only ever inlines: Construct, SetParameters, GetTransform,
//   Parameters::Construct (each with the site it was read off).
//
// X360 asm walk: assert mpParameters (cpp:60; the assert does not early-out), then:
//   mbFirstFrame set   -> mLastTransform = lrTransform (4 row copies), clear the flag;
//   mbUseSlerpSpring   -> broadcast mpParameters->mfSlerpSpring and call
//                         rw::math::vpu::SLerp(mLastTransform, lrTransform, that amount,
//                         &angleOut) -- the same vendor op, and the same four-argument
//                         shape, as the reviewed BrnLooker::Track site. The angle-out
//                         slot is a stack local that is written and never read. Copy the
//                         blended rows into mLastTransform, then overwrite the w row with
//                         lrTransform's (the store of the blended w row is immediately
//                         re-stored from lrTransform: translation always snaps).
//                         The frame delta is NOT part of this call: nothing forwards
//                         lfTimestep into the blend, so the spring amount is used raw.
//   otherwise          -> mLastTransform = lrTransform (straight copy, flag untouched).

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{
    // @ 0x82222FB8
    void OrientationLag::Update(f32 lfTimestep, const rw::math::vpu::Matrix44Affine& lrTransform)
    {
        CGS_ASSERT(mpParameters != NULL, "mpParameters != NULL");

        if (mbFirstFrame)
        {
            mLastTransform = lrTransform;
            mbFirstFrame   = false;
        }
        else if (mpParameters->mbUseSlerpSpring)
        {
            // Spherically blend the held orientation toward the new transform by the
            // slerp spring; the translation row snaps to the new transform. The frame
            // delta plays no part in the blend -- see the banner.
            (void)lfTimestep;
            rw::math::vpu::Vector3 lUnusedAngle;
            const rw::math::vpu::Matrix44Affine lBlended =
                rw::math::vpu::SLerp(mLastTransform, lrTransform,
                                     mpParameters->mfSlerpSpring, &lUnusedAngle);

            mLastTransform       = lBlended;
            mLastTransform.wAxis = lrTransform.wAxis;
        }
        else
        {
            mLastTransform = lrTransform;
        }
    }

    // Point the lag at a caller-owned tunables block. The console never emits this as a
    // standalone symbol -- every embedder inlines it -- so it is transcribed from the one
    // site that shows it whole: BehaviourRig::Prepare stores
    // &parameters->mOrientationLagParams into the lag's parameter slot and only THEN runs
    // the null assert, which carries this file's own assert text.
    void OrientationLag::SetParameters(const Parameters* lpParameters)
    {
        mpParameters = lpParameters;
        CGS_ASSERT(mpParameters != NULL, "mpParameters != NULL");
    }

    // The lagged output transform (DWARF BrnOrientationLag.h:65). Inlined at its one read site,
    // BehaviourRig::Update @0x822427C0, as the header's own tripwire and then the four row loads:
    //     lbz r11, 0x44(lag) ; beq skip        -- mbFirstFrame
    //     assert "!mbFirstFrame"                 (BrnOrientationLag.h:103, 0x82242C80..0x82242C9C)
    //     lvx128 x4 off lag+0x00                 (mLastTransform)
    // [FX-DIRECTOR2 2026-09-25] the tripwire was missing. It fires when the lag is read before
    // the first Update has seeded it.
    const rw::math::vpu::Matrix44Affine& OrientationLag::GetTransform() const
    {
        CGS_ASSERT(!mbFirstFrame, "!mbFirstFrame");   // BrnOrientationLag.h:103
        return mLastTransform;
    }

    // DWARF BrnOrientationLag.h:53. The PS3 build keeps an out-of-line body
    // (._ZN11BrnDirector6Camera5Utils14OrientationLag9ConstructEv @0x168C8: +64 = 0, +68 = 1); the
    // X360 inlines the same two stores into BehaviourRig::Construct @0x82242488 on the lag at rig
    // +0x2C0 -- `stw 0, 0x300(r31)` (0x82242700) and `stb 1, 0x304(r31)` (0x82242704). The held
    // transform is NOT cleared: the first Update snaps it.
    void OrientationLag::Construct()
    {
        mpParameters = NULL;
        mbFirstFrame = true;
    }

    // DWARF BrnOrientationLag.h:90 / cpp:28. PS3 @0x168B4: `+16 = 0.050000001 ; +20 = 1`. The X360
    // inlines it into BehaviourRig::Parameters::Construct @0x821F9680 on the block at +0xC4:
    // `stfs flt_820047C8 (0.05), 0xD4(r3)` (0x821F96AC) and `stb 1, 0xD8(r3)` (0x821F96B0). The three
    // axis springs and muVersion are left alone.
    void OrientationLag::Parameters::Construct()
    {
        mfSlerpSpring    = 0.05f;   // flt_820047C8 == 0x3D4CCCCD
        mbUseSlerpSpring = true;
    }
}
}
}
